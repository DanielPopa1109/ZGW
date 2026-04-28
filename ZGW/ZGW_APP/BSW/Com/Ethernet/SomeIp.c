/* SomeIp.c */
#include "SomeIp.h"
#include <string.h>

static const SomeIp_ConfigType *SomeIp_Cfg;

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

void SomeIp_Init(const SomeIp_ConfigType *config)
{
    SomeIp_Cfg = config;
}

static uint8_t SomeIp_Parse(const uint8_t *data, uint16_t len, SomeIp_MessageType *msg)
{
    uint32_t lengthField;

    if ((data == 0) || (msg == 0) || (len < SOMEIP_HEADER_LEN))
    {
        return 0u;
    }

    lengthField = rd32(&data[4]);

    if (lengthField < 8u)
    {
        return 0u;
    }

    if ((lengthField + 8u) > len)
    {
        return 0u;
    }

    msg->serviceId = rd16(&data[0]);
    msg->methodId = rd16(&data[2]);
    msg->clientId = rd16(&data[8]);
    msg->sessionId = rd16(&data[10]);
    msg->protocolVersion = data[12];
    msg->interfaceVersion = data[13];
    msg->messageType = data[14];
    msg->returnCode = data[15];
    msg->payload = &data[16];
    msg->payloadLen = lengthField - 8u;

    return 1u;
}

static uint16_t SomeIp_Build(uint8_t *buf,
                             uint16_t serviceId,
                             uint16_t methodId,
                             uint16_t clientId,
                             uint16_t sessionId,
                             uint8_t interfaceVersion,
                             uint8_t messageType,
                             uint8_t returnCode,
                             const uint8_t *payload,
                             uint32_t payloadLen)
{
    wr16(&buf[0], serviceId);
    wr16(&buf[2], methodId);
    wr32(&buf[4], payloadLen + 8u);
    wr16(&buf[8], clientId);
    wr16(&buf[10], sessionId);

    buf[12] = 0x01u;
    buf[13] = interfaceVersion;
    buf[14] = messageType;
    buf[15] = returnCode;

    if ((payload != 0) && (payloadLen > 0u))
    {
        memcpy(&buf[16], payload, payloadLen);
    }

    return (uint16_t)(SOMEIP_HEADER_LEN + payloadLen);
}

static void SomeIp_SendError(SoAd_SoConIdType soConId,
                             const TcpIp_SockAddrType *remoteAddr,
                             const SomeIp_MessageType *request,
                             uint8_t errorCode)
{
    uint8_t tx[SOMEIP_HEADER_LEN];

    uint16_t len = SomeIp_Build(tx,
                                request->serviceId,
                                request->methodId,
                                request->clientId,
                                request->sessionId,
                                request->interfaceVersion,
                                SOMEIP_MSG_ERROR,
                                errorCode,
                                0,
                                0u);

    (void)SoAd_IfTransmit(soConId, remoteAddr, tx, len);
}

void SomeIp_SoAdRxIndication(SoAd_SoConIdType soConId,
                             const TcpIp_SockAddrType *remoteAddr,
                             const uint8_t *data,
                             uint16_t len)
{
    SomeIp_MessageType msg;
    uint16_t i;

    if ((SomeIp_Cfg == 0) || (SomeIp_Parse(data, len, &msg) == 0u))
    {
        return;
    }

    for (i = 0u; i < SomeIp_Cfg->methodCount; i++)
    {
        const SomeIp_MethodConfigType *m = &SomeIp_Cfg->methods[i];

        if ((m->serviceId == msg.serviceId) &&
            (m->methodId == msg.methodId) &&
            (m->interfaceVersion == msg.interfaceVersion))
        {
            if (m->handler != 0)
            {
                m->handler(soConId, remoteAddr, &msg);
            }
            return;
        }
    }

    SomeIp_SendError(soConId, remoteAddr, &msg, SOMEIP_RET_UNKNOWN_METHOD);
}

uint8_t SomeIp_SendResponse(SoAd_SoConIdType soConId,
                            const TcpIp_SockAddrType *remoteAddr,
                            const SomeIp_MessageType *request,
                            const uint8_t *payload,
                            uint32_t payloadLen,
                            uint8_t returnCode)
{
    uint8_t tx[SOMEIP_HEADER_LEN + SOMEIP_MAX_PAYLOAD_LEN];
    uint16_t len;

    if ((request == 0) || (payloadLen > SOMEIP_MAX_PAYLOAD_LEN))
    {
        return 0u;
    }

    len = SomeIp_Build(tx,
                       request->serviceId,
                       request->methodId,
                       request->clientId,
                       request->sessionId,
                       request->interfaceVersion,
                       SOMEIP_MSG_RESPONSE,
                       returnCode,
                       payload,
                       payloadLen);

    return (SoAd_IfTransmit(soConId, remoteAddr, tx, len) == SOAD_OK) ? 1u : 0u;
}

uint8_t SomeIp_SendNotification(SoAd_SoConIdType soConId,
                                const TcpIp_SockAddrType *remoteAddr,
                                uint16_t serviceId,
                                uint16_t eventId,
                                uint16_t clientId,
                                uint16_t sessionId,
                                uint8_t interfaceVersion,
                                const uint8_t *payload,
                                uint32_t payloadLen)
{
    uint8_t tx[SOMEIP_HEADER_LEN + SOMEIP_MAX_PAYLOAD_LEN];
    uint16_t len;

    if (payloadLen > SOMEIP_MAX_PAYLOAD_LEN)
    {
        return 0u;
    }

    len = SomeIp_Build(tx,
                       serviceId,
                       eventId,
                       clientId,
                       sessionId,
                       interfaceVersion,
                       SOMEIP_MSG_NOTIFICATION,
                       SOMEIP_RET_OK,
                       payload,
                       payloadLen);

    return (SoAd_IfTransmit(soConId, remoteAddr, tx, len) == SOAD_OK) ? 1u : 0u;
}
