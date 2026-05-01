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
#define FBL_ETH_RX_QUEUE_DEPTH     4u
#define FBL_ETH_RX_BUF_SIZE        1600u

typedef struct
{
    uint8 data[FBL_ETH_RX_BUF_SIZE];
    uint16 len;
} FblEth_RxItemType;

static struct tcp_pcb *g_tcpListenPcb;
static struct tcp_pcb *g_tcpActivePcb;
static struct udp_pcb *g_udpPcb;

static FblEth_RxItemType g_tcpQ[FBL_ETH_RX_QUEUE_DEPTH];
static FblEth_RxItemType g_udpQ[FBL_ETH_RX_QUEUE_DEPTH];

static volatile uint8 g_tcpWr;
static volatile uint8 g_tcpRd;
static volatile uint8 g_tcpCnt;

static volatile uint8 g_udpWr;
static volatile uint8 g_udpRd;
static volatile uint8 g_udpCnt;

static ip_addr_t g_udpRemoteIp;
static uint16 g_udpRemotePort;
static uint8 g_udpRemoteValid;

static uint8 g_ethInitDone;

static void FblEth_CopyPbuf(uint8 *dst, uint16 *dstLen, const struct pbuf *p)
{
    const struct pbuf *q;
    uint16 copied = 0u;

    for(q = p; q != NULL; q = q->next)
    {
        uint16 chunk = q->len;

        if((copied + chunk) > FBL_ETH_RX_BUF_SIZE)
        {
            chunk = (uint16)(FBL_ETH_RX_BUF_SIZE - copied);
        }

        if(chunk > 0u)
        {
            memcpy(&dst[copied], q->payload, chunk);
            copied = (uint16)(copied + chunk);
        }

        if(copied >= FBL_ETH_RX_BUF_SIZE)
        {
            break;
        }
    }

    *dstLen = copied;
}

static void FblEth_QueueTcp(const struct pbuf *p)
{
    if(g_tcpCnt >= FBL_ETH_RX_QUEUE_DEPTH)
    {
        return;
    }

    FblEth_CopyPbuf(g_tcpQ[g_tcpWr].data, &g_tcpQ[g_tcpWr].len, p);
    g_tcpWr = (uint8)((g_tcpWr + 1u) % FBL_ETH_RX_QUEUE_DEPTH);
    g_tcpCnt++;
}

static void FblEth_QueueUdp(const struct pbuf *p)
{
    if(g_udpCnt >= FBL_ETH_RX_QUEUE_DEPTH)
    {
        return;
    }

    FblEth_CopyPbuf(g_udpQ[g_udpWr].data, &g_udpQ[g_udpWr].len, p);
    g_udpWr = (uint8)((g_udpWr + 1u) % FBL_ETH_RX_QUEUE_DEPTH);
    g_udpCnt++;
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
        }

        tcp_arg(tpcb, NULL);
        tcp_recv(tpcb, NULL);
        tcp_err(tpcb, NULL);
        tcp_close(tpcb);

        return ERR_OK;
    }

    FblEth_QueueTcp(p);
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    return ERR_OK;
}

static void FblEth_TcpErrCb(void *arg, err_t err)
{
    (void)arg;
    (void)err;

    g_tcpActivePcb = NULL;
}

static err_t FblEth_TcpAcceptCb(void *arg, struct tcp_pcb *newPcb, err_t err)
{
    (void)arg;

    if((err != ERR_OK) || (newPcb == NULL))
    {
        return ERR_VAL;
    }

    if(g_tcpActivePcb != NULL)
    {
        tcp_abort(newPcb);
        return ERR_ABRT;
    }

    g_tcpActivePcb = newPcb;

    tcp_setprio(newPcb, TCP_PRIO_NORMAL);
    tcp_arg(newPcb, NULL);
    tcp_recv(newPcb, FblEth_TcpRecvCb);
    tcp_err(newPcb, FblEth_TcpErrCb);

#if TCP_NODELAY
    tcp_nagle_disable(newPcb);
#endif

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

    if((p == NULL) || (addr == NULL))
    {
        return;
    }

    FblEth_QueueUdp(p);

    ip_addr_copy(g_udpRemoteIp, *addr);
    g_udpRemotePort = port;
    g_udpRemoteValid = 1u;

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

    memset(g_tcpQ, 0u, sizeof(g_tcpQ));
    memset(g_udpQ, 0u, sizeof(g_udpQ));

    g_tcpWr = 0u;
    g_tcpRd = 0u;
    g_tcpCnt = 0u;

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

#if (LWIP_GETH_IS_ISR == 0)
    /*
     * Warning:
     * Your generated lwip_geth_netif_input() contains an infinite loop.
     * Keep LWIP_GETH_IS_ISR = 1 unless you rewrite lwip_geth_netif_input()
     * into a non-blocking polling function.
     */
#endif
}

uint8 FblEth_TcpReceive(uint8 *buf, uint16 *len)
{
    if((buf == NULL) || (len == NULL))
    {
        return 0u;
    }

    if(g_tcpCnt == 0u)
    {
        return 0u;
    }

    memcpy(buf, g_tcpQ[g_tcpRd].data, g_tcpQ[g_tcpRd].len);
    *len = g_tcpQ[g_tcpRd].len;

    g_tcpQ[g_tcpRd].len = 0u;
    g_tcpRd = (uint8)((g_tcpRd + 1u) % FBL_ETH_RX_QUEUE_DEPTH);
    g_tcpCnt--;

    return 1u;
}

uint8 FblEth_UdpReceive(uint8 *buf, uint16 *len)
{
    if((buf == NULL) || (len == NULL))
    {
        return 0u;
    }

    if(g_udpCnt == 0u)
    {
        return 0u;
    }

    memcpy(buf, g_udpQ[g_udpRd].data, g_udpQ[g_udpRd].len);
    *len = g_udpQ[g_udpRd].len;

    g_udpQ[g_udpRd].len = 0u;
    g_udpRd = (uint8)((g_udpRd + 1u) % FBL_ETH_RX_QUEUE_DEPTH);
    g_udpCnt--;

    return 1u;
}

void FblEth_TcpSend(const uint8 *buf, uint16 len)
{
    err_t err;

    if((g_tcpActivePcb == NULL) || (buf == NULL) || (len == 0u))
    {
        return;
    }

    if(tcp_sndbuf(g_tcpActivePcb) < len)
    {
        return;
    }

    err = tcp_write(g_tcpActivePcb, buf, len, TCP_WRITE_FLAG_COPY);

    if(err == ERR_OK)
    {
        (void)tcp_output(g_tcpActivePcb);
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
