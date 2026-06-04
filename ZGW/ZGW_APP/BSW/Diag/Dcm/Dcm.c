/* Dcm.c */
#include "Dcm.h"
#include <string.h>
#include "Dem.h"
#include "IfxCpu.h"
#include "IfxScuRcu.h"
#include "McuSm.h"
#include "PduR.h"
#include "Os.h"
#include "NvM.h"
#include "BSW/Time/Dcm_TimeRoutine.h"

/* ===================== Internal types ===================== */

typedef enum
{
    DCM_CONN_IDLE = 0u,
    DCM_CONN_RECEIVING,
    DCM_CONN_PROCESSING,
    DCM_CONN_TX_READY,
    DCM_CONN_TX_BUSY
} Dcm_ConnStateType;

typedef enum
{
    DCM_NVM_WRITE_ALL_IDLE = 0u,
    DCM_NVM_WRITE_ALL_WAIT_IDLE,
    DCM_NVM_WRITE_ALL_ACTIVE
} Dcm_NvMWriteAllStateType;

#define DCM_DTC_FORMAT_IDENTIFIER_UDS  0x01u
#define DCM_RESET_INFO_ENTER_FBL       0xFCD0u
#define DCM_ROUTINE_SELECT_FBL_INTERFACE 0x0205u
/* UDS has no dedicated server-side operation timeout NRC. */
#define DCM_PENDING_TIMEOUT_NRC        DCM_NRC_GENERAL_REJECT

typedef struct
{
        uint8 *data;
        Dcm_PduLengthType len;
        uint8 valid;
        Dcm_AddressingType addressing;
} Dcm_RxRequestType;

typedef struct
{
        Dcm_ConnStateType state;

        uint8 *rxBuffer;
        Dcm_PduLengthType rxExpectedLen;
        Dcm_PduLengthType rxCopiedLen;
        uint8 rxInProgress;

        Dcm_RxRequestType queue[DCM_RX_QUEUE_DEPTH];
        uint8 qHead;
        uint8 qTail;
        uint8 qCount;

        uint8 *activeReq;
        Dcm_PduLengthType activeReqLen;
        Dcm_AddressingType activeAddressing;

        uint8 *txBuffer;
        Dcm_PduLengthType txLen;
        Dcm_PduLengthType txOffset;

        uint8 sid;
        const Dcm_ServiceType* service;
        Dcm_OpStatusType opStatus;

        uint16 p2Timer;
        uint16 p2StarTimer;
        uint8 pendingStarted;
        uint8 pendingResponseCount;
        uint32 lastTimerTick;

        uint8 suppressPositiveResponse;
        uint8 session;
        uint8 pendingSessionAfterResponse;
        uint8 sessionAfterResponse;
        uint8 pendingResetAfterResponse;
        uint8 resetAfterResponse;

        uint16 s3Timer;
        uint8 downloadActive;
        uint8 expectedBlockSequenceCounter;
} Dcm_ConnectionStateType;


long long Dcm_MainFunction_Counter = 0;

/* ===================== Static ===================== */

static const Dcm_ConfigType* Dcm_ConfigPtr = NULL_PTR;
static Dcm_ConnectionStateType Dcm_Conn[DCM_MAX_CONNECTIONS];
static uint8 Dcm_RxBuffer[DCM_MAX_PDU_LEN];
static uint8 Dcm_ActiveReqBuffer[DCM_MAX_PDU_LEN];
static uint8 Dcm_TxBuffer[DCM_MAX_RESPONSE_LEN];
static uint8 Dcm_QueueBuffer[DCM_RX_QUEUE_DEPTH][DCM_MAX_PDU_LEN];
static uint8 Dcm_ActiveProtocolConnection = 0xFFu;
static Dcm_NvMWriteAllStateType Dcm_NvMWriteAllState = DCM_NVM_WRITE_ALL_IDLE;
static uint8 Dcm_NvMWriteAllOwnerConn = 0xFFu;
static uint8 Dcm_NvMWriteAllOwnerSid = 0u;

/* Deferred hard-reset state. A hardReset must not happen inside the TX
 * confirmation callback: that callback only signals that lwIP has accepted the
 * 0x51 response into its send buffer, not that GETH has transmitted and the
 * tester has ACKed it. Resetting (and disabling interrupts) there races the
 * Ethernet transmit and drops the positive response, so the tester sees a
 * timeout instead of 51 01. Instead the reset is armed here and serviced from
 * Dcm_MainFunction with interrupts still enabled, giving lwIP/GETH several main
 * cycles to put the response on the wire (and retransmit if needed) first. */
#define DCM_HARD_RESET_DELAY_TICKS 40u   /* ~200 ms at the 5 ms main period */
static uint8 Dcm_PendingHardReset = FALSE;
static uint16 Dcm_HardResetCountdown = 0u;

/* ===================== Forward ===================== */
static void Dcm_SendBusyRepeatIfPossible(uint8 connIdx, uint8 sid, Dcm_AddressingType addressing);
static uint8 Dcm_CheckServiceAccess(uint8 connIdx, uint8 sid, uint8* nrc);
static uint8 Dcm_FindRxConnection(Dcm_PduIdType rxPduId);
static uint8 Dcm_FindTxConnection(Dcm_PduIdType txPduId);
static Dcm_PduLengthType Dcm_GetTxPayloadLimit(uint8 connIdx);
static void Dcm_ProcessConnection(uint8 connIdx);
static void Dcm_LoadNextRequest(uint8 connIdx);
static void Dcm_StartProcessing(uint8 connIdx, const uint8* req, Dcm_PduLengthType len, Dcm_AddressingType addressing);

static uint32 Dcm_GetTick(void)
{
    return (uint32)OS_Counter_core0;
}

static void Dcm_AssignSharedBuffers(uint8 connIdx)
{
    uint8 q;

    Dcm_Conn[connIdx].rxBuffer = Dcm_RxBuffer;
    Dcm_Conn[connIdx].activeReq = Dcm_ActiveReqBuffer;
    Dcm_Conn[connIdx].txBuffer = Dcm_TxBuffer;

    for (q = 0u; q < DCM_RX_QUEUE_DEPTH; q++)
    {
        Dcm_Conn[connIdx].queue[q].data = Dcm_QueueBuffer[q];
    }
}

static uint8 Dcm_ShouldSuppressFunctionalNrc(uint8 nrc)
{
    if ((nrc == DCM_NRC_SERVICE_NOT_SUPPORTED) ||
            (nrc == DCM_NRC_SUBFUNCTION_NOT_SUPPORTED) ||
            (nrc == DCM_NRC_REQUEST_OUT_OF_RANGE) ||
            (nrc == DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION) ||
            (nrc == DCM_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_SESSION) ||
            (nrc == DCM_NRC_RESPONSE_PENDING))
    {
        return TRUE;
    }

    return FALSE;
}

static void Dcm_BuildNegativeResponse(uint8 connIdx, uint8 sid, uint8 nrc)
{
    Dcm_ConnectionStateType* c;

    c = &Dcm_Conn[connIdx];

    c->txBuffer[0] = 0x7Fu;
    c->txBuffer[1] = sid;
    c->txBuffer[2] = nrc;
    c->txLen = 3u;
    c->txOffset = 0u;
}

static Dcm_PduLengthType Dcm_GetTxPayloadLimit(uint8 connIdx)
{
    Dcm_BusType busType;

    if ((Dcm_ConfigPtr == NULL_PTR) ||
            (connIdx >= Dcm_ConfigPtr->numConnections))
    {
        return DCM_CLASSIC_ISOTP_MAX_LEN;
    }

    busType = Dcm_ConfigPtr->connections[connIdx].busType;

    /* Lab deviation: classic CAN also allowed the full multi-frame length.
     * CanTp carries it via the ISO 15765-2 32-bit escape FirstFrame (see
     * CanTp_CanUseExtendedFfLength). Capped by CANTP_MAX_PAYLOAD_LEN (8192). */
    if ((busType == DCM_BUS_CAN_CLASSIC) ||
            (busType == DCM_BUS_CAN_FD) ||
            (busType == DCM_BUS_ETHERNET))
    {
        return DCM_MAX_RESPONSE_LEN;
    }

    return DCM_CLASSIC_ISOTP_MAX_LEN;
}

static void Dcm_StartProcessing(
        uint8 connIdx,
        const uint8* req,
        Dcm_PduLengthType len,
        Dcm_AddressingType addressing)
{
    Dcm_ConnectionStateType* c;

    c = &Dcm_Conn[connIdx];

    if ((req == NULL_PTR) || (len == 0u))
    {
        return;
    }

    memcpy(c->activeReq, req, len);
    c->activeReqLen = len;
    c->activeAddressing = addressing;
    c->state = DCM_CONN_PROCESSING;
    c->opStatus = DCM_INITIAL;
    c->sid = c->activeReq[0];
    c->service = NULL_PTR;
    c->p2Timer = DCM_P2_SERVER_TICKS;
    c->p2StarTimer = DCM_P2STAR_SERVER_TICKS;
    c->pendingStarted = FALSE;
    c->pendingResponseCount = 0u;
    c->lastTimerTick = Dcm_GetTick();
    c->suppressPositiveResponse = FALSE;
    c->pendingSessionAfterResponse = FALSE;
    c->sessionAfterResponse = 0u;
    c->pendingResetAfterResponse = FALSE;
    c->resetAfterResponse = 0u;
    c->s3Timer = DCM_S3_SERVER_TICKS;

    if (Dcm_ActiveProtocolConnection == 0xFFu)
    {
        Dcm_ActiveProtocolConnection = connIdx;
    }
}


static void Dcm_ClearProcessing(uint8 connIdx);
static Std_ReturnType Dcm_StartTx(uint8 connIdx);
static uint16 Dcm_ConsumeElapsedTicks(uint8 connIdx);
static void Dcm_UpdatePendingTimers(uint8 connIdx, uint16 elapsedTicks);
static uint8 Dcm_IsConnectionActive(uint8 connIdx);
static uint8 Dcm_CanStartProtocol(uint8 connIdx);
static void Dcm_ReleaseProtocol(uint8 connIdx);
static void Dcm_SendResponsePending(uint8 connIdx);
static void Dcm_AbortPendingOperation(uint8 connIdx, uint8 nrc);

static void Dcm_SendBusyRepeatIfPossible(uint8 connIdx, uint8 sid, Dcm_AddressingType addressing)
{
    Dcm_ConnectionStateType* c;

    if (connIdx >= DCM_MAX_CONNECTIONS)
    {
        return;
    }

    if ((addressing == DCM_ADDR_FUNCTIONAL) &&
            (Dcm_ShouldSuppressFunctionalNrc(DCM_NRC_BUSY_REPEAT_REQUEST) == TRUE))
    {
        return;
    }

    c = &Dcm_Conn[connIdx];

    if ((c->state == DCM_CONN_IDLE) && (c->rxInProgress == FALSE))
    {
        Dcm_BuildNegativeResponse(connIdx, sid, DCM_NRC_BUSY_REPEAT_REQUEST);
        c->service = NULL_PTR;
        (void)Dcm_StartTx(connIdx);
    }
}

static uint16 Dcm_ConsumeElapsedTicks(uint8 connIdx)
{
    Dcm_ConnectionStateType* c;
    uint32 now;
    uint32 elapsed;

    c = &Dcm_Conn[connIdx];
    now = Dcm_GetTick();
    elapsed = now - c->lastTimerTick;
    c->lastTimerTick = now;

    if (elapsed > 0xFFFFu)
    {
        elapsed = 0xFFFFu;
    }

    return (uint16)elapsed;
}

static void Dcm_UpdatePendingTimers(uint8 connIdx, uint16 elapsedTicks)
{
    Dcm_ConnectionStateType* c;

    c = &Dcm_Conn[connIdx];

    while (elapsedTicks > 0u)
    {
        if (c->p2Timer > 0u)
        {
            c->p2Timer--;
        }
        else
        {
            if (c->p2StarTimer > 0u)
            {
                c->p2StarTimer--;
            }
        }

        elapsedTicks--;
    }
}

static uint8 Dcm_IsConnectionActive(uint8 connIdx)
{
    const Dcm_ConnectionStateType* c;

    c = &Dcm_Conn[connIdx];

    if ((c->state == DCM_CONN_PROCESSING) ||
            (c->state == DCM_CONN_TX_READY) ||
            (c->state == DCM_CONN_TX_BUSY) ||
            (c->qCount > 0u) ||
            (c->rxInProgress == TRUE))
    {
        return TRUE;
    }

    return FALSE;
}

static uint8 Dcm_CanStartProtocol(uint8 connIdx)
{
    if (Dcm_ActiveProtocolConnection == 0xFFu)
    {
        return TRUE;
    }

    if (Dcm_ActiveProtocolConnection == connIdx)
    {
        return TRUE;
    }

    if (Dcm_IsConnectionActive(Dcm_ActiveProtocolConnection) == FALSE)
    {
        Dcm_ActiveProtocolConnection = 0xFFu;
        return TRUE;
    }

    return FALSE;
}

static void Dcm_ReleaseProtocol(uint8 connIdx)
{
    if (Dcm_ActiveProtocolConnection == connIdx)
    {
        if (Dcm_IsConnectionActive(connIdx) == FALSE)
        {
            Dcm_ActiveProtocolConnection = 0xFFu;
        }
    }
}

static void Dcm_SendResponsePending(uint8 connIdx)
{
    Dcm_ConnectionStateType* c;

    c = &Dcm_Conn[connIdx];

    c->pendingResponseCount++;
    c->p2StarTimer = DCM_P2STAR_SERVER_TICKS;
    c->pendingStarted = TRUE;
    c->txLen = 0u;
    c->txOffset = 0u;

    if ((c->activeAddressing == DCM_ADDR_FUNCTIONAL) &&
            (Dcm_ShouldSuppressFunctionalNrc(DCM_NRC_RESPONSE_PENDING) == TRUE))
    {
        return;
    }

    Dcm_BuildNegativeResponse(connIdx, c->sid, DCM_NRC_RESPONSE_PENDING);
    (void)Dcm_StartTx(connIdx);
}

static void Dcm_AbortPendingOperation(uint8 connIdx, uint8 nrc)
{
    Dcm_ConnectionStateType* c;
    Dcm_PduLengthType respLen;

    c = &Dcm_Conn[connIdx];
    respLen = 0u;

    if (c->service != NULL_PTR)
    {
        c->opStatus = DCM_CANCEL;
        (void)c->service->handler(
                connIdx,
                c->opStatus,
                &c->activeReq[1],
                (Dcm_PduLengthType)(c->activeReqLen - 1u),
                &c->txBuffer[1],
                &respLen
        );
    }

    if ((c->activeAddressing == DCM_ADDR_FUNCTIONAL) &&
            (Dcm_ShouldSuppressFunctionalNrc(nrc) == TRUE))
    {
        Dcm_ClearProcessing(connIdx);
        return;
    }

    c->service = NULL_PTR;
    c->opStatus = DCM_INITIAL;
    c->pendingStarted = FALSE;
    c->pendingResponseCount = 0u;
    c->p2Timer = 0u;
    c->p2StarTimer = 0u;

    Dcm_BuildNegativeResponse(connIdx, c->sid, nrc);
    (void)Dcm_StartTx(connIdx);
}

static const Dcm_ServiceType* Dcm_FindService(uint8 sid); // @suppress("Unused function declaration")
static uint8 Dcm_GetSubFunction(uint8 rawSubFunction);
static uint8 Dcm_IsSuppressPosRspBitSet(uint8 rawSubFunction);
static uint16 Dcm_GetSessionMask(uint8 session);
static void Dcm_ResetConnectionRuntime(uint8 connIdx);
static void Dcm_ResetNvMWriteAllState(void);
static Dcm_ReturnType Dcm_PollNvMWriteAllBeforeDiagnosticReset(uint8 connIdx, uint8 sid, Dcm_OpStatusType opStatus);
static Dcm_ReturnType Dcm_Service_ForwardStandard(uint8 sid, uint8 connIdx, Dcm_OpStatusType opStatus, const uint8* req, Dcm_PduLengthType len, uint8* resp, Dcm_PduLengthType* respLen);
static Dcm_ReturnType Dcm_DiagnosticSessionControl(uint8 connIdx, Dcm_OpStatusType opStatus, uint8 session, uint8* respData, Dcm_PduLengthType* respLen);
static Dcm_ReturnType Dcm_EcuReset(uint8 connIdx, Dcm_OpStatusType opStatus, uint8 resetType, uint8* respData, Dcm_PduLengthType* respLen);
static void Dcm_SessionChangeAfterResponse(uint8 connIdx, uint8 session);
static void Dcm_EcuResetAfterResponse(uint8 connIdx, uint8 resetType);
static uint8 Dcm_NormalizeFblInterface(uint8 requestedInterface);
static void Dcm_ResetDelay(void);
static Dcm_ReturnType Dcm_ClearDiagnosticInformation(uint8 connIdx, Dcm_OpStatusType opStatus, uint32 dtcGroup);
static Dcm_ReturnType Dcm_ReadDtcInformation(uint8 connIdx, Dcm_OpStatusType opStatus, const uint8* reqData, Dcm_PduLengthType reqLen, uint8* respData, Dcm_PduLengthType* respLen);
static Dcm_ReturnType Dcm_ReadDtcByStatusMask(uint8 subFunction, uint8 statusMask, uint8* respData, Dcm_PduLengthType* respLen);
static Dcm_ReturnType Dcm_ReadDtcStoredDataByDtc(uint8 subFunction, Dem_DTCType dtc, uint8 recordNumber, uint8* respData, Dcm_PduLengthType* respLen);
static Dcm_ReturnType Dcm_SelectFblInterfaceRoutine(uint8 routineControlType, const uint8* reqData, Dcm_PduLengthType reqLen, uint8* respData, Dcm_PduLengthType* respLen);

/* Services */
static Dcm_ReturnType Dcm_Service_0x10(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x11(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x14(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x19(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x22(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x28(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x31(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x34(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x35(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x36(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x37(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x3E(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x85(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
/* Unsupported services (handlers removed): 0x23, 0x24, 0x29, 0x2A, 0x2C, 0x2E,
 * 0x2F, 0x38, 0x3D, 0x83, 0x84, 0x86, 0x87. */

/* ===================== Default config ===================== */

/* Only the services listed below are supported. Any other service - whether
 * absent from this table or absent from the build entirely - is rejected with
 * ServiceNotSupported (0x11) by the dispatcher. Removed services (0x23, 0x24,
 * 0x29, 0x2A, 0x2C, 0x2E, 0x2F, 0x38, 0x3D, 0x83, 0x84, 0x86, 0x87) had their
 * handler implementations deleted as well. */
const Dcm_ServiceType Dcm_DefaultServices[] =
{
        { DCM_SID_DIAGNOSTIC_SESSION_CONTROL, Dcm_Service_0x10 },
        { DCM_SID_ECU_RESET,                  Dcm_Service_0x11 },
        { DCM_SID_CLEAR_DIAGNOSTIC_INFORMATION, Dcm_Service_0x14 },
        { DCM_SID_READ_DTC_INFORMATION,       Dcm_Service_0x19 },
        { DCM_SID_READ_DATA_BY_IDENTIFIER,    Dcm_Service_0x22 },
        { DCM_SID_COMMUNICATION_CONTROL,      Dcm_Service_0x28 },
        { DCM_SID_ROUTINE_CONTROL,            Dcm_Service_0x31 },
        { DCM_SID_REQUEST_DOWNLOAD,           Dcm_Service_0x34 },
        { DCM_SID_REQUEST_UPLOAD,             Dcm_Service_0x35 },
        { DCM_SID_TRANSFER_DATA,              Dcm_Service_0x36 },
        { DCM_SID_REQUEST_TRANSFER_EXIT,      Dcm_Service_0x37 },
        { DCM_SID_TESTER_PRESENT,             Dcm_Service_0x3E },
        { DCM_SID_CONTROL_DTC_SETTING,        Dcm_Service_0x85 }
};

typedef struct
{
        uint8 sid;
        uint16 sessionMask;
} Dcm_ServiceAccessType;

static const Dcm_ServiceAccessType Dcm_ServiceAccessTable[] =
{
        { DCM_SID_DIAGNOSTIC_SESSION_CONTROL, DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_PROGRAMMING | DCM_SESSION_MASK_EXTENDED | DCM_SESSION_MASK_CODING },
        { DCM_SID_ECU_RESET,                  DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_PROGRAMMING | DCM_SESSION_MASK_EXTENDED | DCM_SESSION_MASK_CODING },
        { DCM_SID_CLEAR_DIAGNOSTIC_INFORMATION, DCM_SESSION_MASK_EXTENDED },
        { DCM_SID_READ_DTC_INFORMATION,       DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_EXTENDED },
        { DCM_SID_READ_DATA_BY_IDENTIFIER,    DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_EXTENDED | DCM_SESSION_MASK_CODING },
        { DCM_SID_COMMUNICATION_CONTROL,      DCM_SESSION_MASK_EXTENDED },
        { DCM_SID_ROUTINE_CONTROL,            DCM_SESSION_MASK_EXTENDED | DCM_SESSION_MASK_PROGRAMMING | DCM_SESSION_MASK_CODING },
        { DCM_SID_REQUEST_DOWNLOAD,           DCM_SESSION_MASK_PROGRAMMING },
        { DCM_SID_REQUEST_UPLOAD,             DCM_SESSION_MASK_PROGRAMMING },
        { DCM_SID_TRANSFER_DATA,              DCM_SESSION_MASK_PROGRAMMING },
        { DCM_SID_REQUEST_TRANSFER_EXIT,      DCM_SESSION_MASK_PROGRAMMING },
        { DCM_SID_TESTER_PRESENT,             DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_PROGRAMMING | DCM_SESSION_MASK_EXTENDED | DCM_SESSION_MASK_CODING },
        { DCM_SID_CONTROL_DTC_SETTING,        DCM_SESSION_MASK_EXTENDED }
};

/* ===================== API ===================== */

static uint16 Dcm_GetSessionMask(uint8 session)
{
    if (session == DCM_SESSION_DEFAULT)
    {
        return DCM_SESSION_MASK_DEFAULT;
    }

    if (session == DCM_SESSION_PROGRAMMING)
    {
        return DCM_SESSION_MASK_PROGRAMMING;
    }

    if (session == DCM_SESSION_EXTENDED)
    {
        return DCM_SESSION_MASK_EXTENDED;
    }

    if (session == DCM_SESSION_CODING)
    {
        return DCM_SESSION_MASK_CODING;
    }

    return 0u;
}

static uint8 Dcm_CheckServiceAccess(uint8 connIdx, uint8 sid, uint8* nrc)
{
    uint8 i;
    uint16 activeSessionMask = Dcm_GetSessionMask(Dcm_Conn[connIdx].session);

    for (i = 0u; i < (uint8)(sizeof(Dcm_ServiceAccessTable) / sizeof(Dcm_ServiceAccessTable[0])); i++)
    {
        if (Dcm_ServiceAccessTable[i].sid == sid)
        {
            if ((Dcm_ServiceAccessTable[i].sessionMask & activeSessionMask) == 0u)
            {
                *nrc = DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION;
                return FALSE;
            }

            return TRUE;
        }
    }

    *nrc = DCM_NRC_SERVICE_NOT_SUPPORTED;
    return FALSE;
}

static void Dcm_ResetConnectionRuntime(uint8 connIdx)
{
    if (connIdx >= DCM_MAX_CONNECTIONS)
    {
        return;
    }

    memset(&Dcm_Conn[connIdx], 0, sizeof(Dcm_Conn[connIdx]));
    Dcm_AssignSharedBuffers(connIdx);
    Dcm_Conn[connIdx].state = DCM_CONN_IDLE;
    Dcm_Conn[connIdx].session = DCM_SESSION_DEFAULT;
    Dcm_Conn[connIdx].activeAddressing =
            ((Dcm_ConfigPtr != NULL_PTR) && (connIdx < Dcm_ConfigPtr->numConnections)) ?
                    Dcm_ConfigPtr->connections[connIdx].addressing :
                    DCM_ADDR_PHYSICAL;
    Dcm_Conn[connIdx].lastTimerTick = Dcm_GetTick();
    Dcm_Conn[connIdx].downloadActive = FALSE;
    Dcm_Conn[connIdx].expectedBlockSequenceCounter = 1u;
}

void Dcm_Init(const Dcm_ConfigType* ConfigPtr)
{
    uint8 i;

    Dcm_ConfigPtr = (ConfigPtr == NULL_PTR) ? &Dcm_Config : ConfigPtr;

    memset(Dcm_Conn, 0, sizeof(Dcm_Conn));
    memset(Dcm_RxBuffer, 0, sizeof(Dcm_RxBuffer));
    memset(Dcm_ActiveReqBuffer, 0, sizeof(Dcm_ActiveReqBuffer));
    memset(Dcm_TxBuffer, 0, sizeof(Dcm_TxBuffer));
    memset(Dcm_QueueBuffer, 0, sizeof(Dcm_QueueBuffer));
    Dcm_ActiveProtocolConnection = 0xFFu;
    Dcm_ResetNvMWriteAllState();

    for (i = 0u; i < DCM_MAX_CONNECTIONS; i++)
    {
        Dcm_ResetConnectionRuntime(i);
    }
}

void Dcm_ResetDoIPSession(void)
{
    uint8 i;
    uint8 resetNvMWriteAll;

    if (Dcm_ConfigPtr == NULL_PTR)
    {
        return;
    }

    resetNvMWriteAll = FALSE;

    for (i = 0u; (i < Dcm_ConfigPtr->numConnections) && (i < DCM_MAX_CONNECTIONS); i++)
    {
        if (Dcm_ConfigPtr->connections[i].busType != DCM_BUS_ETHERNET)
        {
            continue;
        }

        if (Dcm_Conn[i].session != DCM_SESSION_DEFAULT)
        {
            DcmAppl_DiagnosticSessionChanged(i, DCM_SESSION_DEFAULT);
        }

        if (Dcm_NvMWriteAllOwnerConn == i)
        {
            resetNvMWriteAll = TRUE;
        }

        Dcm_ResetConnectionRuntime(i);
    }

    if ((Dcm_ActiveProtocolConnection < Dcm_ConfigPtr->numConnections) &&
            (Dcm_ActiveProtocolConnection < DCM_MAX_CONNECTIONS) &&
            (Dcm_ConfigPtr->connections[Dcm_ActiveProtocolConnection].busType == DCM_BUS_ETHERNET))
    {
        Dcm_ActiveProtocolConnection = 0xFFu;
    }

    if (resetNvMWriteAll == TRUE)
    {
        Dcm_ResetNvMWriteAllState();
    }
}

void Dcm_MainFunction(void)
{
    uint8 i;

    if (Dcm_ConfigPtr == NULL_PTR)
    {
        return;
    }

    for (i = 0u; (i < Dcm_ConfigPtr->numConnections) && (i < DCM_MAX_CONNECTIONS); i++)
    {
        if ((Dcm_Conn[i].session != DCM_SESSION_DEFAULT) &&
                (Dcm_Conn[i].state == DCM_CONN_IDLE) &&
                (Dcm_Conn[i].qCount == 0u))
        {
            if (Dcm_Conn[i].s3Timer > 0u)
            {
                Dcm_Conn[i].s3Timer--;
            }
            else
            {
                Dcm_Conn[i].session = DCM_SESSION_DEFAULT;
                Dcm_Conn[i].downloadActive = FALSE;
                Dcm_Conn[i].expectedBlockSequenceCounter = 1u;
            }
        }

        Dcm_ProcessConnection(i);
    }

    /* Serviced here, not in the TX confirmation callback, so the 0x51 hardReset
     * response is actually transmitted and ACKed before the MCU resets. The
     * countdown runs with interrupts enabled, letting lwIP/GETH flush (and
     * retransmit) the response across several main cycles; only the final reset
     * disables interrupts immediately before performReset. */
    if (Dcm_PendingHardReset == TRUE)
    {
        if (Dcm_HardResetCountdown > 0u)
        {
            Dcm_HardResetCountdown--;
        }
        else
        {
            Dcm_PendingHardReset = FALSE;
            Dcm_ResetDelay();
            IfxCpu_disableInterrupts();
            IfxScuRcu_performReset(IfxScuRcu_ResetType_application, 0u);
        }
    }

    Dcm_MainFunction_Counter++;
}

void Dcm_RxIndication(PduIdType rxPduId, const uint8* data, PduLengthType len)
{
    Dcm_PduInfoType info;
    Dcm_PduLengthType bufferSize;

    if ((data == NULL_PTR) || (len == 0u))
    {
        return;
    }

    info.SduDataPtr = (uint8*)data;
    info.SduLength = (Dcm_PduLengthType)len;

    if (Dcm_StartOfReception((Dcm_PduIdType)rxPduId,
            &info,
            (Dcm_PduLengthType)len,
            &bufferSize) != BUFREQ_OK)
    {
        return;
    }

    if (Dcm_CopyRxData((Dcm_PduIdType)rxPduId,
            &info,
            &bufferSize) != BUFREQ_OK)
    {
        Dcm_TpRxIndication((Dcm_PduIdType)rxPduId, NTFRSLT_E_NOT_OK);
        return;
    }

    Dcm_TpRxIndication((Dcm_PduIdType)rxPduId, NTFRSLT_OK);
}

uint8 Dcm_GetActiveSession(uint8 connIdx)
{
    if ((Dcm_ConfigPtr == NULL_PTR) ||
            (connIdx >= DCM_MAX_CONNECTIONS) ||
            (connIdx >= Dcm_ConfigPtr->numConnections))
    {
        return DCM_SESSION_DEFAULT;
    }

    return Dcm_Conn[connIdx].session;
}

void Dcm_TxConfirmation(PduIdType txPduId, Std_ReturnType result)
{
    Dcm_TpTxConfirmation(
            (Dcm_PduIdType)txPduId,
            (result == E_OK) ? NTFRSLT_OK : NTFRSLT_E_NOT_OK
    );
}

BufReq_ReturnType Dcm_StartOfReception(
        Dcm_PduIdType id,
        const Dcm_PduInfoType* info,
        Dcm_PduLengthType TpSduLength,
        Dcm_PduLengthType* bufferSizePtr)
{
    uint8 connIdx;
    Dcm_ConnectionStateType* c;

    (void)info;

    if ((Dcm_ConfigPtr == NULL_PTR) || (bufferSizePtr == NULL_PTR))
    {
        return BUFREQ_E_NOT_OK;
    }

    connIdx = Dcm_FindRxConnection(id);

    if (connIdx >= Dcm_ConfigPtr->numConnections)
    {
        return BUFREQ_E_NOT_OK;
    }

    if (TpSduLength > DCM_MAX_PDU_LEN)
    {
        return BUFREQ_E_OVFL;
    }

    c = &Dcm_Conn[connIdx];

    if (c->rxInProgress == TRUE)
    {
        return BUFREQ_E_BUSY;
    }

    if (Dcm_CanStartProtocol(connIdx) == FALSE)
    {
        *bufferSizePtr = 0u;
        return BUFREQ_E_BUSY;
    }

    if ((c->state != DCM_CONN_IDLE) && (c->qCount >= DCM_RX_QUEUE_DEPTH))
    {
        *bufferSizePtr = 0u;
        return BUFREQ_E_BUSY;
    }

    c->rxInProgress = TRUE;
    c->rxExpectedLen = TpSduLength;
    c->rxCopiedLen = 0u;

    *bufferSizePtr = (Dcm_PduLengthType)(DCM_MAX_PDU_LEN - c->rxCopiedLen);
    return BUFREQ_OK;
}

BufReq_ReturnType Dcm_CopyRxData(
        Dcm_PduIdType id,
        const Dcm_PduInfoType* info,
        Dcm_PduLengthType* bufferSizePtr)
{
    uint8 connIdx;
    Dcm_ConnectionStateType* c;

    if ((Dcm_ConfigPtr == NULL_PTR) || (info == NULL_PTR) || (bufferSizePtr == NULL_PTR))
    {
        return BUFREQ_E_NOT_OK;
    }

    connIdx = Dcm_FindRxConnection(id);

    if (connIdx >= Dcm_ConfigPtr->numConnections)
    {
        return BUFREQ_E_NOT_OK;
    }

    c = &Dcm_Conn[connIdx];

    if (c->rxInProgress != TRUE)
    {
        return BUFREQ_E_NOT_OK;
    }

    if ((c->rxCopiedLen + info->SduLength) > c->rxExpectedLen)
    {
        return BUFREQ_E_OVFL;
    }

    if ((c->rxCopiedLen + info->SduLength) > DCM_MAX_PDU_LEN)
    {
        return BUFREQ_E_OVFL;
    }

    if ((info->SduLength > 0u) && (info->SduDataPtr != NULL_PTR))
    {
        memcpy(&c->rxBuffer[c->rxCopiedLen], info->SduDataPtr, info->SduLength);
        c->rxCopiedLen += info->SduLength;
    }

    *bufferSizePtr = (Dcm_PduLengthType)(DCM_MAX_PDU_LEN - c->rxCopiedLen);
    return BUFREQ_OK;
}

void Dcm_TpRxIndication(Dcm_PduIdType id, Dcm_NotifResultType result)
{
    uint8 connIdx;
    Dcm_ConnectionStateType* c;
    Dcm_RxRequestType* q;

    if (Dcm_ConfigPtr == NULL_PTR)
    {
        return;
    }

    connIdx = Dcm_FindRxConnection(id);

    if (connIdx >= Dcm_ConfigPtr->numConnections)
    {
        return;
    }

    c = &Dcm_Conn[connIdx];

    if (c->rxInProgress != TRUE)
    {
        return;
    }

    c->rxInProgress = FALSE;

    if ((result != NTFRSLT_OK) || (c->rxCopiedLen != c->rxExpectedLen) || (c->rxCopiedLen == 0u))
    {
        c->rxCopiedLen = 0u;
        c->rxExpectedLen = 0u;
        return;
    }

    c->s3Timer = DCM_S3_SERVER_TICKS;

    if (c->state == DCM_CONN_IDLE)
    {
        Dcm_StartProcessing(connIdx, c->rxBuffer, c->rxCopiedLen, Dcm_ConfigPtr->connections[connIdx].addressing);
    }
    else
    {
        if (c->qCount < DCM_RX_QUEUE_DEPTH)
        {
            q = &c->queue[c->qHead];
            memcpy(q->data, c->rxBuffer, c->rxCopiedLen);
            q->len = c->rxCopiedLen;
            q->valid = TRUE;
            q->addressing = Dcm_ConfigPtr->connections[connIdx].addressing;

            c->qHead++;
            if (c->qHead >= DCM_RX_QUEUE_DEPTH)
            {
                c->qHead = 0u;
            }

            c->qCount++;
        }
        else
        {
            Dcm_SendBusyRepeatIfPossible(connIdx,
                    c->rxBuffer[0],
                    Dcm_ConfigPtr->connections[connIdx].addressing);
        }
    }

    c->rxCopiedLen = 0u;
    c->rxExpectedLen = 0u;
}

BufReq_ReturnType Dcm_CopyTxData(
        Dcm_PduIdType id,
        const Dcm_PduInfoType* info,
        Dcm_PduLengthType* availableDataPtr)
{
    uint8 connIdx;
    Dcm_ConnectionStateType* c;
    Dcm_PduLengthType copyLen;

    if ((Dcm_ConfigPtr == NULL_PTR) || (info == NULL_PTR) || (availableDataPtr == NULL_PTR))
    {
        return BUFREQ_E_NOT_OK;
    }

    connIdx = Dcm_FindTxConnection(id);

    if (connIdx >= Dcm_ConfigPtr->numConnections)
    {
        return BUFREQ_E_NOT_OK;
    }

    c = &Dcm_Conn[connIdx];

    if (c->state != DCM_CONN_TX_BUSY)
    {
        return BUFREQ_E_NOT_OK;
    }

    if (c->txOffset > c->txLen)
    {
        return BUFREQ_E_NOT_OK;
    }

    copyLen = info->SduLength;

    if ((c->txOffset + copyLen) > c->txLen)
    {
        copyLen = (Dcm_PduLengthType)(c->txLen - c->txOffset);
    }

    if ((copyLen > 0u) && (info->SduDataPtr != NULL_PTR))
    {
        memcpy(info->SduDataPtr, &c->txBuffer[c->txOffset], copyLen);
        c->txOffset += copyLen;
    }

    *availableDataPtr = (Dcm_PduLengthType)(c->txLen - c->txOffset);
    return BUFREQ_OK;
}

void Dcm_TpTxConfirmation(Dcm_PduIdType id, Dcm_NotifResultType result)
{
    uint8 connIdx;
    Dcm_ConnectionStateType* c;

    if (Dcm_ConfigPtr == NULL_PTR)
    {
        return;
    }

    connIdx = Dcm_FindTxConnection(id);

    if (connIdx >= Dcm_ConfigPtr->numConnections)
    {
        return;
    }

    c = &Dcm_Conn[connIdx];

    if (c->state != DCM_CONN_TX_BUSY)
    {
        return;
    }

    c->txLen = 0u;
    c->txOffset = 0u;

    if (result != NTFRSLT_OK)
    {
        Dcm_ClearProcessing(connIdx);
        return;
    }

    if (c->pendingSessionAfterResponse == TRUE)
    {
        uint8 session;

        session = c->sessionAfterResponse;
        c->pendingSessionAfterResponse = FALSE;
        c->sessionAfterResponse = 0u;
        Dcm_SessionChangeAfterResponse(connIdx, session);
    }

    if (c->pendingResetAfterResponse == TRUE)
    {
        uint8 resetType;

        resetType = c->resetAfterResponse;
        c->pendingResetAfterResponse = FALSE;
        c->resetAfterResponse = 0u;
        Dcm_EcuResetAfterResponse(connIdx, resetType);
    }

    if ((c->service != NULL_PTR) && (c->pendingStarted == TRUE))
    {
        c->state = DCM_CONN_PROCESSING;
    }
    else
    {
        c->service = NULL_PTR;
        c->state = DCM_CONN_IDLE;
        Dcm_LoadNextRequest(connIdx);
        Dcm_ReleaseProtocol(connIdx);
    }
}

/* ===================== Processing ===================== */

static void Dcm_ProcessConnection(uint8 connIdx)
{
    Dcm_ConnectionStateType* c;
    Dcm_PduLengthType respLen;
    Dcm_PduLengthType txLen;
    Dcm_ReturnType ret;
    uint8 nrc;

    c = &Dcm_Conn[connIdx];

    if (c->state == DCM_CONN_IDLE)
    {
        Dcm_LoadNextRequest(connIdx);
        return;
    }

    if (c->state == DCM_CONN_TX_READY)
    {
        (void)Dcm_StartTx(connIdx);
        return;
    }

    if (c->state != DCM_CONN_PROCESSING)
    {
        return;
    }

    if (c->activeReqLen == 0u)
    {
        Dcm_ClearProcessing(connIdx);
        return;
    }

    if (c->service == NULL_PTR)
    {
        c->sid = c->activeReq[0];
        c->service = Dcm_FindService(c->sid);

        if (c->service == NULL_PTR)
        {
            nrc = DCM_NRC_SERVICE_NOT_SUPPORTED;

            if ((c->activeAddressing == DCM_ADDR_FUNCTIONAL) &&
                    (Dcm_ShouldSuppressFunctionalNrc(nrc) == TRUE))
            {
                Dcm_ClearProcessing(connIdx);
                return;
            }

            Dcm_BuildNegativeResponse(connIdx, c->sid, nrc);
            c->service = NULL_PTR;
            (void)Dcm_StartTx(connIdx);
            return;
        }

        if (Dcm_CheckServiceAccess(connIdx, c->sid, &nrc) == FALSE)
        {
            if ((c->activeAddressing == DCM_ADDR_FUNCTIONAL) &&
                    (Dcm_ShouldSuppressFunctionalNrc(nrc) == TRUE))
            {
                Dcm_ClearProcessing(connIdx);
                return;
            }

            Dcm_BuildNegativeResponse(connIdx, c->sid, nrc);
            c->service = NULL_PTR;
            (void)Dcm_StartTx(connIdx);
            return;
        }
    }

    respLen = 0u;

    ret = c->service->handler(
            connIdx,
            c->opStatus,
            &c->activeReq[1],
            (Dcm_PduLengthType)(c->activeReqLen - 1u),
            &c->txBuffer[1],
            &respLen
    );

    if (ret == DCM_E_PENDING)
    {
        c->opStatus = DCM_PENDING;
        Dcm_UpdatePendingTimers(connIdx, Dcm_ConsumeElapsedTicks(connIdx));

        if (c->p2Timer > 0u)
        {
            return;
        }

        if ((c->pendingResponseCount > 0u) && (c->p2StarTimer > 0u))
        {
            return;
        }

        if (c->pendingResponseCount < DCM_RESPONSE_PENDING_MAX)
        {
            Dcm_SendResponsePending(connIdx);
            return;
        }

        Dcm_AbortPendingOperation(connIdx, DCM_PENDING_TIMEOUT_NRC);
        return;
    }

    if (ret == DCM_E_OK)
    {
        if (c->suppressPositiveResponse == TRUE)
        {
            Dcm_ClearProcessing(connIdx);
            return;
        }

        if (respLen > DCM_MAX_SERVICE_RESPONSE_LEN)
        {
            Dcm_BuildNegativeResponse(connIdx, c->sid, DCM_NRC_RESPONSE_TOO_LONG);
            c->service = NULL_PTR;
            c->pendingStarted = FALSE;
            c->pendingResponseCount = 0u;
            (void)Dcm_StartTx(connIdx);
            return;
        }

        txLen = (Dcm_PduLengthType)(respLen + 1u);

        if (txLen > Dcm_GetTxPayloadLimit(connIdx))
        {
            Dcm_BuildNegativeResponse(connIdx, c->sid, DCM_NRC_RESPONSE_TOO_LONG);
            c->service = NULL_PTR;
            c->pendingStarted = FALSE;
            c->pendingResponseCount = 0u;
            (void)Dcm_StartTx(connIdx);
            return;
        }

        c->txBuffer[0] = (uint8)(c->sid + 0x40u);
        c->txLen = txLen;
        c->txOffset = 0u;
        c->service = NULL_PTR;
        c->pendingStarted = FALSE;
        c->pendingResponseCount = 0u;
        (void)Dcm_StartTx(connIdx);
        return;
    }

    nrc = ret;

    if ((c->activeAddressing == DCM_ADDR_FUNCTIONAL) && (Dcm_ShouldSuppressFunctionalNrc(nrc) == TRUE))
    {
        Dcm_ClearProcessing(connIdx);
        return;
    }

    Dcm_BuildNegativeResponse(connIdx, c->sid, nrc);
    c->service = NULL_PTR;
    c->pendingStarted = FALSE;
    c->pendingResponseCount = 0u;
    (void)Dcm_StartTx(connIdx);
}

static void Dcm_LoadNextRequest(uint8 connIdx)
{
    Dcm_ConnectionStateType* c;
    Dcm_RxRequestType* q;

    c = &Dcm_Conn[connIdx];

    if ((c->state != DCM_CONN_IDLE) || (c->qCount == 0u))
    {
        return;
    }

    q = &c->queue[c->qTail];

    if (q->valid == TRUE)
    {
        Dcm_StartProcessing(connIdx, q->data, q->len, q->addressing);

        q->valid = FALSE;
        q->len = 0u;

        c->qTail++;
        if (c->qTail >= DCM_RX_QUEUE_DEPTH)
        {
            c->qTail = 0u;
        }

        c->qCount--;
    }
}

static void Dcm_ClearProcessing(uint8 connIdx)
{
    Dcm_ConnectionStateType* c;

    c = &Dcm_Conn[connIdx];

    if ((Dcm_ConfigPtr != NULL_PTR) && (connIdx < Dcm_ConfigPtr->numConnections))
    {
        PduR_DcmReleaseNoResponse(Dcm_ConfigPtr->connections[connIdx].txPduId);
    }

    c->state = DCM_CONN_IDLE;
    c->activeReqLen = 0u;
    c->sid = 0u;
    c->service = NULL_PTR;
    c->opStatus = DCM_INITIAL;
    c->txLen = 0u;
    c->txOffset = 0u;
    c->p2Timer = 0u;
    c->p2StarTimer = 0u;
    c->pendingStarted = FALSE;
    c->pendingResponseCount = 0u;
    c->suppressPositiveResponse = FALSE;
    c->pendingSessionAfterResponse = FALSE;
    c->sessionAfterResponse = 0u;
    c->pendingResetAfterResponse = FALSE;
    c->resetAfterResponse = 0u;
    c->rxInProgress = FALSE;
    c->rxExpectedLen = 0u;
    c->rxCopiedLen = 0u;

    Dcm_ReleaseProtocol(connIdx);
    Dcm_LoadNextRequest(connIdx);
}

static Std_ReturnType Dcm_StartTx(uint8 connIdx)
{
    Dcm_ConnectionStateType* c;
    Std_ReturnType ret;

    c = &Dcm_Conn[connIdx];

    if ((c->txLen == 0u) || (c->txLen > DCM_MAX_RESPONSE_LEN))
    {
        return E_NOT_OK;
    }

    c->txOffset = 0u;
    c->state = DCM_CONN_TX_BUSY;

    ret = PduR_DcmTransmit(Dcm_ConfigPtr->connections[connIdx].txPduId,
            c->txBuffer,
            c->txLen);

    if (ret != E_OK)
    {
        c->state = DCM_CONN_TX_READY;
    }

    return ret;
}


static const Dcm_ServiceType* Dcm_FindService(uint8 sid)
{
    uint8 i;

    for (i = 0u; i < Dcm_ConfigPtr->numServices; i++)
    {
        if (Dcm_ConfigPtr->services[i].sid == sid)
        {
            return &Dcm_ConfigPtr->services[i];
        }
    }

    return NULL_PTR;
}

static uint8 Dcm_FindRxConnection(Dcm_PduIdType rxPduId)
{
    uint8 i;
    uint8 limit;

    if (Dcm_ConfigPtr == NULL_PTR)
    {
        return 0xFFu;
    }

    limit = Dcm_ConfigPtr->numConnections;

    if (limit > DCM_MAX_CONNECTIONS)
    {
        limit = DCM_MAX_CONNECTIONS;
    }

    for (i = 0u; i < limit; i++)
    {
        if (Dcm_ConfigPtr->connections[i].rxPduId == rxPduId)
        {
            return i;
        }
    }

    return 0xFFu;
}

static uint8 Dcm_FindTxConnection(Dcm_PduIdType txPduId)
{
    uint8 i;
    uint8 limit;

    if (Dcm_ConfigPtr == NULL_PTR)
    {
        return 0xFFu;
    }

    limit = Dcm_ConfigPtr->numConnections;

    if (limit > DCM_MAX_CONNECTIONS)
    {
        limit = DCM_MAX_CONNECTIONS;
    }

    for (i = 0u; i < limit; i++)
    {
        if ((Dcm_ConfigPtr->connections[i].txPduId == txPduId) &&
                (Dcm_Conn[i].state == DCM_CONN_TX_BUSY))
        {
            return i;
        }
    }

    for (i = 0u; i < limit; i++)
    {
        if (Dcm_ConfigPtr->connections[i].txPduId == txPduId)
        {
            return i;
        }
    }

    return 0xFFu;
}

static uint8 Dcm_GetSubFunction(uint8 rawSubFunction)
{
    return (uint8)(rawSubFunction & 0x7Fu);
}

static uint8 Dcm_IsSuppressPosRspBitSet(uint8 rawSubFunction)
{
    return ((rawSubFunction & DCM_SUPPRESS_POS_RSP_MSG_INDICATION_BIT) != 0u) ? TRUE : FALSE;
}

static Dcm_ReturnType Dcm_Service_ForwardStandard(
        uint8 sid,
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    return DcmAppl_StandardService(connIdx, opStatus, sid, req, len, resp, respLen);
}

static void Dcm_ResetNvMWriteAllState(void)
{
    Dcm_NvMWriteAllState = DCM_NVM_WRITE_ALL_IDLE;
    Dcm_NvMWriteAllOwnerConn = 0xFFu;
    Dcm_NvMWriteAllOwnerSid = 0u;
}

static Dcm_ReturnType Dcm_NvMWriteAllPending(uint8 connIdx)
{
    (void)connIdx;
    return DCM_E_PENDING;
}

static Dcm_ReturnType Dcm_PollNvMWriteAllBeforeDiagnosticReset(
        uint8 connIdx,
        uint8 sid,
        Dcm_OpStatusType opStatus)
{
    NvM_StatusType nvmStatus;

    if (opStatus == DCM_CANCEL)
    {
        if ((Dcm_NvMWriteAllOwnerConn == connIdx) &&
                (Dcm_NvMWriteAllOwnerSid == sid))
        {
            Dcm_ResetNvMWriteAllState();
        }

        return DCM_E_NOT_OK;
    }

    if (NvM_GetStatus() == NVM_UNINIT)
    {
        Dcm_ResetNvMWriteAllState();
        return DCM_NRC_GENERAL_PROGRAMMING_FAILURE;
    }

    if (Dcm_NvMWriteAllState == DCM_NVM_WRITE_ALL_IDLE)
    {
        Dcm_NvMWriteAllOwnerConn = connIdx;
        Dcm_NvMWriteAllOwnerSid = sid;
        Dcm_NvMWriteAllState = DCM_NVM_WRITE_ALL_WAIT_IDLE;
    }

    if ((Dcm_NvMWriteAllOwnerConn != connIdx) ||
            (Dcm_NvMWriteAllOwnerSid != sid))
    {
        return Dcm_NvMWriteAllPending(connIdx);
    }

    nvmStatus = NvM_GetStatus();
    if (Dcm_NvMWriteAllState == DCM_NVM_WRITE_ALL_WAIT_IDLE)
    {
        if (nvmStatus != NVM_IDLE)
        {
            return Dcm_NvMWriteAllPending(connIdx);
        }

        if (NvM_WriteAll() != E_OK)
        {
            Dcm_ResetNvMWriteAllState();
            return DCM_NRC_GENERAL_PROGRAMMING_FAILURE;
        }

        Dcm_NvMWriteAllState = DCM_NVM_WRITE_ALL_ACTIVE;
        return Dcm_NvMWriteAllPending(connIdx);
    }

    if (Dcm_NvMWriteAllState == DCM_NVM_WRITE_ALL_ACTIVE)
    {
        if (nvmStatus != NVM_IDLE)
        {
            return Dcm_NvMWriteAllPending(connIdx);
        }

        if (NvM_WriteAllBlocksFailed != 0u)
        {
            Dcm_ResetNvMWriteAllState();
            return DCM_NRC_GENERAL_PROGRAMMING_FAILURE;
        }

        Dcm_ResetNvMWriteAllState();
        return DCM_E_OK;
    }

    Dcm_ResetNvMWriteAllState();
    return DCM_NRC_GENERAL_REJECT;
}

/* ===================== Services ===================== */

static Dcm_ReturnType Dcm_Service_0x10(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    uint8 session;
    Dcm_ReturnType ret;

    if (len != 1u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    session = Dcm_GetSubFunction(req[0]);
    Dcm_Conn[connIdx].suppressPositiveResponse = Dcm_IsSuppressPosRspBitSet(req[0]);

    if ((session != DCM_SESSION_DEFAULT) &&
            (session != DCM_SESSION_PROGRAMMING) &&
            (session != DCM_SESSION_EXTENDED) &&
            (session != DCM_SESSION_CODING))
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }

    ret = Dcm_DiagnosticSessionControl(connIdx, opStatus, session, &resp[1], respLen);

    /* Persist NVM before the diagnostic reset only. Entering the programming
     * session (0x10 0x02) resets the ECU into the FBL, so a WriteAll is required
     * here. The coding session (0x10 0x41) does NOT reset and must not trigger a
     * WriteAll; coding data is committed explicitly via the CodingApp
     * RoutineControl WriteAll service instead. */
    if ((ret == DCM_E_OK) && (session == DCM_SESSION_PROGRAMMING))
    {
        ret = Dcm_PollNvMWriteAllBeforeDiagnosticReset(
                connIdx,
                DCM_SID_DIAGNOSTIC_SESSION_CONTROL,
                opStatus);
    }

    if (ret == DCM_E_OK)
    {
        Dcm_Conn[connIdx].session = session;
        Dcm_Conn[connIdx].s3Timer = DCM_S3_SERVER_TICKS;
        Dcm_Conn[connIdx].pendingSessionAfterResponse = TRUE;
        Dcm_Conn[connIdx].sessionAfterResponse = session;

        if (session == DCM_SESSION_DEFAULT)
        {
            Dcm_Conn[connIdx].downloadActive = FALSE;
            Dcm_Conn[connIdx].expectedBlockSequenceCounter = 1u;
        }

        DcmAppl_DiagnosticSessionChanged(connIdx, session);

        resp[0] = session;
        resp[1] = 0x00u;
        resp[2] = 0x32u;
        resp[3] = 0x01u;
        resp[4] = 0xF4u;
        *respLen = 5u;
    }

    return ret;
}

static Dcm_ReturnType Dcm_Service_0x11(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    uint8 resetType;
    Dcm_ReturnType ret;

    if (len != 1u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    resetType = Dcm_GetSubFunction(req[0]);
    Dcm_Conn[connIdx].suppressPositiveResponse = Dcm_IsSuppressPosRspBitSet(req[0]);

    ret = Dcm_EcuReset(connIdx, opStatus, resetType, &resp[1], respLen);

    if (ret == DCM_E_OK)
    {
        ret = Dcm_PollNvMWriteAllBeforeDiagnosticReset(
                connIdx,
                DCM_SID_ECU_RESET,
                opStatus);
    }

    if (ret == DCM_E_OK)
    {
        Dcm_Conn[connIdx].pendingResetAfterResponse = TRUE;
        Dcm_Conn[connIdx].resetAfterResponse = resetType;
        resp[0] = resetType;
        *respLen = 1u;
    }

    return ret;
}

static Dcm_ReturnType Dcm_DiagnosticSessionControl(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint8 session,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    (void)connIdx;
    (void)opStatus;
    (void)respData;
    (void)respLen;

    if ((session != DCM_SESSION_DEFAULT) &&
            (session != DCM_SESSION_PROGRAMMING) &&
            (session != DCM_SESSION_EXTENDED) &&
            (session != DCM_SESSION_CODING))
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }

    if (session == DCM_SESSION_DEFAULT)
    {
        McuSm_FBL_ProgrammingRequest = MCUSM_FBL_PROGRAMMING_REQUEST_NONE;
    }

    return DCM_E_OK;
}

static Dcm_ReturnType Dcm_EcuReset(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint8 resetType,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    (void)connIdx;
    (void)opStatus;
    (void)respData;

    /* Only hardReset (0x01) is supported. keyOffOnReset (0x02), softReset (0x03),
     * enableRapidPowerShutDown (0x04) and disableRapidPowerShutDown (0x05) are
     * intentionally rejected with SubFunctionNotSupported. */
    if (resetType != 0x01u)
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }

    *respLen = 0u;
    return DCM_E_OK;
}

static void Dcm_SessionChangeAfterResponse(uint8 connIdx, uint8 session)
{
    (void)connIdx;

    if (session == DCM_SESSION_PROGRAMMING)
    {
        McuSm_FBL_CommInterface = Dcm_NormalizeFblInterface(McuSm_FBL_CommInterface);
        McuSm_FBL_ProgrammingRequest = MCUSM_FBL_PROGRAMMING_REQUEST_ACTIVE;
        Dcm_ResetDelay();
        IfxCpu_disableInterrupts();
        IfxScuRcu_performReset(
                IfxScuRcu_ResetType_application,
                (uint16)DCM_RESET_INFO_ENTER_FBL);
    }
}

static void Dcm_EcuResetAfterResponse(uint8 connIdx, uint8 resetType)
{
    (void)connIdx;

    if (resetType == 0x01u)
    {
        /* Arm the deferred reset instead of resetting inline. See the comment on
         * Dcm_PendingHardReset: doing the reset here (on TX confirmation) drops
         * the 0x51 response because it has only been buffered, not yet sent. */
        McuSm_FBL_ProgrammingRequest = MCUSM_FBL_PROGRAMMING_REQUEST_NONE;
        Dcm_PendingHardReset = TRUE;
        Dcm_HardResetCountdown = DCM_HARD_RESET_DELAY_TICKS;
    }
}

static uint8 Dcm_NormalizeFblInterface(uint8 requestedInterface)
{
    if ((requestedInterface == MCUSM_FBL_COMM_CANFD) ||
            (requestedInterface == MCUSM_FBL_COMM_CAN_CLASSIC))
    {
        return requestedInterface;
    }

    return MCUSM_FBL_COMM_ETHERNET;
}

static void Dcm_ResetDelay(void)
{
    volatile uint32 delay;

    for (delay = 0u; delay < 200000u; delay++)
    {
    }
}

static Dcm_ReturnType Dcm_Service_0x14(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    uint32 dtcGroup;

    (void)resp;

    if (len != 3u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    dtcGroup = ((uint32)req[0] << 16u) |
            ((uint32)req[1] << 8u) |
            (uint32)req[2];

    *respLen = 0u;
    return Dcm_ClearDiagnosticInformation(connIdx, opStatus, dtcGroup);
}

static Dcm_ReturnType Dcm_Service_0x19(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    Dcm_ReturnType ret;

    if (len < 1u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    ret = Dcm_ReadDtcInformation(connIdx, opStatus, req, len, &resp[1], respLen);

    if (ret == DCM_E_OK)
    {
        resp[0] = req[0];
        *respLen = (Dcm_PduLengthType)(*respLen + 1u);
    }

    return ret;
}

static Dcm_ReturnType Dcm_ClearDiagnosticInformation(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint32 dtcGroup)
{
    (void)connIdx;
    (void)opStatus;

    if (Dem_ClearDTC(
            (Dem_DTCType)dtcGroup,
            DEM_DTC_FORMAT_UDS,
            DEM_DTC_ORIGIN_PRIMARY_MEMORY) == E_OK)
    {
        return DCM_E_OK;
    }

    return DCM_NRC_REQUEST_OUT_OF_RANGE;
}

static Dcm_ReturnType Dcm_ReadDtcInformation(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    uint8 subFunction;

    (void)connIdx;
    (void)opStatus;

    if ((reqData == NULL_PTR) || (respData == NULL_PTR) || (respLen == NULL_PTR) || (reqLen < 1u))
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    subFunction = reqData[0u];

    if (subFunction == 0x01u)
    {
        if (reqLen != 2u)
        {
            return DCM_NRC_INCORRECT_LENGTH;
        }

        return Dcm_ReadDtcByStatusMask(subFunction, reqData[1u], respData, respLen);
    }

    if (subFunction == 0x02u)
    {
        if (reqLen != 2u)
        {
            return DCM_NRC_INCORRECT_LENGTH;
        }

        return Dcm_ReadDtcByStatusMask(subFunction, reqData[1u], respData, respLen);
    }

    if (subFunction == 0x0Au)
    {
        if (reqLen != 1u)
        {
            return DCM_NRC_INCORRECT_LENGTH;
        }

        return Dcm_ReadDtcByStatusMask(subFunction, 0u, respData, respLen);
    }

    if ((subFunction == 0x04u) || (subFunction == 0x06u))
    {
        Dem_DTCType dtc;

        if (reqLen != 5u)
        {
            return DCM_NRC_INCORRECT_LENGTH;
        }

        dtc = ((Dem_DTCType)reqData[1u] << 16u) |
                ((Dem_DTCType)reqData[2u] << 8u) |
                (Dem_DTCType)reqData[3u];

        return Dcm_ReadDtcStoredDataByDtc(subFunction, dtc, reqData[4u], respData, respLen);
    }

    return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
}

static Dcm_ReturnType Dcm_ReadDtcByStatusMask(
        uint8 subFunction,
        uint8 statusMask,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    uint16 count;
    uint16 pos;
    Dem_DTCType dtc;
    Dem_UdsStatusByteType status;

    if ((respData == NULL_PTR) || (respLen == NULL_PTR))
    {
        return DCM_NRC_GENERAL_REJECT;
    }

    if (Dem_IsReady() == FALSE)
    {
        return DCM_NRC_CONDITIONS_NOT_CORRECT;
    }

    if (Dem_SetDTCFilter(
            statusMask,
            DEM_DTC_FORMAT_UDS,
            DEM_DTC_ORIGIN_PRIMARY_MEMORY,
            FALSE,
            0u,
            FALSE) != E_OK)
    {
        return DCM_NRC_CONDITIONS_NOT_CORRECT;
    }

    if (subFunction == 0x01u)
    {
        if (Dem_GetNumberOfFilteredDTC(&count) != E_OK)
        {
            return DCM_NRC_CONDITIONS_NOT_CORRECT;
        }

        respData[0u] = Dem_GetDTCStatusAvailabilityMask();
        respData[1u] = DCM_DTC_FORMAT_IDENTIFIER_UDS;
        respData[2u] = (uint8)((count >> 8u) & 0xFFu);
        respData[3u] = (uint8)(count & 0xFFu);
        *respLen = 4u;
        return DCM_E_OK;
    }

    respData[0u] = Dem_GetDTCStatusAvailabilityMask();
    pos = 1u;

    while (Dem_GetNextFilteredDTC(&dtc, &status) == E_OK)
    {
        if ((pos + 4u) > DCM_MAX_DTC_RESPONSE_LEN)
        {
            return DCM_NRC_RESPONSE_TOO_LONG;
        }

        respData[pos] = (uint8)((dtc >> 16u) & 0xFFu);
        pos++;
        respData[pos] = (uint8)((dtc >> 8u) & 0xFFu);
        pos++;
        respData[pos] = (uint8)(dtc & 0xFFu);
        pos++;
        respData[pos] = status;
        pos++;
    }

    *respLen = (Dcm_PduLengthType)pos;
    return DCM_E_OK;
}

static Dcm_ReturnType Dcm_ReadDtcStoredDataByDtc(
        uint8 subFunction,
        Dem_DTCType dtc,
        uint8 recordNumber,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    uint16 dataLen;
    Dem_UdsStatusByteType status;
    Std_ReturnType ret;

    if ((respData == NULL_PTR) || (respLen == NULL_PTR))
    {
        return DCM_NRC_GENERAL_REJECT;
    }

    if (Dem_IsReady() == FALSE)
    {
        return DCM_NRC_CONDITIONS_NOT_CORRECT;
    }

    if (Dem_GetStatusOfDTC(
            dtc,
            DEM_DTC_FORMAT_UDS,
            DEM_DTC_ORIGIN_PRIMARY_MEMORY,
            &status) != E_OK)
    {
        return DCM_NRC_REQUEST_OUT_OF_RANGE;
    }

    if (DCM_MAX_DTC_RESPONSE_LEN < 5u)
    {
        return DCM_NRC_RESPONSE_TOO_LONG;
    }

    dataLen = (uint16)(DCM_MAX_DTC_RESPONSE_LEN - 5u);

    if (subFunction == 0x04u)
    {
        ret = Dem_GetFreezeFrameDataByDTC(
                dtc,
                DEM_DTC_FORMAT_UDS,
                DEM_DTC_ORIGIN_PRIMARY_MEMORY,
                recordNumber,
                &respData[5u],
                &dataLen);
    }
    else
    {
        ret = Dem_GetExtendedDataRecordByDTC(
                dtc,
                DEM_DTC_FORMAT_UDS,
                DEM_DTC_ORIGIN_PRIMARY_MEMORY,
                recordNumber,
                &respData[5u],
                &dataLen);
    }

    if (ret != E_OK)
    {
        return DCM_NRC_REQUEST_OUT_OF_RANGE;
    }

    respData[0u] = (uint8)((dtc >> 16u) & 0xFFu);
    respData[1u] = (uint8)((dtc >> 8u) & 0xFFu);
    respData[2u] = (uint8)(dtc & 0xFFu);
    respData[3u] = status;
    respData[4u] = recordNumber;
    *respLen = (Dcm_PduLengthType)(5u + dataLen);

    return DCM_E_OK;
}

static Dcm_ReturnType Dcm_Service_0x22(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    Dcm_PduLengthType pos;
    Dcm_PduLengthType dataLen;
    Dcm_ReturnType ret;
    uint16 did;
    Dcm_PduLengthType i;

    if ((len < 2u) || ((len % 2u) != 0u))
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    pos = 0u;

    for (i = 0u; i < len; i += 2u)
    {
        did = (uint16)(((uint16)req[i] << 8u) | req[i + 1u]);

        if ((pos + 2u) > DCM_MAX_SERVICE_RESPONSE_LEN)
        {
            return DCM_NRC_RESPONSE_TOO_LONG;
        }

        resp[pos++] = req[i];
        resp[pos++] = req[i + 1u];

        if (did == DCM_DID_ACTIVE_SOFTWARE_BLOCK)
        {
            if ((pos + 1u) > DCM_MAX_SERVICE_RESPONSE_LEN)
            {
                return DCM_NRC_RESPONSE_TOO_LONG;
            }

            resp[pos] = DCM_ACTIVE_SOFTWARE_BLOCK_APP;
            pos++;
            continue;
        }

        if (did == DCM_DID_APPLICATION_SOFTWARE_VERSION)
        {
            if ((pos + 6u) > DCM_MAX_SERVICE_RESPONSE_LEN)
            {
                return DCM_NRC_RESPONSE_TOO_LONG;
            }

            resp[pos] = DCM_APP_SW_VERSION_MAJOR;
            pos++;
            resp[pos] = DCM_APP_SW_VERSION_MINOR;
            pos++;
            resp[pos] = DCM_APP_SW_VERSION_PATCH;
            pos++;
            resp[pos] = DCM_FBL_SW_VERSION_MAJOR;
            pos++;
            resp[pos] = DCM_FBL_SW_VERSION_MINOR;
            pos++;
            resp[pos] = DCM_FBL_SW_VERSION_PATCH;
            pos++;
            continue;
        }

        if (did == DCM_DID_ACTIVE_DIAGNOSTIC_SESSION)
        {
            if ((pos + 1u) > DCM_MAX_SERVICE_RESPONSE_LEN)
            {
                return DCM_NRC_RESPONSE_TOO_LONG;
            }

            resp[pos] = Dcm_GetActiveSession(connIdx);
            pos++;
            continue;
        }

        dataLen = (Dcm_PduLengthType)(DCM_MAX_SERVICE_RESPONSE_LEN - pos);

        ret = DcmAppl_ReadDataByIdentifier(connIdx, opStatus, did, &resp[pos], &dataLen);

        if (ret != DCM_E_OK)
        {
            return ret;
        }

        pos = (Dcm_PduLengthType)(pos + dataLen);
    }

    *respLen = pos;
    return DCM_E_OK;
}

static Dcm_ReturnType Dcm_Service_0x28(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    Dcm_ReturnType ret;
    uint8 controlType;

    if (len != 2u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    controlType = Dcm_GetSubFunction(req[0]);
    Dcm_Conn[connIdx].suppressPositiveResponse = Dcm_IsSuppressPosRspBitSet(req[0]);

    /* disableRxAndEnableTx (0x02) and disableRxAndTx (0x03) are intentionally
     * unsupported and rejected with SubFunctionNotSupported. */
    if ((controlType == 0x02u) || (controlType == 0x03u))
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }

    ret = DcmAppl_CommunicationControl(connIdx, opStatus, controlType, req[1]);

    if (ret == DCM_E_OK)
    {
        resp[0] = controlType;
        *respLen = 1u;
    }

    return ret;
}

static Dcm_ReturnType Dcm_Service_0x31(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    uint8 routineControlType;
    uint16 routineId;
    Dcm_ReturnType ret;

    if (len < 3u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    routineControlType = Dcm_GetSubFunction(req[0]);
    Dcm_Conn[connIdx].suppressPositiveResponse = Dcm_IsSuppressPosRspBitSet(req[0]);
    routineId = (uint16)(((uint16)req[1] << 8u) | req[2]);

    if (Dcm_TimeRoutine_IsRoutineId(routineId) != FALSE)
    {
        ret = Dcm_TimeRoutine_HandleRoutineControl(
                opStatus,
                routineControlType,
                routineId,
                &req[3],
                (Dcm_PduLengthType)(len - 3u),
                &resp[3],
                respLen);
    }
    else if (routineId == DCM_ROUTINE_SELECT_FBL_INTERFACE)
    {
        ret = Dcm_SelectFblInterfaceRoutine(
                routineControlType,
                &req[3],
                (Dcm_PduLengthType)(len - 3u),
                &resp[3],
                respLen);
    }
    else
    {
        ret = DcmAppl_RoutineControl(
                connIdx,
                opStatus,
                routineControlType,
                routineId,
                &req[3],
                (Dcm_PduLengthType)(len - 3u),
                &resp[3],
                respLen);
    }

    if (ret == DCM_E_OK)
    {
        resp[0] = routineControlType;
        resp[1] = req[1];
        resp[2] = req[2];
        *respLen = (Dcm_PduLengthType)(*respLen + 3u);
    }

    return ret;
}

static Dcm_ReturnType Dcm_SelectFblInterfaceRoutine(
        uint8 routineControlType,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    if (routineControlType != 0x01u)
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }

    if ((reqData == NULL_PTR) || (respData == NULL_PTR) || (respLen == NULL_PTR))
    {
        return DCM_NRC_GENERAL_REJECT;
    }

    if (reqLen != 1u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    McuSm_FBL_CommInterface = Dcm_NormalizeFblInterface(reqData[0u]);
    McuSm_FBL_ProgrammingRequest = MCUSM_FBL_PROGRAMMING_REQUEST_NONE;
    respData[0u] = McuSm_FBL_CommInterface;
    *respLen = 1u;
    return DCM_E_OK;
}

static Dcm_ReturnType Dcm_Service_0x34(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    Dcm_ReturnType ret;

    if (len < 3u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    ret = DcmAppl_RequestDownload(connIdx, opStatus, req, len, resp, respLen);

    if (ret == DCM_E_OK)
    {
        Dcm_Conn[connIdx].downloadActive = TRUE;
        Dcm_Conn[connIdx].expectedBlockSequenceCounter = 1u;
    }

    return ret;
}

static Dcm_ReturnType Dcm_Service_0x35(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    if (len < 3u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    return Dcm_Service_ForwardStandard(DCM_SID_REQUEST_UPLOAD, connIdx, opStatus, req, len, resp, respLen);
}

static Dcm_ReturnType Dcm_Service_0x36(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    Dcm_ReturnType ret;
    uint8 blockSequenceCounter;

    if (len < 1u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    if (Dcm_Conn[connIdx].downloadActive == FALSE)
    {
        return DCM_NRC_REQUEST_SEQUENCE_ERROR;
    }

    blockSequenceCounter = req[0];

    if (blockSequenceCounter != Dcm_Conn[connIdx].expectedBlockSequenceCounter)
    {
        return DCM_NRC_WRONG_BLOCK_SEQUENCE_COUNTER;
    }

    ret = DcmAppl_TransferData(
            connIdx,
            opStatus,
            blockSequenceCounter,
            &req[1],
            (Dcm_PduLengthType)(len - 1u),
            &resp[1],
            respLen
    );

    if (ret == DCM_E_OK)
    {
        resp[0] = blockSequenceCounter;
        *respLen = (Dcm_PduLengthType)(*respLen + 1u);
        Dcm_Conn[connIdx].expectedBlockSequenceCounter++;
    }

    return ret;
}

static Dcm_ReturnType Dcm_Service_0x37(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    Dcm_ReturnType ret;

    if (Dcm_Conn[connIdx].downloadActive == FALSE)
    {
        return DCM_NRC_REQUEST_SEQUENCE_ERROR;
    }

    ret = DcmAppl_RequestTransferExit(connIdx, opStatus, req, len, resp, respLen);

    if (ret == DCM_E_OK)
    {
        Dcm_Conn[connIdx].downloadActive = FALSE;
        Dcm_Conn[connIdx].expectedBlockSequenceCounter = 1u;
    }

    return ret;
}

static Dcm_ReturnType Dcm_Service_0x3E(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    uint8 zeroSub;

    (void)opStatus;

    if (len != 1u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    zeroSub = Dcm_GetSubFunction(req[0]);

    if (zeroSub != 0x00u)
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }

    Dcm_Conn[connIdx].suppressPositiveResponse = Dcm_IsSuppressPosRspBitSet(req[0]);
    Dcm_Conn[connIdx].s3Timer = DCM_S3_SERVER_TICKS;

    resp[0] = 0x00u;
    *respLen = 1u;

    return DCM_E_OK;
}

static Dcm_ReturnType Dcm_Service_0x85(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    uint8 settingType;
    Dcm_ReturnType ret;

    if (len != 1u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    settingType = Dcm_GetSubFunction(req[0]);
    Dcm_Conn[connIdx].suppressPositiveResponse = Dcm_IsSuppressPosRspBitSet(req[0]);

    ret = DcmAppl_ControlDtcSetting(connIdx, opStatus, settingType);

    if (ret == DCM_E_OK)
    {
        resp[0] = settingType;
        *respLen = 1u;
    }

    return ret;
}

/* ===================== Weak hooks ===================== */

#if defined(__TASKING__)
#define DCM_WEAK __attribute__((weak))
#else
#define DCM_WEAK __attribute__((weak))
#endif

DCM_WEAK Std_ReturnType Dcm_LoTransmit(Dcm_PduIdType txPduId, Dcm_PduLengthType len)
{
    (void)txPduId;
    (void)len;
    return E_NOT_OK;
}

DCM_WEAK void DcmAppl_DiagnosticSessionChanged(uint8 connIdx, uint8 session)
{
    (void)connIdx;
    (void)session;
}

DCM_WEAK Dcm_ReturnType DcmAppl_ReadDataByIdentifier(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint16 did,
        uint8* data,
        Dcm_PduLengthType* dataLen)
{
    (void)connIdx;
    (void)opStatus;
    (void)did;
    (void)data;
    (void)dataLen;
    return DCM_NRC_REQUEST_OUT_OF_RANGE;
}

DCM_WEAK Dcm_ReturnType DcmAppl_WriteDataByIdentifier(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint16 did,
        const uint8* data,
        Dcm_PduLengthType dataLen)
{
    (void)connIdx;
    (void)opStatus;
    (void)did;
    (void)data;
    (void)dataLen;
    return DCM_NRC_REQUEST_OUT_OF_RANGE;
}

DCM_WEAK Dcm_ReturnType DcmAppl_RoutineControl(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint8 routineControlType,
        uint16 routineId,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    (void)connIdx;
    (void)opStatus;
    (void)routineControlType;
    (void)routineId;
    (void)reqData;
    (void)reqLen;
    (void)respData;
    (void)respLen;
    return DCM_NRC_REQUEST_OUT_OF_RANGE;
}

DCM_WEAK Dcm_ReturnType DcmAppl_CommunicationControl(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint8 controlType,
        uint8 communicationType)
{
    (void)connIdx;
    (void)opStatus;
    (void)controlType;
    (void)communicationType;
    return DCM_NRC_REQUEST_OUT_OF_RANGE;
}

DCM_WEAK Dcm_ReturnType DcmAppl_ControlDtcSetting(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint8 settingType)
{
    (void)connIdx;
    (void)opStatus;
    (void)settingType;
    return DCM_NRC_REQUEST_OUT_OF_RANGE;
}

DCM_WEAK Dcm_ReturnType DcmAppl_RequestDownload(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    (void)connIdx;
    (void)opStatus;
    (void)reqData;
    (void)reqLen;
    (void)respData;
    (void)respLen;

    return DCM_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED;
}

DCM_WEAK Dcm_ReturnType DcmAppl_TransferData(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint8 blockSequenceCounter,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    (void)connIdx;
    (void)opStatus;
    (void)blockSequenceCounter;
    (void)reqData;
    (void)reqLen;
    (void)respData;
    (void)respLen;

    return DCM_NRC_TRANSFER_DATA_SUSPENDED;
}

DCM_WEAK Dcm_ReturnType DcmAppl_RequestTransferExit(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    (void)connIdx;
    (void)opStatus;
    (void)reqData;
    (void)reqLen;
    (void)respData;
    (void)respLen;

    return DCM_NRC_REQUEST_SEQUENCE_ERROR;
}

DCM_WEAK Dcm_ReturnType DcmAppl_StandardService(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint8 sid,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    (void)connIdx;
    (void)opStatus;
    (void)sid;
    (void)reqData;
    (void)reqLen;
    (void)respData;
    (void)respLen;

    return DCM_NRC_REQUEST_OUT_OF_RANGE;
}

uint8 *Dcm_ProvideRxBufferFromDoIP(uint16 len)
{
    (void)len;
    return NULL;
}
