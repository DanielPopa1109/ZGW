#include "SoAd.h"

typedef struct
{
    SoAd_StateType state;
    TcpIp_SocketIdType listenSock;
    TcpIp_SocketIdType activeSock;
    TcpIp_SockAddrType remoteAddr;
    const SoAd_SocketConnectionConfigType *cfg;
} SoAd_SoConRuntimeType;

static SoAd_SoConRuntimeType SoAd_Runtime[SOAD_MAX_CONNECTIONS];
static const SoAd_ConfigType *SoAd_Cfg;

static uint32 SoAd_Ip4ToU32(const uint8 ip[4])
{
    return ((uint32)ip[0] << 24u) |
           ((uint32)ip[1] << 16u) |
           ((uint32)ip[2] << 8u) |
           ((uint32)ip[3]);
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

void SoAd_Init(const SoAd_ConfigType *cfg)
{
    uint8 i;

    SoAd_Cfg = cfg;

    for (i = 0u; i < SOAD_MAX_CONNECTIONS; i++)
    {
        SoAd_Runtime[i].state = SOAD_SOCON_CLOSED;
        SoAd_Runtime[i].listenSock = TCPIP_INVALID_SOCKET;
        SoAd_Runtime[i].activeSock = TCPIP_INVALID_SOCKET;
        SoAd_Runtime[i].remoteAddr.addr = 0u;
        SoAd_Runtime[i].remoteAddr.port = 0u;
        SoAd_Runtime[i].cfg = 0;
    }
}

uint8 SoAd_OpenSoCon(SoAd_SoConIdType id)
{
    SoAd_SoConRuntimeType *rt;
    const SoAd_SocketConnectionConfigType *cfg;
    uint8 isTcp;

    if (id >= SOAD_MAX_CONNECTIONS)
    {
        return 0u;
    }

    cfg = SoAd_FindConfig(id);

    if (cfg == 0)
    {
        return 0u;
    }

    rt = &SoAd_Runtime[id];

    if (rt->state != SOAD_SOCON_CLOSED)
    {
        return 1u;
    }

    rt->cfg = cfg;
    isTcp = (cfg->protocol == TCPIP_PROTOCOL_TCP) ? 1u : 0u;

    rt->listenSock = TcpIp_Create(isTcp);

    if (rt->listenSock == TCPIP_INVALID_SOCKET)
    {
        return 0u;
    }

    if (TcpIp_Bind(rt->listenSock, cfg->localAddr.port) != 0)
    {
        TcpIp_Close(rt->listenSock);
        rt->listenSock = TCPIP_INVALID_SOCKET;
        return 0u;
    }

    if (isTcp != 0u)
    {
        if (TcpIp_Listen(rt->listenSock) != 0)
        {
            TcpIp_Close(rt->listenSock);
            rt->listenSock = TCPIP_INVALID_SOCKET;
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

    return 1u;
}

void SoAd_CloseSoCon(SoAd_SoConIdType id)
{
    SoAd_SoConRuntimeType *rt;

    if (id >= SOAD_MAX_CONNECTIONS)
    {
        return;
    }

    rt = &SoAd_Runtime[id];

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

    rt->listenSock = TCPIP_INVALID_SOCKET;
    rt->activeSock = TCPIP_INVALID_SOCKET;
    rt->state = SOAD_SOCON_CLOSED;
    rt->cfg = 0;
}

void SoAd_MainFunction(void)
{
    uint8 id;
    uint8 buffer[512];
    sint32 len;
    TcpIp_SockAddrType remote;
    SoAd_SoConRuntimeType *rt;

    for (id = 0u; id < SOAD_MAX_CONNECTIONS; id++)
    {
        rt = &SoAd_Runtime[id];

        if ((rt->cfg == 0) || (rt->state == SOAD_SOCON_CLOSED))
        {
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

                    if (rt->cfg->tcpConnected != 0)
                    {
                        rt->cfg->tcpConnected(id);
                    }
                }
            }
            else if (rt->state == SOAD_SOCON_CONNECTED)
            {
                len = TcpIp_Recv(rt->activeSock, buffer, (uint16)sizeof(buffer));

                if (len == 0)
                {
                    if (rt->cfg->tcpDisconnected != 0)
                    {
                        rt->cfg->tcpDisconnected(id);
                    }

                    TcpIp_Close(rt->activeSock);
                    rt->activeSock = TCPIP_INVALID_SOCKET;
                    rt->state = SOAD_SOCON_OPEN;
                }
                else if (len > 0)
                {
                    if (rt->cfg->rxIndication != 0)
                    {
                        rt->cfg->rxIndication(id, &rt->remoteAddr, buffer, (uint16)len);
                    }
                }
            }
        }
        else
        {
            len = TcpIp_RecvFrom(rt->listenSock, &remote, buffer, (uint16)sizeof(buffer));

            if (len > 0)
            {
                rt->remoteAddr = remote;

                if (rt->cfg->rxIndication != 0)
                {
                    rt->cfg->rxIndication(id, &remote, buffer, (uint16)len);
                }
            }
        }
    }
}

sint32 SoAd_Send(SoAd_SoConIdType id, const uint8 *data, uint16 len)
{
    return (SoAd_IfTransmit(id, 0, data, len) == SOAD_OK) ? (sint32)len : -1;
}

SoAd_ReturnType SoAd_IfTransmit(SoAd_SoConIdType id,
                                const TcpIp_SockAddrType *remoteAddr,
                                const uint8 *data,
                                uint16 len)
{
    SoAd_SoConRuntimeType *rt;
    const TcpIp_SockAddrType *dst;

    if ((id >= SOAD_MAX_CONNECTIONS) || (data == 0) || (len == 0u))
    {
        return SOAD_NOT_OK;
    }

    rt = &SoAd_Runtime[id];

    if ((rt->cfg == 0) || (rt->state == SOAD_SOCON_CLOSED))
    {
        return SOAD_NOT_OK;
    }

    if (rt->cfg->protocol == TCPIP_PROTOCOL_TCP)
    {
        if ((rt->state != SOAD_SOCON_CONNECTED) || (rt->activeSock == TCPIP_INVALID_SOCKET))
        {
            return SOAD_NOT_OK;
        }

        return (TcpIp_Send(rt->activeSock, data, len) == (sint32)len) ? SOAD_OK : SOAD_NOT_OK;
    }

    if (rt->listenSock == TCPIP_INVALID_SOCKET)
    {
        return SOAD_NOT_OK;
    }

    dst = (remoteAddr != 0) ? remoteAddr : &rt->remoteAddr;

    return (TcpIp_SendTo(rt->listenSock, dst, data, len) == (sint32)len) ? SOAD_OK : SOAD_NOT_OK;
}
