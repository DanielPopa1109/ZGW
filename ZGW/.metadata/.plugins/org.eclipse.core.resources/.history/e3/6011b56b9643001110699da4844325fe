/* SomeIpSd.c */
#include "SomeIpSd.h"
#include <string.h>

static const SomeIpSd_ConfigType *SomeIpSd_Cfg;
static uint32_t SomeIpSd_TimerMs;

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8u);
    p[1] = (uint8_t)v;
}

static void wr24(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 16u);
    p[1] = (uint8_t)(v >> 8u);
    p[2] = (uint8_t)v;
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24u);
    p[1] = (uint8_t)(v >> 16u);
    p[2] = (uint8_t)(v >> 8u);
    p[3] = (uint8_t)v;
}

static uint16_t SomeIpSd_BuildOffer(uint8_t *buf, const SomeIpSd_OfferedServiceType *svc)
{
    uint16_t idx = 0u;
    uint16_t someIpLenPos;
    uint16_t entriesLenPos;
    uint16_t optionsLenPos;

    wr16(&buf[idx], SOMEIPSD_SERVICE_ID); idx += 2u;
    wr16(&buf[idx], SOMEIPSD_METHOD_ID); idx += 2u;

    someIpLenPos = idx;
    idx += 4u;

    wr16(&buf[idx], 0x0000u); idx += 2u;
    wr16(&buf[idx], 0x0001u); idx += 2u;

    buf[idx++] = 0x01u;
    buf[idx++] = 0x01u;
    buf[idx++] = 0x02u;
    buf[idx++] = 0x00u;

    buf[idx++] = 0xC0u;
    buf[idx++] = 0x00u;
    buf[idx++] = 0x00u;
    buf[idx++] = 0x00u;

    entriesLenPos = idx;
    idx += 4u;

    buf[idx++] = 0x01u;
    buf[idx++] = 0x00u;
    buf[idx++] = 0x00u;
    buf[idx++] = 0x01u;

    wr16(&buf[idx], svc->serviceId); idx += 2u;
    wr16(&buf[idx], svc->instanceId); idx += 2u;
    buf[idx++] = svc->majorVersion;
    wr24(&buf[idx], svc->ttl); idx += 3u;
    wr32(&buf[idx], 0x00000001u); idx += 4u;

    optionsLenPos = idx;
    idx += 4u;

    wr16(&buf[idx], 0x0009u); idx += 2u;
    buf[idx++] = 0x04u;
    buf[idx++] = 0x00u;
    buf[idx++] = 192u;
    buf[idx++] = 168u;
    buf[idx++] = 0u;
    buf[idx++] = 10u;
    buf[idx++] = 0x00u;
    buf[idx++] = 0x11u;
    wr16(&buf[idx], svc->udpPort); idx += 2u;

    wr32(&buf[someIpLenPos], (uint32_t)(idx - 8u));
    wr32(&buf[entriesLenPos], 16u);
    wr32(&buf[optionsLenPos], 12u);

    return idx;
}

static void SomeIpSd_SendOffers(void)
{
    uint8_t tx[256u];
    uint8_t i;
    uint16_t len;

    if (SomeIpSd_Cfg == 0)
    {
        return;
    }

    for (i = 0u; i < SomeIpSd_Cfg->serviceCount; i++)
    {
        len = SomeIpSd_BuildOffer(tx, &SomeIpSd_Cfg->services[i]);

        (void)SoAd_IfTransmit(SomeIpSd_Cfg->sdUdpSoConId,
                              &SomeIpSd_Cfg->multicastAddr,
                              tx,
                              len);
    }
}

void SomeIpSd_Init(const SomeIpSd_ConfigType *config)
{
    SomeIpSd_Cfg = config;
    SomeIpSd_TimerMs = 0u;
}

void SomeIpSd_MainFunction(uint32_t elapsedMs)
{
    if (SomeIpSd_Cfg == 0)
    {
        return;
    }

    SomeIpSd_TimerMs += elapsedMs;

    if (SomeIpSd_TimerMs >= SomeIpSd_Cfg->offerPeriodMs)
    {
        SomeIpSd_TimerMs = 0u;
        SomeIpSd_SendOffers();
    }
}

void SomeIpSd_SoAdRxIndication(SoAd_SoConIdType soConId,
                               const TcpIp_SockAddrType *remoteAddr,
                               const uint8_t *data,
                               uint16_t len)
{
    (void)soConId;
    (void)remoteAddr;
    (void)data;
    (void)len;

    SomeIpSd_SendOffers();
}
