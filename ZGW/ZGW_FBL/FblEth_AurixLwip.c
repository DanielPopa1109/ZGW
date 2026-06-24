#include <string.h>
#include "Ifx_Types.h"

#include "lwip_geth.h"
#include "lwip_geth_lwip.h"
#include "lwip_geth_conf.h"

#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"

#define DOIP_TCP_PORT              13400u
#define DOIP_UDP_PORT              13400u
#define FBL_ETH_UDP_RX_QUEUE_DEPTH 4u
#define FBL_ETH_UDP_RX_BUF_SIZE    1600u
#define FBL_ETH_TCP_RX_STREAM_SIZE 16384u
#define FBL_ETH_TCP_RX_CHUNK_SIZE  1600u
#define FBL_ETH_TCP_SEND_RETRY_MAX 4u

extern void Fbl_DoIpTcpConnected(void);
extern void Fbl_DoIpTcpDisconnected(void);
extern void Fbl_DoIpTcpRxOverflow(void);

typedef struct
{
    uint8 data[FBL_ETH_UDP_RX_BUF_SIZE];
    uint16 len;
    ip_addr_t remoteIp;
    uint16 remotePort;
    uint8 remoteValid;
} FblEth_RxItemType;

static struct tcp_pcb *g_tcpListenPcb;
static struct tcp_pcb *g_tcpActivePcb;
static struct udp_pcb *g_udpPcb;

static uint8 g_tcpStream[FBL_ETH_TCP_RX_STREAM_SIZE];
static volatile uint16 g_tcpStreamLen;

static FblEth_RxItemType g_udpQ[FBL_ETH_UDP_RX_QUEUE_DEPTH];
static volatile uint8 g_udpWr;
static volatile uint8 g_udpRd;
static volatile uint8 g_udpCnt;

static ip_addr_t g_udpRemoteIp;
static uint16 g_udpRemotePort;
static uint8 g_udpRemoteValid;

static uint8 g_ethInitDone;

static void FblEth_CopyPbuf(uint8 *dst, uint16 *dstLen, uint16 maxLen, const struct pbuf *p)
{
    const struct pbuf *q;
    uint16 copied = 0u;

    for(q = p; q != NULL; q = q->next)
    {
        uint16 chunk = q->len;

        if((copied + chunk) > maxLen)
        {
            chunk = (uint16)(maxLen - copied);
        }

        if(chunk > 0u)
        {
            memcpy(&dst[copied], q->payload, chunk);
            copied = (uint16)(copied + chunk);
        }

        if(copied >= maxLen)
        {
            break;
        }
    }

    *dstLen = copied;
}

static void FblEth_ResetTcpStream(void)
{
    g_tcpStreamLen = 0u;
    memset(g_tcpStream, 0u, sizeof(g_tcpStream));
}

static uint8 FblEth_QueueTcp(const struct pbuf *p)
{
    const struct pbuf *q;
    uint16 copied = g_tcpStreamLen;

    if(p == NULL)
    {
        return 0u;
    }

    if((uint32)g_tcpStreamLen + (uint32)p->tot_len > (uint32)sizeof(g_tcpStream))
    {
        FblEth_ResetTcpStream();
        return 0u;
    }

    for(q = p; q != NULL; q = q->next)
    {
        if(q->len > 0u)
        {
            memcpy(&g_tcpStream[copied], q->payload, q->len);
            copied = (uint16)(copied + q->len);
        }
    }

    g_tcpStreamLen = copied;

    return 1u;
}

static uint8 FblEth_QueueUdp(const struct pbuf *p, const ip_addr_t *addr, uint16 port)
{
    if((p == NULL) || (addr == NULL) || (g_udpCnt >= FBL_ETH_UDP_RX_QUEUE_DEPTH))
    {
        return 0u;
    }

    FblEth_CopyPbuf(g_udpQ[g_udpWr].data, &g_udpQ[g_udpWr].len, FBL_ETH_UDP_RX_BUF_SIZE, p);
    ip_addr_copy(g_udpQ[g_udpWr].remoteIp, *addr);
    g_udpQ[g_udpWr].remotePort = port;
    g_udpQ[g_udpWr].remoteValid = 1u;

    g_udpWr = (uint8)((g_udpWr + 1u) % FBL_ETH_UDP_RX_QUEUE_DEPTH);
    g_udpCnt++;

    return 1u;
}

static err_t FblEth_TcpRecvCb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    (void)arg;

    if((err != ERR_OK) || (tpcb == NULL))
    {
        if(p != NULL)
        {
            pbuf_free(p);
        }

        return ERR_OK;
    }

    if(p == NULL)
    {
        if(g_tcpActivePcb == tpcb)
        {
            g_tcpActivePcb = NULL;
            FblEth_ResetTcpStream();
            Fbl_DoIpTcpDisconnected();
        }

        tcp_arg(tpcb, NULL);
        tcp_recv(tpcb, NULL);
        tcp_err(tpcb, NULL);
        tcp_close(tpcb);

        return ERR_OK;
    }

    if(FblEth_QueueTcp(p) == 0u)
    {
        Fbl_DoIpTcpRxOverflow();
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    return ERR_OK;
}

static void FblEth_TcpErrCb(void *arg, err_t err)
{
    (void)arg;
    (void)err;

    g_tcpActivePcb = NULL;
    FblEth_ResetTcpStream();
    Fbl_DoIpTcpDisconnected();
}

static err_t FblEth_TcpAcceptCb(void *arg, struct tcp_pcb *newPcb, err_t err)
{
    (void)arg;

    if((err != ERR_OK) || (newPcb == NULL))
    {
        return ERR_VAL;
    }

    /* A tester reconnecting must always win. The previous connection may be a
     * half-open ESTABLISHED PCB left behind by a tool that vanished without a
     * FIN/RST; refusing the new connection (the old behaviour: tcp_abort(newPcb))
     * would wedge the bootloader's single DoIP slot until a power cycle. Drop the
     * stale PCB and take the new one instead. */
    if(g_tcpActivePcb != NULL)
    {
        struct tcp_pcb *stale = g_tcpActivePcb;

        g_tcpActivePcb = NULL;
        tcp_arg(stale, NULL);
        tcp_recv(stale, NULL);
        tcp_err(stale, NULL);
        tcp_abort(stale);
    }

    g_tcpActivePcb = newPcb;
    FblEth_ResetTcpStream();

    tcp_setprio(newPcb, TCP_PRIO_NORMAL);
    tcp_arg(newPcb, NULL);
    tcp_recv(newPcb, FblEth_TcpRecvCb);
    tcp_err(newPcb, FblEth_TcpErrCb);

    /* Detect a tester that disappears mid-session so the slot frees itself even
     * with no reconnect attempt: probe after 8 s idle, then 3 probes 2 s apart. */
    newPcb->so_options |= SOF_KEEPALIVE;
    newPcb->keep_idle = 8000u;
    newPcb->keep_intvl = 2000u;
    newPcb->keep_cnt = 3u;

#if TCP_NODELAY
    tcp_nagle_disable(newPcb);
#endif

    Fbl_DoIpTcpConnected();

    return ERR_OK;
}

static void FblEth_UdpRecvCb(void *arg,
                             struct udp_pcb *upcb,
                             struct pbuf *p,
                             const ip_addr_t *addr,
                             u16_t port)
{
    (void)arg;
    (void)upcb;

    if(p == NULL)
    {
        return;
    }

    if(addr == NULL)
    {
        pbuf_free(p);
        return;
    }

    (void)FblEth_QueueUdp(p, addr, port);

    pbuf_free(p);
}

static void FblEth_StartTcpServer(void)
{
    err_t err;

    g_tcpListenPcb = tcp_new_ip_type(IPADDR_TYPE_ANY);

    if(g_tcpListenPcb == NULL)
    {
        return;
    }

#if SO_REUSE
    g_tcpListenPcb->so_options |= SOF_REUSEADDR;
#endif

    err = tcp_bind(g_tcpListenPcb, IP_ANY_TYPE, DOIP_TCP_PORT);

    if(err != ERR_OK)
    {
        tcp_close(g_tcpListenPcb);
        g_tcpListenPcb = NULL;
        return;
    }

    g_tcpListenPcb = tcp_listen(g_tcpListenPcb);

    if(g_tcpListenPcb == NULL)
    {
        return;
    }

    tcp_accept(g_tcpListenPcb, FblEth_TcpAcceptCb);
}

static void FblEth_StartUdpServer(void)
{
    err_t err;

    g_udpPcb = udp_new_ip_type(IPADDR_TYPE_ANY);

    if(g_udpPcb == NULL)
    {
        return;
    }

#if SO_REUSE
    g_udpPcb->so_options |= SOF_REUSEADDR;
#endif

    err = udp_bind(g_udpPcb, IP_ANY_TYPE, DOIP_UDP_PORT);

    if(err != ERR_OK)
    {
        udp_remove(g_udpPcb);
        g_udpPcb = NULL;
        return;
    }

    udp_recv(g_udpPcb, FblEth_UdpRecvCb, NULL);
}

void FblEth_Init(void)
{
    if(g_ethInitDone != 0u)
    {
        return;
    }

    FblEth_ResetTcpStream();
    memset(g_udpQ, 0u, sizeof(g_udpQ));

    g_udpWr = 0u;
    g_udpRd = 0u;
    g_udpCnt = 0u;

    g_udpRemoteValid = 0u;
    g_tcpListenPcb = NULL;
    g_tcpActivePcb = NULL;
    g_udpPcb = NULL;

    if(LWIP_GETH_Init(&LWIP_GETH_0) != LWIP_GETH_STATUS_SUCCESS)
    {
        return;
    }

    FblEth_StartTcpServer();
    FblEth_StartUdpServer();

    g_ethInitDone = 1u;
}

void FblEth_MainFunction(void)
{
    if(g_ethInitDone == 0u)
    {
        return;
    }

    lwip_geth_Lwip_pollTimerFlags();
    lwip_geth_Lwip_pollReceiveFlags();
}

uint8 FblEth_TcpReceive(uint8 *buf, uint16 *len)
{
    uint16 outLen;
    uint16 remaining;

    if((buf == NULL) || (len == NULL))
    {
        return 0u;
    }

    if(g_tcpStreamLen == 0u)
    {
        return 0u;
    }

    outLen = g_tcpStreamLen;
    if(outLen > FBL_ETH_TCP_RX_CHUNK_SIZE)
    {
        outLen = FBL_ETH_TCP_RX_CHUNK_SIZE;
    }

    memcpy(buf, g_tcpStream, outLen);
    *len = outLen;

    remaining = (uint16)(g_tcpStreamLen - outLen);
    if(remaining > 0u)
    {
        memmove(g_tcpStream, &g_tcpStream[outLen], remaining);
    }
    g_tcpStreamLen = remaining;

    return 1u;
}

uint8 FblEth_UdpReceive(uint8 *buf, uint16 *len)
{
    FblEth_RxItemType *item;

    if((buf == NULL) || (len == NULL))
    {
        return 0u;
    }

    if(g_udpCnt == 0u)
    {
        return 0u;
    }

    item = &g_udpQ[g_udpRd];

    memcpy(buf, item->data, item->len);
    *len = item->len;

    if(item->remoteValid != 0u)
    {
        ip_addr_copy(g_udpRemoteIp, item->remoteIp);
        g_udpRemotePort = item->remotePort;
        g_udpRemoteValid = 1u;
    }
    else
    {
        g_udpRemoteValid = 0u;
    }

    item->len = 0u;
    item->remoteValid = 0u;
    g_udpRd = (uint8)((g_udpRd + 1u) % FBL_ETH_UDP_RX_QUEUE_DEPTH);
    g_udpCnt--;

    return 1u;
}

void FblEth_TcpSend(const uint8 *buf, uint16 len)
{
    err_t err;
    uint16 sent = 0u;
    uint8 retry = FBL_ETH_TCP_SEND_RETRY_MAX;

    if((g_tcpActivePcb == NULL) || (buf == NULL) || (len == 0u))
    {
        return;
    }

    while((sent < len) && (retry > 0u) && (g_tcpActivePcb != NULL))
    {
        uint16 sndBuf = tcp_sndbuf(g_tcpActivePcb);
        uint16 chunk;

        if(sndBuf == 0u)
        {
            (void)tcp_output(g_tcpActivePcb);
            retry--;
            continue;
        }

        chunk = (uint16)(len - sent);
        if(chunk > sndBuf)
        {
            chunk = sndBuf;
        }
        if(chunk > TCP_MSS)
        {
            chunk = TCP_MSS;
        }

        err = tcp_write(g_tcpActivePcb, &buf[sent], chunk, TCP_WRITE_FLAG_COPY);

        if(err == ERR_OK)
        {
            sent = (uint16)(sent + chunk);
            retry = FBL_ETH_TCP_SEND_RETRY_MAX;
            (void)tcp_output(g_tcpActivePcb);
        }
        else
        {
            (void)tcp_output(g_tcpActivePcb);
            retry--;
        }
    }
}

void FblEth_UdpSend(const uint8 *buf, uint16 len)
{
    struct pbuf *p;
    ip_addr_t broadcastIp;

    if((g_udpPcb == NULL) || (buf == NULL) || (len == 0u))
    {
        return;
    }

    p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);

    if(p == NULL)
    {
        return;
    }

    memcpy(p->payload, buf, len);

    if(g_udpRemoteValid != 0u)
    {
        (void)udp_sendto(g_udpPcb, p, &g_udpRemoteIp, g_udpRemotePort);
    }
    else
    {
        ip_addr_set_ip4_u32(&broadcastIp, PP_HTONL(IPADDR_BROADCAST));
        (void)udp_sendto(g_udpPcb, p, &broadcastIp, DOIP_UDP_PORT);
    }

    pbuf_free(p);
}
