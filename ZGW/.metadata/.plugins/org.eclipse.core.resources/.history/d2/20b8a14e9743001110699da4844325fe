/* EthStack_Cfg.c */
#include "SoAd.h"
#include "DoIP.h"
#include "SomeIp.h"
#include "SomeIpSd.h"
#include "Dcm_EthTpBridge.h"

#define SOAD_SOCON_DOIP_UDP        0u
#define SOAD_SOCON_DOIP_TCP        1u
#define SOAD_SOCON_SOMEIP_SD_UDP   2u
#define SOAD_SOCON_SOMEIP_UDP      3u

static void Demo_MethodHandler(SoAd_SoConIdType soConId,
                               const TcpIp_SockAddrType *remoteAddr,
                               const SomeIp_MessageType *request)
{
    static const uint8_t payload[] = {0x12u, 0x34u};
    (void)SomeIp_SendResponse(soConId,
                              remoteAddr,
                              request,
                              payload,
                              sizeof(payload),
                              SOMEIP_RET_OK);
}

static const SomeIp_MethodConfigType SomeIpMethods[] =
{
    {
        .serviceId = 0x1234u,
        .methodId = 0x0001u,
        .interfaceVersion = 0x01u,
        .handler = Demo_MethodHandler
    }
};

const SomeIp_ConfigType SomeIp_Config =
{
    .udpSoConId = SOAD_SOCON_SOMEIP_UDP,
    .tcpSoConId = 0xFFu,
    .methods = SomeIpMethods,
    .methodCount = sizeof(SomeIpMethods) / sizeof(SomeIpMethods[0])
};

static const SomeIpSd_OfferedServiceType OfferedServices[] =
{
    {
        .serviceId = 0x1234u,
        .instanceId = 0x0001u,
        .majorVersion = 0x01u,
        .ttl = 3u,
        .udpPort = 30500u,
        .tcpPort = 0u
    }
};

const SomeIpSd_ConfigType SomeIpSd_Config =
{
    .sdUdpSoConId = SOAD_SOCON_SOMEIP_SD_UDP,
    .multicastAddr = {{224u, 244u, 224u, 245u}, 30490u},
    .services = OfferedServices,
    .serviceCount = sizeof(OfferedServices) / sizeof(OfferedServices[0]),
    .offerPeriodMs = 1000u
};

static const SoAd_SocketConnectionConfigType SoAdConnections[] =
{
    {
        .soConId = SOAD_SOCON_DOIP_UDP,
        .upperLayer = SOAD_UPPER_DOIP,
        .protocol = TCPIP_PROTOCOL_UDP,
        .isServer = 1u,
        .localAddr = {{192u, 168u, 0u, 10u}, 13400u},
        .remoteAddr = {{255u, 255u, 255u, 255u}, 13400u},
        .rxIndication = DoIP_SoAdUdpRxIndication,
        .tcpConnected = 0,
        .tcpDisconnected = 0
    },
    {
        .soConId = SOAD_SOCON_DOIP_TCP,
        .upperLayer = SOAD_UPPER_DOIP,
        .protocol = TCPIP_PROTOCOL_TCP,
        .isServer = 1u,
        .localAddr = {{192u, 168u, 0u, 10u}, 13400u},
        .remoteAddr = {{0u, 0u, 0u, 0u}, 0u},
        .rxIndication = DoIP_SoAdTcpRxIndication,
        .tcpConnected = DoIP_SoAdTcpConnected,
        .tcpDisconnected = DoIP_SoAdTcpDisconnected
    },
    {
        .soConId = SOAD_SOCON_SOMEIP_SD_UDP,
        .upperLayer = SOAD_UPPER_SOMEIP_SD,
        .protocol = TCPIP_PROTOCOL_UDP,
        .isServer = 1u,
        .localAddr = {{192u, 168u, 0u, 10u}, 30490u},
        .remoteAddr = {{224u, 244u, 224u, 245u}, 30490u},
        .rxIndication = SomeIpSd_SoAdRxIndication,
        .tcpConnected = 0,
        .tcpDisconnected = 0
    },
    {
        .soConId = SOAD_SOCON_SOMEIP_UDP,
        .upperLayer = SOAD_UPPER_SOMEIP,
        .protocol = TCPIP_PROTOCOL_UDP,
        .isServer = 1u,
        .localAddr = {{192u, 168u, 0u, 10u}, 30500u},
        .remoteAddr = {{0u, 0u, 0u, 0u}, 0u},
        .rxIndication = SomeIp_SoAdRxIndication,
        .tcpConnected = 0,
        .tcpDisconnected = 0
    }
};

const SoAd_ConfigType SoAd_Config =
{
    .connections = SoAdConnections,
    .connectionCount = sizeof(SoAdConnections) / sizeof(SoAdConnections[0])
};

const DoIP_ConfigType DoIP_Config =
{
    .vin = "LABTC375DOIP0001",
    .eid = {0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u},
    .gid = {0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u},
    .logicalAddress = 0x1001u,
    .udpSoConId = SOAD_SOCON_DOIP_UDP,
    .tcpSoConId = SOAD_SOCON_DOIP_TCP
};
