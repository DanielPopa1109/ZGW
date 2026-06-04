#include "TcpIpH.h"
#include "lwip_geth_private_phy_dp83825i.h"
#include "lwip_geth_lwip.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/errno.h"
#include "FreeRTOS_core2.h"
#include "semphr_core2.h"

#define TCPIP_MAX_SOCKETS 16u

/* TCP keepalive applied to accepted server connections so a tester that
 * disappears without a FIN/RST is detected and dropped at the TCP layer,
 * freeing the connection instead of leaving a half-open ESTABLISHED PCB.
 * lwIP takes the idle/interval values in seconds via setsockopt. A dead peer
 * is reaped ~14 s after going silent (8 s idle + 3 probes x 2 s); a live but
 * idle tester ACKs the probes and is unaffected. */
#define TCPIP_TCP_KEEPALIVE_IDLE_S      8
#define TCPIP_TCP_KEEPALIVE_INTERVAL_S  2
#define TCPIP_TCP_KEEPALIVE_COUNT       3

typedef struct
{
    TcpIp_SocketIdType sock;
    uint8 used;
    uint8 tcp;
} TcpIp_SocketType;

static TcpIp_SocketType TcpIp_Sockets[TCPIP_MAX_SOCKETS];
static SemaphoreHandle_t_core2 TcpIp_ApiMutex;

long long TcpIp_MainFunction_Counter = 0;
volatile uint32 TcpIp_CreateFailCounter = 0u;
volatile uint32 TcpIp_ApiLockCreateFailCounter = 0u;
volatile uint32 TcpIp_ApiLockTakeFailCounter = 0u;
volatile uint32 TcpIp_ApiLockGiveFailCounter = 0u;
volatile sint32 TcpIp_LastSocketError = 0;
volatile uint8 TcpIp_LastLinkUp = 0u;
volatile uint8 TcpIp_LastNetifFlags = 0u;
volatile sint32 TcpIp_LastCreateResult = 0;
volatile sint32 TcpIp_LastBindResult = 0;
volatile sint32 TcpIp_LastSetSockOptResult = 0;
volatile sint32 TcpIp_LastSendResult = 0;
volatile sint32 TcpIp_LastSendToResult = 0;
volatile TcpIp_SocketIdType TcpIp_LastSocketId = TCPIP_INVALID_SOCKET;
volatile uint16 TcpIp_LastLocalPort = 0u;
volatile uint32 TcpIp_LastLocalAddr = 0u;
volatile uint16 TcpIp_LastRemotePort = 0u;
volatile uint32 TcpIp_LastRemoteAddr = 0u;
volatile uint16 TcpIp_LastTxLength = 0u;
volatile sint32 TcpIp_LastRecvResult = 0;
volatile sint32 TcpIp_LastRecvFromResult = 0;
volatile uint16 TcpIp_LastRxLength = 0u;
volatile uint16 TcpIp_LastRxRemotePort = 0u;
volatile uint32 TcpIp_LastRxRemoteAddr = 0u;

static void TcpIp_InitLock(void)
{
    if (TcpIp_ApiMutex == 0)
    {
        TcpIp_ApiMutex = xSemaphoreCreateRecursiveMutex_core2();

        if (TcpIp_ApiMutex == 0)
        {
            TcpIp_ApiLockCreateFailCounter++;
        }
    }
}

static uint8 TcpIp_Lock(void)
{
    if (TcpIp_ApiMutex == 0)
    {
        TcpIp_InitLock();
    }

    if (TcpIp_ApiMutex == 0)
    {
        TcpIp_ApiLockTakeFailCounter++;
        return 0u;
    }

    if (xSemaphoreTakeRecursive_core2(TcpIp_ApiMutex, portMAX_DELAY_core2) != pdTRUE_core2)
    {
        TcpIp_ApiLockTakeFailCounter++;
        return 0u;
    }

    return 1u;
}

static void TcpIp_Unlock(void)
{
    if (TcpIp_ApiMutex != 0)
    {
        if (xSemaphoreGiveRecursive_core2(TcpIp_ApiMutex) != pdTRUE_core2)
        {
            TcpIp_ApiLockGiveFailCounter++;
        }
    }
}

static uint8 TcpIp_IsLinkUp(void)
{
    const struct netif *netif;

    netif = lwip_geth_Lwip_getNetIf();
    TcpIp_LastNetifFlags = (netif != 0) ? netif->flags : 0u;

#if LWIP_GETH_FORCE_LINK_UP_FOR_BRINGUP
    if (netif != 0)
    {
        ((struct netif *)netif)->flags |= (NETIF_FLAG_UP | NETIF_FLAG_LINK_UP);
    }
    TcpIp_LastLinkUp = 1u;
    return 1u;
#else
    if ((netif != 0) &&
        ((netif->flags & NETIF_FLAG_UP) != 0u) &&
        ((netif->flags & NETIF_FLAG_LINK_UP) != 0u))
    {
        TcpIp_LastLinkUp = 1u;
        return 1u;
    }

#if (PHY_DEVICE_NAME == PHY_DP83825I)
    TcpIp_LastLinkUp = (uint8)lwip_geth_private_Phy_Dp83825i_is_link_up();
    return TcpIp_LastLinkUp;
#else
    TcpIp_LastLinkUp = 1u;
    return 1u;
#endif
#endif
}

uint8 TcpIp_IsLinkAvailable(void)
{
    return TcpIp_IsLinkUp();
}

void TcpIp_Init(void)
{
    uint32 i;

    TcpIp_InitLock();

    for (i = 0u; i < TCPIP_MAX_SOCKETS; i++)
    {
        TcpIp_Sockets[i].sock = TCPIP_INVALID_SOCKET;
        TcpIp_Sockets[i].used = 0u;
        TcpIp_Sockets[i].tcp = 0u;
    }

    TcpIp_LastSocketError = 0;
    TcpIp_LastCreateResult = 0;
    TcpIp_LastBindResult = 0;
    TcpIp_LastSetSockOptResult = 0;
    TcpIp_LastSendResult = 0;
    TcpIp_LastSendToResult = 0;
    TcpIp_LastSocketId = TCPIP_INVALID_SOCKET;
    TcpIp_LastLocalPort = 0u;
    TcpIp_LastLocalAddr = 0u;
    TcpIp_LastRemotePort = 0u;
    TcpIp_LastRemoteAddr = 0u;
    TcpIp_LastTxLength = 0u;
    TcpIp_LastRecvResult = 0;
    TcpIp_LastRecvFromResult = 0;
    TcpIp_LastRxLength = 0u;
    TcpIp_LastRxRemotePort = 0u;
    TcpIp_LastRxRemoteAddr = 0u;
}

void TcpIp_MainFunction(void)
{
    uint32 i;

    if (TcpIp_IsLinkUp() == 0u)
    {
        if (TcpIp_Lock() != 0u)
        {
            for (i = 0u; i < TCPIP_MAX_SOCKETS; i++)
            {
                if (TcpIp_Sockets[i].used != 0u)
                {
                    lwip_close(TcpIp_Sockets[i].sock);
                    TcpIp_Sockets[i].sock = TCPIP_INVALID_SOCKET;
                    TcpIp_Sockets[i].used = 0u;
                    TcpIp_Sockets[i].tcp = 0u;
                }
            }

            TcpIp_Unlock();
        }
    }

    TcpIp_MainFunction_Counter++;
}

static TcpIp_SocketType *TcpIp_Alloc(void)
{
    uint32 i;

    for (i = 0u; i < TCPIP_MAX_SOCKETS; i++)
    {
        if (TcpIp_Sockets[i].used == 0u)
        {
            TcpIp_Sockets[i].used = 1u;
            return &TcpIp_Sockets[i];
        }
    }

    return 0;
}

static TcpIp_SocketType *TcpIp_Find(TcpIp_SocketIdType sock)
{
    uint32 i;

    for (i = 0u; i < TCPIP_MAX_SOCKETS; i++)
    {
        if ((TcpIp_Sockets[i].used != 0u) &&
            (TcpIp_Sockets[i].sock == sock))
        {
            return &TcpIp_Sockets[i];
        }
    }

    return 0;
}

uint8 TcpIp_IsSocketOpen(TcpIp_SocketIdType sock)
{
    uint8 ret;

    if (TcpIp_Lock() == 0u)
    {
        return 0u;
    }

    ret = (TcpIp_Find(sock) != 0) ? 1u : 0u;
    TcpIp_Unlock();

    return ret;
}

TcpIp_SocketIdType TcpIp_Create(uint8 tcp)
{
    TcpIp_SocketType *s;
    sint32 type;
    int optval;

    if (TcpIp_Lock() == 0u)
    {
        TcpIp_LastCreateResult = TCPIP_INVALID_SOCKET;
        TcpIp_LastSocketId = TCPIP_INVALID_SOCKET;
        TcpIp_CreateFailCounter++;
        return TCPIP_INVALID_SOCKET;
    }

    s = TcpIp_Alloc();

    if (s == 0)
    {
        TcpIp_LastCreateResult = TCPIP_INVALID_SOCKET;
        TcpIp_LastSocketId = TCPIP_INVALID_SOCKET;
        TcpIp_CreateFailCounter++;
        TcpIp_Unlock();
        return TCPIP_INVALID_SOCKET;
    }

    type = (tcp != 0u) ? SOCK_STREAM : SOCK_DGRAM;

    s->sock = (TcpIp_SocketIdType)lwip_socket(AF_INET, type, 0);
    TcpIp_LastSocketId = s->sock;
    TcpIp_LastCreateResult = (sint32)s->sock;

    if (s->sock < 0)
    {
        TcpIp_LastSocketError = errno;
        TcpIp_CreateFailCounter++;
        s->used = 0u;
        s->sock = TCPIP_INVALID_SOCKET;
        TcpIp_Unlock();
        return TCPIP_INVALID_SOCKET;
    }

    s->tcp = tcp;
    (void)lwip_fcntl(s->sock, F_SETFL, O_NONBLOCK);

    optval = 1;
    TcpIp_LastSetSockOptResult = (sint32)lwip_setsockopt(s->sock,
                                                         SOL_SOCKET,
                                                         SO_REUSEADDR,
                                                         &optval,
                                                         sizeof(optval));
    if (TcpIp_LastSetSockOptResult < 0)
    {
        TcpIp_LastSocketError = errno;
    }

    if (tcp == 0u)
    {
        optval = 1;
        TcpIp_LastSetSockOptResult = (sint32)lwip_setsockopt(s->sock,
                                                             SOL_SOCKET,
                                                             SO_BROADCAST,
                                                             &optval,
                                                             sizeof(optval));
        if (TcpIp_LastSetSockOptResult < 0)
        {
            TcpIp_LastSocketError = errno;
        }
    }

    TcpIp_Unlock();
    return s->sock;
}

sint32 TcpIp_Bind(TcpIp_SocketIdType sock, uint16 port)
{
    return TcpIp_BindAddr(sock, 0u, port);
}

sint32 TcpIp_BindAddr(TcpIp_SocketIdType sock, uint32 ip, uint16 port)
{
    struct sockaddr_in addr;
    sint32 ret;

    if (TcpIp_Lock() == 0u)
    {
        TcpIp_LastBindResult = -1;
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = (ip == 0u) ? INADDR_ANY : htonl(ip);

    TcpIp_LastSocketId = sock;
    TcpIp_LastLocalPort = port;
    TcpIp_LastLocalAddr = ip;
    ret = (sint32)lwip_bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    TcpIp_LastBindResult = ret;

    if (ret < 0)
    {
        TcpIp_LastSocketError = errno;
    }

    TcpIp_Unlock();
    return ret;
}

sint32 TcpIp_Listen(TcpIp_SocketIdType sock)
{
    sint32 ret;

    if (TcpIp_Lock() == 0u)
    {
        return -1;
    }

    ret = (sint32)lwip_listen(sock, 4);
    TcpIp_Unlock();

    return ret;
}

static void TcpIp_EnableKeepAlive(TcpIp_SocketIdType sock)
{
    int optval;

    optval = 1;
    (void)lwip_setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));

    optval = TCPIP_TCP_KEEPALIVE_IDLE_S;
    (void)lwip_setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &optval, sizeof(optval));

    optval = TCPIP_TCP_KEEPALIVE_INTERVAL_S;
    (void)lwip_setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &optval, sizeof(optval));

    optval = TCPIP_TCP_KEEPALIVE_COUNT;
    (void)lwip_setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &optval, sizeof(optval));
}

TcpIp_SocketIdType TcpIp_Accept(TcpIp_SocketIdType sock, TcpIp_SockAddrType *remoteAddr)
{
    struct sockaddr_in addr;
    socklen_t len;
    sint32 newSock;
    TcpIp_SocketType *slot;

    if (TcpIp_Lock() == 0u)
    {
        return TCPIP_INVALID_SOCKET;
    }

    len = sizeof(addr);
    newSock = lwip_accept(sock, (struct sockaddr *)&addr, &len);

    if (newSock >= 0)
    {
        slot = TcpIp_Alloc();

        if (slot == 0)
        {
            (void)lwip_close(newSock);
            TcpIp_Unlock();
            return TCPIP_INVALID_SOCKET;
        }

        slot->sock = (TcpIp_SocketIdType)newSock;
        slot->tcp = 1u;

        (void)lwip_fcntl(newSock, F_SETFL, O_NONBLOCK);
        TcpIp_EnableKeepAlive(slot->sock);

        if (remoteAddr != 0)
        {
            remoteAddr->addr = ntohl(addr.sin_addr.s_addr);
            remoteAddr->port = ntohs(addr.sin_port);
        }
    }

    TcpIp_Unlock();
    return (TcpIp_SocketIdType)newSock;
}

sint32 TcpIp_Connect(TcpIp_SocketIdType sock, uint32 ip, uint16 port)
{
    struct sockaddr_in addr;
    sint32 ret;

    if (TcpIp_Lock() == 0u)
    {
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(ip);

    ret = (sint32)lwip_connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    TcpIp_Unlock();

    return ret;
}

sint32 TcpIp_Send(TcpIp_SocketIdType sock, const uint8 *data, uint16 len)
{
    sint32 ret;

    if ((data == 0) || (len == 0u) || (TcpIp_IsLinkUp() == 0u))
    {
        TcpIp_LastSendResult = -1;
        TcpIp_LastTxLength = len;
        return -1;
    }

    TcpIp_LastSocketId = sock;
    TcpIp_LastTxLength = len;
    if (TcpIp_Lock() == 0u)
    {
        TcpIp_LastSendResult = -1;
        return -1;
    }

    if (TcpIp_Find(sock) == 0)
    {
        TcpIp_LastSendResult = -1;
        TcpIp_Unlock();
        return -1;
    }

    ret = (sint32)lwip_send(sock, data, len, 0);
    TcpIp_LastSendResult = ret;

    if (ret < 0)
    {
        TcpIp_LastSocketError = errno;
    }

    TcpIp_Unlock();
    return ret;
}

sint32 TcpIp_SendTo(TcpIp_SocketIdType sock,
                    const TcpIp_SockAddrType *remoteAddr,
                    const uint8 *data,
                    uint16 len)
{
    struct sockaddr_in addr;
    sint32 ret;

    if ((remoteAddr == 0) || (data == 0) || (len == 0u) || (TcpIp_IsLinkUp() == 0u))
    {
        TcpIp_LastSendToResult = -1;
        TcpIp_LastTxLength = len;
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(remoteAddr->port);
    addr.sin_addr.s_addr = htonl(remoteAddr->addr);

    TcpIp_LastSocketId = sock;
    TcpIp_LastRemoteAddr = remoteAddr->addr;
    TcpIp_LastRemotePort = remoteAddr->port;
    TcpIp_LastTxLength = len;
    if (TcpIp_Lock() == 0u)
    {
        TcpIp_LastSendToResult = -1;
        return -1;
    }

    if (TcpIp_Find(sock) == 0)
    {
        TcpIp_LastSendToResult = -1;
        TcpIp_Unlock();
        return -1;
    }

    ret = (sint32)lwip_sendto(sock, data, len, 0, (struct sockaddr *)&addr, sizeof(addr));
    TcpIp_LastSendToResult = ret;

    if (ret < 0)
    {
        TcpIp_LastSocketError = errno;
    }

    TcpIp_Unlock();
    return ret;
}

sint32 TcpIp_Recv(TcpIp_SocketIdType sock, uint8 *data, uint16 len)
{
    sint32 ret;

    if ((data == 0) || (len == 0u))
    {
        return -1;
    }

    if (TcpIp_Lock() == 0u)
    {
        return -1;
    }

    if (TcpIp_Find(sock) == 0)
    {
        TcpIp_Unlock();
        return -1;
    }

    ret = (sint32)lwip_recv(sock, data, len, 0);
    TcpIp_LastRecvResult = ret;
    TcpIp_LastRxLength = (ret > 0) ? (uint16)ret : 0u;
    if (ret < 0)
    {
        TcpIp_LastSocketError = errno;
    }
    TcpIp_Unlock();

    return ret;
}

sint32 TcpIp_RecvFrom(TcpIp_SocketIdType sock,
                      TcpIp_SockAddrType *remoteAddr,
                      uint8 *data,
                      uint16 len)
{
    struct sockaddr_in addr;
    socklen_t addrLen;
    sint32 ret;

    if ((data == 0) || (len == 0u))
    {
        return -1;
    }

    addrLen = sizeof(addr);

    if (TcpIp_Lock() == 0u)
    {
        return -1;
    }

    if (TcpIp_Find(sock) == 0)
    {
        TcpIp_Unlock();
        return -1;
    }

    ret = (sint32)lwip_recvfrom(sock, data, len, 0, (struct sockaddr *)&addr, &addrLen);
    TcpIp_LastRecvFromResult = ret;
    TcpIp_LastRxLength = (ret > 0) ? (uint16)ret : 0u;

    if ((ret > 0) && (remoteAddr != 0))
    {
        remoteAddr->addr = ntohl(addr.sin_addr.s_addr);
        remoteAddr->port = ntohs(addr.sin_port);
        TcpIp_LastRxRemoteAddr = remoteAddr->addr;
        TcpIp_LastRxRemotePort = remoteAddr->port;
    }
    else if (ret < 0)
    {
        TcpIp_LastSocketError = errno;
    }

    TcpIp_Unlock();
    return ret;
}

void TcpIp_Close(TcpIp_SocketIdType sock)
{
    TcpIp_SocketType *slot;

    if (sock != TCPIP_INVALID_SOCKET)
    {
        if (TcpIp_Lock() == 0u)
        {
            return;
        }

        slot = TcpIp_Find(sock);

        if (slot == 0)
        {
            TcpIp_Unlock();
            return;
        }

        (void)lwip_close(sock);

        slot->sock = TCPIP_INVALID_SOCKET;
        slot->used = 0u;
        slot->tcp = 0u;

        TcpIp_Unlock();
    }
}
