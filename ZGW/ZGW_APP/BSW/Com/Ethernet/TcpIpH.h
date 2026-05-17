#ifndef TCPIPH_H
#define TCPIPH_H

#include "Ifx_Types.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

typedef sint32 TcpIp_SocketIdType;

typedef struct
{
    uint32 addr;
    uint16 port;
} TcpIp_SockAddrType;

#define TCPIP_INVALID_SOCKET   ((TcpIp_SocketIdType)-1)

extern volatile uint8 TcpIp_LastLinkUp;
extern volatile uint8 TcpIp_LastNetifFlags;
extern volatile uint32 TcpIp_CreateFailCounter;
extern volatile uint32 TcpIp_ApiLockCreateFailCounter;
extern volatile uint32 TcpIp_ApiLockTakeFailCounter;
extern volatile uint32 TcpIp_ApiLockGiveFailCounter;
extern volatile sint32 TcpIp_LastSocketError;
extern volatile sint32 TcpIp_LastCreateResult;
extern volatile sint32 TcpIp_LastBindResult;
extern volatile sint32 TcpIp_LastSetSockOptResult;
extern volatile sint32 TcpIp_LastSendResult;
extern volatile sint32 TcpIp_LastSendToResult;
extern volatile TcpIp_SocketIdType TcpIp_LastSocketId;
extern volatile uint16 TcpIp_LastLocalPort;
extern volatile uint32 TcpIp_LastLocalAddr;
extern volatile uint16 TcpIp_LastRemotePort;
extern volatile uint32 TcpIp_LastRemoteAddr;
extern volatile uint16 TcpIp_LastTxLength;
extern volatile sint32 TcpIp_LastRecvResult;
extern volatile sint32 TcpIp_LastRecvFromResult;
extern volatile uint16 TcpIp_LastRxLength;
extern volatile uint16 TcpIp_LastRxRemotePort;
extern volatile uint32 TcpIp_LastRxRemoteAddr;

void TcpIp_Init(void);
void TcpIp_MainFunction(void);
uint8 TcpIp_IsLinkAvailable(void);
uint8 TcpIp_IsSocketOpen(TcpIp_SocketIdType sock);

TcpIp_SocketIdType TcpIp_Create(uint8 tcp);
sint32 TcpIp_Bind(TcpIp_SocketIdType sock, uint16 port);
sint32 TcpIp_BindAddr(TcpIp_SocketIdType sock, uint32 ip, uint16 port);
sint32 TcpIp_Listen(TcpIp_SocketIdType sock);
TcpIp_SocketIdType TcpIp_Accept(TcpIp_SocketIdType sock, TcpIp_SockAddrType *remoteAddr);

sint32 TcpIp_Connect(TcpIp_SocketIdType sock, uint32 ip, uint16 port);

sint32 TcpIp_Send(TcpIp_SocketIdType sock, const uint8 *data, uint16 len);
sint32 TcpIp_SendTo(TcpIp_SocketIdType sock,
                    const TcpIp_SockAddrType *remoteAddr,
                    const uint8 *data,
                    uint16 len);

sint32 TcpIp_Recv(TcpIp_SocketIdType sock, uint8 *data, uint16 len);
sint32 TcpIp_RecvFrom(TcpIp_SocketIdType sock,
                      TcpIp_SockAddrType *remoteAddr,
                      uint8 *data,
                      uint16 len);

void TcpIp_Close(TcpIp_SocketIdType sock);

#endif
