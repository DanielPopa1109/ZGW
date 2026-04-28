/* SomeIpSd.h */
#ifndef SOMEIPSD_H_
#define SOMEIPSD_H_

#include <stdint.h>
#include "SoAd.h"

#define SOMEIPSD_SERVICE_ID       0xFFFFu
#define SOMEIPSD_METHOD_ID        0x8100u
#define SOMEIPSD_PORT             30490u
#define SOMEIPSD_MAX_SERVICES     8u

typedef struct
{
    uint16_t serviceId;
    uint16_t instanceId;
    uint8_t majorVersion;
    uint32_t ttl;
    uint16_t udpPort;
    uint16_t tcpPort;
} SomeIpSd_OfferedServiceType;

typedef struct
{
    SoAd_SoConIdType sdUdpSoConId;
    TcpIp_SockAddrType multicastAddr;
    const SomeIpSd_OfferedServiceType *services;
    uint8_t serviceCount;
    uint32_t offerPeriodMs;
} SomeIpSd_ConfigType;

void SomeIpSd_Init(const SomeIpSd_ConfigType *config);
void SomeIpSd_MainFunction(uint32_t elapsedMs);

void SomeIpSd_SoAdRxIndication(SoAd_SoConIdType soConId,
                               const TcpIp_SockAddrType *remoteAddr,
                               const uint8_t *data,
                               uint16_t len);

#endif
