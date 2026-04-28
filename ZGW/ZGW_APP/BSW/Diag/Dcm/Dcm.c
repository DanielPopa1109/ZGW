/* Dcm.c */
#include "Dcm.h"
#include <string.h>
#include "PduR.h"

extern volatile uint32 OS_Counter_core0;

/* ===================== Internal types ===================== */

typedef enum
{
    DCM_CONN_IDLE = 0u,
    DCM_CONN_RECEIVING,
    DCM_CONN_PROCESSING,
    DCM_CONN_TX_READY,
    DCM_CONN_TX_BUSY
} Dcm_ConnStateType;

typedef struct
{
        uint8 data[DCM_MAX_PDU_LEN];
        Dcm_PduLengthType len;
        uint8 valid;
        Dcm_AddressingType addressing;
} Dcm_RxRequestType;

typedef struct
{
        uint8 unlocked;
        uint8 attempts;
        uint8 seedValid;
        uint8 seed[4];
        uint32 lockUntil;
} Dcm_SecStateType;

typedef struct
{
        Dcm_ConnStateType state;

        uint8 rxBuffer[DCM_MAX_PDU_LEN];
        Dcm_PduLengthType rxExpectedLen;
        Dcm_PduLengthType rxCopiedLen;
        uint8 rxInProgress;

        Dcm_RxRequestType queue[DCM_RX_QUEUE_DEPTH];
        uint8 qHead;
        uint8 qTail;
        uint8 qCount;

        uint8 activeReq[DCM_MAX_PDU_LEN];
        Dcm_PduLengthType activeReqLen;
        Dcm_AddressingType activeAddressing;

        uint8 txBuffer[DCM_MAX_RESPONSE_LEN];
        Dcm_PduLengthType txLen;
        Dcm_PduLengthType txOffset;

        uint8 sid;
        const Dcm_ServiceType* service;
        Dcm_OpStatusType opStatus;

        uint16 p2Timer;
        uint16 p2StarTimer;
        uint16 pendingTimer;
        uint8 pendingStarted;
        uint32 lastTimerTick;

        uint8 suppressPositiveResponse;
        uint8 session;

        uint16 s3Timer;
        uint8 downloadActive;
        uint8 expectedBlockSequenceCounter;

        Dcm_SecStateType security[DCM_MAX_SECURITY_LEVELS];
} Dcm_ConnectionStateType;

/* ===================== Static ===================== */

static const Dcm_ConfigType* Dcm_ConfigPtr = NULL_PTR;
static Dcm_ConnectionStateType Dcm_Conn[DCM_MAX_CONNECTIONS];
static uint8 Dcm_ActiveProtocolConnection = 0xFFu;

/* ===================== Forward ===================== */
static uint8 Dcm_CheckServiceAccess(uint8 connIdx, uint8 sid, uint8* nrc);
static uint8 Dcm_FindRxConnection(Dcm_PduIdType rxPduId);
static uint8 Dcm_FindTxConnection(Dcm_PduIdType txPduId);
static void Dcm_ProcessConnection(uint8 connIdx);
static void Dcm_LoadNextRequest(uint8 connIdx);
static void Dcm_StartProcessing(uint8 connIdx, const uint8* req, Dcm_PduLengthType len, Dcm_AddressingType addressing);
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
    c->pendingTimer = DCM_RESPONSE_PENDING_PERIOD;
    c->pendingStarted = FALSE;
    c->lastTimerTick = DCM_S3_SERVER_TICKS;
    c->suppressPositiveResponse = FALSE;

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
static uint16 Dcm_ConsumeElapsedTicks(uint8 connIdx)
{
    Dcm_ConnectionStateType* c;
    uint32 now;
    uint32 elapsed;

    c = &Dcm_Conn[connIdx];
    now = OS_Counter_core0;
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

            if (c->pendingTimer > 0u)
            {
                c->pendingTimer--;
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


static void Dcm_BuildNegativeResponse(uint8 connIdx, uint8 sid, uint8 nrc);
static uint8 Dcm_ShouldSuppressFunctionalNrc(uint8 nrc);
static const Dcm_ServiceType* Dcm_FindService(uint8 sid);
static uint8 Dcm_GetSubFunction(uint8 rawSubFunction);
static uint8 Dcm_IsSuppressPosRspBitSet(uint8 rawSubFunction);

/* Services */
static Dcm_ReturnType Dcm_Service_0x10(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x11(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x19(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x22(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x27(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x28(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x2E(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x31(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x34(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x36(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x37(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x3E(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);
static Dcm_ReturnType Dcm_Service_0x85(uint8, Dcm_OpStatusType, const uint8*, Dcm_PduLengthType, uint8*, Dcm_PduLengthType*);

/* Security */
static uint32 Dcm_SecMix(uint32 x);
static void Dcm_SecGenSeed(uint8 level, uint8 out[4]);
static uint32 Dcm_SecSeedToU32(const uint8 s[4]);
static uint32 Dcm_SecCalcKey(uint32 seed, uint8 level);

/* ===================== Default config ===================== */

const Dcm_ServiceType Dcm_DefaultServices[] =
{
        { DCM_SID_DIAGNOSTIC_SESSION_CONTROL, Dcm_Service_0x10 },
        { DCM_SID_ECU_RESET,                  Dcm_Service_0x11 },
        { DCM_SID_READ_DTC_INFORMATION,       Dcm_Service_0x19 },
        { DCM_SID_READ_DATA_BY_IDENTIFIER,    Dcm_Service_0x22 },
        { DCM_SID_SECURITY_ACCESS,            Dcm_Service_0x27 },
        { DCM_SID_COMMUNICATION_CONTROL,      Dcm_Service_0x28 },
        { DCM_SID_WRITE_DATA_BY_IDENTIFIER,   Dcm_Service_0x2E },
        { DCM_SID_ROUTINE_CONTROL,            Dcm_Service_0x31 },
        { DCM_SID_REQUEST_DOWNLOAD,           Dcm_Service_0x34 },
        { DCM_SID_TRANSFER_DATA,              Dcm_Service_0x36 },
        { DCM_SID_REQUEST_TRANSFER_EXIT,      Dcm_Service_0x37 },
        { DCM_SID_TESTER_PRESENT,             Dcm_Service_0x3E },
        { DCM_SID_CONTROL_DTC_SETTING,        Dcm_Service_0x85 }
};

typedef struct
{
        uint8 sid;
        uint16 sessionMask;
        uint8 securityMask;
} Dcm_ServiceAccessType;

static const Dcm_ServiceAccessType Dcm_ServiceAccessTable[] =
{
        { DCM_SID_DIAGNOSTIC_SESSION_CONTROL, DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_PROGRAMMING | DCM_SESSION_MASK_EXTENDED, DCM_SEC_MASK_LOCKED | DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_ECU_RESET,                  DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_PROGRAMMING | DCM_SESSION_MASK_EXTENDED, DCM_SEC_MASK_LOCKED | DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_READ_DTC_INFORMATION,       DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_EXTENDED,                              DCM_SEC_MASK_LOCKED | DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_READ_DATA_BY_IDENTIFIER,    DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_EXTENDED,                              DCM_SEC_MASK_LOCKED | DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_SECURITY_ACCESS,            DCM_SESSION_MASK_EXTENDED | DCM_SESSION_MASK_PROGRAMMING,                          DCM_SEC_MASK_LOCKED | DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_COMMUNICATION_CONTROL,      DCM_SESSION_MASK_EXTENDED,                                                        DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_WRITE_DATA_BY_IDENTIFIER,   DCM_SESSION_MASK_EXTENDED,                                                        DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_ROUTINE_CONTROL,            DCM_SESSION_MASK_EXTENDED | DCM_SESSION_MASK_PROGRAMMING,                          DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_REQUEST_DOWNLOAD,           DCM_SESSION_MASK_PROGRAMMING,                                                     DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_TRANSFER_DATA,              DCM_SESSION_MASK_PROGRAMMING,                                                     DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_REQUEST_TRANSFER_EXIT,      DCM_SESSION_MASK_PROGRAMMING,                                                     DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_TESTER_PRESENT,             DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_PROGRAMMING | DCM_SESSION_MASK_EXTENDED, DCM_SEC_MASK_LOCKED | DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 },
        { DCM_SID_CONTROL_DTC_SETTING,        DCM_SESSION_MASK_EXTENDED,                                                        DCM_SEC_MASK_L1 | DCM_SEC_MASK_L2 | DCM_SEC_MASK_L3 }
};

/* ===================== API ===================== */

static uint8 Dcm_CheckServiceAccess(uint8 connIdx, uint8 sid, uint8* nrc)
{
    uint8 i;
    uint8 level;
    uint8 unlockedMask = DCM_SEC_MASK_LOCKED;

    for (level = 0u; level < DCM_MAX_SECURITY_LEVELS; level++)
    {
        if (Dcm_Conn[connIdx].security[level].unlocked != FALSE)
        {
            unlockedMask |= (uint8)(1u << (level + 1u));
        }
    }

    for (i = 0u; i < (uint8)(sizeof(Dcm_ServiceAccessTable) / sizeof(Dcm_ServiceAccessTable[0])); i++)
    {
        if (Dcm_ServiceAccessTable[i].sid == sid)
        {
            if ((Dcm_ServiceAccessTable[i].sessionMask & (uint16)(1u << Dcm_Conn[connIdx].session)) == 0u)
            {
                *nrc = DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION;
                return FALSE;
            }

            if ((Dcm_ServiceAccessTable[i].securityMask & unlockedMask) == 0u)
            {
                *nrc = DCM_NRC_SECURITY_ACCESS_DENIED;
                return FALSE;
            }

            return TRUE;
        }
    }

    *nrc = DCM_NRC_SERVICE_NOT_SUPPORTED;
    return FALSE;
}

void Dcm_Init(const Dcm_ConfigType* ConfigPtr)
{
    uint8 i;

    Dcm_ConfigPtr = (ConfigPtr == NULL_PTR) ? &Dcm_Config : ConfigPtr;

    memset(Dcm_Conn, 0, sizeof(Dcm_Conn));
    Dcm_ActiveProtocolConnection = 0xFFu;

    for (i = 0u; i < DCM_MAX_CONNECTIONS; i++)
    {
        Dcm_Conn[i].state = DCM_CONN_IDLE;
        Dcm_Conn[i].session = DCM_SESSION_DEFAULT;
        Dcm_Conn[i].s3Timer = 0u;
        Dcm_Conn[i].lastTimerTick = OS_Counter_core0;
        Dcm_Conn[i].downloadActive = FALSE;
        Dcm_Conn[i].expectedBlockSequenceCounter = 1u;
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
                memset(Dcm_Conn[i].security, 0, sizeof(Dcm_Conn[i].security));
                Dcm_Conn[i].downloadActive = FALSE;
                Dcm_Conn[i].expectedBlockSequenceCounter = 1u;
            }
        }

        Dcm_ProcessConnection(i);
    }
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
        return BUFREQ_E_BUSY;
    }

    if ((c->state != DCM_CONN_IDLE) && (c->qCount >= DCM_RX_QUEUE_DEPTH))
    {
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

    if (c->service != NULL_PTR)
    {
        c->state = DCM_CONN_PROCESSING;
    }
    else
    {
        c->state = DCM_CONN_IDLE;
        Dcm_LoadNextRequest(connIdx);
    }
}

/* ===================== Processing ===================== */

static void Dcm_ProcessConnection(uint8 connIdx)
{
    Dcm_ConnectionStateType* c;
    Dcm_PduLengthType respLen;
    Dcm_ReturnType ret;
    uint8 nrc;

    c = &Dcm_Conn[connIdx];

    if (c->state == DCM_CONN_IDLE)
    {
        Dcm_LoadNextRequest(connIdx);
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

            if (Dcm_CheckServiceAccess(connIdx, c->sid, &nrc) == FALSE)
            {
                if ((c->activeAddressing == DCM_ADDR_FUNCTIONAL) && (Dcm_ShouldSuppressFunctionalNrc(nrc) == TRUE))
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

        if (c->p2StarTimer == 0u)
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
            Dcm_ClearProcessing(connIdx);
            return;
        }

        if (c->pendingTimer > 0u)
        {
            return;
        }

        c->pendingTimer = DCM_RESPONSE_PENDING_PERIOD;
        c->pendingStarted = TRUE;

        if ((c->activeAddressing == DCM_ADDR_FUNCTIONAL) &&
                (Dcm_ShouldSuppressFunctionalNrc(DCM_NRC_RESPONSE_PENDING) == TRUE))
        {
            return;
        }

        Dcm_BuildNegativeResponse(connIdx, c->sid, DCM_NRC_RESPONSE_PENDING);
        (void)Dcm_StartTx(connIdx);
        return;
    }

    if (ret == DCM_E_OK)
    {
        if (c->suppressPositiveResponse == TRUE)
        {
            Dcm_ClearProcessing(connIdx);
            return;
        }

        if ((respLen + 1u) > DCM_MAX_RESPONSE_LEN)
        {
            Dcm_BuildNegativeResponse(connIdx, c->sid, DCM_NRC_RESPONSE_TOO_LONG);
            c->service = NULL_PTR;
            (void)Dcm_StartTx(connIdx);
            return;
        }

        c->txBuffer[0] = (uint8)(c->sid + 0x40u);
        c->txLen = (Dcm_PduLengthType)(respLen + 1u);
        c->txOffset = 0u;
        c->service = NULL_PTR;
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

    c->state = DCM_CONN_IDLE;
    c->activeReqLen = 0u;
    c->sid = 0u;
    c->service = NULL_PTR;
    c->opStatus = DCM_INITIAL;
    c->txLen = 0u;
    c->txOffset = 0u;
    c->p2Timer = 0u;
    c->p2StarTimer = 0u;
    c->pendingTimer = 0u;
    c->pendingStarted = FALSE;
    c->suppressPositiveResponse = FALSE;
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
        c->state = DCM_CONN_PROCESSING;
    }

    return ret;
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
        (session != DCM_SESSION_EXTENDED))
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }

    ret = DcmAppl_DiagnosticSessionControl(connIdx, opStatus, session, &resp[1], respLen);

    if (ret == DCM_E_OK)
    {
        Dcm_Conn[connIdx].session = session;
        Dcm_Conn[connIdx].s3Timer = DCM_S3_SERVER_TICKS;

        if (session == DCM_SESSION_DEFAULT)
        {
            memset(Dcm_Conn[connIdx].security, 0, sizeof(Dcm_Conn[connIdx].security));
            Dcm_Conn[connIdx].downloadActive = FALSE;
            Dcm_Conn[connIdx].expectedBlockSequenceCounter = 1u;
        }

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

    ret = DcmAppl_EcuReset(connIdx, opStatus, resetType, &resp[1], respLen);

    if (ret == DCM_E_OK)
    {
        resp[0] = resetType;
        *respLen = 1u;
    }

    return ret;
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

    ret = DcmAppl_ReadDtcInformation(connIdx, opStatus, req, len, &resp[1], respLen);

    if (ret == DCM_E_OK)
    {
        resp[0] = req[0];
        *respLen = (Dcm_PduLengthType)(*respLen + 1u);
    }

    return ret;
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

        if ((pos + 2u) >= DCM_MAX_RESPONSE_LEN)
        {
            return DCM_NRC_RESPONSE_TOO_LONG;
        }

        resp[pos++] = req[i];
        resp[pos++] = req[i + 1u];

        dataLen = (Dcm_PduLengthType)(DCM_MAX_RESPONSE_LEN - pos);

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

static Dcm_ReturnType Dcm_Service_0x27(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    uint8 sub;
    uint8 level;
    uint32 seed;
    uint32 expected;
    uint32 received;
    Dcm_SecStateType* sec;

    (void)opStatus;

    if (len < 1u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    sub = req[0];

    if ((sub == 0u) || (sub > ((DCM_MAX_SECURITY_LEVELS * 2u))))
    {
        return DCM_NRC_REQUEST_OUT_OF_RANGE;
    }

    level = (uint8)((sub - 1u) / 2u);

    if (level >= DCM_MAX_SECURITY_LEVELS)
    {
        return DCM_NRC_REQUEST_OUT_OF_RANGE;
    }

    sec = &Dcm_Conn[connIdx].security[level];

    if (sec->lockUntil > OS_Counter_core0)
    {
        return DCM_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED;
    }

    if ((sub & 0x01u) != 0u)
    {
        if (len != 1u)
        {
            return DCM_NRC_INCORRECT_LENGTH;
        }

        Dcm_SecGenSeed(level, sec->seed);
        sec->seedValid = TRUE;

        resp[0] = sub;
        resp[1] = sec->seed[0];
        resp[2] = sec->seed[1];
        resp[3] = sec->seed[2];
        resp[4] = sec->seed[3];
        *respLen = 5u;

        return DCM_E_OK;
    }

    if (len != 5u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    if (sec->seedValid == FALSE)
    {
        return DCM_NRC_REQUEST_SEQUENCE_ERROR;
    }

    seed = Dcm_SecSeedToU32(sec->seed);
    expected = Dcm_SecCalcKey(seed, level);
    received = Dcm_SecSeedToU32(&req[1]);

    if (received != expected)
    {
        sec->attempts++;

        if (sec->attempts >= 3u)
        {
            sec->lockUntil = OS_Counter_core0 + 30000u;
            sec->attempts = 0u;
            sec->seedValid = FALSE;
            return DCM_NRC_EXCEED_NUMBER_OF_ATTEMPTS;
        }

        return DCM_NRC_INVALID_KEY;
    }

    sec->unlocked = TRUE;
    sec->attempts = 0u;
    sec->seedValid = FALSE;

    resp[0] = sub;
    *respLen = 1u;

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

    ret = DcmAppl_CommunicationControl(connIdx, opStatus, controlType, req[1]);

    if (ret == DCM_E_OK)
    {
        resp[0] = controlType;
        *respLen = 1u;
    }

    return ret;
}

static Dcm_ReturnType Dcm_Service_0x2E(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        const uint8* req,
        Dcm_PduLengthType len,
        uint8* resp,
        Dcm_PduLengthType* respLen)
{
    uint16 did;
    Dcm_ReturnType ret;

    if (len < 3u)
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    did = (uint16)(((uint16)req[0] << 8u) | req[1]);

    ret = DcmAppl_WriteDataByIdentifier(connIdx, opStatus, did, &req[2], (Dcm_PduLengthType)(len - 2u));

    if (ret == DCM_E_OK)
    {
        resp[0] = req[0];
        resp[1] = req[1];
        *respLen = 2u;
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

    ret = DcmAppl_RoutineControl(
            connIdx,
            opStatus,
            routineControlType,
            routineId,
            &req[3],
            (Dcm_PduLengthType)(len - 3u),
            &resp[3],
            respLen
    );

    if (ret == DCM_E_OK)
    {
        resp[0] = routineControlType;
        resp[1] = req[1];
        resp[2] = req[2];
        *respLen = (Dcm_PduLengthType)(*respLen + 3u);
    }

    return ret;
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

/* ===================== Security algorithm ===================== */

static uint32 Dcm_SecMix(uint32 x)
{
    x ^= 0x6A09E667u;
    x *= 0x45D9F3Bu;
    x ^= (x >> 16u);
    x *= 0x45D9F3Bu;
    x ^= (x >> 16u);
    return x;
}

static void Dcm_SecGenSeed(uint8 level, uint8 out[4])
{
    uint32 t = OS_Counter_core0;
    uint32 v = Dcm_SecMix(t ^ ((uint32)level << 24u) ^ 1u);

    out[0] = (uint8)(v >> 24u);
    out[1] = (uint8)(v >> 16u);
    out[2] = (uint8)(v >> 8u);
    out[3] = (uint8)(v);
}

static uint32 Dcm_SecSeedToU32(const uint8 s[4])
{
    return ((uint32)s[0] << 24u) |
            ((uint32)s[1] << 16u) |
            ((uint32)s[2] << 8u)  |
            ((uint32)s[3]);
}

static uint32 Dcm_SecCalcKey(uint32 seed, uint8 level)
{
    uint32 x;

    x = seed ^ 0x6A09E667u;
    x += 0x13572468u + ((uint32)level * 0x1F3D5B79u);
    x = (x << 3u) | (x >> 29u);
    x ^= ((seed << 16u) | (seed >> 16u));

    return x;
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

DCM_WEAK Dcm_ReturnType DcmAppl_DiagnosticSessionControl(
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
            (session != DCM_SESSION_EXTENDED))
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }

    return DCM_E_OK;
}

DCM_WEAK Dcm_ReturnType DcmAppl_EcuReset(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint8 resetType,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    (void)connIdx;
    (void)opStatus;
    (void)resetType;
    (void)respData;
    (void)respLen;
    return DCM_E_OK;
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

DCM_WEAK Dcm_ReturnType DcmAppl_ReadDtcInformation(
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
    return DCM_E_OK;
}

DCM_WEAK Dcm_ReturnType DcmAppl_ControlDtcSetting(
        uint8 connIdx,
        Dcm_OpStatusType opStatus,
        uint8 settingType)
{
    (void)connIdx;
    (void)opStatus;
    (void)settingType;
    return DCM_E_OK;
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

    respData[0] = 0x20u;
    respData[1] = (uint8)((DCM_TRANSFER_BLOCK_LEN >> 8u) & 0xFFu);
    respData[2] = (uint8)(DCM_TRANSFER_BLOCK_LEN & 0xFFu);
    *respLen = 3u;

    return DCM_E_OK;
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

    *respLen = 0u;
    return DCM_E_OK;
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

    *respLen = 0u;
    return DCM_E_OK;
}

uint8 *Dcm_ProvideRxBufferFromDoIP(uint16 len)
{
    (void)len;
    return NULL;
}
