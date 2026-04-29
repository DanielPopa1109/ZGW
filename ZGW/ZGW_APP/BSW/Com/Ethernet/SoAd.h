#ifndef SOAD_H
#define SOAD_H

#include "Ifx_Types.h"
#include "TcpIpH.h"

#define SOAD_MAX_CONNECTIONS 8u

typedef uint8 SoAd_SoConIdType;

typedef enum
{
    SOAD_NOT_OK = 0,
    SOAD_OK = 1
} SoAd_ReturnType;

typedef enum
{
    SOAD_SOCON_CLOSED = 0,
    SOAD_SOCON_OPEN,
    SOAD_SOCON_CONNECTED
} SoAd_StateType;

typedef enum
{
    TCPIP_PROTOCOL_UDP = 0,
    TCPIP_PROTOCOL_TCP = 1
} TcpIp_ProtocolType;

typedef enum
{
    SOAD_UPPER_NONE = 0,
    SOAD_UPPER_DOIP,
    SOAD_UPPER_SOMEIP,
    SOAD_UPPER_SOMEIP_SD
} SoAd_UpperLayerType;

typedef struct
{
    uint8 addr[4];
    uint16 port;
} SoAd_SockAddrType;

typedef void (*SoAd_RxIndicationFct)(SoAd_SoConIdType soConId,
                                     const TcpIp_SockAddrType *remoteAddr,
                                     const uint8 *data,
                                     uint16 len);

typedef void (*SoAd_TcpConnectionFct)(SoAd_SoConIdType soConId);

typedef struct
{
    SoAd_SoConIdType soConId;
    SoAd_UpperLayerType upperLayer;
    TcpIp_ProtocolType protocol;
    uint8 isServer;
    SoAd_SockAddrType localAddr;
    SoAd_SockAddrType remoteAddr;
    SoAd_RxIndicationFct rxIndication;
    SoAd_TcpConnectionFct tcpConnected;
    SoAd_TcpConnectionFct tcpDisconnected;
} SoAd_SocketConnectionConfigType;

typedef struct
{
    const SoAd_SocketConnectionConfigType *connections;
    uint8 numConnections;
} SoAd_ConfigType;

void SoAd_Init(const SoAd_ConfigType *cfg);
void SoAd_MainFunction(void);

uint8 SoAd_OpenSoCon(SoAd_SoConIdType id);
void SoAd_CloseSoCon(SoAd_SoConIdType id);

sint32 SoAd_Send(SoAd_SoConIdType id, const uint8 *data, uint16 len);

SoAd_ReturnType SoAd_IfTransmit(SoAd_SoConIdType id,
                                const TcpIp_SockAddrType *remoteAddr,
                                const uint8 *data,
                                uint16 len);

#endif
