#include "ParallelFlashSwc.h"

#include "CanIf.h"
#include "CanTp.h"
#include "Dcm_Cfg.h"
#include "Ifx_Cfg.h"
#include "LinTp.h"

#include <string.h>

#define PARALLELFLASHSWC_ERROR_NONE                 0u
#define PARALLELFLASHSWC_ERROR_BAD_BUNDLE           1u
#define PARALLELFLASHSWC_ERROR_BUSY                 2u
#define PARALLELFLASHSWC_ERROR_TRANSPORT            3u
#define PARALLELFLASHSWC_ERROR_ZGW_PREREQ_FAILED    4u
#define PARALLELFLASHSWC_FORWARD_MAX_REQUEST        CANTP_MAX_PAYLOAD_LEN
#define PARALLELFLASHSWC_FORWARD_QUEUE_DEPTH        40u
#define PARALLELFLASHSWC_FORWARD_QUEUE_PAYLOAD_LEN  320u
#define PARALLELFLASHSWC_FORWARD_DISPATCH_BUDGET    12u
/* FCD dry-run forwarding must drain even when simulated targets do not send FC. */
#define PARALLELFLASHSWC_FORWARD_ASSUME_CAN_FC      TRUE

typedef struct
{
    ParallelFlashSwc_JobBundleType bundle;
    ParallelFlashSwc_TargetContextType contexts[PARALLELFLASHSWC_MAX_TARGETS];
    ParallelFlashSwc_StatusType status;
    ParallelFlashSwc_PhaseType phase;
    boolean flashPhaseDone;
    boolean codingPhaseDone;
} ParallelFlashSwc_RuntimeType;

typedef struct
{
    uint8 used;
    uint32 sequence;
    GatewaySwc_BusType busType;
    uint8 extendedAddress;
    Dcm_PduLengthType length;
    uint8 data[PARALLELFLASHSWC_FORWARD_QUEUE_PAYLOAD_LEN];
} ParallelFlashSwc_ForwardQueueEntryType;

static ParallelFlashSwc_RuntimeType ParallelFlashSwc_Runtime;
static ParallelFlashSwc_ForwardQueueEntryType ParallelFlashSwc_ForwardQueue[PARALLELFLASHSWC_FORWARD_QUEUE_DEPTH] AURIX_LMU_CACHED_BSS;
static uint32 ParallelFlashSwc_ForwardSequence = 0u;
static uint8 ParallelFlashSwc_ForwardQueueProcessing = FALSE;

/* Correlation map for the routed-response relay. Records the node extended address
 * last forwarded on each diagnostic TP PDU id (the DCM TX and RX PDU id spaces share
 * the same numbering, e.g. DCM_TX_CAN_EXT_PHYS == DCM_RX_CAN_EXT_PHYS == 6), so a
 * slave response that later arrives on DCM_RX_..._EXT_PHYS / DCM_RX_LIN_PHYS can be
 * tagged with the originating node's extended address before the Dcm relays it back
 * to the DoIP tester. */
#define PARALLELFLASHSWC_RESP_EXT_PDU_COUNT 14u
static uint8 ParallelFlashSwc_RespExtByPdu[PARALLELFLASHSWC_RESP_EXT_PDU_COUNT];
static uint8 ParallelFlashSwc_RespExtValid[PARALLELFLASHSWC_RESP_EXT_PDU_COUNT];

/* The most recently forwarded node extended address. The two CanTp channels per CAN
 * bus share one diagnostic RX CAN-ID, so after a single-frame request the channel
 * goes idle and a single-frame reply can surface on the sibling channel's PDU, where
 * the per-PDU record above is absent. The tester serialises routed requests (one
 * outstanding at a time on the DoIP socket), so the last-forwarded address is the
 * correct tag in that case. */
static uint8 ParallelFlashSwc_LastForwardExt;
static uint8 ParallelFlashSwc_LastForwardExtValid;

static const PduIdType ParallelFlashSwc_CanExtTxPdus[] =
{
    DCM_TX_CAN_EXT_PHYS,
    DCM_TX_CAN_EXT_PHYS_2
};

static const PduIdType ParallelFlashSwc_CanFdExtTxPdus[] =
{
    DCM_TX_CANFD_EXT_PHYS,
    DCM_TX_CANFD_EXT_PHYS_2
};

volatile uint8 ParallelFlashSwc_DebugActiveCan = 0u;
volatile uint8 ParallelFlashSwc_DebugActiveCanFd = 0u;
volatile uint8 ParallelFlashSwc_DebugActiveLin = 0u;
volatile uint8 ParallelFlashSwc_DebugCompletedJobs = 0u;
volatile uint8 ParallelFlashSwc_DebugFailedJobs = 0u;
volatile uint32 ParallelFlashSwc_DebugLastError = 0u;
volatile uint32 ParallelFlashSwc_DebugForwardRequests = 0u;
volatile uint32 ParallelFlashSwc_DebugForwardCanOk = 0u;
volatile uint32 ParallelFlashSwc_DebugForwardCanFail = 0u;
volatile uint8 ParallelFlashSwc_DebugForwardLastExt = 0u;
volatile uint8 ParallelFlashSwc_DebugForwardLastSid = 0u;
volatile uint16 ParallelFlashSwc_DebugForwardLastPdu = 0u;
volatile uint8 ParallelFlashSwc_DebugForwardLastResult = E_NOT_OK;
volatile uint32 ParallelFlashSwc_DebugForwardQueued = 0u;
volatile uint32 ParallelFlashSwc_DebugForwardQueueFull = 0u;
volatile uint32 ParallelFlashSwc_DebugForwardDispatched = 0u;
volatile uint32 ParallelFlashSwc_DebugForwardQueueDepth = 0u;
volatile uint32 ParallelFlashSwc_DebugForwardQueueMaxDepth = 0u;
volatile uint32 ParallelFlashSwc_DebugForwardLinBusy = 0u;
volatile uint32 ParallelFlashSwc_DebugForwardLinDropped = 0u;

static Std_ReturnType ParallelFlashSwc_ValidateBundle(const ParallelFlashSwc_JobBundleType *bundle);
static void ParallelFlashSwc_ResetRuntime(void);
static void ParallelFlashSwc_ResetForwardQueue(void);
static void ParallelFlashSwc_UpdateDebug(void);
static void ParallelFlashSwc_ProcessForwardQueue(void);
static void ParallelFlashSwc_SchedulePhase(ParallelFlashSwc_PhaseType phase);
static void ParallelFlashSwc_AdvanceActiveContexts(void);
static boolean ParallelFlashSwc_PhaseComplete(ParallelFlashSwc_PhaseType phase);
static boolean ParallelFlashSwc_NonZgwPhaseFailed(ParallelFlashSwc_PhaseType phase);
static uint8 ParallelFlashSwc_GetActiveBusCount(GatewaySwc_BusType busType);
static boolean ParallelFlashSwc_CanStartTarget(uint8 targetIndex, ParallelFlashSwc_PhaseType phase);
static void ParallelFlashSwc_StartTarget(uint8 targetIndex, ParallelFlashSwc_PhaseType phase);
static void ParallelFlashSwc_SkipZgwTargets(ParallelFlashSwc_PhaseType phase);
static void ParallelFlashSwc_RunContext(ParallelFlashSwc_TargetContextType *context);
static Std_ReturnType ParallelFlashSwc_TransportPrepareSession(const ParallelFlashSwc_TargetConfigType *target);
static Std_ReturnType ParallelFlashSwc_TransportFlashStep(const ParallelFlashSwc_TargetConfigType *target,
                                                         ParallelFlashSwc_TargetContextType *context);
static Std_ReturnType ParallelFlashSwc_TransportCodingStep(const ParallelFlashSwc_TargetConfigType *target,
                                                          ParallelFlashSwc_TargetContextType *context);
static Std_ReturnType ParallelFlashSwc_TransportTesterPresent(const ParallelFlashSwc_TargetConfigType *target);
static Std_ReturnType ParallelFlashSwc_TransportForwardCodingRequest(uint8 extendedAddress,
                                                                    const uint8 *udsRequest,
                                                                    Dcm_PduLengthType udsRequestLength);
static Std_ReturnType ParallelFlashSwc_QueueForwardRequest(uint8 extendedAddress,
                                                           GatewaySwc_BusType busType,
                                                           const uint8 *udsRequest,
                                                           Dcm_PduLengthType udsRequestLength);
static Std_ReturnType ParallelFlashSwc_SendForwardOnBus(GatewaySwc_BusType busType,
                                                        uint8 extendedAddress,
                                                        const uint8 *udsRequest,
                                                        Dcm_PduLengthType udsRequestLength);
static uint8 ParallelFlashSwc_BusCanAcceptForward(GatewaySwc_BusType busType,
                                                  uint8 extendedAddress);
static uint8 ParallelFlashSwc_FindDispatchableForwardEntry(void);
static uint8 ParallelFlashSwc_HasQueuedForwardEntryForAddress(uint8 extendedAddress);
static uint8 ParallelFlashSwc_HasOlderForwardEntryForAddress(uint8 entryIndex);
static Std_ReturnType ParallelFlashSwc_FindForwardBus(uint8 extendedAddress,
                                                      GatewaySwc_BusType *busType);
static Std_ReturnType ParallelFlashSwc_SendDiagnosticFrame(const PduIdType *canTpTxSduIds,
                                                           uint8 canTpTxSduCount,
                                                           uint8 extendedAddress,
                                                           const uint8 *udsRequest,
                                                           Dcm_PduLengthType udsRequestLength);
static Dcm_ReturnType ParallelFlashSwc_BuildForwardNrc(uint8 extendedAddress,
                                                       uint8 sid,
                                                       uint8 nrc,
                                                       uint8 *response,
                                                       Dcm_PduLengthType *responseLength);
static void ParallelFlashSwc_RecordForwardResponseExt(PduIdType pduId, uint8 extendedAddress);

void ParallelFlashSwc_Init(void)
{
    ParallelFlashSwc_ResetRuntime();
    ParallelFlashSwc_ResetForwardQueue();
    ParallelFlashSwc_Runtime.status.initialized = TRUE;
    ParallelFlashSwc_UpdateDebug();
}

void ParallelFlashSwc_MainFunction(void)
{
    if (ParallelFlashSwc_Runtime.status.initialized == FALSE)
    {
        return;
    }

    ParallelFlashSwc_ProcessForwardQueue();

    ParallelFlashSwc_Runtime.status.mainCycles++;

    if (ParallelFlashSwc_Runtime.status.cancelled != FALSE)
    {
        return;
    }

    ParallelFlashSwc_AdvanceActiveContexts();

    if (ParallelFlashSwc_Runtime.flashPhaseDone == FALSE)
    {
        if (ParallelFlashSwc_NonZgwPhaseFailed(PARALLELFLASHSWC_PHASE_FLASH) != FALSE)
        {
            ParallelFlashSwc_SkipZgwTargets(PARALLELFLASHSWC_PHASE_FLASH);
        }
        ParallelFlashSwc_SchedulePhase(PARALLELFLASHSWC_PHASE_FLASH);
        ParallelFlashSwc_Runtime.flashPhaseDone = ParallelFlashSwc_PhaseComplete(PARALLELFLASHSWC_PHASE_FLASH);
    }
    else if (ParallelFlashSwc_Runtime.codingPhaseDone == FALSE)
    {
        if (ParallelFlashSwc_NonZgwPhaseFailed(PARALLELFLASHSWC_PHASE_CODING) != FALSE)
        {
            ParallelFlashSwc_SkipZgwTargets(PARALLELFLASHSWC_PHASE_CODING);
        }
        ParallelFlashSwc_SchedulePhase(PARALLELFLASHSWC_PHASE_CODING);
        ParallelFlashSwc_Runtime.codingPhaseDone = ParallelFlashSwc_PhaseComplete(PARALLELFLASHSWC_PHASE_CODING);
    }

    ParallelFlashSwc_UpdateDebug();
}

Std_ReturnType ParallelFlashSwc_SubmitJob(const ParallelFlashSwc_JobBundleType *bundle)
{
    uint8 i;

    if (bundle == NULL_PTR)
    {
        ParallelFlashSwc_Runtime.status.lastError = PARALLELFLASHSWC_ERROR_BAD_BUNDLE;
        ParallelFlashSwc_UpdateDebug();
        return E_NOT_OK;
    }

    if (ParallelFlashSwc_Runtime.status.activeJobs != 0u)
    {
        ParallelFlashSwc_Runtime.status.lastError = PARALLELFLASHSWC_ERROR_BUSY;
        ParallelFlashSwc_UpdateDebug();
        return E_NOT_OK;
    }

    if (ParallelFlashSwc_ValidateBundle(bundle) != E_OK)
    {
        ParallelFlashSwc_Runtime.status.lastError = PARALLELFLASHSWC_ERROR_BAD_BUNDLE;
        ParallelFlashSwc_UpdateDebug();
        return E_NOT_OK;
    }

    ParallelFlashSwc_ResetRuntime();
    ParallelFlashSwc_ResetForwardQueue();
    ParallelFlashSwc_Runtime.status.initialized = TRUE;
    (void)memcpy(&ParallelFlashSwc_Runtime.bundle, bundle, sizeof(ParallelFlashSwc_JobBundleType));
    ParallelFlashSwc_Runtime.status.acceptedJobs = bundle->targetCount;
    ParallelFlashSwc_Runtime.phase = PARALLELFLASHSWC_PHASE_FLASH;

    for (i = 0u; i < bundle->targetCount; i++)
    {
        ParallelFlashSwc_Runtime.contexts[i].targetIndex = i;
        ParallelFlashSwc_Runtime.contexts[i].phase = PARALLELFLASHSWC_PHASE_FLASH;
        ParallelFlashSwc_Runtime.contexts[i].state = PARALLELFLASHSWC_JOB_ACCEPTED;
    }

    ParallelFlashSwc_UpdateDebug();
    return E_OK;
}

void ParallelFlashSwc_Cancel(void)
{
    uint8 i;

    ParallelFlashSwc_Runtime.status.cancelled = TRUE;
    ParallelFlashSwc_ResetForwardQueue();
    for (i = 0u; i < ParallelFlashSwc_Runtime.bundle.targetCount; i++)
    {
        if (ParallelFlashSwc_Runtime.contexts[i].active != FALSE)
        {
            ParallelFlashSwc_Runtime.contexts[i].active = FALSE;
            ParallelFlashSwc_Runtime.contexts[i].state = PARALLELFLASHSWC_JOB_CANCELLED;
        }
    }
    ParallelFlashSwc_UpdateDebug();
}

Std_ReturnType ParallelFlashSwc_GetStatus(ParallelFlashSwc_StatusType *status)
{
    if (status == NULL_PTR)
    {
        return E_NOT_OK;
    }
    *status = ParallelFlashSwc_Runtime.status;
    return E_OK;
}

Std_ReturnType ParallelFlashSwc_GetTargetContext(uint8 index, ParallelFlashSwc_TargetContextType *context)
{
    if ((context == NULL_PTR) || (index >= ParallelFlashSwc_Runtime.bundle.targetCount))
    {
        return E_NOT_OK;
    }
    *context = ParallelFlashSwc_Runtime.contexts[index];
    return E_OK;
}

Dcm_ReturnType ParallelFlashSwc_ForwardCodingRequest(uint8 extendedAddress,
                                                     Dcm_OpStatusType opStatus,
                                                     const uint8 *udsRequest,
                                                     Dcm_PduLengthType udsRequestLength,
                                                     uint8 *response,
                                                     Dcm_PduLengthType *responseLength)
{
    if ((udsRequest == NULL_PTR) || (response == NULL_PTR) || (responseLength == NULL_PTR) ||
        (udsRequestLength == 0u))
    {
        return ParallelFlashSwc_BuildForwardNrc(extendedAddress,
                                                0x00u,
                                                DCM_NRC_INCORRECT_LENGTH,
                                                response,
                                                responseLength);
    }

    ParallelFlashSwc_DebugForwardRequests++;
    ParallelFlashSwc_DebugForwardLastExt = extendedAddress;
    ParallelFlashSwc_DebugForwardLastSid = udsRequest[0];

    if (opStatus == DCM_INITIAL)
    {
        if (ParallelFlashSwc_TransportForwardCodingRequest(extendedAddress, udsRequest, udsRequestLength) != E_OK)
        {
            return ParallelFlashSwc_BuildForwardNrc(extendedAddress,
                                                    udsRequest[0],
                                                    DCM_NRC_CONDITIONS_NOT_CORRECT,
                                                    response,
                                                    responseLength);
        }

        ParallelFlashSwc_ProcessForwardQueue();
    }

    *responseLength = 0u;
    return DCM_E_OK;
}

void ParallelFlashSwc_BroadcastTesterPresent(void)
{
    /* Every configured bus node, by extended diagnostic address. No functional
     * (broadcast) diagnostic addressing is configured, so a physical TesterPresent is
     * forwarded to each node individually - the same path every routed request uses. */
    static const uint8 nodes[] =
    {
        0x42u, 0x43u, 0x44u, 0x45u,                            /* CAN-FD: PDM1..PDM4        */
        0x50u, 0x51u, 0x52u,                                   /* LIN:    ALT, HVDCDC, PCU48 */
        0x53u, 0x54u, 0x55u, 0x56u, 0x57u, 0x58u, 0x59u        /* CAN:    AGS..FRBE          */
    };
    /* 3E 80: TesterPresent, suppressPositiveResponse - the nodes refresh their session
     * timeout without replying, so no response relay traffic is generated. */
    static const uint8 testerPresent[2] = { 0x3Eu, 0x80u };
    uint8 i;
    uint8 response[4];
    Dcm_PduLengthType responseLength;

    for (i = 0u; i < (uint8)(sizeof(nodes) / sizeof(nodes[0])); i++)
    {
        responseLength = 0u;
        (void)ParallelFlashSwc_ForwardCodingRequest(nodes[i],
                                                     DCM_INITIAL,
                                                     testerPresent,
                                                     2u,
                                                     response,
                                                     &responseLength);
    }
}

static Std_ReturnType ParallelFlashSwc_ValidateBundle(const ParallelFlashSwc_JobBundleType *bundle)
{
    uint8 i;

    if ((bundle->targetCount == 0u) || (bundle->targetCount > PARALLELFLASHSWC_MAX_TARGETS))
    {
        return E_NOT_OK;
    }

    for (i = 0u; i < bundle->targetCount; i++)
    {
        if (bundle->targets[i].busType == 0u)
        {
            return E_NOT_OK;
        }
        if (bundle->targets[i].flashBlockCount > PARALLELFLASHSWC_MAX_BLOCKS_PER_TARGET)
        {
            return E_NOT_OK;
        }
    }

    return E_OK;
}

static void ParallelFlashSwc_ResetRuntime(void)
{
    (void)memset(&ParallelFlashSwc_Runtime, 0, sizeof(ParallelFlashSwc_Runtime));
    ParallelFlashSwc_Runtime.status.lastError = PARALLELFLASHSWC_ERROR_NONE;
}

static void ParallelFlashSwc_ResetForwardQueue(void)
{
    (void)memset(ParallelFlashSwc_ForwardQueue, 0, sizeof(ParallelFlashSwc_ForwardQueue));
    ParallelFlashSwc_ForwardSequence = 0u;
    ParallelFlashSwc_ForwardQueueProcessing = FALSE;
    ParallelFlashSwc_DebugForwardQueueDepth = 0u;
    ParallelFlashSwc_DebugForwardQueueMaxDepth = 0u;
}

static void ParallelFlashSwc_UpdateDebug(void)
{
    ParallelFlashSwc_Runtime.status.activeCan = ParallelFlashSwc_GetActiveBusCount(GATEWAYSWC_BUS_CAN);
    ParallelFlashSwc_Runtime.status.activeCanFd = ParallelFlashSwc_GetActiveBusCount(GATEWAYSWC_BUS_CANFD);
    ParallelFlashSwc_Runtime.status.activeLin = ParallelFlashSwc_GetActiveBusCount(GATEWAYSWC_BUS_LIN);
    ParallelFlashSwc_DebugActiveCan = ParallelFlashSwc_Runtime.status.activeCan;
    ParallelFlashSwc_DebugActiveCanFd = ParallelFlashSwc_Runtime.status.activeCanFd;
    ParallelFlashSwc_DebugActiveLin = ParallelFlashSwc_Runtime.status.activeLin;
    ParallelFlashSwc_DebugCompletedJobs = ParallelFlashSwc_Runtime.status.completedJobs;
    ParallelFlashSwc_DebugFailedJobs = ParallelFlashSwc_Runtime.status.failedJobs;
    ParallelFlashSwc_DebugLastError = ParallelFlashSwc_Runtime.status.lastError;
}

static void ParallelFlashSwc_SchedulePhase(ParallelFlashSwc_PhaseType phase)
{
    uint8 i;

    ParallelFlashSwc_Runtime.status.currentPhase = (uint8)phase;

    for (i = 0u; i < ParallelFlashSwc_Runtime.bundle.targetCount; i++)
    {
        if (ParallelFlashSwc_CanStartTarget(i, phase) != FALSE)
        {
            ParallelFlashSwc_StartTarget(i, phase);
        }
    }
}

static boolean ParallelFlashSwc_CanStartTarget(uint8 targetIndex, ParallelFlashSwc_PhaseType phase)
{
    const ParallelFlashSwc_TargetConfigType *target = &ParallelFlashSwc_Runtime.bundle.targets[targetIndex];
    const ParallelFlashSwc_TargetContextType *context = &ParallelFlashSwc_Runtime.contexts[targetIndex];
    uint8 i;

    if (context->active != FALSE)
    {
        return FALSE;
    }

    if ((context->state != PARALLELFLASHSWC_JOB_ACCEPTED) &&
        !((phase == PARALLELFLASHSWC_PHASE_CODING) && (context->state == PARALLELFLASHSWC_JOB_COMPLETED)))
    {
        return FALSE;
    }

    if (ParallelFlashSwc_GetActiveBusCount(target->busType) >= PARALLELFLASHSWC_MAX_ACTIVE_PER_BUS)
    {
        return FALSE;
    }

    if (target->isZgw != FALSE)
    {
        for (i = 0u; i < ParallelFlashSwc_Runtime.bundle.targetCount; i++)
        {
            if ((ParallelFlashSwc_Runtime.bundle.targets[i].isZgw == FALSE) &&
                ((ParallelFlashSwc_Runtime.contexts[i].phase != phase) ||
                 (ParallelFlashSwc_Runtime.contexts[i].state != PARALLELFLASHSWC_JOB_COMPLETED)))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

static void ParallelFlashSwc_StartTarget(uint8 targetIndex, ParallelFlashSwc_PhaseType phase)
{
    ParallelFlashSwc_TargetContextType *context = &ParallelFlashSwc_Runtime.contexts[targetIndex];

    context->phase = phase;
    context->active = TRUE;
    context->activeBlock = 0u;
    context->progressBytes = 0u;
    context->retryCount = 0u;
    context->resultCode = 0u;
    context->state = PARALLELFLASHSWC_JOB_BUNDLE_VALIDATED;
    ParallelFlashSwc_Runtime.status.activeJobs++;
}

static void ParallelFlashSwc_AdvanceActiveContexts(void)
{
    uint8 i;

    for (i = 0u; i < ParallelFlashSwc_Runtime.bundle.targetCount; i++)
    {
        if (ParallelFlashSwc_Runtime.contexts[i].active != FALSE)
        {
            ParallelFlashSwc_RunContext(&ParallelFlashSwc_Runtime.contexts[i]);
        }
    }
}

static void ParallelFlashSwc_RunContext(ParallelFlashSwc_TargetContextType *context)
{
    const ParallelFlashSwc_TargetConfigType *target = &ParallelFlashSwc_Runtime.bundle.targets[context->targetIndex];
    Std_ReturnType result = E_OK;

    if (context->testerPresentTicks >= 1000u)
    {
        result = ParallelFlashSwc_TransportTesterPresent(target);
        context->testerPresentTicks = 0u;
    }
    else
    {
        context->testerPresentTicks += GATEWAYSWC_MAIN_PERIOD_MS;
    }

    if (result == E_OK)
    {
        if (context->state == PARALLELFLASHSWC_JOB_BUNDLE_VALIDATED)
        {
            context->state = PARALLELFLASHSWC_JOB_NODE_ROUTING_RESOLVED;
        }
        else if (context->state == PARALLELFLASHSWC_JOB_NODE_ROUTING_RESOLVED)
        {
            result = ParallelFlashSwc_TransportPrepareSession(target);
            context->state = (result == E_OK) ? PARALLELFLASHSWC_JOB_SESSION_PREPARED : PARALLELFLASHSWC_JOB_FAILED;
        }
        else if (context->phase == PARALLELFLASHSWC_PHASE_FLASH)
        {
            result = ParallelFlashSwc_TransportFlashStep(target, context);
        }
        else
        {
            result = ParallelFlashSwc_TransportCodingStep(target, context);
        }
    }

    if (result != E_OK)
    {
        context->state = PARALLELFLASHSWC_JOB_FAILED;
        context->resultCode = PARALLELFLASHSWC_ERROR_TRANSPORT;
        ParallelFlashSwc_Runtime.status.lastError = PARALLELFLASHSWC_ERROR_TRANSPORT;
    }

    if ((context->state == PARALLELFLASHSWC_JOB_COMPLETED) ||
        (context->state == PARALLELFLASHSWC_JOB_FAILED) ||
        (context->state == PARALLELFLASHSWC_JOB_CANCELLED) ||
        (context->state == PARALLELFLASHSWC_JOB_SKIPPED))
    {
        if (context->active != FALSE)
        {
            context->active = FALSE;
            if (ParallelFlashSwc_Runtime.status.activeJobs > 0u)
            {
                ParallelFlashSwc_Runtime.status.activeJobs--;
            }
            if (context->state == PARALLELFLASHSWC_JOB_COMPLETED)
            {
                ParallelFlashSwc_Runtime.status.completedJobs++;
            }
            else if (context->state == PARALLELFLASHSWC_JOB_SKIPPED)
            {
                ParallelFlashSwc_Runtime.status.skippedJobs++;
            }
            else
            {
                ParallelFlashSwc_Runtime.status.failedJobs++;
            }
        }
    }
}

static Std_ReturnType ParallelFlashSwc_TransportPrepareSession(const ParallelFlashSwc_TargetConfigType *target)
{
    (void)target;
    return E_OK;
}

static Std_ReturnType ParallelFlashSwc_TransportFlashStep(const ParallelFlashSwc_TargetConfigType *target,
                                                         ParallelFlashSwc_TargetContextType *context)
{
    if (target->flashBlockCount == 0u)
    {
        context->state = PARALLELFLASHSWC_JOB_RESET_VALIDATE;
    }

    switch (context->state)
    {
        case PARALLELFLASHSWC_JOB_SESSION_PREPARED:
            context->state = PARALLELFLASHSWC_JOB_FLASH_ERASE;
            break;
        case PARALLELFLASHSWC_JOB_FLASH_ERASE:
            context->state = PARALLELFLASHSWC_JOB_FLASH_DOWNLOAD;
            break;
        case PARALLELFLASHSWC_JOB_FLASH_DOWNLOAD:
            context->state = PARALLELFLASHSWC_JOB_FLASH_TRANSFER;
            break;
        case PARALLELFLASHSWC_JOB_FLASH_TRANSFER:
            context->progressBytes += target->flashBlocks[context->activeBlock].size;
            context->state = PARALLELFLASHSWC_JOB_FLASH_TRANSFER_EXIT;
            break;
        case PARALLELFLASHSWC_JOB_FLASH_TRANSFER_EXIT:
            context->state = PARALLELFLASHSWC_JOB_FLASH_VERIFY;
            break;
        case PARALLELFLASHSWC_JOB_FLASH_VERIFY:
            context->activeBlock++;
            context->state = (context->activeBlock < target->flashBlockCount) ?
                    PARALLELFLASHSWC_JOB_FLASH_ERASE : PARALLELFLASHSWC_JOB_RESET_VALIDATE;
            break;
        case PARALLELFLASHSWC_JOB_RESET_VALIDATE:
            context->state = PARALLELFLASHSWC_JOB_COMPLETED;
            break;
        default:
            break;
    }

    return E_OK;
}

static Std_ReturnType ParallelFlashSwc_TransportCodingStep(const ParallelFlashSwc_TargetConfigType *target,
                                                          ParallelFlashSwc_TargetContextType *context)
{
    (void)target;

    switch (context->state)
    {
        case PARALLELFLASHSWC_JOB_COMPLETED:
        case PARALLELFLASHSWC_JOB_SESSION_PREPARED:
            context->state = PARALLELFLASHSWC_JOB_CODING_PREPARE;
            break;
        case PARALLELFLASHSWC_JOB_CODING_PREPARE:
            context->state = PARALLELFLASHSWC_JOB_CODING_WRITE;
            break;
        case PARALLELFLASHSWC_JOB_CODING_WRITE:
            context->state = PARALLELFLASHSWC_JOB_CODING_VALIDATE;
            break;
        case PARALLELFLASHSWC_JOB_CODING_VALIDATE:
            context->state = PARALLELFLASHSWC_JOB_RESET_VALIDATE;
            break;
        case PARALLELFLASHSWC_JOB_RESET_VALIDATE:
            context->state = PARALLELFLASHSWC_JOB_COMPLETED;
            break;
        default:
            break;
    }

    return E_OK;
}

static Std_ReturnType ParallelFlashSwc_TransportTesterPresent(const ParallelFlashSwc_TargetConfigType *target)
{
    (void)target;
    return E_OK;
}

static boolean ParallelFlashSwc_PhaseComplete(ParallelFlashSwc_PhaseType phase)
{
    uint8 i;

    for (i = 0u; i < ParallelFlashSwc_Runtime.bundle.targetCount; i++)
    {
        if (ParallelFlashSwc_Runtime.contexts[i].phase == phase)
        {
            if ((ParallelFlashSwc_Runtime.contexts[i].state != PARALLELFLASHSWC_JOB_COMPLETED) &&
                (ParallelFlashSwc_Runtime.contexts[i].state != PARALLELFLASHSWC_JOB_FAILED) &&
                (ParallelFlashSwc_Runtime.contexts[i].state != PARALLELFLASHSWC_JOB_SKIPPED))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

static boolean ParallelFlashSwc_NonZgwPhaseFailed(ParallelFlashSwc_PhaseType phase)
{
    uint8 i;

    for (i = 0u; i < ParallelFlashSwc_Runtime.bundle.targetCount; i++)
    {
        if ((ParallelFlashSwc_Runtime.bundle.targets[i].isZgw == FALSE) &&
            (ParallelFlashSwc_Runtime.contexts[i].phase == phase) &&
            (ParallelFlashSwc_Runtime.contexts[i].state == PARALLELFLASHSWC_JOB_FAILED))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void ParallelFlashSwc_SkipZgwTargets(ParallelFlashSwc_PhaseType phase)
{
    uint8 i;

    ParallelFlashSwc_Runtime.status.lastError = PARALLELFLASHSWC_ERROR_ZGW_PREREQ_FAILED;

    for (i = 0u; i < ParallelFlashSwc_Runtime.bundle.targetCount; i++)
    {
        if ((ParallelFlashSwc_Runtime.bundle.targets[i].isZgw != FALSE) &&
            ((ParallelFlashSwc_Runtime.contexts[i].state != PARALLELFLASHSWC_JOB_COMPLETED) ||
             (phase == PARALLELFLASHSWC_PHASE_CODING)))
        {
            ParallelFlashSwc_Runtime.contexts[i].phase = phase;
            ParallelFlashSwc_Runtime.contexts[i].state = PARALLELFLASHSWC_JOB_SKIPPED;
        }
    }
}

static uint8 ParallelFlashSwc_GetActiveBusCount(GatewaySwc_BusType busType)
{
    uint8 i;
    uint8 count = 0u;

    for (i = 0u; i < ParallelFlashSwc_Runtime.bundle.targetCount; i++)
    {
        if ((ParallelFlashSwc_Runtime.contexts[i].active != FALSE) &&
            (ParallelFlashSwc_Runtime.bundle.targets[i].busType == busType))
        {
            count++;
        }
    }

    return count;
}

static Std_ReturnType ParallelFlashSwc_TransportForwardCodingRequest(uint8 extendedAddress,
                                                                    const uint8 *udsRequest,
                                                                    Dcm_PduLengthType udsRequestLength)
{
    GatewaySwc_BusType busType;
    Std_ReturnType result;

    if ((udsRequest == NULL_PTR) ||
        (udsRequestLength == 0u) ||
        (udsRequestLength > PARALLELFLASHSWC_FORWARD_MAX_REQUEST))
    {
        return E_NOT_OK;
    }

    if (ParallelFlashSwc_FindForwardBus(extendedAddress, &busType) != E_OK)
    {
        return E_NOT_OK;
    }

    if ((busType == GATEWAYSWC_BUS_LIN) &&
        (LinTp_CanAcceptTransmitToNad(extendedAddress) == FALSE))
    {
        ParallelFlashSwc_DebugForwardLinBusy++;
        return E_OK;
    }

    if ((busType != GATEWAYSWC_BUS_LIN) &&
        (ParallelFlashSwc_HasQueuedForwardEntryForAddress(extendedAddress) != FALSE))
    {
        return ParallelFlashSwc_QueueForwardRequest(extendedAddress, busType, udsRequest, udsRequestLength);
    }

    result = ParallelFlashSwc_SendForwardOnBus(busType, extendedAddress, udsRequest, udsRequestLength);
    if (result == E_OK)
    {
        return E_OK;
    }

    if (busType == GATEWAYSWC_BUS_LIN)
    {
        ParallelFlashSwc_DebugForwardLinBusy++;
        return E_OK;
    }

    return ParallelFlashSwc_QueueForwardRequest(extendedAddress, busType, udsRequest, udsRequestLength);
}

static Std_ReturnType ParallelFlashSwc_SendForwardOnBus(GatewaySwc_BusType busType,
                                                        uint8 extendedAddress,
                                                        const uint8 *udsRequest,
                                                        Dcm_PduLengthType udsRequestLength)
{
    if ((udsRequest == NULL_PTR) ||
        (udsRequestLength == 0u) ||
        (udsRequestLength > PARALLELFLASHSWC_FORWARD_MAX_REQUEST))
    {
        return E_NOT_OK;
    }

    switch (busType)
    {
        case GATEWAYSWC_BUS_CAN:
            return ParallelFlashSwc_SendDiagnosticFrame(ParallelFlashSwc_CanExtTxPdus,
                                                       (uint8)(sizeof(ParallelFlashSwc_CanExtTxPdus) /
                                                               sizeof(ParallelFlashSwc_CanExtTxPdus[0])),
                                                       extendedAddress,
                                                       udsRequest,
                                                       udsRequestLength);

        case GATEWAYSWC_BUS_CANFD:
            return ParallelFlashSwc_SendDiagnosticFrame(ParallelFlashSwc_CanFdExtTxPdus,
                                                       (uint8)(sizeof(ParallelFlashSwc_CanFdExtTxPdus) /
                                                               sizeof(ParallelFlashSwc_CanFdExtTxPdus[0])),
                                                       extendedAddress,
                                                       udsRequest,
                                                       udsRequestLength);

        case GATEWAYSWC_BUS_LIN:
        {
            Std_ReturnType linResult = LinTp_TransmitToNad(DCM_TX_LIN_PHYS,
                                                           extendedAddress,
                                                           udsRequest,
                                                           (PduLengthType)udsRequestLength);
            if (linResult == E_OK)
            {
                /* LIN slave responses are reassembled by the master onto
                 * DCM_RX_LIN_PHYS (== DCM_TX_LIN_PHYS numerically). */
                ParallelFlashSwc_RecordForwardResponseExt(DCM_TX_LIN_PHYS, extendedAddress);
            }
            return linResult;
        }

        default:
            break;
    }

    return E_NOT_OK;
}

static Std_ReturnType ParallelFlashSwc_QueueForwardRequest(uint8 extendedAddress,
                                                           GatewaySwc_BusType busType,
                                                           const uint8 *udsRequest,
                                                           Dcm_PduLengthType udsRequestLength)
{
    uint8 i;
    uint32 depth;

    if ((udsRequest == NULL_PTR) ||
        (udsRequestLength == 0u) ||
        (udsRequestLength > PARALLELFLASHSWC_FORWARD_QUEUE_PAYLOAD_LEN))
    {
        return E_NOT_OK;
    }

    depth = 0u;
    for (i = 0u; i < PARALLELFLASHSWC_FORWARD_QUEUE_DEPTH; i++)
    {
        if (ParallelFlashSwc_ForwardQueue[i].used != FALSE)
        {
            depth++;
        }
    }

    for (i = 0u; i < PARALLELFLASHSWC_FORWARD_QUEUE_DEPTH; i++)
    {
        if (ParallelFlashSwc_ForwardQueue[i].used == FALSE)
        {
            ParallelFlashSwc_ForwardQueue[i].sequence = ParallelFlashSwc_ForwardSequence++;
            ParallelFlashSwc_ForwardQueue[i].busType = busType;
            ParallelFlashSwc_ForwardQueue[i].extendedAddress = extendedAddress;
            ParallelFlashSwc_ForwardQueue[i].length = udsRequestLength;
            (void)memcpy(ParallelFlashSwc_ForwardQueue[i].data, udsRequest, udsRequestLength);
            ParallelFlashSwc_ForwardQueue[i].used = TRUE;

            depth++;
            ParallelFlashSwc_DebugForwardQueued++;
            ParallelFlashSwc_DebugForwardQueueDepth = depth;
            if (depth > ParallelFlashSwc_DebugForwardQueueMaxDepth)
            {
                ParallelFlashSwc_DebugForwardQueueMaxDepth = depth;
            }

            return E_OK;
        }
    }

    ParallelFlashSwc_DebugForwardQueueFull++;
    ParallelFlashSwc_DebugForwardQueueDepth = depth;
    return E_NOT_OK;
}

static uint8 ParallelFlashSwc_HasQueuedForwardEntryForAddress(uint8 extendedAddress)
{
    uint8 i;

    for (i = 0u; i < PARALLELFLASHSWC_FORWARD_QUEUE_DEPTH; i++)
    {
        if ((ParallelFlashSwc_ForwardQueue[i].used != FALSE) &&
            (ParallelFlashSwc_ForwardQueue[i].extendedAddress == extendedAddress))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static uint8 ParallelFlashSwc_HasOlderForwardEntryForAddress(uint8 entryIndex)
{
    uint8 i;
    uint8 extendedAddress;
    uint32 sequence;

    if ((entryIndex >= PARALLELFLASHSWC_FORWARD_QUEUE_DEPTH) ||
        (ParallelFlashSwc_ForwardQueue[entryIndex].used == FALSE))
    {
        return FALSE;
    }

    extendedAddress = ParallelFlashSwc_ForwardQueue[entryIndex].extendedAddress;
    sequence = ParallelFlashSwc_ForwardQueue[entryIndex].sequence;

    for (i = 0u; i < PARALLELFLASHSWC_FORWARD_QUEUE_DEPTH; i++)
    {
        if ((ParallelFlashSwc_ForwardQueue[i].used != FALSE) &&
            (ParallelFlashSwc_ForwardQueue[i].extendedAddress == extendedAddress) &&
            (ParallelFlashSwc_ForwardQueue[i].sequence < sequence))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static uint8 ParallelFlashSwc_BusCanAcceptForward(GatewaySwc_BusType busType,
                                                  uint8 extendedAddress)
{
    uint8 i;

    if ((busType == GATEWAYSWC_BUS_CAN) || (busType == GATEWAYSWC_BUS_CANFD))
    {
        const PduIdType *pdus;
        uint8 pduCount;

        if (CanTp_IsExtendedAddressTxActive(extendedAddress) != FALSE)
        {
            return FALSE;
        }

        if (busType == GATEWAYSWC_BUS_CAN)
        {
            pdus = ParallelFlashSwc_CanExtTxPdus;
            pduCount = (uint8)(sizeof(ParallelFlashSwc_CanExtTxPdus) /
                               sizeof(ParallelFlashSwc_CanExtTxPdus[0]));
        }
        else
        {
            pdus = ParallelFlashSwc_CanFdExtTxPdus;
            pduCount = (uint8)(sizeof(ParallelFlashSwc_CanFdExtTxPdus) /
                               sizeof(ParallelFlashSwc_CanFdExtTxPdus[0]));
        }

        for (i = 0u; i < pduCount; i++)
        {
            if (CanTp_IsTxSduIdle(pdus[i]) != FALSE)
            {
                return TRUE;
            }
        }

        return FALSE;
    }

    if (busType == GATEWAYSWC_BUS_LIN)
    {
        return LinTp_CanAcceptTransmitToNad(extendedAddress);
    }

    return TRUE;
}

static uint8 ParallelFlashSwc_FindDispatchableForwardEntry(void)
{
    uint8 i;
    uint8 selected = 0xFFu;
    uint32 selectedSequence = 0xFFFFFFFFu;
    const ParallelFlashSwc_ForwardQueueEntryType *entry;

    for (i = 0u; i < PARALLELFLASHSWC_FORWARD_QUEUE_DEPTH; i++)
    {
        entry = &ParallelFlashSwc_ForwardQueue[i];
        if (entry->used == FALSE)
        {
            continue;
        }

        if (ParallelFlashSwc_HasOlderForwardEntryForAddress(i) != FALSE)
        {
            continue;
        }

        if (ParallelFlashSwc_BusCanAcceptForward(entry->busType, entry->extendedAddress) == FALSE)
        {
            continue;
        }

        if ((selected == 0xFFu) || (entry->sequence < selectedSequence))
        {
            selected = i;
            selectedSequence = entry->sequence;
        }
    }

    return selected;
}

static void ParallelFlashSwc_ProcessForwardQueue(void)
{
    ParallelFlashSwc_ForwardQueueEntryType entry;
    uint8 index;
    uint8 dispatched;
    uint32 depth;

    if (ParallelFlashSwc_ForwardQueueProcessing != FALSE)
    {
        return;
    }

    ParallelFlashSwc_ForwardQueueProcessing = TRUE;

    for (dispatched = 0u; dispatched < PARALLELFLASHSWC_FORWARD_DISPATCH_BUDGET; dispatched++)
    {
        index = ParallelFlashSwc_FindDispatchableForwardEntry();
        if (index >= PARALLELFLASHSWC_FORWARD_QUEUE_DEPTH)
        {
            break;
        }

        entry = ParallelFlashSwc_ForwardQueue[index];

        if (ParallelFlashSwc_SendForwardOnBus(entry.busType,
                                              entry.extendedAddress,
                                              entry.data,
                                              entry.length) != E_OK)
        {
            if (entry.busType == GATEWAYSWC_BUS_LIN)
            {
                ParallelFlashSwc_ForwardQueue[index].used = FALSE;
                ParallelFlashSwc_DebugForwardLinDropped++;
                continue;
            }

            break;
        }

        ParallelFlashSwc_ForwardQueue[index].used = FALSE;
        ParallelFlashSwc_DebugForwardDispatched++;
    }

    depth = 0u;
    for (index = 0u; index < PARALLELFLASHSWC_FORWARD_QUEUE_DEPTH; index++)
    {
        if (ParallelFlashSwc_ForwardQueue[index].used != FALSE)
        {
            depth++;
        }
    }
    ParallelFlashSwc_DebugForwardQueueDepth = depth;
    ParallelFlashSwc_ForwardQueueProcessing = FALSE;
}

static Std_ReturnType ParallelFlashSwc_FindForwardBus(uint8 extendedAddress,
                                                      GatewaySwc_BusType *busType)
{
    uint8 i;
    const ParallelFlashSwc_TargetConfigType *target;

    if (busType == NULL_PTR)
    {
        return E_NOT_OK;
    }

    for (i = 0u; i < ParallelFlashSwc_Runtime.bundle.targetCount; i++)
    {
        target = &ParallelFlashSwc_Runtime.bundle.targets[i];

        if ((target->extendedDiagAddressLength > 0u) &&
            (target->extendedDiagAddress[0u] == extendedAddress))
        {
            *busType = target->busType;
            return E_OK;
        }
    }

    if ((extendedAddress >= 0x42u) && (extendedAddress <= 0x45u))
    {
        *busType = GATEWAYSWC_BUS_CANFD;
        return E_OK;
    }

    if ((extendedAddress >= 0x60u) && (extendedAddress <= 0x7Fu))
    {
        *busType = GATEWAYSWC_BUS_CANFD;
        return E_OK;
    }

    if ((extendedAddress >= 0x50u) && (extendedAddress <= 0x52u))
    {
        *busType = GATEWAYSWC_BUS_LIN;
        return E_OK;
    }

    if ((extendedAddress >= 0x46u) && (extendedAddress <= 0x4Fu))
    {
        *busType = GATEWAYSWC_BUS_LIN;
        return E_OK;
    }

    if ((extendedAddress >= 0x53u) && (extendedAddress <= 0x59u))
    {
        *busType = GATEWAYSWC_BUS_CAN;
        return E_OK;
    }

    if ((extendedAddress >= 0x90u) && (extendedAddress <= 0xAFu))
    {
        *busType = GATEWAYSWC_BUS_CAN;
        return E_OK;
    }

    return E_NOT_OK;
}

static Std_ReturnType ParallelFlashSwc_SendDiagnosticFrame(const PduIdType *canTpTxSduIds,
                                                           uint8 canTpTxSduCount,
                                                           uint8 extendedAddress,
                                                           const uint8 *udsRequest,
                                                           Dcm_PduLengthType udsRequestLength)
{
    Std_ReturnType result = E_NOT_OK;
    PduIdType usedCanTpTxSduId = 0u;
    uint8 startIdx;
    uint8 attempt;
    uint8 idx;

    if ((canTpTxSduIds == NULL_PTR) ||
        (canTpTxSduCount == 0u) ||
        (udsRequest == NULL_PTR) ||
        (udsRequestLength == 0u) ||
        (udsRequestLength > PARALLELFLASHSWC_FORWARD_MAX_REQUEST))
    {
        return E_NOT_OK;
    }

    startIdx = (uint8)(extendedAddress % canTpTxSduCount);

    for (attempt = 0u; attempt < canTpTxSduCount; attempt++)
    {
        idx = (uint8)(startIdx + attempt);
        if (idx >= canTpTxSduCount)
        {
            idx = (uint8)(idx - canTpTxSduCount);
        }

        usedCanTpTxSduId = canTpTxSduIds[idx];
        if (PARALLELFLASHSWC_FORWARD_ASSUME_CAN_FC != FALSE)
        {
            result = CanTp_TransmitExtendedAddressAssumeFlowControl(usedCanTpTxSduId,
                                                                    extendedAddress,
                                                                    udsRequest,
                                                                    (PduLengthType)udsRequestLength);
        }
        else
        {
            result = CanTp_TransmitExtendedAddress(usedCanTpTxSduId,
                                                   extendedAddress,
                                                   udsRequest,
                                                   (PduLengthType)udsRequestLength);
        }

        if (result == E_OK)
        {
            break;
        }
    }

    ParallelFlashSwc_DebugForwardLastPdu = (uint16)usedCanTpTxSduId;
    ParallelFlashSwc_DebugForwardLastResult = (uint8)result;

    if (result == E_OK)
    {
        ParallelFlashSwc_DebugForwardCanOk++;
        /* Remember which node was addressed on this TP channel so its reply can be
         * tagged and relayed back to the tester (DCM TX/RX PDU ids share numbering). */
        ParallelFlashSwc_RecordForwardResponseExt(usedCanTpTxSduId, extendedAddress);
    }
    else
    {
        ParallelFlashSwc_DebugForwardCanFail++;
    }

    return result;
}

static void ParallelFlashSwc_RecordForwardResponseExt(PduIdType pduId, uint8 extendedAddress)
{
    if ((uint16)pduId < PARALLELFLASHSWC_RESP_EXT_PDU_COUNT)
    {
        ParallelFlashSwc_RespExtByPdu[pduId] = extendedAddress;
        ParallelFlashSwc_RespExtValid[pduId] = TRUE;
    }

    ParallelFlashSwc_LastForwardExt = extendedAddress;
    ParallelFlashSwc_LastForwardExtValid = TRUE;
}

Std_ReturnType ParallelFlashSwc_GetForwardResponseExt(PduIdType rxPduId, uint8 *extendedAddress)
{
    if ((extendedAddress == NULL_PTR) ||
        ((uint16)rxPduId >= PARALLELFLASHSWC_RESP_EXT_PDU_COUNT) ||
        (ParallelFlashSwc_RespExtValid[rxPduId] == FALSE))
    {
        return E_NOT_OK;
    }

    *extendedAddress = ParallelFlashSwc_RespExtByPdu[rxPduId];
    return E_OK;
}

Std_ReturnType ParallelFlashSwc_GetLastForwardExt(uint8 *extendedAddress)
{
    if ((extendedAddress == NULL_PTR) || (ParallelFlashSwc_LastForwardExtValid == FALSE))
    {
        return E_NOT_OK;
    }

    *extendedAddress = ParallelFlashSwc_LastForwardExt;
    return E_OK;
}

static Dcm_ReturnType ParallelFlashSwc_BuildForwardNrc(uint8 extendedAddress,
                                                       uint8 sid,
                                                       uint8 nrc,
                                                       uint8 *response,
                                                       Dcm_PduLengthType *responseLength)
{
    if ((response == NULL_PTR) || (responseLength == NULL_PTR))
    {
        return DCM_NRC_GENERAL_REJECT;
    }

    response[0] = extendedAddress;
    response[1] = 0x7Fu;
    response[2] = sid;
    response[3] = nrc;
    *responseLength = 4u;
    return DCM_E_OK;
}
