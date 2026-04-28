/* SoAd.h */
#ifndef SOAD_H_
#define SOAD_H_

#include <stdint.h>
#include <TcpIpH.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOAD_MAX_SOCKET_CONNECTIONS   12u
#define SOAD_MAX_RX_BUFFER_LEN        2048u

typedef uint8_t SoAd_SoConIdType;

typedef enum
{
    SOAD_OK = 0,
    SOAD_NOT_OK,
    SOAD_BUSY,
    SOAD_PARAM_ERROR
} SoAd_ReturnType;

typedef enum
{
    SOAD_SOCON_OFFLINE = 0,
    SOAD_SOCON_RECONNECT,
    SOAD_SOCON_ONLINE,
    SOAD_SOCON_LISTENING
} SoAd_SoConModeType;

typedef enum
{
    SOAD_UPPER_DOIP = 0,
    SOAD_UPPER_SOMEIP,
    SOAD_UPPER_SOMEIP_SD,
    SOAD_UPPER_USER
} SoAd_UpperLayerType;

typedef void (*SoAd_RxIndicationFct)(SoAd_SoConIdType soConId,
                                     const TcpIp_SockAddrType *remoteAddr,
                                     const uint8_t *data,
                                     uint16_t len);

typedef void (*SoAd_TcpConnectedFct)(SoAd_SoConIdType soConId);

typedef void (*SoAd_TcpDisconnectedFct)(SoAd_SoConIdType soConId);

typedef struct
{
    SoAd_SoConIdType soConId;
    SoAd_UpperLayerType upperLayer;
    TcpIp_ProtocolType protocol;
    uint8_t isServer;
    TcpIp_SockAddrType localAddr;
    TcpIp_SockAddrType remoteAddr;
    SoAd_RxIndicationFct rxIndication;
    SoAd_TcpConnectedFct tcpConnected;
    SoAd_TcpDisconnectedFct tcpDisconnected;
} SoAd_SocketConnectionConfigType;

typedef struct
{
    const SoAd_SocketConnectionConfigType *connections;
    uint8_t connectionCount;
} SoAd_ConfigType;

void SoAd_Init(const SoAd_ConfigType *config);
void SoAd_MainFunction(void);

SoAd_ReturnType SoAd_OpenSoCon(SoAd_SoConIdType soConId);
SoAd_ReturnType SoAd_CloseSoCon(SoAd_SoConIdType soConId);

SoAd_ReturnType SoAd_IfTransmit(SoAd_SoConIdType soConId,
                                const TcpIp_SockAddrType *remoteAddr,
                                const uint8_t *data,
                                uint16_t len);

SoAd_SoConModeType SoAd_GetSoConMode(SoAd_SoConIdType soConId);

#ifdef __cplusplus
}
#endif

#endif
