#include "APP/AiModel/AiModel.h"
#include "Com.h"
#include "Os.h"
#include "IfxCpu.h"
#include "Cpu/Std/IfxCpu_Intrinsics.h"
#include "BSW/Time/TimeBase.h"

#define AIMODEL_VOLTAGE_MIN_V                  0.0f
#define AIMODEL_VOLTAGE_MAX_V                  18.0f
#define AIMODEL_CURRENT_MIN_A                  0.0f
#define AIMODEL_CURRENT_MAX_A                  80.0f
#define AIMODEL_TEMPERATURE_MIN_C              (-40.0f)
#define AIMODEL_TEMPERATURE_MAX_C              175.0f
#define AIMODEL_PDM1_STALE_LIMIT_MS            (AIMODEL_PERIOD_MS * 3u)
#define AIMODEL_MAILBOX_LOCK_TIMEOUT           100000u

typedef struct
{
    float32 voltage[BCM_SEQ_LEN];
    float32 current[BCM_SEQ_LEN];
    float32 temperature[BCM_SEQ_LEN];
    uint8 writeIndex;
    uint8 validCount;
} AiModel_WindowType;

static AiModel_ResultType AiModel_Mailbox[2u];
static volatile uint32 AiModel_MailboxSeq;
static volatile uint8 AiModel_MailboxIndex;
static IfxCpu_spinLock AiModel_MailboxLock;
static boolean AiModel_MailboxIrqState[6u];
static volatile uint8 AiModel_MailboxLockOwned[6u];

static AiModel_WindowType AiModel_Window;
static float32 AiModel_ChronologicalVoltage[BCM_SEQ_LEN];
static float32 AiModel_ChronologicalCurrent[BCM_SEQ_LEN];
static float32 AiModel_ChronologicalTemperature[BCM_SEQ_LEN];
static float32 AiModel_Input[BCM_INPUT_SIZE];
static float32 AiModel_RawOutput[BCM_OUTPUT_SIZE];
static uint32 AiModel_LastPdm1Cycle;
static uint32 AiModel_InferenceSequence;
static uint8 AiModel_Initialized;

extern long long AiModel_MainFunction_Counter;

static uint8 AiModel_IsFinite(float32 value)
{
    return ((value == value) && (value <= 3.402823466e+38f) && (value >= -3.402823466e+38f)) ? 1u : 0u;
}

static uint8 AiModel_FloatInRange(float32 value, float32 min, float32 max)
{
    return ((AiModel_IsFinite(value) != 0u) && (value >= min) && (value <= max)) ? 1u : 0u;
}

static uint32 AiModel_NowMs(void)
{
    return (uint32)(TimeBase_PlatformGetCounterNs() / TIMEBASE_NS_PER_MS);
}

static uint32 AiModel_NowUs(void)
{
    return (uint32)(TimeBase_PlatformGetCounterNs() / 1000ull);
}

static uint8 AiModel_EnterMailboxCritical(void)
{
    uint32 coreIndex = (uint32)IfxCpu_getCoreIndex();

    if (coreIndex >= 6u)
    {
        coreIndex = 0u;
    }

    AiModel_MailboxIrqState[coreIndex] = IfxCpu_disableInterrupts();
    if (IfxCpu_setSpinLock(&AiModel_MailboxLock, AIMODEL_MAILBOX_LOCK_TIMEOUT) == FALSE)
    {
        IfxCpu_restoreInterrupts(AiModel_MailboxIrqState[coreIndex]);
        return FALSE;
    }

    AiModel_MailboxLockOwned[coreIndex] = TRUE;
    __dsync();
    return TRUE;
}

static void AiModel_ExitMailboxCritical(void)
{
    uint32 coreIndex = (uint32)IfxCpu_getCoreIndex();

    if (coreIndex >= 6u)
    {
        coreIndex = 0u;
    }

    __dsync();
    if (AiModel_MailboxLockOwned[coreIndex] != FALSE)
    {
        AiModel_MailboxLockOwned[coreIndex] = FALSE;
        IfxCpu_resetSpinLock(&AiModel_MailboxLock);
    }

    IfxCpu_restoreInterrupts(AiModel_MailboxIrqState[coreIndex]);
}

static void AiModel_PublishResult(const AiModel_ResultType *result)
{
    uint8 nextIndex;

    if (result == NULL_PTR)
    {
        return;
    }

    if (AiModel_EnterMailboxCritical() == FALSE)
    {
        return;
    }

    nextIndex = (AiModel_MailboxIndex == 0u) ? 1u : 0u;
    AiModel_Mailbox[nextIndex] = *result;
    __dsync();
    AiModel_MailboxIndex = nextIndex;
    AiModel_MailboxSeq++;
    AiModel_ExitMailboxCritical();
}

Std_ReturnType AiModel_GetLatestResult(AiModel_ResultType *result)
{
    if (result == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (AiModel_EnterMailboxCritical() == FALSE)
    {
        return E_NOT_OK;
    }

    *result = AiModel_Mailbox[AiModel_MailboxIndex];
    AiModel_ExitMailboxCritical();
    return E_OK;
}

void AiModel_Init(void)
{
    uint8 i;

    AiModel_MailboxSeq = 0u;
    AiModel_MailboxIndex = 0u;
    AiModel_LastPdm1Cycle = 0u;
    AiModel_InferenceSequence = 0u;
    AiModel_Window.writeIndex = 0u;
    AiModel_Window.validCount = 0u;
    AiModel_Initialized = 1u;

    for (i = 0u; i < 2u; i++)
    {
        AiModel_Mailbox[i].dominantFault = BCM_FAULT_CLASS_UNKNOWN;
        AiModel_Mailbox[i].errorFlags = AIMODEL_ERROR_WINDOW_NOT_READY;
    }
}

Std_ReturnType AiModel_GetPdm1Snapshot(AiModel_Pdm1SnapshotType *snapshot)
{
    uint32 voltage;
    uint32 current;
    uint32 temperature;
    uint8 diagStatus;
    uint8 currentDiagStatus;
    uint8 temperatureDiagStatus;
    uint32 nowMs;

    if (snapshot == NULL_PTR)
    {
        return E_NOT_OK;
    }

    nowMs = AiModel_NowMs();
    if ((Com_ReceiveSignal(COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_001, &voltage) != E_OK) ||
            (Com_ReceiveSignal(COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_001, &current) != E_OK) ||
            (Com_ReceiveSignal(COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_001, &temperature) != E_OK) ||
            (Com_GetRxPduDiagStatus(COM_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_1, &diagStatus) != E_OK) ||
            (Com_GetRxPduDiagStatus(COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_1, &currentDiagStatus) != E_OK) ||
            (Com_GetRxPduDiagStatus(COM_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_1, &temperatureDiagStatus) != E_OK) ||
            (diagStatus != COM_RX_DIAG_STATUS_OK) ||
            (currentDiagStatus != COM_RX_DIAG_STATUS_OK) ||
            (temperatureDiagStatus != COM_RX_DIAG_STATUS_OK))
    {
        snapshot->signalValid = 0u;
    }
    else
    {
        snapshot->signalValid = 1u;
    }

    snapshot->voltage_V = (float32)voltage;
    snapshot->current_A = (float32)current;
    snapshot->temperature_C = (float32)((sint32)temperature);
    snapshot->timestamp = nowMs;
    snapshot->cycleCounter = (uint32)OS_Counter_core0;

    return E_OK;
}

static void AiModel_ClearWindow(void)
{
    AiModel_Window.writeIndex = 0u;
    AiModel_Window.validCount = 0u;
}

static void AiModel_PushSample(const AiModel_Pdm1SnapshotType *snapshot)
{
    AiModel_Window.voltage[AiModel_Window.writeIndex] = snapshot->voltage_V;
    AiModel_Window.current[AiModel_Window.writeIndex] = snapshot->current_A;
    AiModel_Window.temperature[AiModel_Window.writeIndex] = snapshot->temperature_C;
    AiModel_Window.writeIndex = (uint8)((AiModel_Window.writeIndex + 1u) % BCM_SEQ_LEN);
    if (AiModel_Window.validCount < BCM_SEQ_LEN)
    {
        AiModel_Window.validCount++;
    }
}

static void AiModel_BuildChronological(float32 voltage[BCM_SEQ_LEN],
        float32 current[BCM_SEQ_LEN],
        float32 temperature[BCM_SEQ_LEN])
{
    uint8 i;

    for (i = 0u; i < BCM_SEQ_LEN; i++)
    {
        uint8 source = (uint8)((AiModel_Window.writeIndex + i) % BCM_SEQ_LEN);
        voltage[i] = AiModel_Window.voltage[source];
        current[i] = AiModel_Window.current[source];
        temperature[i] = AiModel_Window.temperature[source];
    }
}

static uint8 AiModel_ResultPlausible(const BCM_InferenceResult *decoded)
{
    uint8 i;

    if ((AiModel_FloatInRange(decoded->expected_voltage_v, AIMODEL_VOLTAGE_MIN_V, AIMODEL_VOLTAGE_MAX_V) == 0u) ||
            (AiModel_FloatInRange(decoded->expected_current_a, AIMODEL_CURRENT_MIN_A, AIMODEL_CURRENT_MAX_A) == 0u) ||
            (AiModel_FloatInRange(decoded->expected_temperature_c, AIMODEL_TEMPERATURE_MIN_C, AIMODEL_TEMPERATURE_MAX_C) == 0u) ||
            (AiModel_FloatInRange(decoded->anomaly, 0.0f, 1.0f) == 0u) ||
            (AiModel_FloatInRange(decoded->health, 0.0f, 1.0f) == 0u) ||
            (decoded->fault_class >= BCM_NUM_FAULT_CLASSES))
    {
        return FALSE;
    }

    for (i = 0u; i < BCM_NUM_FAULT_CLASSES; i++)
    {
        if (AiModel_FloatInRange(decoded->fault_probability[i], 0.0f, 1.0f) == 0u)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static void AiModel_SetDecodedResult(AiModel_ResultType *result, const BCM_InferenceResult *decoded)
{
    uint8 i;

    result->expectedVoltage_V = decoded->expected_voltage_v;
    result->expectedCurrent_A = decoded->expected_current_a;
    result->expectedTemperature_C = decoded->expected_temperature_c;
    result->anomaly = decoded->anomaly;
    result->health = decoded->health;
    result->dominantFault = decoded->fault_class;
    for (i = 0u; i < BCM_NUM_FAULT_CLASSES; i++)
    {
        result->faultProbability[i] = decoded->fault_probability[i];
    }
}

void AiModel_MainFunction(void)
{
    AiModel_Pdm1SnapshotType snapshot;
    AiModel_ResultType result;
    BCM_InferenceResult decoded;
    uint32 startUs;
    uint32 elapsedUs;

    if (AiModel_Initialized == 0u)
    {
        AiModel_Init();
    }

    result = AiModel_Mailbox[AiModel_MailboxIndex];
    result.errorFlags = 0u;
    result.inputValid = 0u;
    result.inferenceValid = 0u;
    result.windowReady = (AiModel_Window.validCount >= BCM_SEQ_LEN) ? 1u : 0u;
    result.dominantFault = BCM_FAULT_CLASS_UNKNOWN;

    if (AiModel_GetPdm1Snapshot(&snapshot) != E_OK)
    {
        result.errorFlags |= AIMODEL_ERROR_PDM1_INPUT_INVALID;
    }
    else
    {
        result.inputVoltage_V = snapshot.voltage_V;
        result.inputCurrent_A = snapshot.current_A;
        result.inputTemperature_C = snapshot.temperature_C;
        result.inputTimestamp = snapshot.timestamp;

        if ((snapshot.signalValid == 0u) ||
                (AiModel_FloatInRange(snapshot.voltage_V, AIMODEL_VOLTAGE_MIN_V, AIMODEL_VOLTAGE_MAX_V) == 0u) ||
                (AiModel_FloatInRange(snapshot.current_A, AIMODEL_CURRENT_MIN_A, AIMODEL_CURRENT_MAX_A) == 0u) ||
                (AiModel_FloatInRange(snapshot.temperature_C, AIMODEL_TEMPERATURE_MIN_C, AIMODEL_TEMPERATURE_MAX_C) == 0u))
        {
            result.errorFlags |= AIMODEL_ERROR_PDM1_INPUT_INVALID;
            AiModel_ClearWindow();
        }
        else if ((snapshot.cycleCounter == AiModel_LastPdm1Cycle) ||
                ((uint32)(AiModel_NowMs() - snapshot.timestamp) > AIMODEL_PDM1_STALE_LIMIT_MS))
        {
            result.errorFlags |= AIMODEL_ERROR_PDM1_INPUT_STALE;
            AiModel_ClearWindow();
        }
        else
        {
            result.inputValid = 1u;
            AiModel_LastPdm1Cycle = snapshot.cycleCounter;
            AiModel_PushSample(&snapshot);
            result.windowReady = (AiModel_Window.validCount >= BCM_SEQ_LEN) ? 1u : 0u;
        }
    }

    if (result.windowReady == 0u)
    {
        result.errorFlags |= AIMODEL_ERROR_WINDOW_NOT_READY;
    }
    else if (result.inputValid != 0u)
    {
        AiModel_BuildChronological(AiModel_ChronologicalVoltage,
                AiModel_ChronologicalCurrent,
                AiModel_ChronologicalTemperature);
        BCM_BuildInput(AiModel_ChronologicalVoltage,
                AiModel_ChronologicalCurrent,
                AiModel_ChronologicalTemperature,
                AiModel_Input);
        startUs = AiModel_NowUs();
        BCM_Infer(AiModel_Input, AiModel_RawOutput);
        elapsedUs = AiModel_NowUs() - startUs;
        result.executionTimeUs = elapsedUs;
        BCM_DecodeOutput(AiModel_RawOutput, &decoded);

        if (elapsedUs > AIMODEL_WCET_BUDGET_US)
        {
            result.errorFlags |= AIMODEL_ERROR_DEADLINE_EXCEEDED;
        }

        if (AiModel_ResultPlausible(&decoded) == 0u)
        {
            result.errorFlags |= AIMODEL_ERROR_OUTPUT_OUT_OF_RANGE;
            result.inferenceValid = 0u;
        }
        else
        {
            AiModel_SetDecodedResult(&result, &decoded);
            result.inferenceValid = 1u;
            result.inferenceSequence = ++AiModel_InferenceSequence;
        }
    }

    AiModel_PublishResult(&result);
    AiModel_MainFunction_Counter++;
}
