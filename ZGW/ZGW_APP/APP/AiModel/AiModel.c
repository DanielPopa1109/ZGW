#include "APP/AiModel/AiModel.h"
#include "Com.h"
#include "Os.h"
#include "IfxCpu.h"
#include "Cpu/Std/IfxCpu_Intrinsics.h"
#include "APP/TimeSync/TimeBase.h"
#include "Dem.h"

#define AIMODEL_INPUT_STALE_LIMIT_MS           (AIMODEL_PERIOD_MS * 3u)
#define AIMODEL_MAILBOX_LOCK_TIMEOUT           100000u
#define AIMODEL_INFERENCE_TICKS                (AIMODEL_INFERENCE_PERIOD_MS / AIMODEL_PERIOD_MS)
#define AIMODEL_IMPENDING_OVERCURRENT_CLASS    1u
#define AIMODEL_IMPENDING_OPEN_LOAD_CLASS      2u
#define AIMODEL_IMPENDING_INTERMITTENT_CLASS   3u
#define AIMODEL_DIAG_SNAPSHOT_SIZE             98u
#define AIMODEL_DIAG_SCALE_1000                1000.0f
#define AIMODEL_CONSUMER_FAULT_FAIL_THRESHOLD  0.90f
#define AIMODEL_CONSUMER_FAULT_PASS_THRESHOLD  0.70f
#define AIMODEL_OVERCURRENT_EVIDENCE_MIN       0.85f
#define AIMODEL_OPEN_LOAD_EVIDENCE_MAX         0.20f
#define AIMODEL_INTERMITTENT_EVIDENCE_MAX      0.30f

static AiModel_ResultType AiModel_Mailbox[2u];
static volatile uint32 AiModel_MailboxSeq;
static volatile uint8 AiModel_MailboxIndex;
static IfxCpu_spinLock AiModel_MailboxLock;
static boolean AiModel_MailboxIrqState[6u];
static volatile uint8 AiModel_MailboxLockOwned[6u];

static float32 AiModel_VoltageHistory[BCM_SEQ_LEN];
static float32 AiModel_CurrentHistory[BCM_NUM_CHANNELS][BCM_SEQ_LEN];
static float32 AiModel_ChronologicalVoltage[BCM_SEQ_LEN];
static float32 AiModel_ChronologicalCurrent[BCM_SEQ_LEN];
static float32 AiModel_Input[BCM_INPUT_SIZE];
static float32 AiModel_RawOutput[BCM_OUTPUT_SIZE];
static AiModel_Pdm1SnapshotType AiModel_WorkSnapshot;
static AiModel_ResultType AiModel_WorkResult;
static uint8 AiModel_HistoryWriteIndex;
static uint16 AiModel_HistoryValidCount;
static uint8 AiModel_InferenceDivider;
static uint32 AiModel_LastInputCycle;
static uint32 AiModel_InferenceSequence;
static uint8 AiModel_Initialized;

static const float32 AiModel_ChannelRating_A[BCM_NUM_CHANNELS] =
{
    5.0f, 7.5f, 10.0f, 12.5f, 15.0f, 17.5f, 20.0f, 22.5f, 25.0f, 27.5f,
    30.0f, 32.5f, 35.0f, 37.5f, 40.0f, 45.0f, 50.0f, 55.0f, 60.0f, 65.0f,
    70.0f, 75.0f, 80.0f, 85.0f, 90.0f, 95.0f, 100.0f, 105.0f, 110.0f, 115.0f,
    120.0f, 125.0f, 130.0f, 135.0f, 140.0f, 145.0f, 150.0f, 155.0f, 160.0f, 165.0f,
    170.0f, 175.0f, 180.0f, 185.0f, 190.0f, 195.0f, 200.0f, 205.0f, 210.0f, 215.0f,
    220.0f, 225.0f, 230.0f, 235.0f, 240.0f, 245.0f, 248.0f, 7.5f, 10.0f, 15.0f,
    20.0f, 25.0f, 30.0f, 40.0f, 50.0f, 60.0f, 80.0f, 100.0f, 125.0f, 150.0f,
    175.0f, 200.0f, 225.0f, 230.0f, 250.0f
};

extern long long AiModel_MainFunction_Counter;

static void AiModel_StoreU32(uint8 *buffer, uint16 offset, uint32 value)
{
    buffer[offset] = (uint8)((value >> 24u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)((value >> 16u) & 0xFFu);
    buffer[(uint16)(offset + 2u)] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 3u)] = (uint8)(value & 0xFFu);
}

static void AiModel_StoreS32(uint8 *buffer, uint16 offset, sint32 value)
{
    AiModel_StoreU32(buffer, offset, (uint32)value);
}

static sint32 AiModel_ScaleFloat1000(float32 value)
{
    float32 scaled = value * AIMODEL_DIAG_SCALE_1000;

    if (scaled > 2147483647.0f)
    {
        return 2147483647;
    }

    if (scaled < -2147483648.0f)
    {
        return (-2147483647 - 1);
    }

    return (sint32)scaled;
}

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

static uint32 AiModel_MaxU32(uint32 a, uint32 b)
{
    return (a > b) ? a : b;
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

Std_ReturnType AiModel_GetLatestGlobalResult(AiModel_GlobalResultType *result)
{
    const AiModel_ResultType *mailbox;

    if (result == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (AiModel_EnterMailboxCritical() == FALSE)
    {
        return E_NOT_OK;
    }

    mailbox = &AiModel_Mailbox[AiModel_MailboxIndex];
    result->inputVoltage_V = mailbox->inputVoltage_V;
    result->undervoltage = mailbox->undervoltage;
    result->overvoltage = mailbox->overvoltage;
    result->inputValid = mailbox->inputValid;
    result->inferenceValid = mailbox->inferenceValid;
    result->windowReady = mailbox->windowReady;
    result->dominantFaultChannel = mailbox->dominantFaultChannel;
    result->dominantFault = mailbox->dominantFault;
    result->inputTimestamp = mailbox->inputTimestamp;
    result->inferenceSequence = mailbox->inferenceSequence;
    result->channelExecutionTimeUs = mailbox->channelExecutionTimeUs;
    result->totalInferenceTimeUs = mailbox->totalInferenceTimeUs;
    result->processingTimeUs = mailbox->processingTimeUs;
    result->maxChannelExecutionTimeUs = mailbox->maxChannelExecutionTimeUs;
    result->maxTotalInferenceTimeUs = mailbox->maxTotalInferenceTimeUs;
    result->maxProcessingTimeUs = mailbox->maxProcessingTimeUs;
    result->errorFlags = mailbox->errorFlags;
    AiModel_ExitMailboxCritical();
    return E_OK;
}

Std_ReturnType AiModel_GetLatestChannelResult(uint8 channel, AiModel_ChannelResultType *result)
{
    if ((result == NULL_PTR) || (channel >= BCM_NUM_CHANNELS))
    {
        return E_NOT_OK;
    }

    if (AiModel_EnterMailboxCritical() == FALSE)
    {
        return E_NOT_OK;
    }

    *result = AiModel_Mailbox[AiModel_MailboxIndex].channel[channel];
    AiModel_ExitMailboxCritical();
    return E_OK;
}

Std_ReturnType AiModel_CaptureDiagSnapshotData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length)
{
    AiModel_ResultType result;
    AiModel_ChannelResultType *channelResult;
    uint8 channel;
    uint8 faultIndex;
    uint16 offset;

    if ((buffer == NULL_PTR) ||
        (length == NULL_PTR) ||
        (*length < AIMODEL_DIAG_SNAPSHOT_SIZE))
    {
        return E_NOT_OK;
    }

    if (AiModel_GetLatestResult(&result) != E_OK)
    {
        return E_NOT_OK;
    }

    if ((eventId >= DEM_EVENT_ID_AIMODEL_CONSUMER_FAULT_FIRST) &&
        (eventId <= DEM_EVENT_ID_AIMODEL_CONSUMER_FAULT_LAST))
    {
        channel = (uint8)(eventId - DEM_EVENT_ID_AIMODEL_CONSUMER_FAULT_FIRST);
        if (Dem_Cfg_CaptureTimestampTemperatureData(buffer, length, DEM_SNAPSHOT_KIND_AIMODEL_CONSUMER) != E_OK)
        {
            return E_NOT_OK;
        }
    }
    else
    {
        channel = result.dominantFaultChannel;
        if (Dem_Cfg_CaptureTimestampTemperatureData(buffer, length, DEM_SNAPSHOT_KIND_AIMODEL) != E_OK)
        {
            return E_NOT_OK;
        }
    }

    if (channel >= BCM_NUM_CHANNELS)
    {
        channel = 0u;
    }

    channelResult = &result.channel[channel];
    offset = DEM_DTC_TIMESTAMP_DATA_SIZE;
    AiModel_StoreU32(buffer, offset, (uint32)eventId);
    AiModel_StoreU32(buffer, (uint16)(offset + 4u), result.errorFlags);
    AiModel_StoreS32(buffer, (uint16)(offset + 8u), AiModel_ScaleFloat1000(result.inputVoltage_V));
    buffer[(uint16)(offset + 12u)] = result.inputValid;
    buffer[(uint16)(offset + 13u)] = result.inferenceValid;
    buffer[(uint16)(offset + 14u)] = result.windowReady;
    buffer[(uint16)(offset + 15u)] = channel;
    buffer[(uint16)(offset + 16u)] = channelResult->predictedFaultClass;
    buffer[(uint16)(offset + 17u)] = result.undervoltage;
    buffer[(uint16)(offset + 18u)] = result.overvoltage;
    buffer[(uint16)(offset + 19u)] = channelResult->actualOvercurrent;
    AiModel_StoreU32(buffer, (uint16)(offset + 20u), result.inputTimestamp);
    AiModel_StoreU32(buffer, (uint16)(offset + 24u), result.inferenceSequence);
    AiModel_StoreU32(buffer, (uint16)(offset + 28u), result.channelExecutionTimeUs);
    AiModel_StoreU32(buffer, (uint16)(offset + 32u), result.totalInferenceTimeUs);
    AiModel_StoreU32(buffer, (uint16)(offset + 36u), result.processingTimeUs);
    AiModel_StoreS32(buffer, (uint16)(offset + 40u), AiModel_ScaleFloat1000(channelResult->measuredCurrent_A));
    AiModel_StoreS32(buffer, (uint16)(offset + 44u), AiModel_ScaleFloat1000(channelResult->currentRating_A));
    AiModel_StoreS32(buffer, (uint16)(offset + 48u), AiModel_ScaleFloat1000(channelResult->predictedCurrent_A));
    AiModel_StoreS32(buffer, (uint16)(offset + 52u), AiModel_ScaleFloat1000(channelResult->faultSoonProbability));
    AiModel_StoreS32(buffer, (uint16)(offset + 56u), AiModel_ScaleFloat1000(channelResult->currentUtilization));

    for (faultIndex = 0u; faultIndex < BCM_NUM_FAULT_CLASSES; faultIndex++)
    {
        AiModel_StoreS32(buffer,
                (uint16)(offset + 60u + ((uint16)faultIndex * 4u)),
                AiModel_ScaleFloat1000(channelResult->faultProbability[faultIndex]));
    }

    *length = AIMODEL_DIAG_SNAPSHOT_SIZE;
    return E_OK;
}

static void AiModel_ReportDiagnostics(const AiModel_ResultType *result)
{
    Dem_EventStatusType inputStatus;
    Dem_EventStatusType inferenceStatus;
    Dem_EventStatusType deadlineStatus;
    Dem_EventStatusType rangeStatus;
    Dem_EventStatusType consumerStatus;
    uint8 channel;

    if ((result == NULL_PTR) || (Dem_IsReady() == FALSE))
    {
        return;
    }

    inputStatus = ((result->errorFlags & (AIMODEL_ERROR_INPUT_INVALID | AIMODEL_ERROR_INPUT_STALE)) != 0u) ?
            DEM_EVENT_STATUS_FAILED : DEM_EVENT_STATUS_PASSED;
    inferenceStatus = ((result->windowReady != 0u) &&
            ((result->inferenceValid == 0u) ||
             ((result->errorFlags & AIMODEL_ERROR_INTERNAL_FAILURE) != 0u))) ?
            DEM_EVENT_STATUS_FAILED : DEM_EVENT_STATUS_PASSED;
    deadlineStatus = ((result->errorFlags & AIMODEL_ERROR_DEADLINE_EXCEEDED) != 0u) ?
            DEM_EVENT_STATUS_FAILED : DEM_EVENT_STATUS_PASSED;
    rangeStatus = ((result->errorFlags & AIMODEL_ERROR_OUTPUT_OUT_OF_RANGE) != 0u) ?
            DEM_EVENT_STATUS_FAILED : DEM_EVENT_STATUS_PASSED;

    (void)Dem_ReportErrorStatus(DEM_EVENT_ID_AIMODEL_INPUT_INVALID, inputStatus);
    (void)Dem_ReportErrorStatus(DEM_EVENT_ID_AIMODEL_INFERENCE_INVALID, inferenceStatus);
    (void)Dem_ReportErrorStatus(DEM_EVENT_ID_AIMODEL_DEADLINE_EXCEEDED, deadlineStatus);
    (void)Dem_ReportErrorStatus(DEM_EVENT_ID_AIMODEL_OUTPUT_OUT_OF_RANGE, rangeStatus);

    for (channel = 0u; channel < BCM_NUM_CHANNELS; channel++)
    {
        const AiModel_ChannelResultType *channelResult = &result->channel[channel];

        if ((result->windowReady == 0u) ||
            (result->inputValid == 0u) ||
            (channelResult->inferenceValid == 0u))
        {
            consumerStatus = DEM_EVENT_STATUS_PREPASSED;
        }
        else if ((channelResult->predictedFaultClass == AIMODEL_IMPENDING_OVERCURRENT_CLASS) &&
                 (channelResult->faultSoonProbability >= AIMODEL_CONSUMER_FAULT_FAIL_THRESHOLD) &&
                 (channelResult->currentUtilization >= AIMODEL_OVERCURRENT_EVIDENCE_MIN))
        {
            consumerStatus = DEM_EVENT_STATUS_FAILED;
        }
        else if ((channelResult->predictedFaultClass == AIMODEL_IMPENDING_OPEN_LOAD_CLASS) &&
                 (channelResult->faultSoonProbability >= AIMODEL_CONSUMER_FAULT_FAIL_THRESHOLD) &&
                 (channelResult->currentUtilization <= AIMODEL_OPEN_LOAD_EVIDENCE_MAX))
        {
            consumerStatus = DEM_EVENT_STATUS_FAILED;
        }
        else if ((channelResult->predictedFaultClass == AIMODEL_IMPENDING_INTERMITTENT_CLASS) &&
                 (channelResult->faultSoonProbability >= AIMODEL_CONSUMER_FAULT_FAIL_THRESHOLD) &&
                 (channelResult->currentUtilization <= AIMODEL_INTERMITTENT_EVIDENCE_MAX))
        {
            consumerStatus = DEM_EVENT_STATUS_FAILED;
        }
        else if ((channelResult->predictedFaultClass == 0u) ||
                 (channelResult->faultSoonProbability <= AIMODEL_CONSUMER_FAULT_PASS_THRESHOLD))
        {
            consumerStatus = DEM_EVENT_STATUS_PASSED;
        }
        else
        {
            continue;
        }

        (void)Dem_ReportErrorStatus(
                (Dem_EventIdType)(DEM_EVENT_ID_AIMODEL_CONSUMER_FAULT_FIRST + channel),
                consumerStatus);
    }
}

static void AiModel_ClearHistory(void)
{
    AiModel_HistoryWriteIndex = 0u;
    AiModel_HistoryValidCount = 0u;
    AiModel_InferenceDivider = 0u;
}

static void AiModel_InitMailboxResult(AiModel_ResultType *result)
{
    uint8 channel;

    result->inputVoltage_V = 0.0f;
    result->undervoltage = 0u;
    result->overvoltage = 0u;
    result->inputValid = 0u;
    result->inferenceValid = 0u;
    result->windowReady = 0u;
    result->dominantFaultChannel = 0u;
    result->dominantFault = 0u;
    result->inputTimestamp = 0u;
    result->inferenceSequence = 0u;
    result->channelExecutionTimeUs = 0u;
    result->totalInferenceTimeUs = 0u;
    result->processingTimeUs = 0u;
    result->maxChannelExecutionTimeUs = 0u;
    result->maxTotalInferenceTimeUs = 0u;
    result->maxProcessingTimeUs = 0u;
    result->errorFlags = AIMODEL_ERROR_WINDOW_NOT_READY;

    for (channel = 0u; channel < BCM_NUM_CHANNELS; channel++)
    {
        result->channel[channel].channelId = channel;
        result->channel[channel].measuredCurrent_A = 0.0f;
        result->channel[channel].currentRating_A = AiModel_ChannelRating_A[channel];
        result->channel[channel].predictedCurrent_A = 0.0f;
        result->channel[channel].faultSoonProbability = 0.0f;
        result->channel[channel].faultProbability[0u] = 0.0f;
        result->channel[channel].faultProbability[1u] = 0.0f;
        result->channel[channel].faultProbability[2u] = 0.0f;
        result->channel[channel].faultProbability[3u] = 0.0f;
        result->channel[channel].currentUtilization = 0.0f;
        result->channel[channel].predictedFaultClass = 0u;
        result->channel[channel].impendingOvercurrent = 0u;
        result->channel[channel].actualOvercurrent = 0u;
        result->channel[channel].inferenceValid = 0u;
    }
}

void AiModel_Init(void)
{
    uint8 i;

    AiModel_MailboxSeq = 0u;
    AiModel_MailboxIndex = 0u;
    AiModel_LastInputCycle = 0u;
    AiModel_InferenceSequence = 0u;
    AiModel_ClearHistory();
    AiModel_Initialized = 1u;

    for (i = 0u; i < 2u; i++)
    {
        AiModel_InitMailboxResult(&AiModel_Mailbox[i]);
    }
}

static PduIdType AiModel_CurrentFeedbackPdu(uint8 channel)
{
    return (PduIdType)(COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_1 + (channel / 16u));
}

Std_ReturnType AiModel_GetPdm1Snapshot(AiModel_Pdm1SnapshotType *snapshot)
{
    uint32 rawValue;
    uint8 channel;
    uint8 diagStatus;
    uint32 nowMs;

    if (snapshot == NULL_PTR)
    {
        return E_NOT_OK;
    }

    nowMs = AiModel_NowMs();
    snapshot->signalValid = 1u;

    rawValue = 0u;
    if ((Com_ReceiveSignal(COM_SIG_RX_CANFD_PDM1_INPUTT30_INPUTT30, &rawValue) != E_OK) ||
            (Com_GetRxPduDiagStatus(COM_RX_PDU_CANFD_PDM1_INPUTT30, &diagStatus) != E_OK) ||
            (diagStatus != COM_RX_DIAG_STATUS_OK))
    {
        snapshot->signalValid = 0u;
        snapshot->voltage_V = 0.0f;
    }
    else
    {
        snapshot->voltage_V = (float32)rawValue;
    }

    for (channel = 0u; channel < BCM_NUM_CHANNELS; channel++)
    {
        rawValue = 0u;
        if ((Com_ReceiveSignal((Com_SignalIdType)(COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_001 + channel), &rawValue) != E_OK) ||
                (Com_GetRxPduDiagStatus(AiModel_CurrentFeedbackPdu(channel), &diagStatus) != E_OK) ||
                (diagStatus != COM_RX_DIAG_STATUS_OK))
        {
            snapshot->signalValid = 0u;
            snapshot->current_A[channel] = 0.0f;
        }
        else
        {
            snapshot->current_A[channel] = (float32)rawValue;
        }
    }

    snapshot->timestamp = nowMs;
    snapshot->cycleCounter = (uint32)OS_Counter_core0;
    return E_OK;
}

static uint8 AiModel_SnapshotPlausible(const AiModel_Pdm1SnapshotType *snapshot)
{
    uint8 channel;

    if ((snapshot->signalValid == 0u) ||
            (AiModel_FloatInRange(snapshot->voltage_V, BCM_VOLTAGE_MIN_V, BCM_VOLTAGE_MAX_V) == 0u))
    {
        return 0u;
    }

    for (channel = 0u; channel < BCM_NUM_CHANNELS; channel++)
    {
        if (AiModel_FloatInRange(snapshot->current_A[channel], BCM_CURRENT_MIN_A, BCM_CURRENT_MAX_A) == 0u)
        {
            return 0u;
        }
    }

    return 1u;
}

static void AiModel_PushSample(const AiModel_Pdm1SnapshotType *snapshot)
{
    uint8 channel;

    AiModel_VoltageHistory[AiModel_HistoryWriteIndex] = snapshot->voltage_V;
    for (channel = 0u; channel < BCM_NUM_CHANNELS; channel++)
    {
        AiModel_CurrentHistory[channel][AiModel_HistoryWriteIndex] = snapshot->current_A[channel];
    }

    AiModel_HistoryWriteIndex = (uint8)((AiModel_HistoryWriteIndex + 1u) % BCM_SEQ_LEN);
    if (AiModel_HistoryValidCount < BCM_SEQ_LEN)
    {
        AiModel_HistoryValidCount++;
    }
}

static void AiModel_BuildChronologicalVoltage(void)
{
    uint16 i;

    for (i = 0u; i < BCM_SEQ_LEN; i++)
    {
        uint16 source = (uint16)((AiModel_HistoryWriteIndex + i) % BCM_SEQ_LEN);
        AiModel_ChronologicalVoltage[i] = AiModel_VoltageHistory[source];
    }
}

static void AiModel_BuildChronologicalCurrent(uint8 channel)
{
    uint16 i;

    for (i = 0u; i < BCM_SEQ_LEN; i++)
    {
        uint16 source = (uint16)((AiModel_HistoryWriteIndex + i) % BCM_SEQ_LEN);
        AiModel_ChronologicalCurrent[i] = AiModel_CurrentHistory[channel][source];
    }
}

static uint8 AiModel_DecodedPlausible(const BCM_InferenceResult *decoded)
{
    uint8 i;

    if ((AiModel_FloatInRange(decoded->predicted_current_a, BCM_CURRENT_MIN_A, BCM_CURRENT_MAX_A) == 0u) ||
            (AiModel_FloatInRange(decoded->fault_soon_probability, 0.0f, 1.0f) == 0u) ||
            (decoded->fault_class >= BCM_NUM_FAULT_CLASSES))
    {
        return 0u;
    }

    for (i = 0u; i < BCM_NUM_FAULT_CLASSES; i++)
    {
        if (AiModel_FloatInRange(decoded->fault_probability[i], 0.0f, 1.0f) == 0u)
        {
            return 0u;
        }
    }

    return 1u;
}

static void AiModel_UpdateProtection(AiModel_ResultType *result, const AiModel_Pdm1SnapshotType *snapshot)
{
    BCM_ProtectionStatus status;
    uint8 channel;

    result->undervoltage = (snapshot->voltage_V < BCM_UNDERVOLTAGE_THRESHOLD_V) ? 1u : 0u;
    result->overvoltage = (snapshot->voltage_V > BCM_OVERVOLTAGE_THRESHOLD_V) ? 1u : 0u;

    for (channel = 0u; channel < BCM_NUM_CHANNELS; channel++)
    {
        BCM_EvaluateProtectionStatus(snapshot->voltage_V,
                snapshot->current_A[channel],
                AiModel_ChannelRating_A[channel],
                &status);
        result->channel[channel].measuredCurrent_A = snapshot->current_A[channel];
        result->channel[channel].currentRating_A = AiModel_ChannelRating_A[channel];
        result->channel[channel].currentUtilization = status.current_utilization;
        result->channel[channel].actualOvercurrent = status.overcurrent_now;
    }
}

static void AiModel_RunInference(AiModel_ResultType *result)
{
    BCM_InferenceResult decoded;
    uint8 channel;
    uint8 faultIndex;
    uint32 inferenceStartUs;
    uint32 channelStartUs;
    uint32 elapsedUs;
    float32 highestFaultSoon = -1.0f;

    AiModel_BuildChronologicalVoltage();
    inferenceStartUs = AiModel_NowUs();

    result->inferenceValid = 1u;
    result->dominantFaultChannel = 0u;
    result->dominantFault = 0u;
    result->channelExecutionTimeUs = 0u;

    for (channel = 0u; channel < BCM_NUM_CHANNELS; channel++)
    {
        AiModel_BuildChronologicalCurrent(channel);
        BCM_BuildInput(AiModel_ChronologicalVoltage,
                AiModel_ChronologicalCurrent,
                AiModel_ChannelRating_A[channel],
                AiModel_Input);

        channelStartUs = AiModel_NowUs();
        BCM_Infer(AiModel_Input, AiModel_RawOutput);
        elapsedUs = AiModel_NowUs() - channelStartUs;
        result->channelExecutionTimeUs = AiModel_MaxU32(result->channelExecutionTimeUs, elapsedUs);
        result->maxChannelExecutionTimeUs = AiModel_MaxU32(result->maxChannelExecutionTimeUs, elapsedUs);

        BCM_DecodeOutput(AiModel_RawOutput, &decoded);
        if (AiModel_DecodedPlausible(&decoded) == 0u)
        {
            result->errorFlags |= AIMODEL_ERROR_OUTPUT_OUT_OF_RANGE;
            result->inferenceValid = 0u;
            result->channel[channel].inferenceValid = 0u;
        }
        else
        {
            result->channel[channel].predictedCurrent_A = decoded.predicted_current_a;
            result->channel[channel].faultSoonProbability = decoded.fault_soon_probability;
            result->channel[channel].predictedFaultClass = decoded.fault_class;
            result->channel[channel].impendingOvercurrent =
                    ((decoded.fault_class == AIMODEL_IMPENDING_OVERCURRENT_CLASS) ||
                     (decoded.fault_probability[AIMODEL_IMPENDING_OVERCURRENT_CLASS] >= 0.5f)) ? 1u : 0u;
            result->channel[channel].inferenceValid = 1u;

            for (faultIndex = 0u; faultIndex < BCM_NUM_FAULT_CLASSES; faultIndex++)
            {
                result->channel[channel].faultProbability[faultIndex] = decoded.fault_probability[faultIndex];
            }

            if (decoded.fault_soon_probability > highestFaultSoon)
            {
                highestFaultSoon = decoded.fault_soon_probability;
                result->dominantFaultChannel = channel;
                result->dominantFault = decoded.fault_class;
            }
        }
    }

    result->totalInferenceTimeUs = AiModel_NowUs() - inferenceStartUs;
    result->maxTotalInferenceTimeUs = AiModel_MaxU32(result->maxTotalInferenceTimeUs, result->totalInferenceTimeUs);
    if (result->totalInferenceTimeUs > AIMODEL_WCET_BUDGET_US)
    {
        result->errorFlags |= AIMODEL_ERROR_DEADLINE_EXCEEDED;
    }

    result->inferenceSequence = ++AiModel_InferenceSequence;
}

void AiModel_MainFunction(void)
{
    AiModel_Pdm1SnapshotType *snapshot = &AiModel_WorkSnapshot;
    AiModel_ResultType *result = &AiModel_WorkResult;
    uint32 processingStartUs;

    if (AiModel_Initialized == 0u)
    {
        AiModel_Init();
    }

    processingStartUs = AiModel_NowUs();
    *result = AiModel_Mailbox[AiModel_MailboxIndex];
    result->errorFlags = 0u;
    result->inputValid = 0u;
    result->windowReady = (AiModel_HistoryValidCount >= BCM_SEQ_LEN) ? 1u : 0u;

    if (AiModel_GetPdm1Snapshot(snapshot) != E_OK)
    {
        result->errorFlags |= AIMODEL_ERROR_INPUT_INVALID;
    }
    else
    {
        result->inputVoltage_V = snapshot->voltage_V;
        result->inputTimestamp = snapshot->timestamp;

        if (AiModel_SnapshotPlausible(snapshot) == 0u)
        {
            result->errorFlags |= AIMODEL_ERROR_INPUT_INVALID;
            AiModel_ClearHistory();
        }
        else if ((snapshot->cycleCounter == AiModel_LastInputCycle) ||
                ((uint32)(AiModel_NowMs() - snapshot->timestamp) > AIMODEL_INPUT_STALE_LIMIT_MS))
        {
            result->errorFlags |= AIMODEL_ERROR_INPUT_STALE;
            AiModel_ClearHistory();
        }
        else
        {
            result->inputValid = 1u;
            AiModel_LastInputCycle = snapshot->cycleCounter;
            AiModel_PushSample(snapshot);
            AiModel_UpdateProtection(result, snapshot);
            result->windowReady = (AiModel_HistoryValidCount >= BCM_SEQ_LEN) ? 1u : 0u;
        }
    }

    if (result->windowReady == 0u)
    {
        result->errorFlags |= AIMODEL_ERROR_WINDOW_NOT_READY;
    }
    else if (result->inputValid != 0u)
    {
        AiModel_InferenceDivider++;
        if (AiModel_InferenceDivider >= AIMODEL_INFERENCE_TICKS)
        {
            AiModel_InferenceDivider = 0u;
            AiModel_RunInference(result);
        }
    }

    result->processingTimeUs = AiModel_NowUs() - processingStartUs;
    result->maxProcessingTimeUs = AiModel_MaxU32(result->maxProcessingTimeUs, result->processingTimeUs);
    AiModel_PublishResult(result);
    AiModel_ReportDiagnostics(result);
    AiModel_MainFunction_Counter++;
}
