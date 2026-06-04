#include "DoIP.h"
#include "GatewaySwc.h"
#include <string.h>

volatile uint32 DoIP_DebugUdpRxCounter = 0u;
volatile uint32 DoIP_DebugVehicleIdReqCounter = 0u;
volatile uint32 DoIP_DebugVehicleIdTxCounter = 0u;
volatile uint32 DoIP_DebugVehicleIdTxFailCounter = 0u;
volatile uint32 DoIP_DebugTcpRxCounter = 0u;
volatile uint32 DoIP_DebugRoutingActivationReqCounter = 0u;
volatile uint32 DoIP_DebugRoutingActivationTxCounter = 0u;
volatile uint32 DoIP_DebugRoutingActivationTxFailCounter = 0u;
volatile uint32 DoIP_DebugAliveReqTxCounter = 0u;
volatile uint32 DoIP_DebugAliveResRxCounter = 0u;
volatile uint32 DoIP_DebugTcpTimeoutCounter = 0u;
volatile uint32 DoIP_DebugTcpDisconnectCounter = 0u;
volatile uint32 DoIP_DebugTcpState = 0u;
volatile uint32 DoIP_DebugAliveTimerMs = 0u;
volatile uint32 DoIP_DebugInactivityTimerMs = 0u;

typedef struct
{
        const DoIP_ConfigType *cfg;
        DoIP_DcmRxIndicationFct dcmRxIndication;
        DoIP_SessionResetFct sessionReset;

        DoIP_TcpStateType tcpState;

        uint16_t testerLogicalAddress;
        uint8_t routingActive;
        volatile uint8_t tcpActivityPending;

        uint8_t tcpStream[DOIP_TCP_RX_STREAM_LEN];
        uint16_t tcpStreamLen;

        uint8_t txBuffer[DOIP_HEADER_LEN + DOIP_MAX_UDS_PAYLOAD_LEN + 4u];

        uint32_t malformedCnt;
        uint32_t diagRxCnt;
        uint32_t diagTxCnt;
        uint32_t diagNackCnt;
        uint32_t routingActivationCnt;
        uint32_t aliveCheckCnt;
        uint32_t vehicleIdReqCnt;

        uint32 aliveTimerMs;
        uint32 inactivityTimerMs;
        uint32 vehicleAnnouncementTimerMs;
        uint32 tcpDisconnectCnt;
        uint32 tcpTimeoutCnt;
        uint8 vehicleAnnouncementRemaining;

} DoIP_RuntimeType;

static DoIP_RuntimeType DoIP_Rt;
long long DoIP_MainFunction_Counter = 0;

static void DoIP_ResetTcpActivityTimers(void)
{
    DoIP_Rt.inactivityTimerMs = 0u;
    DoIP_Rt.aliveTimerMs = 0u;
    DoIP_Rt.tcpActivityPending = 0u;
}

static void DoIP_MarkTcpActivity(void)
{
    DoIP_Rt.tcpActivityPending = 1u;
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8u) | p[1]);
}

static uint32_t rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24u) |
            ((uint32_t)p[1] << 16u) |
            ((uint32_t)p[2] << 8u) |
            ((uint32_t)p[3]);
}

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8u);
    p[1] = (uint8_t)v;
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24u);
    p[1] = (uint8_t)(v >> 16u);
    p[2] = (uint8_t)(v >> 8u);
    p[3] = (uint8_t)v;
}

static uint16_t DoIP_MakeHeader(uint8_t *buf, uint16_t payloadType, uint32_t payloadLen)
{
    buf[0] = DOIP_PROTOCOL_VERSION;
    buf[1] = DOIP_INVERSE_PROTOCOL_VERSION;
    wr16(&buf[2], payloadType);
    wr32(&buf[4], payloadLen);
    return DOIP_HEADER_LEN;
}

static void DoIP_SendGenericNack(SoAd_SoConIdType soConId,
        const TcpIp_SockAddrType *remoteAddr,
        uint8_t code)
{
    uint8_t tx[DOIP_HEADER_LEN + 1u];
    uint16_t idx;
    SoAd_ReturnType ret;

    idx = DoIP_MakeHeader(tx, DOIP_PAYLOAD_GENERIC_NACK, 1u);
    tx[idx++] = code;

    ret = GatewaySwc_RequestSoAdIfTransmit(soConId, remoteAddr, tx, idx);

    if ((ret == SOAD_OK) && (DoIP_Rt.cfg != 0) && (soConId == DoIP_Rt.cfg->tcpSoConId))
    {
        DoIP_MarkTcpActivity();
    }
}

static uint8_t DoIP_CheckHeader(const uint8_t *data,
        uint16_t len,
        uint16_t *payloadType,
        uint32_t *payloadLen)
{
    if ((data == 0) || (payloadType == 0) || (payloadLen == 0) || (len < DOIP_HEADER_LEN))
    {
        return 0u;
    }

    if ((data[0] != DOIP_PROTOCOL_VERSION) ||
            (data[1] != DOIP_INVERSE_PROTOCOL_VERSION))
    {
        return 0u;
    }

    *payloadType = rd16(&data[2]);
    *payloadLen = rd32(&data[4]);

    return 1u;
}

static void DoIP_SendVehicleId(const TcpIp_SockAddrType *remoteAddr)
{
    uint8_t tx[DOIP_HEADER_LEN + 33u];
    uint16_t idx;
    uint8_t i;

    if (DoIP_Rt.cfg == 0)
    {
        return;
    }

    idx = DoIP_MakeHeader(tx, DOIP_PAYLOAD_VEHICLE_ID_RES, 33u);

    for (i = 0u; i < DOIP_MAX_VIN_LEN; i++)
    {
        tx[idx++] = (uint8_t)DoIP_Rt.cfg->vin[i];
    }

    wr16(&tx[idx], DoIP_Rt.cfg->logicalAddress);
    idx += 2u;

    for (i = 0u; i < DOIP_MAX_EID_LEN; i++)
    {
        tx[idx++] = DoIP_Rt.cfg->eid[i];
    }

    for (i = 0u; i < DOIP_MAX_GID_LEN; i++)
    {
        tx[idx++] = DoIP_Rt.cfg->gid[i];
    }

    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;

    DoIP_Rt.vehicleIdReqCnt++;
    DoIP_DebugVehicleIdTxCounter++;

    if (GatewaySwc_RequestSoAdIfTransmit(DoIP_Rt.cfg->udpSoConId, remoteAddr, tx, idx) != SOAD_OK)
    {
        DoIP_DebugVehicleIdTxFailCounter++;
    }
}

static void DoIP_ResetVehicleAnnouncement(void)
{
    DoIP_Rt.vehicleAnnouncementTimerMs = DOIP_VEHICLE_ANNOUNCE_INTERVAL_MS;
    DoIP_Rt.vehicleAnnouncementRemaining = DOIP_VEHICLE_ANNOUNCE_COUNT;
}

static void DoIP_SendVehicleAnnouncement(void)
{
    TcpIp_SockAddrType broadcast;

    broadcast.addr = 0xFFFFFFFFu;
    broadcast.port = DOIP_UDP_PORT;

    DoIP_SendVehicleId(&broadcast);
}

static void DoIP_SendRoutingActivationRes(uint16_t testerAddr, uint8_t code)
{
    uint8_t tx[DOIP_HEADER_LEN + 13u];
    uint16_t idx;

    idx = DoIP_MakeHeader(tx, DOIP_PAYLOAD_ROUTING_ACTIVATION_RES, 13u);

    wr16(&tx[idx], testerAddr);
    idx += 2u;

    wr16(&tx[idx], DoIP_Rt.cfg->logicalAddress);
    idx += 2u;

    tx[idx++] = code;

    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;

    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;

    DoIP_DebugRoutingActivationTxCounter++;
    if (GatewaySwc_RequestSoAdIfTransmit(DoIP_Rt.cfg->tcpSoConId, 0, tx, idx) != SOAD_OK)
    {
        DoIP_DebugRoutingActivationTxFailCounter++;
    }
    else
    {
        DoIP_MarkTcpActivity();
    }
}

static void DoIP_SendAliveRes(void)
{
    uint8_t tx[DOIP_HEADER_LEN + 2u];
    uint16_t idx;

    idx = DoIP_MakeHeader(tx, DOIP_PAYLOAD_ALIVE_CHECK_RES, 2u);

    wr16(&tx[idx], DoIP_Rt.cfg->logicalAddress);
    idx += 2u;

    DoIP_Rt.aliveCheckCnt++;

    if (GatewaySwc_RequestSoAdIfTransmit(DoIP_Rt.cfg->tcpSoConId, 0, tx, idx) == SOAD_OK)
    {
        DoIP_MarkTcpActivity();
    }
}

static void DoIP_SendAliveReq(void)
{
    uint8_t tx[DOIP_HEADER_LEN];
    uint16_t idx;

    idx = DoIP_MakeHeader(tx, DOIP_PAYLOAD_ALIVE_CHECK_REQ, 0u);

    DoIP_Rt.aliveCheckCnt++;
    DoIP_DebugAliveReqTxCounter++;

    (void)GatewaySwc_RequestSoAdIfTransmit(DoIP_Rt.cfg->tcpSoConId, 0, tx, idx);
}

static void DoIP_SendDiagAck(uint16_t testerAddr, uint16_t ecuAddr)
{
    uint8_t tx[DOIP_HEADER_LEN + 5u];
    uint16_t idx;

    idx = DoIP_MakeHeader(tx, DOIP_PAYLOAD_DIAG_MSG_POS_ACK, 5u);

    wr16(&tx[idx], testerAddr);
    idx += 2u;

    wr16(&tx[idx], ecuAddr);
    idx += 2u;

    tx[idx++] = 0x00u;

    if (GatewaySwc_RequestSoAdIfTransmit(DoIP_Rt.cfg->tcpSoConId, 0, tx, idx) == SOAD_OK)
    {
        DoIP_MarkTcpActivity();
    }
}

static void DoIP_SendDiagNack(uint16_t testerAddr, uint16_t ecuAddr, uint8_t nack)
{
    uint8_t tx[DOIP_HEADER_LEN + 5u];
    uint16_t idx;

    idx = DoIP_MakeHeader(tx, DOIP_PAYLOAD_DIAG_MSG_NEG_ACK, 5u);

    wr16(&tx[idx], testerAddr);
    idx += 2u;

    wr16(&tx[idx], ecuAddr);
    idx += 2u;

    tx[idx++] = nack;

    DoIP_Rt.diagNackCnt++;

    if (GatewaySwc_RequestSoAdIfTransmit(DoIP_Rt.cfg->tcpSoConId, 0, tx, idx) == SOAD_OK)
    {
        DoIP_MarkTcpActivity();
    }
}

static void DoIP_HandleRoutingActivation(const uint8_t *p, uint32_t len)
{
    uint16_t testerAddr;
    uint8_t activationType;

    if (len < 7u)
    {
        DoIP_Rt.malformedCnt++;
        return;
    }

    testerAddr = rd16(&p[0]);
    activationType = p[2];

    if ((testerAddr == 0u) ||
            (testerAddr != DoIP_Rt.cfg->testerLogicalAddress))
    {
        DoIP_SendRoutingActivationRes(testerAddr, DOIP_RA_RES_DENIED_UNKNOWN_SOURCE);
        return;
    }

    if ((activationType != 0x00u) && (activationType != 0x01u))
    {
        DoIP_SendRoutingActivationRes(testerAddr, DOIP_RA_RES_UNSUPPORTED_ACTIVATION);
        return;
    }

    DoIP_Rt.testerLogicalAddress = testerAddr;
    DoIP_Rt.routingActive = 1u;
    DoIP_Rt.tcpState = DOIP_TCP_ROUTING_ACTIVE;
    DoIP_Rt.routingActivationCnt++;
    DoIP_ResetTcpActivityTimers();

    DoIP_DebugRoutingActivationReqCounter++;
    DoIP_SendRoutingActivationRes(testerAddr, DOIP_RA_RES_OK);
}

static void DoIP_HandleDiagnostic(const uint8_t *p, uint32_t len)
{
    uint16_t testerAddr;
    uint16_t ecuAddr;
    uint16_t udsLen;

    if (len < 5u)
    {
        DoIP_Rt.malformedCnt++;
        return;
    }

    testerAddr = rd16(&p[0]);
    ecuAddr = rd16(&p[2]);
    udsLen = (uint16_t)(len - 4u);

    if ((DoIP_Rt.tcpState != DOIP_TCP_ROUTING_ACTIVE) ||
            (DoIP_Rt.routingActive == 0u))
    {
        DoIP_SendDiagNack(testerAddr, ecuAddr, DOIP_NACK_TRANSPORT_PROTOCOL_ERROR);
        return;
    }

    if ((testerAddr != DoIP_Rt.testerLogicalAddress) ||
            (testerAddr != DoIP_Rt.cfg->testerLogicalAddress))
    {
        DoIP_SendDiagNack(testerAddr, ecuAddr, DOIP_NACK_INVALID_SOURCE_ADDR);
        return;
    }

    if (ecuAddr != DoIP_Rt.cfg->logicalAddress)
    {
        DoIP_SendDiagNack(testerAddr, ecuAddr, DOIP_NACK_UNKNOWN_TARGET_ADDR);
        return;
    }

    if (udsLen > DOIP_MAX_UDS_PAYLOAD_LEN)
    {
        DoIP_SendDiagNack(testerAddr, ecuAddr, DOIP_NACK_DIAG_MSG_TOO_LARGE);
        return;
    }

    DoIP_MarkTcpActivity();
    DoIP_SendDiagAck(testerAddr, ecuAddr);

    DoIP_Rt.diagRxCnt++;

    if (DoIP_Rt.dcmRxIndication != 0)
    {
        DoIP_Rt.dcmRxIndication(testerAddr, ecuAddr, &p[4], udsLen);
    }
}

static void DoIP_HandleTcpPayload(uint16_t type, const uint8_t *payload, uint32_t len)
{
    switch (type)
    {
        case DOIP_PAYLOAD_ROUTING_ACTIVATION_REQ:
            DoIP_HandleRoutingActivation(payload, len);
            break;

        case DOIP_PAYLOAD_ALIVE_CHECK_REQ:
            if (len == 0u)
            {
                DoIP_MarkTcpActivity();
                DoIP_SendAliveRes();
            }
            else
            {
                DoIP_Rt.malformedCnt++;
            }
            break;

        case DOIP_PAYLOAD_ALIVE_CHECK_RES:
            if (len >= 2u)
            {
                DoIP_MarkTcpActivity();
                DoIP_DebugAliveResRxCounter++;
            }
            else
            {
                DoIP_Rt.malformedCnt++;
            }
            break;

        case DOIP_PAYLOAD_DIAG_MSG:
            DoIP_HandleDiagnostic(payload, len);
            break;

        default:
            DoIP_SendGenericNack(DoIP_Rt.cfg->tcpSoConId, 0, DOIP_GEN_NACK_UNKNOWN_PAYLOAD);
            break;
    }
}

static void DoIP_ProcessTcpStream(void)
{
    uint16_t payloadType;
    uint32_t payloadLen;
    uint32_t frameLen;

    while (DoIP_Rt.tcpStreamLen >= DOIP_HEADER_LEN)
    {
        if (DoIP_CheckHeader(DoIP_Rt.tcpStream,
                DoIP_Rt.tcpStreamLen,
                &payloadType,
                &payloadLen) == 0u)
        {
            DoIP_Rt.tcpStreamLen = 0u;
            DoIP_Rt.malformedCnt++;
            DoIP_SendGenericNack(DoIP_Rt.cfg->tcpSoConId, 0, DOIP_GEN_NACK_INCORRECT_PATTERN);
            return;
        }

        if (payloadLen > (DOIP_TCP_RX_STREAM_LEN - DOIP_HEADER_LEN))
        {
            DoIP_Rt.tcpStreamLen = 0u;
            DoIP_Rt.malformedCnt++;
            DoIP_SendGenericNack(DoIP_Rt.cfg->tcpSoConId, 0, DOIP_GEN_NACK_MESSAGE_TOO_LARGE);
            return;
        }

        frameLen = DOIP_HEADER_LEN + payloadLen;

        if (DoIP_Rt.tcpStreamLen < frameLen)
        {
            return;
        }

        DoIP_HandleTcpPayload(payloadType,
                &DoIP_Rt.tcpStream[DOIP_HEADER_LEN],
                payloadLen);

        if (DoIP_Rt.tcpStreamLen > frameLen)
        {
            memmove(&DoIP_Rt.tcpStream[0],
                    &DoIP_Rt.tcpStream[frameLen],
                    DoIP_Rt.tcpStreamLen - frameLen);
        }

        DoIP_Rt.tcpStreamLen = (uint16_t)(DoIP_Rt.tcpStreamLen - frameLen);
    }
}

void DoIP_Init(const DoIP_ConfigType *config)
{
    memset(&DoIP_Rt, 0, sizeof(DoIP_Rt));
    DoIP_Rt.cfg = config;
    DoIP_Rt.tcpState = DOIP_TCP_OFFLINE;
    DoIP_ResetVehicleAnnouncement();
}

void DoIP_MainFunction(uint32 elapsedMs)
{
    if ((DoIP_Rt.cfg != 0) && (DoIP_Rt.vehicleAnnouncementRemaining > 0u))
    {
        DoIP_Rt.vehicleAnnouncementTimerMs += elapsedMs;

        if (DoIP_Rt.vehicleAnnouncementTimerMs >= DOIP_VEHICLE_ANNOUNCE_INTERVAL_MS)
        {
            DoIP_Rt.vehicleAnnouncementTimerMs = 0u;
            DoIP_Rt.vehicleAnnouncementRemaining--;
            DoIP_SendVehicleAnnouncement();
        }
    }

    if (DoIP_Rt.tcpState == DOIP_TCP_OFFLINE)
    {
        DoIP_DebugTcpState = (uint32)DoIP_Rt.tcpState;
        DoIP_DebugAliveTimerMs = DoIP_Rt.aliveTimerMs;
        DoIP_DebugInactivityTimerMs = DoIP_Rt.inactivityTimerMs;
        DoIP_MainFunction_Counter++;
        return;
    }

    if (DoIP_Rt.tcpActivityPending != 0u)
    {
        DoIP_ResetTcpActivityTimers();
    }
    else
    {
#if (DOIP_LAB_DISABLE_TCP_INACTIVITY_ABORT != STD_ON)
        DoIP_Rt.inactivityTimerMs += elapsedMs;
#else
        DoIP_Rt.inactivityTimerMs = 0u;
#endif

#if (DOIP_LAB_DISABLE_SERVER_ALIVE_REQ != STD_ON)
        if (DoIP_Rt.tcpState == DOIP_TCP_ROUTING_ACTIVE)
        {
            DoIP_Rt.aliveTimerMs += elapsedMs;

            if (DoIP_Rt.aliveTimerMs >= DOIP_ALIVE_TIMEOUT_MS)
            {
                DoIP_Rt.aliveTimerMs = 0u;
                DoIP_SendAliveReq();
            }
        }
#else
        DoIP_Rt.aliveTimerMs = 0u;
#endif
    }

#if (DOIP_LAB_DISABLE_TCP_INACTIVITY_ABORT != STD_ON)
    if (DoIP_Rt.inactivityTimerMs >= DOIP_INACTIVITY_TIMEOUT_MS)
    {
        DoIP_Rt.tcpTimeoutCnt++;
        DoIP_DebugTcpTimeoutCounter++;
        DoIP_Rt.tcpState = DOIP_TCP_OFFLINE;
        DoIP_Rt.routingActive = 0u;
        DoIP_Rt.tcpStreamLen = 0u;
        DoIP_Rt.testerLogicalAddress = 0u;
        DoIP_ResetTcpActivityTimers();

        if (DoIP_Rt.cfg != 0)
        {
            SoAd_AbortTcpConnection(DoIP_Rt.cfg->tcpSoConId);
        }
    }
#endif

    DoIP_DebugTcpState = (uint32)DoIP_Rt.tcpState;
    DoIP_DebugAliveTimerMs = DoIP_Rt.aliveTimerMs;
    DoIP_DebugInactivityTimerMs = DoIP_Rt.inactivityTimerMs;
    DoIP_MainFunction_Counter++;
}
void DoIP_SetDcmRxIndication(DoIP_DcmRxIndicationFct cb)
{
    DoIP_Rt.dcmRxIndication = cb;
}

void DoIP_SetSessionResetIndication(DoIP_SessionResetFct cb)
{
    DoIP_Rt.sessionReset = cb;
}

void DoIP_SoAdUdpRxIndication(SoAd_SoConIdType soConId,
        const TcpIp_SockAddrType *remoteAddr,
        const uint8_t *data,
        uint16_t len)
{
    uint16_t payloadType;
    uint32_t payloadLen;

    (void)soConId;

    DoIP_DebugUdpRxCounter++;

    if ((DoIP_Rt.cfg == 0) || (remoteAddr == 0) || (data == 0))
    {
        return;
    }

    if (DoIP_CheckHeader(data, len, &payloadType, &payloadLen) == 0u)
    {
        DoIP_SendGenericNack(DoIP_Rt.cfg->udpSoConId, remoteAddr, DOIP_GEN_NACK_INCORRECT_PATTERN);
        return;
    }

    if ((uint32_t)len != (DOIP_HEADER_LEN + payloadLen))
    {
        DoIP_SendGenericNack(DoIP_Rt.cfg->udpSoConId, remoteAddr, DOIP_GEN_NACK_INVALID_LENGTH);
        return;
    }

    switch (payloadType)
    {
        case DOIP_PAYLOAD_VEHICLE_ID_REQ:
            DoIP_DebugVehicleIdReqCounter++;
            if (payloadLen == 0u)
            {
                DoIP_SendVehicleId(remoteAddr);
            }
            break;

        default:
            DoIP_SendGenericNack(DoIP_Rt.cfg->udpSoConId, remoteAddr, DOIP_GEN_NACK_UNKNOWN_PAYLOAD);
            break;
    }
}

void DoIP_SoAdTcpRxIndication(SoAd_SoConIdType soConId,
        const TcpIp_SockAddrType *remoteAddr,
        const uint8_t *data,
        uint16_t len)
{
    (void)soConId;
    (void)remoteAddr;

    if ((DoIP_Rt.cfg == 0) || (data == 0) || (len == 0u))
    {
        return;
    }

    DoIP_DebugTcpRxCounter++;
    DoIP_MarkTcpActivity();

    if ((uint32_t)DoIP_Rt.tcpStreamLen + len > DOIP_TCP_RX_STREAM_LEN)
    {
        DoIP_Rt.tcpStreamLen = 0u;
        DoIP_Rt.malformedCnt++;
        DoIP_MarkTcpActivity();
        DoIP_SendGenericNack(DoIP_Rt.cfg->tcpSoConId, 0, DOIP_GEN_NACK_MESSAGE_TOO_LARGE);
        return;
    }

    memcpy(&DoIP_Rt.tcpStream[DoIP_Rt.tcpStreamLen], data, len);
    DoIP_Rt.tcpStreamLen = (uint16_t)(DoIP_Rt.tcpStreamLen + len);

    DoIP_ProcessTcpStream();
}

void DoIP_SoAdTcpConnected(SoAd_SoConIdType soConId)
{
    (void)soConId;

    DoIP_Rt.tcpState = DOIP_TCP_CONNECTED;
    DoIP_DebugTcpState = (uint32)DoIP_Rt.tcpState;
    DoIP_Rt.routingActive = 0u;
    DoIP_Rt.tcpStreamLen = 0u;
    DoIP_Rt.testerLogicalAddress = 0u;
    DoIP_ResetTcpActivityTimers();
    DoIP_Rt.vehicleAnnouncementRemaining = 0u;
    DoIP_Rt.vehicleAnnouncementTimerMs = 0u;

    /* Start the upper-layer (PduR/DCM) DoIP transaction state from a clean slate
     * so a context/mailbox left stuck by a previous connection cannot gate this
     * new connection's diagnostics. */
    if (DoIP_Rt.sessionReset != 0)
    {
        DoIP_Rt.sessionReset();
    }
}

void DoIP_SoAdTcpDisconnected(SoAd_SoConIdType soConId)
{
    (void)soConId;

    DoIP_Rt.tcpDisconnectCnt++;
    DoIP_DebugTcpDisconnectCounter++;
    DoIP_Rt.tcpState = DOIP_TCP_OFFLINE;
    DoIP_DebugTcpState = (uint32)DoIP_Rt.tcpState;
    DoIP_Rt.routingActive = 0u;
    DoIP_Rt.tcpStreamLen = 0u;
    DoIP_Rt.testerLogicalAddress = 0u;
    DoIP_ResetTcpActivityTimers();
    DoIP_ResetVehicleAnnouncement();

    /* Release any upper-layer DoIP transaction context tied to the connection
     * that just dropped, so it cannot leak into the next one. */
    if (DoIP_Rt.sessionReset != 0)
    {
        DoIP_Rt.sessionReset();
    }
}

DoIP_ReturnType DoIP_SendDiagnosticResponse(uint16_t sourceAddress,
        uint16_t targetAddress,
        const uint8_t *uds,
        uint16_t udsLen)
{
    uint16_t idx;

    if ((DoIP_Rt.cfg == 0) || (uds == 0) || (udsLen > DOIP_MAX_UDS_PAYLOAD_LEN))
    {
        return DOIP_PARAM_ERROR;
    }

    if ((DoIP_Rt.tcpState != DOIP_TCP_ROUTING_ACTIVE) ||
            (DoIP_Rt.routingActive == 0u))
    {
        return DOIP_BUSY;
    }

    idx = DoIP_MakeHeader(DoIP_Rt.txBuffer,
            DOIP_PAYLOAD_DIAG_MSG,
            (uint32_t)udsLen + 4u);

    wr16(&DoIP_Rt.txBuffer[idx], sourceAddress);
    idx += 2u;

    wr16(&DoIP_Rt.txBuffer[idx], targetAddress);
    idx += 2u;

    memcpy(&DoIP_Rt.txBuffer[idx], uds, udsLen);
    idx = (uint16_t)(idx + udsLen);

    if (GatewaySwc_RequestSoAdIfTransmit(DoIP_Rt.cfg->tcpSoConId, 0, DoIP_Rt.txBuffer, idx) != SOAD_OK)
    {
        return DOIP_BUSY;
    }

    DoIP_MarkTcpActivity();
    DoIP_Rt.diagTxCnt++;

    return DOIP_OK;
}

DoIP_TcpStateType DoIP_GetTcpState(void)
{
    return DoIP_Rt.tcpState;
}
