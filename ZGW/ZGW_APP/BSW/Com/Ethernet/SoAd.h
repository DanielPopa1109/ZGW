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
    uint8 useConfiguredRemote;
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
void SoAd_AbortTcpConnection(SoAd_SoConIdType id);
sint32 SoAd_Send(SoAd_SoConIdType id, const uint8 *data, uint16 len);

extern volatile uint32 SoAd_OpenFailNoLinkCounter;
extern volatile uint32 SoAd_OpenFailCreateCounter;
extern volatile uint32 SoAd_OpenFailBindCounter;
extern volatile uint32 SoAd_ApiLockCreateFailCounter;
extern volatile uint32 SoAd_ApiLockTakeFailCounter;
extern volatile uint32 SoAd_ApiLockGiveFailCounter;
extern volatile uint8 SoAd_DebugState[SOAD_MAX_CONNECTIONS];
extern volatile uint8 SoAd_DebugRequestedOpen[SOAD_MAX_CONNECTIONS];
extern volatile sint32 SoAd_DebugSocket[SOAD_MAX_CONNECTIONS];
extern volatile uint8 SoAd_DebugLastOpenResult[SOAD_MAX_CONNECTIONS];
extern volatile SoAd_SoConIdType SoAd_DebugLastTxSoConId;
extern volatile uint8 SoAd_DebugLastTxResult;
extern volatile sint32 SoAd_DebugLastTxTcpIpResult;
extern volatile uint16 SoAd_DebugLastTxLength;
extern volatile uint32 SoAd_DebugRxCounter[SOAD_MAX_CONNECTIONS];
extern volatile uint32 SoAd_DebugRxDropCounter[SOAD_MAX_CONNECTIONS];
extern volatile uint16 SoAd_DebugLastRxLength[SOAD_MAX_CONNECTIONS];
extern volatile uint16 SoAd_DebugLastRxRemotePort[SOAD_MAX_CONNECTIONS];
extern volatile uint32 SoAd_DebugLastRxRemoteAddr[SOAD_MAX_CONNECTIONS];
extern volatile uint32 SoAd_DebugPcHeartbeatRxCounter;
extern volatile uint32 SoAd_DebugPcHeartbeatAckCounter;
extern volatile uint32 SoAd_DebugPcHeartbeatLastRemoteAddr;
extern volatile uint16 SoAd_DebugPcHeartbeatLastRemotePort;
extern volatile uint32 SoAd_DebugTcpStaleReplaceCounter;
extern volatile sint32 SoAd_DebugTcpLastAcceptedSocket[SOAD_MAX_CONNECTIONS];

SoAd_ReturnType SoAd_IfTransmit(SoAd_SoConIdType id,
                                const TcpIp_SockAddrType *remoteAddr,
                                const uint8 *data,
                                uint16 len);

#endif
