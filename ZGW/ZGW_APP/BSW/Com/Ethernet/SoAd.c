#include "SoAd.h"
#include "GatewaySwc.h"
#include "EthernetDiag.h"
#include "../ComM/ComM.h"
#include "SysMgr.h"
#include "FreeRTOS_core2.h"
#include "semphr_core2.h"
#include <string.h>
#include <errno.h>

typedef struct
{
        SoAd_StateType state;
        TcpIp_SocketIdType listenSock;
        TcpIp_SocketIdType activeSock;
        TcpIp_SockAddrType remoteAddr;
        const SoAd_SocketConnectionConfigType *cfg;
        uint8 requestedOpen;
} SoAd_SoConRuntimeType;

static SoAd_SoConRuntimeType SoAd_Runtime[SOAD_MAX_CONNECTIONS];
static const SoAd_ConfigType *SoAd_Cfg;
static SemaphoreHandle_t_core2 SoAd_ApiMutex;

long long SoAd_MainFunction_Counter = 0;

#define SOAD_RX_DRAIN_BUDGET_PER_SOCON      128u
#define SOAD_RX_BUFFER_SIZE                 512u
#define SOAD_PC_HEARTBEAT_PORT              30600u
#define SOAD_PC_HEARTBEAT_PAYLOAD           "PCHeartbeat"
#define SOAD_PC_HEARTBEAT_ACK_PAYLOAD       "AURIXHeartbeatAck"

volatile uint32 SoAd_OpenFailNoLinkCounter = 0u;
volatile uint32 SoAd_OpenFailCreateCounter = 0u;
volatile uint32 SoAd_OpenFailBindCounter = 0u;
volatile uint32 SoAd_ApiLockCreateFailCounter = 0u;
volatile uint32 SoAd_ApiLockTakeFailCounter = 0u;
volatile uint32 SoAd_ApiLockGiveFailCounter = 0u;
volatile uint8 SoAd_DebugState[SOAD_MAX_CONNECTIONS];
#if SOAD_DEBUG_INSTRUMENTATION
volatile uint8 SoAd_DebugRequestedOpen[SOAD_MAX_CONNECTIONS];
volatile sint32 SoAd_DebugSocket[SOAD_MAX_CONNECTIONS];
volatile uint8 SoAd_DebugLastOpenResult[SOAD_MAX_CONNECTIONS];
volatile SoAd_SoConIdType SoAd_DebugLastTxSoConId = 0u;
volatile uint8 SoAd_DebugLastTxResult = SOAD_NOT_OK;
volatile sint32 SoAd_DebugLastTxTcpIpResult = 0;
volatile uint16 SoAd_DebugLastTxLength = 0u;
volatile uint32 SoAd_DebugRxCounter[SOAD_MAX_CONNECTIONS];
volatile uint32 SoAd_DebugRxDropCounter[SOAD_MAX_CONNECTIONS];
volatile uint16 SoAd_DebugLastRxLength[SOAD_MAX_CONNECTIONS];
volatile uint16 SoAd_DebugLastRxRemotePort[SOAD_MAX_CONNECTIONS];
volatile uint32 SoAd_DebugLastRxRemoteAddr[SOAD_MAX_CONNECTIONS];
volatile uint32 SoAd_DebugPcHeartbeatRxCounter = 0u;
volatile uint32 SoAd_DebugPcHeartbeatAckCounter = 0u;
volatile uint32 SoAd_DebugPcHeartbeatBroadcastAckCounter = 0u;
volatile uint32 SoAd_DebugPcHeartbeatLastRemoteAddr = 0u;
volatile uint16 SoAd_DebugPcHeartbeatLastRemotePort = 0u;
volatile uint32 SoAd_DebugTcpStaleReplaceCounter = 0u;
volatile uint32 SoAd_DebugSocketLossCounter = 0u;
volatile SoAd_SoConIdType SoAd_DebugLastSocketLossSoConId = 0u;
volatile sint32 SoAd_DebugTcpLastAcceptedSocket[SOAD_MAX_CONNECTIONS];
#endif

#if SOAD_DEBUG_INSTRUMENTATION
#define SOAD_DEBUG_ASSIGN(lhs, rhs) do { (lhs) = (rhs); } while (0)
#define SOAD_DEBUG_INC(lhs) do { (lhs)++; } while (0)
#else
#define SOAD_DEBUG_ASSIGN(lhs, rhs) do { (void)0; } while (0)
#define SOAD_DEBUG_INC(lhs) do { (void)0; } while (0)
#endif

static void SoAd_InitLock(void)
{
    if (SoAd_ApiMutex == 0)
    {
        SoAd_ApiMutex = xSemaphoreCreateRecursiveMutex_core2();

        if (SoAd_ApiMutex == 0)
        {
            SoAd_ApiLockCreateFailCounter++;
        }
    }
}

static uint8 SoAd_Lock(void)
{
    if (SoAd_ApiMutex == 0)
    {
        SoAd_InitLock();
    }

    if (SoAd_ApiMutex == 0)
    {
        SoAd_ApiLockTakeFailCounter++;
        return 0u;
    }

    if (xSemaphoreTakeRecursive_core2(SoAd_ApiMutex, portMAX_DELAY_core2) != pdTRUE_core2)
    {
        SoAd_ApiLockTakeFailCounter++;
        return 0u;
    }

    return 1u;
}

static void SoAd_Unlock(void)
{
    if (SoAd_ApiMutex != 0)
    {
        if (xSemaphoreGiveRecursive_core2(SoAd_ApiMutex) != pdTRUE_core2)
        {
            SoAd_ApiLockGiveFailCounter++;
        }
    }
}

static uint32 SoAd_Ip4ToU32(const uint8 ip[4])
{
    return ((uint32)ip[0] << 24u) |
            ((uint32)ip[1] << 16u) |
            ((uint32)ip[2] << 8u) |
            ((uint32)ip[3]);
}

static uint8 SoAd_IsPcHeartbeat(const uint8 *data, uint16 len)
{
    const uint16 hbLen = (uint16)(sizeof(SOAD_PC_HEARTBEAT_PAYLOAD) - 1u);

    if ((data == 0) || (len != hbLen))
    {
        return 0u;
    }

    return (memcmp(data, SOAD_PC_HEARTBEAT_PAYLOAD, hbLen) == 0) ? 1u : 0u;
}

void SoAd_AbortTcpConnection(SoAd_SoConIdType id)
{
    SoAd_SoConRuntimeType *rt;

    if (SoAd_Lock() == 0u)
    {
        return;
    }

    if (id >= SOAD_MAX_CONNECTIONS)
    {
        SoAd_Unlock();
        return;
    }

    rt = &SoAd_Runtime[id];

    if ((rt->cfg == 0) || (rt->cfg->protocol != TCPIP_PROTOCOL_TCP))
    {
        SoAd_Unlock();
        return;
    }

    if ((rt->activeSock != TCPIP_INVALID_SOCKET) &&
            (rt->activeSock != rt->listenSock))
    {
        TcpIp_Close(rt->activeSock);
    }

    if (rt->cfg->tcpDisconnected != 0)
    {
        rt->cfg->tcpDisconnected(id);
    }

    rt->activeSock = TCPIP_INVALID_SOCKET;
    rt->remoteAddr.addr = 0u;
    rt->remoteAddr.port = 0u;

    if ((rt->listenSock != TCPIP_INVALID_SOCKET) &&
            (TcpIp_IsSocketOpen(rt->listenSock) != 0u))
    {
        rt->state = SOAD_SOCON_OPEN;
        SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], rt->listenSock);
    }
    else
    {
        rt->state = SOAD_SOCON_CLOSED;
        SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], TCPIP_INVALID_SOCKET);
    }

    SoAd_DebugState[id] = (uint8)rt->state;
    SoAd_Unlock();
}

static uint8 SoAd_IsTcpWouldBlock(sint32 err)
{
#if defined(EWOULDBLOCK) && defined(EAGAIN)
    return ((err == EWOULDBLOCK) || (err == EAGAIN)) ? 1u : 0u;
#elif defined(EWOULDBLOCK)
    return (err == EWOULDBLOCK) ? 1u : 0u;
#elif defined(EAGAIN)
    return (err == EAGAIN) ? 1u : 0u;
#else
    return 0u;
#endif
}

/* Only a definitely-fatal connection error should tear down a socket. A
 * would-block / try-again result, or an unset / unrecognized errno, is treated
 * as transient and must NOT trigger SoAd_HandleSocketLoss / disconnect: doing so
 * for the DoIP server resets the DoIP TP session and takes routing out of
 * ROUTING_ACTIVE, which makes DoIP_SendDiagnosticResponse drop the in-flight UDS
 * response (e.g. the 50 03 to a 10 03) - the tester then times out even though
 * the link is healthy. Genuinely lost sockets are still recovered via the
 * socket-table check in SoAd_RuntimeSocketLost and the len==0 peer-close path. */
static uint8 SoAd_IsTcpFatalError(sint32 err)
{
    if (SoAd_IsTcpWouldBlock(err) != 0u)
    {
        return 0u;
    }

#if defined(ECONNRESET)
    if (err == ECONNRESET)   { return 1u; }
#endif
#if defined(ECONNABORTED)
    if (err == ECONNABORTED) { return 1u; }
#endif
#if defined(ENOTCONN)
    if (err == ENOTCONN)     { return 1u; }
#endif
#if defined(EPIPE)
    if (err == EPIPE)        { return 1u; }
#endif
#if defined(ESHUTDOWN)
    if (err == ESHUTDOWN)    { return 1u; }
#endif
#if defined(EBADF)
    if (err == EBADF)        { return 1u; }
#endif
#if defined(ECONNREFUSED)
    if (err == ECONNREFUSED) { return 1u; }
#endif

    return 0u;
}

static EthernetDiagCloseReasonType SoAd_ToDiagCloseReason(sint32 err, uint8 peerFin)
{
    if (peerFin != 0u)
    {
        return ETHERNETDIAG_CLOSE_PEER_FIN;
    }
#if defined(ECONNRESET)
    if (err == ECONNRESET) { return ETHERNETDIAG_CLOSE_RESET; }
#endif
#if defined(ETIMEDOUT)
    if (err == ETIMEDOUT) { return ETHERNETDIAG_CLOSE_TIMEOUT; }
#endif
#if defined(EPIPE)
    if (err == EPIPE) { return ETHERNETDIAG_CLOSE_PIPE; }
#endif
#if defined(ENOTCONN)
    if (err == ENOTCONN) { return ETHERNETDIAG_CLOSE_NOT_CONNECTED; }
#endif
    return ETHERNETDIAG_CLOSE_SOCKET_LOST;
}

static void SoAd_TcpDisconnectToOpen(SoAd_SoConRuntimeType *rt, SoAd_SoConIdType id)
{
    if ((rt == 0) || (rt->cfg == 0))
    {
        return;
    }

    if (rt->cfg->tcpDisconnected != 0)
    {
        rt->cfg->tcpDisconnected(id);
    }

    if ((rt->activeSock != TCPIP_INVALID_SOCKET) &&
            (rt->activeSock != rt->listenSock))
    {
        TcpIp_Close(rt->activeSock);
    }

    rt->activeSock = TCPIP_INVALID_SOCKET;
    rt->remoteAddr.addr = 0u;
    rt->remoteAddr.port = 0u;

    if ((rt->listenSock != TCPIP_INVALID_SOCKET) &&
            (TcpIp_IsSocketOpen(rt->listenSock) != 0u))
    {
        rt->state = SOAD_SOCON_OPEN;
        SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], rt->listenSock);
    }
    else
    {
        rt->state = SOAD_SOCON_CLOSED;
        SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], TCPIP_INVALID_SOCKET);
    }

    SoAd_DebugState[id] = (uint8)rt->state;
}

static uint8 SoAd_HandlePcHeartbeat(SoAd_SoConRuntimeType *rt,
        SoAd_SoConIdType id,
        const TcpIp_SockAddrType *remoteAddr,
        const uint8 *data,
        uint16 len)
{
    TcpIp_SockAddrType ackDst;
    const uint8 ackPayload[] = SOAD_PC_HEARTBEAT_ACK_PAYLOAD;

    if ((rt == 0) || (rt->cfg == 0) || (remoteAddr == 0))
    {
        return 0u;
    }

    if ((rt->cfg->protocol != TCPIP_PROTOCOL_UDP) ||
            (SoAd_IsPcHeartbeat(data, len) == 0u))
    {
        return 0u;
    }

    SOAD_DEBUG_INC(SoAd_DebugPcHeartbeatRxCounter);
    SOAD_DEBUG_ASSIGN(SoAd_DebugPcHeartbeatLastRemoteAddr, remoteAddr->addr);
    SOAD_DEBUG_ASSIGN(SoAd_DebugPcHeartbeatLastRemotePort, remoteAddr->port);
    ackDst = *remoteAddr;

    if ((ackDst.addr == 0u) || (ackDst.port == 0u))
    {
        ackDst.addr = SoAd_Ip4ToU32(rt->cfg->remoteAddr.addr);
        ackDst.port = rt->cfg->remoteAddr.port;
    }

    if (GatewaySwc_RequestTcpIpSendTo(rt->listenSock,
            &ackDst,
            ackPayload,
            (uint16)(sizeof(ackPayload) - 1u)) > 0)
    {
        SOAD_DEBUG_INC(SoAd_DebugPcHeartbeatAckCounter);
    }
    else
    {
        ackDst.addr = SoAd_Ip4ToU32(rt->cfg->remoteAddr.addr);
        if (ackDst.addr == 0u)
        {
            ackDst.addr = 0xFFFFFFFFu;
        }

        if (GatewaySwc_RequestTcpIpSendTo(rt->listenSock,
                &ackDst,
                ackPayload,
                (uint16)(sizeof(ackPayload) - 1u)) > 0)
        {
            SOAD_DEBUG_INC(SoAd_DebugPcHeartbeatBroadcastAckCounter);
        }

        if (ackDst.port != SOAD_PC_HEARTBEAT_PORT)
        {
            ackDst.port = SOAD_PC_HEARTBEAT_PORT;

            if (GatewaySwc_RequestTcpIpSendTo(rt->listenSock,
                    &ackDst,
                    ackPayload,
                    (uint16)(sizeof(ackPayload) - 1u)) > 0)
            {
                SOAD_DEBUG_INC(SoAd_DebugPcHeartbeatBroadcastAckCounter);
            }
        }
    }

    (void)id;
    return 1u;
}

static const SoAd_SocketConnectionConfigType *SoAd_FindConfig(SoAd_SoConIdType id)
{
    uint8 i;

    if (SoAd_Cfg == 0)
    {
        return 0;
    }

    for (i = 0u; i < SoAd_Cfg->numConnections; i++)
    {
        if (SoAd_Cfg->connections[i].soConId == id)
        {
            return &SoAd_Cfg->connections[i];
        }
    }

    return 0;
}

static uint8 SoAd_RuntimeSocketLost(const SoAd_SoConRuntimeType *rt)
{
    if (rt == 0)
    {
        return 0u;
    }

    if (rt->state == SOAD_SOCON_CLOSED)
    {
        return 0u;
    }

    if ((rt->listenSock == TCPIP_INVALID_SOCKET) ||
            (TcpIp_IsSocketOpen(rt->listenSock) == 0u))
    {
        return 1u;
    }

    if ((rt->state == SOAD_SOCON_CONNECTED) &&
            ((rt->activeSock == TCPIP_INVALID_SOCKET) ||
                    (TcpIp_IsSocketOpen(rt->activeSock) == 0u)))
    {
        return 1u;
    }

    return 0u;
}

static void SoAd_HandleSocketLoss(SoAd_SoConIdType id)
{
    SoAd_SoConRuntimeType *rt;
    const SoAd_SocketConnectionConfigType *cfg;
    uint8 reopen;

    if (id >= SOAD_MAX_CONNECTIONS)
    {
        return;
    }

    rt = &SoAd_Runtime[id];
    cfg = rt->cfg;
    reopen = rt->requestedOpen;
    SOAD_DEBUG_INC(SoAd_DebugSocketLossCounter);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastSocketLossSoConId, id);
    if ((cfg != 0) &&
            (cfg->tcpDisconnected != 0) &&
            (rt->state == SOAD_SOCON_CONNECTED))
    {
        cfg->tcpDisconnected(id);
        EthernetDiag_ReportSocketClosed(EthernetDiag_GetConnectionIdForSoCon(id),
                ETHERNETDIAG_CLOSE_SOCKET_LOST);
    }

    if ((rt->activeSock != TCPIP_INVALID_SOCKET) &&
            (rt->activeSock != rt->listenSock) &&
            (TcpIp_IsSocketOpen(rt->activeSock) != 0u))
    {
        TcpIp_Close(rt->activeSock);
    }

    if ((rt->listenSock != TCPIP_INVALID_SOCKET) &&
            (TcpIp_IsSocketOpen(rt->listenSock) != 0u))
    {
        TcpIp_Close(rt->listenSock);
    }

    rt->listenSock = TCPIP_INVALID_SOCKET;
    rt->activeSock = TCPIP_INVALID_SOCKET;
    rt->remoteAddr.addr = 0u;
    rt->remoteAddr.port = 0u;
    rt->state = SOAD_SOCON_CLOSED;
    SoAd_DebugState[id] = (uint8)rt->state;
    SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], TCPIP_INVALID_SOCKET);
    rt->cfg = 0;
    rt->requestedOpen = reopen;
    SOAD_DEBUG_ASSIGN(SoAd_DebugRequestedOpen[id], reopen);
    if ((reopen != 0u) &&
            (((cfg != 0) && (cfg->isServer != 0u)) ||
                    (TcpIp_IsLinkAvailable() != 0u)))
    {
        (void)SoAd_OpenSoCon(id);
    }
}

void SoAd_Init(const SoAd_ConfigType *cfg)
{
    uint8 i;

    SoAd_InitLock();
    SoAd_Cfg = cfg;

    for (i = 0u; i < SOAD_MAX_CONNECTIONS; i++)
    {
        SoAd_Runtime[i].state = SOAD_SOCON_CLOSED;
        SoAd_Runtime[i].listenSock = TCPIP_INVALID_SOCKET;
        SoAd_Runtime[i].activeSock = TCPIP_INVALID_SOCKET;
        SoAd_Runtime[i].remoteAddr.addr = 0u;
        SoAd_Runtime[i].remoteAddr.port = 0u;
        SoAd_Runtime[i].cfg = 0;
        SoAd_Runtime[i].requestedOpen = 0u;
        SoAd_DebugState[i] = SOAD_SOCON_CLOSED;
        SOAD_DEBUG_ASSIGN(SoAd_DebugRequestedOpen[i], 0u);
        SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[i], TCPIP_INVALID_SOCKET);
        SOAD_DEBUG_ASSIGN(SoAd_DebugTcpLastAcceptedSocket[i], TCPIP_INVALID_SOCKET);
        SOAD_DEBUG_ASSIGN(SoAd_DebugLastOpenResult[i], 0u);
        SOAD_DEBUG_ASSIGN(SoAd_DebugRxCounter[i], 0u);
        SOAD_DEBUG_ASSIGN(SoAd_DebugRxDropCounter[i], 0u);
        SOAD_DEBUG_ASSIGN(SoAd_DebugLastRxLength[i], 0u);
        SOAD_DEBUG_ASSIGN(SoAd_DebugLastRxRemotePort[i], 0u);
        SOAD_DEBUG_ASSIGN(SoAd_DebugLastRxRemoteAddr[i], 0u);
    }

    SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxSoConId, 0u);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxResult, SOAD_NOT_OK);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxTcpIpResult, 0);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxLength, 0u);
    SOAD_DEBUG_ASSIGN(SoAd_DebugPcHeartbeatRxCounter, 0u);
    SOAD_DEBUG_ASSIGN(SoAd_DebugPcHeartbeatAckCounter, 0u);
    SOAD_DEBUG_ASSIGN(SoAd_DebugPcHeartbeatLastRemoteAddr, 0u);
    SOAD_DEBUG_ASSIGN(SoAd_DebugPcHeartbeatLastRemotePort, 0u);
    SOAD_DEBUG_ASSIGN(SoAd_DebugSocketLossCounter, 0u);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastSocketLossSoConId, 0u);
}

uint8 SoAd_OpenSoCon(SoAd_SoConIdType id)
{
    SoAd_SoConRuntimeType *rt;
    const SoAd_SocketConnectionConfigType *cfg;
    uint8 isTcp;

    if (SoAd_Lock() == 0u)
    {
        return 0u;
    }

    if (id >= SOAD_MAX_CONNECTIONS)
    {
        SoAd_Unlock();
        return 0u;
    }

    cfg = SoAd_FindConfig(id);

    if (cfg == 0)
    {
        SoAd_Unlock();
        return 0u;
    }

    rt = &SoAd_Runtime[id];
    rt->requestedOpen = 1u;
    SOAD_DEBUG_ASSIGN(SoAd_DebugRequestedOpen[id], 1u);
    if (rt->state != SOAD_SOCON_CLOSED)
    {
        SOAD_DEBUG_ASSIGN(SoAd_DebugLastOpenResult[id], 1u);
        SoAd_Unlock();
        return 1u;
    }

    if ((cfg->isServer == 0u) && (TcpIp_IsLinkAvailable() == 0u))
    {
        SoAd_OpenFailNoLinkCounter++;
        SOAD_DEBUG_ASSIGN(SoAd_DebugLastOpenResult[id], 0u);
        EthernetDiag_ReportSocketOpenFailed(EthernetDiag_GetConnectionIdForSoCon(id));
        SoAd_Unlock();
        return 0u;
    }

    rt->cfg = cfg;
    isTcp = (cfg->protocol == TCPIP_PROTOCOL_TCP) ? 1u : 0u;

    rt->listenSock = TcpIp_Create(isTcp);

    if (rt->listenSock == TCPIP_INVALID_SOCKET)
    {
        SoAd_OpenFailCreateCounter++;
        EthernetDiag_ReportSocketOpenFailed(EthernetDiag_GetConnectionIdForSoCon(id));
        SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], TCPIP_INVALID_SOCKET);
        SOAD_DEBUG_ASSIGN(SoAd_DebugLastOpenResult[id], 0u);
        SoAd_Unlock(); // @suppress("Unused static function")
        return 0u;
    }
    SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], rt->listenSock);
    /* Server sockets must bind to INADDR_ANY.  Binding UDP sockets to the
     * exact ECU unicast address prevents delivery of subnet-directed
     * broadcasts such as 192.168.1.255:30600 and makes lab bring-up fragile
     * when the PC address changes.  The configured local address is still kept
     * as the ECU identity; only the socket bind address is widened.
     */
    if (TcpIp_BindAddr(rt->listenSock,
            (cfg->isServer != 0u) ? 0u : SoAd_Ip4ToU32(cfg->localAddr.addr),
                    cfg->localAddr.port) != 0)
    {
        SoAd_OpenFailBindCounter++;
        EthernetDiag_ReportSocketOpenFailed(EthernetDiag_GetConnectionIdForSoCon(id));
        TcpIp_Close(rt->listenSock);
        rt->listenSock = TCPIP_INVALID_SOCKET;
        SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], TCPIP_INVALID_SOCKET);
        SOAD_DEBUG_ASSIGN(SoAd_DebugLastOpenResult[id], 0u);
        SoAd_Unlock();
        return 0u;
    }

    if (isTcp != 0u)
    {
        rt->activeSock = TCPIP_INVALID_SOCKET;

        if (TcpIp_Listen(rt->listenSock) != 0)
        {
            EthernetDiag_ReportSocketOpenFailed(EthernetDiag_GetConnectionIdForSoCon(id));
            TcpIp_Close(rt->listenSock);
            rt->listenSock = TCPIP_INVALID_SOCKET;
            SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], TCPIP_INVALID_SOCKET);
            SOAD_DEBUG_ASSIGN(SoAd_DebugLastOpenResult[id], 0u);
            SoAd_Unlock();
            return 0u;
        }
    }
    else
    {
        rt->activeSock = rt->listenSock;
        rt->remoteAddr.addr = SoAd_Ip4ToU32(cfg->remoteAddr.addr);
        rt->remoteAddr.port = cfg->remoteAddr.port;
    }

    rt->state = SOAD_SOCON_OPEN;
    SoAd_DebugState[id] = (uint8)rt->state;
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastOpenResult[id], 1u);
    SoAd_Unlock();
    return 1u;
}

void SoAd_CloseSoCon(SoAd_SoConIdType id)
{
    SoAd_SoConRuntimeType *rt;

    if (SoAd_Lock() == 0u)
    {
        return;
    }

    if (id >= SOAD_MAX_CONNECTIONS)
    {
        SoAd_Unlock();
        return;
    }

    rt = &SoAd_Runtime[id];
    EthernetDiag_ReportSocketCloseRequested(EthernetDiag_GetConnectionIdForSoCon(id));

    if ((rt->activeSock != TCPIP_INVALID_SOCKET) &&
            (rt->activeSock != rt->listenSock))
    {
        TcpIp_Close(rt->activeSock);
    }

    if (rt->listenSock != TCPIP_INVALID_SOCKET)
    {
        TcpIp_Close(rt->listenSock);
    }

    if ((rt->cfg != 0) && (rt->cfg->tcpDisconnected != 0))
    {
        rt->cfg->tcpDisconnected(id);
    }
    EthernetDiag_ReportSocketClosed(EthernetDiag_GetConnectionIdForSoCon(id),
            ETHERNETDIAG_CLOSE_INTENTIONAL);

    rt->listenSock = TCPIP_INVALID_SOCKET;
    rt->activeSock = TCPIP_INVALID_SOCKET;
    rt->state = SOAD_SOCON_CLOSED;
    SoAd_DebugState[id] = (uint8)rt->state;
    SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], TCPIP_INVALID_SOCKET);
    rt->cfg = 0;
    rt->requestedOpen = 0u;
    SOAD_DEBUG_ASSIGN(SoAd_DebugRequestedOpen[id], 0u);
    SoAd_Unlock();
}

static uint8 SoAd_TryReplaceStaleTcpClient(SoAd_SoConRuntimeType *rt, SoAd_SoConIdType id)
{
    TcpIp_SockAddrType newRemote;
    TcpIp_SocketIdType newSock;

    if ((rt == 0) || (rt->cfg == 0) || (rt->cfg->protocol != TCPIP_PROTOCOL_TCP))
    {
        return 0u;
    }

    if (rt->listenSock == TCPIP_INVALID_SOCKET)
    {
        return 0u;
    }

    newSock = TcpIp_Accept(rt->listenSock, &newRemote);

    if (newSock < 0)
    {
        if (SoAd_IsTcpFatalError(TcpIp_LastSocketError) != 0u)
        {
            SoAd_HandleSocketLoss(id);
        }
        return 0u;
    }

    if ((rt->activeSock != TCPIP_INVALID_SOCKET) &&
            (rt->activeSock != rt->listenSock))
    {
        TcpIp_Close(rt->activeSock);
    }

    if (rt->cfg->tcpDisconnected != 0)
    {
        rt->cfg->tcpDisconnected(id);
    }

    rt->activeSock = newSock;
    rt->remoteAddr = newRemote;
    rt->state = SOAD_SOCON_CONNECTED;

    SoAd_DebugState[id] = (uint8)rt->state;
    SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], rt->listenSock);
    SOAD_DEBUG_ASSIGN(SoAd_DebugTcpLastAcceptedSocket[id], newSock);
    SOAD_DEBUG_INC(SoAd_DebugTcpStaleReplaceCounter);
    if (rt->cfg->tcpConnected != 0)
    {
        rt->cfg->tcpConnected(id);
    }
    EthernetDiag_ReportSocketConnected(EthernetDiag_GetConnectionIdForSoCon(id));

    return 1u;
}

static void SoAd_DispatchTcpRx(SoAd_SoConRuntimeType *rt,
        SoAd_SoConIdType id,
        const uint8 *buffer,
        uint16 len)
{
    if ((rt == 0) || (rt->cfg == 0) || (buffer == 0) || (len == 0u))
    {
        return;
    }

    SOAD_DEBUG_INC(SoAd_DebugRxCounter[id]);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastRxLength[id], len);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastRxRemoteAddr[id], rt->remoteAddr.addr);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastRxRemotePort[id], rt->remoteAddr.port);
    if ((rt->cfg->rxIndication != 0) &&
            ((rt->cfg->upperLayer == SOAD_UPPER_DOIP) ||
                    (ComM_IsRxAllowed(COMM_CH_ETH) != FALSE)))
    {
        rt->cfg->rxIndication(id, &rt->remoteAddr, buffer, len);
        SysMgr_NotifyBusActivity();
    }
}

void SoAd_MainFunction(void)
{
    uint8 id;
    uint8 buffer[SOAD_RX_BUFFER_SIZE];
    sint32 len;
    TcpIp_SockAddrType remote;
    SoAd_SoConRuntimeType *rt;
    const SoAd_SocketConnectionConfigType *cfg;

    if (SoAd_Lock() == 0u)
    {
        return;
    }

    for (id = 0u; id < SOAD_MAX_CONNECTIONS; id++)
    {
        rt = &SoAd_Runtime[id];

        if (rt->state == SOAD_SOCON_CLOSED)
        {
            cfg = (rt->cfg != 0) ? rt->cfg : SoAd_FindConfig(id);

            if ((rt->requestedOpen != 0u) &&
                    (((cfg != 0) && (cfg->isServer != 0u)) ||
                            (TcpIp_IsLinkAvailable() != 0u)))
            {
                (void)SoAd_OpenSoCon(id);
            }
            continue;
        }

        if (rt->cfg == 0)
        {
            continue;
        }

        if (SoAd_RuntimeSocketLost(rt) != 0u)
        {
            SoAd_HandleSocketLoss(id);
            continue;
        }

        if (rt->cfg->protocol == TCPIP_PROTOCOL_TCP)
        {
            if (rt->state == SOAD_SOCON_OPEN)
            {
                rt->activeSock = TcpIp_Accept(rt->listenSock, &rt->remoteAddr);

                if (rt->activeSock >= 0)
                {
                    rt->state = SOAD_SOCON_CONNECTED;
                    SoAd_DebugState[id] = (uint8)rt->state;
                    SOAD_DEBUG_ASSIGN(SoAd_DebugSocket[id], rt->listenSock);
                    SOAD_DEBUG_ASSIGN(SoAd_DebugTcpLastAcceptedSocket[id], rt->activeSock);
                    SOAD_DEBUG_ASSIGN(SoAd_DebugLastRxRemoteAddr[id], rt->remoteAddr.addr);
                    SOAD_DEBUG_ASSIGN(SoAd_DebugLastRxRemotePort[id], rt->remoteAddr.port);
                    if (rt->cfg->tcpConnected != 0)
                    {
                        rt->cfg->tcpConnected(id);
                    }
                    EthernetDiag_ReportSocketConnected(EthernetDiag_GetConnectionIdForSoCon(id));

                    {
                        uint8 budget = SOAD_RX_DRAIN_BUDGET_PER_SOCON;

                        do
                        {
                            len = TcpIp_Recv(rt->activeSock, buffer, (uint16)sizeof(buffer));

                            if (len == 0)
                            {
                                EthernetDiag_ReportSocketClosed(EthernetDiag_GetConnectionIdForSoCon(id),
                                        ETHERNETDIAG_CLOSE_PEER_FIN);
                                SoAd_TcpDisconnectToOpen(rt, id);
                            }
                            else if (len < 0)
                            {
                                if (SoAd_IsTcpFatalError(TcpIp_LastSocketError) != 0u)
                                {
                                    EthernetDiag_ReportSocketClosed(EthernetDiag_GetConnectionIdForSoCon(id),
                                            SoAd_ToDiagCloseReason(TcpIp_LastSocketError, 0u));
                                    SoAd_TcpDisconnectToOpen(rt, id);
                                }
                            }
                            else
                            {
                                EthernetDiag_ReportRxActivity(EthernetDiag_GetConnectionIdForSoCon(id));
                                SoAd_DispatchTcpRx(rt, id, buffer, (uint16)len);
                            }

                            if (budget > 0u)
                            {
                                budget--;
                            }
                        } while ((rt->state == SOAD_SOCON_CONNECTED) &&
                                (len > 0) &&
                                (budget > 0u));
                    }
                }
                else if (SoAd_IsTcpFatalError(TcpIp_LastSocketError) != 0u)
                {
                    SoAd_HandleSocketLoss(id);
                }
            }
            else if (rt->state == SOAD_SOCON_CONNECTED)
            {
                uint8 budget = SOAD_RX_DRAIN_BUDGET_PER_SOCON;

                (void)SoAd_TryReplaceStaleTcpClient(rt, id);

                if (rt->state != SOAD_SOCON_CONNECTED)
                {
                    continue;
                }

                do
                {
                    len = TcpIp_Recv(rt->activeSock, buffer, (uint16)sizeof(buffer));

                    if (len == 0)
                    {
                        EthernetDiag_ReportSocketClosed(EthernetDiag_GetConnectionIdForSoCon(id),
                                ETHERNETDIAG_CLOSE_PEER_FIN);
                        SoAd_TcpDisconnectToOpen(rt, id);
                    }
                    else if (len < 0)
                    {
                        if (SoAd_IsTcpFatalError(TcpIp_LastSocketError) != 0u)
                        {
                            EthernetDiag_ReportSocketClosed(EthernetDiag_GetConnectionIdForSoCon(id),
                                    SoAd_ToDiagCloseReason(TcpIp_LastSocketError, 0u));
                            SoAd_TcpDisconnectToOpen(rt, id);
                        }
                    }
                    else
                    {
                        EthernetDiag_ReportRxActivity(EthernetDiag_GetConnectionIdForSoCon(id));
                        SoAd_DispatchTcpRx(rt, id, buffer, (uint16)len);
                    }

                    if (budget > 0u)
                    {
                        budget--;
                    }
                } while ((rt->state == SOAD_SOCON_CONNECTED) &&
                        (len > 0) &&
                        (budget > 0u));
            }
        }
        else
        {
            uint8 budget = SOAD_RX_DRAIN_BUDGET_PER_SOCON;

            do
            {
                len = TcpIp_RecvFrom(rt->listenSock, &remote, buffer, (uint16)sizeof(buffer));

                if (len > 0)
                {
                    SOAD_DEBUG_INC(SoAd_DebugRxCounter[id]);
                    SOAD_DEBUG_ASSIGN(SoAd_DebugLastRxLength[id], (uint16)len);
                    SOAD_DEBUG_ASSIGN(SoAd_DebugLastRxRemoteAddr[id], remote.addr);
                    SOAD_DEBUG_ASSIGN(SoAd_DebugLastRxRemotePort[id], remote.port);
                    if (rt->cfg->useConfiguredRemote == 0u)
                    {
                        rt->remoteAddr = remote;
                    }

                    if (SoAd_HandlePcHeartbeat(rt, id, &remote, buffer, (uint16)len) != 0u)
                    {
                        EthernetDiag_ReportRxActivity(EthernetDiag_GetConnectionIdForSoCon(id));
                        SysMgr_NotifyBusActivity();
                    }
                    else if ((rt->cfg->rxIndication != 0) &&
                            ((rt->cfg->upperLayer == SOAD_UPPER_DOIP) ||
                                    (ComM_IsRxAllowed(COMM_CH_ETH) != FALSE)))
                    {
                        EthernetDiag_ReportRxActivity(EthernetDiag_GetConnectionIdForSoCon(id));
                        rt->cfg->rxIndication(id, &remote, buffer, (uint16)len);
                        SysMgr_NotifyBusActivity();
                    }
                }
                else if ((len < 0) && (SoAd_IsTcpFatalError(TcpIp_LastSocketError) != 0u))
                {
                    SoAd_HandleSocketLoss(id);
                    break;
                }

                if (budget > 0u)
                {
                    budget--;
                }
            } while ((len > 0) && (budget > 0u));
        }
    }

    SoAd_MainFunction_Counter++;
    SoAd_Unlock();
}

Std_ReturnType SoAd_GetDiagSnapshot(SoAd_SoConIdType id, SoAd_DiagSnapshotType *snapshot)
{
    const SoAd_SoConRuntimeType *rt;

    if ((snapshot == 0) || (id >= SOAD_MAX_CONNECTIONS) || (SoAd_Cfg == 0) ||
            (id >= SoAd_Cfg->numConnections))
    {
        return E_NOT_OK;
    }

    if (SoAd_Lock() == 0u)
    {
        return E_NOT_OK;
    }

    rt = &SoAd_Runtime[id];
    if (rt->cfg == 0)
    {
        SoAd_Unlock();
        return E_NOT_OK;
    }

    snapshot->state = rt->state;
    snapshot->listenSock = rt->listenSock;
    snapshot->activeSock = rt->activeSock;
    snapshot->localAddr.addr = SoAd_Ip4ToU32(rt->cfg->localAddr.addr);
    snapshot->localAddr.port = rt->cfg->localAddr.port;
    snapshot->remoteAddr = rt->remoteAddr;
    snapshot->requestedOpen = rt->requestedOpen;
    snapshot->protocol = (uint8)rt->cfg->protocol;
    snapshot->upperLayer = (uint8)rt->cfg->upperLayer;

    SoAd_Unlock();
    return E_OK;
}

sint32 SoAd_Send(SoAd_SoConIdType id, const uint8 *data, uint16 len)
{
    return (GatewaySwc_RequestSoAdIfTransmit(id, 0, data, len) == SOAD_OK) ? (sint32)len : -1;
}

SoAd_ReturnType SoAd_IfTransmit(SoAd_SoConIdType id,
        const TcpIp_SockAddrType *remoteAddr,
        const uint8 *data,
        uint16 len)
{
    SoAd_SoConRuntimeType *rt;
    const TcpIp_SockAddrType *dst;
    sint32 tcpIpResult;
    SoAd_ReturnType soAdResult;

    SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxSoConId, id);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxLength, len);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxTcpIpResult, -1);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxResult, SOAD_NOT_OK);
    if (SoAd_Lock() == 0u)
    {
        return SOAD_NOT_OK;
    }

    if ((id >= SOAD_MAX_CONNECTIONS) || (data == 0) || (len == 0u))
    {
        SoAd_Unlock();
        return SOAD_NOT_OK;
    }

    rt = &SoAd_Runtime[id];

    if ((rt->cfg == 0) || (rt->state == SOAD_SOCON_CLOSED))
    {
        SoAd_Unlock();
        return SOAD_NOT_OK;
    }

    if ((rt->cfg->upperLayer != SOAD_UPPER_DOIP) &&
            (ComM_IsTxAllowed(COMM_CH_ETH) == FALSE))
    {
        SoAd_Unlock();
        return SOAD_NOT_OK;
    }

    if (rt->cfg->protocol == TCPIP_PROTOCOL_TCP)
    {
        if ((rt->state != SOAD_SOCON_CONNECTED) || (rt->activeSock == TCPIP_INVALID_SOCKET))
        {
            SoAd_Unlock();
            return SOAD_NOT_OK;
        }

        tcpIpResult = TcpIp_Send(rt->activeSock, data, len);
        soAdResult = (tcpIpResult == (sint32)len) ? SOAD_OK : SOAD_NOT_OK;
        SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxTcpIpResult, tcpIpResult);
        SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxResult, (uint8)soAdResult);
        if ((soAdResult != SOAD_OK) &&
                (TcpIp_IsSocketOpen(rt->activeSock) == 0u))
        {
            EthernetDiag_ReportSocketClosed(EthernetDiag_GetConnectionIdForSoCon(id),
                    ETHERNETDIAG_CLOSE_SEND_FAILURE);
            SoAd_TcpDisconnectToOpen(rt, id);
        }
        else if (soAdResult == SOAD_OK)
        {
            EthernetDiag_ReportTxActivity(EthernetDiag_GetConnectionIdForSoCon(id));
        }
        else
        {
            EthernetDiag_ReportTxError(1u);
        }

        SoAd_Unlock();
        return soAdResult;
    }

    if (rt->listenSock == TCPIP_INVALID_SOCKET)
    {
        SoAd_Unlock();
        return SOAD_NOT_OK;
    }

    dst = (remoteAddr != 0) ? remoteAddr : &rt->remoteAddr;

    tcpIpResult = TcpIp_SendTo(rt->listenSock, dst, data, len);
    soAdResult = (tcpIpResult == (sint32)len) ? SOAD_OK : SOAD_NOT_OK;
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxTcpIpResult, tcpIpResult);
    SOAD_DEBUG_ASSIGN(SoAd_DebugLastTxResult, (uint8)soAdResult);
    if (soAdResult == SOAD_OK)
    {
        EthernetDiag_ReportTxActivity(EthernetDiag_GetConnectionIdForSoCon(id));
    }
    else
    {
        EthernetDiag_ReportTxError(1u);
    }
    SoAd_Unlock();
    return soAdResult;
}
