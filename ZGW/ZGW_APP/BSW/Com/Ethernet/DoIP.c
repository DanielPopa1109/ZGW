/* DoIP.c - replacement with TCP stream reassembly */
#include "DoIP.h"
#include <string.h>

#define DOIP_TCP_RX_STREAM_LEN 8192u

typedef enum
{
    DOIP_TCP_OFFLINE = 0,
    DOIP_TCP_CONNECTED,
    DOIP_TCP_ROUTING_ACTIVE
} DoIP_TcpStateType;

typedef struct
{
    const DoIP_ConfigType *cfg;
    DoIP_DcmRxIndicationFct dcmRxIndication;
    DoIP_TcpStateType tcpState;
    TcpIp_SockAddrType lastUdpRemote;
    uint16_t testerLogicalAddress;
    uint8_t tcpStream[DOIP_TCP_RX_STREAM_LEN];
    uint16_t tcpStreamLen;
    uint8_t txBuffer[DOIP_HEADER_LEN + DOIP_MAX_UDS_PAYLOAD_LEN + 4u];
} DoIP_RuntimeType;

static DoIP_RuntimeType DoIP_Rt;

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8u) | p[1]);
}

static uint32_t rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24u) |
           ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) |
           p[3];
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

static uint16_t MakeHeader(uint8_t *buf, uint16_t payloadType, uint32_t payloadLen)
{
    buf[0] = DOIP_PROTOCOL_VERSION;
    buf[1] = DOIP_INVERSE_PROTOCOL_VERSION;
    wr16(&buf[2], payloadType);
    wr32(&buf[4], payloadLen);
    return DOIP_HEADER_LEN;
}

static void SendVehicleId(const TcpIp_SockAddrType *remoteAddr)
{
    uint8_t tx[DOIP_HEADER_LEN + 33u];
    uint16_t idx;
    uint8_t i;

    idx = MakeHeader(tx, DOIP_PAYLOAD_VEHICLE_ID_RES, 33u);

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

    (void)SoAd_IfTransmit(DoIP_Rt.cfg->udpSoConId, remoteAddr, tx, idx);
}

static void SendRoutingActivationRes(uint8_t code)
{
    uint8_t tx[DOIP_HEADER_LEN + 13u];
    uint16_t idx;

    idx = MakeHeader(tx, DOIP_PAYLOAD_ROUTING_ACTIVATION_RES, 13u);

    wr16(&tx[idx], DoIP_Rt.testerLogicalAddress); idx += 2u;
    wr16(&tx[idx], DoIP_Rt.cfg->logicalAddress); idx += 2u;

    tx[idx++] = code;

    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;

    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;
    tx[idx++] = 0x00u;

    (void)SoAd_IfTransmit(DoIP_Rt.cfg->tcpSoConId, 0, tx, idx);
}

static void SendAliveRes(void)
{
    uint8_t tx[DOIP_HEADER_LEN + 2u];
    uint16_t idx;

    idx = MakeHeader(tx, DOIP_PAYLOAD_ALIVE_CHECK_RES, 2u);
    wr16(&tx[idx], DoIP_Rt.cfg->logicalAddress);
    idx += 2u;

    (void)SoAd_IfTransmit(DoIP_Rt.cfg->tcpSoConId, 0, tx, idx);
}

static void SendDiagAck(uint16_t src, uint16_t tgt)
{
    uint8_t tx[DOIP_HEADER_LEN + 5u];
    uint16_t idx;

    idx = MakeHeader(tx, DOIP_PAYLOAD_DIAG_MSG_POS_ACK, 5u);
    wr16(&tx[idx], tgt); idx += 2u;
    wr16(&tx[idx], src); idx += 2u;
    tx[idx++] = 0x00u;

    (void)SoAd_IfTransmit(DoIP_Rt.cfg->tcpSoConId, 0, tx, idx);
}

static void SendDiagNack(uint16_t src, uint16_t tgt, uint8_t nack)
{
    uint8_t tx[DOIP_HEADER_LEN + 5u];
    uint16_t idx;

    idx = MakeHeader(tx, DOIP_PAYLOAD_DIAG_MSG_NEG_ACK, 5u);
    wr16(&tx[idx], tgt); idx += 2u;
    wr16(&tx[idx], src); idx += 2u;
    tx[idx++] = nack;

    (void)SoAd_IfTransmit(DoIP_Rt.cfg->tcpSoConId, 0, tx, idx);
}

static void HandleRoutingActivation(const uint8_t *p, uint32_t len)
{
    if (len < 7u)
    {
        return;
    }

    DoIP_Rt.testerLogicalAddress = rd16(&p[0]);
    DoIP_Rt.tcpState = DOIP_TCP_ROUTING_ACTIVE;

    SendRoutingActivationRes(0x10u);
}

static void HandleDiagnostic(const uint8_t *p, uint32_t len)
{
    uint16_t src;
    uint16_t tgt;
    uint16_t udsLen;

    if (len < 5u)
    {
        return;
    }

    src = rd16(&p[0]);
    tgt = rd16(&p[2]);
    udsLen = (uint16_t)(len - 4u);

    if (DoIP_Rt.tcpState != DOIP_TCP_ROUTING_ACTIVE)
    {
        SendDiagNack(src, tgt, DOIP_NACK_TRANSPORT_PROTOCOL_ERROR);
        return;
    }

    if (tgt != DoIP_Rt.cfg->logicalAddress)
    {
        SendDiagNack(src, tgt, DOIP_NACK_UNKNOWN_TARGET_ADDR);
        return;
    }

    if (udsLen > DOIP_MAX_UDS_PAYLOAD_LEN)
    {
        SendDiagNack(src, tgt, DOIP_NACK_DIAG_MSG_TOO_LARGE);
        return;
    }

    SendDiagAck(src, tgt);

    if (DoIP_Rt.dcmRxIndication != 0)
    {
        DoIP_Rt.dcmRxIndication(src, tgt, &p[4], udsLen);
    }
}

static void HandlePayload(uint16_t type, const uint8_t *payload, uint32_t len)
{
    switch (type)
    {
        case DOIP_PAYLOAD_ROUTING_ACTIVATION_REQ:
            HandleRoutingActivation(payload, len);
            break;

        case DOIP_PAYLOAD_ALIVE_CHECK_REQ:
            SendAliveRes();
            break;

        case DOIP_PAYLOAD_DIAG_MSG:
            HandleDiagnostic(payload, len);
            break;

        default:
            break;
    }
}

static void ProcessTcpStream(void)
{
    uint16_t type;
    uint32_t len;
    uint32_t frameLen;

    while (DoIP_Rt.tcpStreamLen >= DOIP_HEADER_LEN)
    {
        if ((DoIP_Rt.tcpStream[0] != DOIP_PROTOCOL_VERSION) ||
            (DoIP_Rt.tcpStream[1] != DOIP_INVERSE_PROTOCOL_VERSION))
        {
            DoIP_Rt.tcpStreamLen = 0u;
            return;
        }

        type = rd16(&DoIP_Rt.tcpStream[2]);
        len = rd32(&DoIP_Rt.tcpStream[4]);
        frameLen = DOIP_HEADER_LEN + len;

        if (frameLen > DOIP_TCP_RX_STREAM_LEN)
        {
            DoIP_Rt.tcpStreamLen = 0u;
            return;
        }

        if (DoIP_Rt.tcpStreamLen < frameLen)
        {
            return;
        }

        HandlePayload(type, &DoIP_Rt.tcpStream[DOIP_HEADER_LEN], len);

        memmove(&DoIP_Rt.tcpStream[0],
                &DoIP_Rt.tcpStream[frameLen],
                DoIP_Rt.tcpStreamLen - frameLen);

        DoIP_Rt.tcpStreamLen = (uint16_t)(DoIP_Rt.tcpStreamLen - frameLen);
    }
}

void DoIP_Init(const DoIP_ConfigType *config)
{
    memset(&DoIP_Rt, 0, sizeof(DoIP_Rt));
    DoIP_Rt.cfg = config;
    DoIP_Rt.tcpState = DOIP_TCP_OFFLINE;
}

void DoIP_MainFunction(void)
{
}

void DoIP_SetDcmRxIndication(DoIP_DcmRxIndicationFct cb)
{
    DoIP_Rt.dcmRxIndication = cb;
}

void DoIP_SoAdUdpRxIndication(SoAd_SoConIdType soConId,
                              const TcpIp_SockAddrType *remoteAddr,
                              const uint8_t *data,
                              uint16_t len)
{
    uint16_t type;

    (void)soConId;

    if ((DoIP_Rt.cfg == 0) || (remoteAddr == 0) || (data == 0) || (len < DOIP_HEADER_LEN))
    {
        return;
    }

    if ((data[0] != DOIP_PROTOCOL_VERSION) || (data[1] != DOIP_INVERSE_PROTOCOL_VERSION))
    {
        return;
    }

    type = rd16(&data[2]);

    if (type == DOIP_PAYLOAD_VEHICLE_ID_REQ)
    {
        SendVehicleId(remoteAddr);
    }
}

void DoIP_SoAdTcpRxIndication(SoAd_SoConIdType soConId,
                              const TcpIp_SockAddrType *remoteAddr,
                              const uint8_t *data,
                              uint16_t len)
{
    (void)soConId;
    (void)remoteAddr;

    if ((data == 0) || (len == 0u))
    {
        return;
    }

    if ((DoIP_Rt.tcpStreamLen + len) > DOIP_TCP_RX_STREAM_LEN)
    {
        DoIP_Rt.tcpStreamLen = 0u;
        return;
    }

    memcpy(&DoIP_Rt.tcpStream[DoIP_Rt.tcpStreamLen], data, len);
    DoIP_Rt.tcpStreamLen += len;

    ProcessTcpStream();
}

void DoIP_SoAdTcpConnected(SoAd_SoConIdType soConId)
{
    (void)soConId;
    DoIP_Rt.tcpState = DOIP_TCP_CONNECTED;
    DoIP_Rt.tcpStreamLen = 0u;
}

void DoIP_SoAdTcpDisconnected(SoAd_SoConIdType soConId)
{
    (void)soConId;
    DoIP_Rt.tcpState = DOIP_TCP_OFFLINE;
    DoIP_Rt.tcpStreamLen = 0u;
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

    if (DoIP_Rt.tcpState != DOIP_TCP_ROUTING_ACTIVE)
    {
        return DOIP_BUSY;
    }

    idx = MakeHeader(DoIP_Rt.txBuffer, DOIP_PAYLOAD_DIAG_MSG, (uint32_t)udsLen + 4u);

    wr16(&DoIP_Rt.txBuffer[idx], sourceAddress); idx += 2u;
    wr16(&DoIP_Rt.txBuffer[idx], targetAddress); idx += 2u;

    memcpy(&DoIP_Rt.txBuffer[idx], uds, udsLen);
    idx += udsLen;

    return (SoAd_IfTransmit(DoIP_Rt.cfg->tcpSoConId, 0, DoIP_Rt.txBuffer, idx) == SOAD_OK)
           ? DOIP_OK
           : DOIP_BUSY;
}
