#include "EthTimeSync.h"
#include "GatewaySwc.h"
#include "TimeBase.h"
#include "TimeSync_Cfg.h"

#include "BSW/Com/Ethernet/TcpIpH.h"
#include "BSW/Sys/Crc/Crc.h"

#define ETHTIMESYNC_MAGIC                      0x5A545331u
#define ETHTIMESYNC_VERSION                    1u
#define ETHTIMESYNC_MESSAGE_TIME_BROADCAST     1u
#define ETHTIMESYNC_PACKET_LENGTH              40u
#define ETHTIMESYNC_CRC_OFFSET                 36u
#define ETHTIMESYNC_TX_PERIOD_NS               ((uint64)TIMESYNC_UDP_PERIOD_MS * TIMEBASE_NS_PER_MS)

static TcpIp_SocketIdType EthTimeSync_Socket = TCPIP_INVALID_SOCKET;
static uint64 EthTimeSync_NextTxTimeNs = 0ull;
static uint32 EthTimeSync_SequenceCounter = 0u;
static uint8 EthTimeSync_Status = ETHTIMESYNC_STATUS_DISABLED;
static uint32 EthTimeSync_TxCounter = 0u;

static void EthTimeSync_PutU16(uint8 *data, uint16 value);
static void EthTimeSync_PutU32(uint8 *data, uint32 value);
static void EthTimeSync_PutU64(uint8 *data, uint64 value);
static void EthTimeSync_OpenSocket(void);
static void EthTimeSync_CloseSocket(void);
static void EthTimeSync_BuildPacket(uint8 *packet);
static void EthTimeSync_SendPacket(void);
static void EthTimeSync_UpdateNextTxDeadline(uint64 nowNs);

static void EthTimeSync_UpdateNextTxDeadline(uint64 nowNs)
{
    EthTimeSync_NextTxTimeNs = nowNs + ETHTIMESYNC_TX_PERIOD_NS;
}

static void EthTimeSync_PutU16(uint8 *data, uint16 value)
{
    data[0u] = (uint8)(value >> 8u);
    data[1u] = (uint8)value;
}

static void EthTimeSync_PutU32(uint8 *data, uint32 value)
{
    data[0u] = (uint8)(value >> 24u);
    data[1u] = (uint8)(value >> 16u);
    data[2u] = (uint8)(value >> 8u);
    data[3u] = (uint8)value;
}

static void EthTimeSync_PutU64(uint8 *data, uint64 value)
{
    data[0u] = (uint8)(value >> 56u);
    data[1u] = (uint8)(value >> 48u);
    data[2u] = (uint8)(value >> 40u);
    data[3u] = (uint8)(value >> 32u);
    data[4u] = (uint8)(value >> 24u);
    data[5u] = (uint8)(value >> 16u);
    data[6u] = (uint8)(value >> 8u);
    data[7u] = (uint8)value;
}

void EthTimeSync_Init(void)
{
#if (TIMESYNC_UDP_ENABLE == STD_ON)
    EthTimeSync_NextTxTimeNs = 0ull;
    EthTimeSync_SequenceCounter = 0u;
    EthTimeSync_TxCounter = 0u;
    EthTimeSync_Status = ETHTIMESYNC_STATUS_WAIT_LINK;
    EthTimeSync_OpenSocket();
#else
    EthTimeSync_Status = ETHTIMESYNC_STATUS_DISABLED;
#endif
}

static void EthTimeSync_OpenSocket(void)
{
#if (TIMESYNC_UDP_ENABLE == STD_ON)
    if (EthTimeSync_Socket != TCPIP_INVALID_SOCKET)
    {
        return;
    }

    if (TcpIp_IsLinkAvailable() == 0u)
    {
        EthTimeSync_Status = ETHTIMESYNC_STATUS_WAIT_LINK;
        return;
    }

    EthTimeSync_Socket = TcpIp_Create(0u);
    if (EthTimeSync_Socket == TCPIP_INVALID_SOCKET)
    {
        EthTimeSync_Status = ETHTIMESYNC_STATUS_TX_ERROR;
        return;
    }

    if (TcpIp_Bind(EthTimeSync_Socket, 0u) < 0)
    {
        EthTimeSync_CloseSocket();
        EthTimeSync_Status = ETHTIMESYNC_STATUS_TX_ERROR;
        return;
    }

    EthTimeSync_Status = ETHTIMESYNC_STATUS_SOCKET_READY;
#endif
}

static void EthTimeSync_CloseSocket(void)
{
#if (TIMESYNC_UDP_ENABLE == STD_ON)
    if (EthTimeSync_Socket != TCPIP_INVALID_SOCKET)
    {
        TcpIp_Close(EthTimeSync_Socket);
        EthTimeSync_Socket = TCPIP_INVALID_SOCKET;
    }
#endif
}

static void EthTimeSync_BuildPacket(uint8 *packet)
{
    TimeBase_TimestampSnapshotType snapshot;
    uint32 crc;

    TimeBase_GetTimestampSnapshot(&snapshot);

    EthTimeSync_PutU32(&packet[0u], ETHTIMESYNC_MAGIC);
    packet[4u] = ETHTIMESYNC_VERSION;
    packet[5u] = ETHTIMESYNC_MESSAGE_TIME_BROADCAST;
    EthTimeSync_PutU16(&packet[6u], ETHTIMESYNC_PACKET_LENGTH);
    EthTimeSync_PutU32(&packet[8u], EthTimeSync_SequenceCounter);
    EthTimeSync_PutU64(&packet[12u], snapshot.vehicle_time_ns);
    EthTimeSync_PutU64(&packet[20u], snapshot.utc_time_ns);
    packet[28u] = snapshot.utc_valid;
    packet[29u] = snapshot.time_source;
    EthTimeSync_PutU16(&packet[30u], 0u);
    EthTimeSync_PutU32(&packet[32u], snapshot.sync_status);

    crc = Crc_CalculateCRC32(packet, ETHTIMESYNC_CRC_OFFSET, 0u, TRUE);
    EthTimeSync_PutU32(&packet[ETHTIMESYNC_CRC_OFFSET], crc);
}

static void EthTimeSync_SendPacket(void)
{
#if (TIMESYNC_UDP_ENABLE == STD_ON)
    uint8 packet[ETHTIMESYNC_PACKET_LENGTH];
    TcpIp_SockAddrType remoteAddr;
    sint32 sentLength;

    if ((EthTimeSync_Socket == TCPIP_INVALID_SOCKET) ||
        (TcpIp_IsSocketOpen(EthTimeSync_Socket) == 0u))
    {
        EthTimeSync_Socket = TCPIP_INVALID_SOCKET;
        EthTimeSync_OpenSocket();
        return;
    }

    remoteAddr.addr = TIMESYNC_UDP_BROADCAST_ADDR;
    remoteAddr.port = TIMESYNC_UDP_PORT;

    EthTimeSync_BuildPacket(packet);
    sentLength = GatewaySwc_RequestTcpIpSendTo(EthTimeSync_Socket, &remoteAddr, packet, ETHTIMESYNC_PACKET_LENGTH);
    if (sentLength == (sint32)ETHTIMESYNC_PACKET_LENGTH)
    {
        EthTimeSync_SequenceCounter++;
        EthTimeSync_TxCounter++;
        EthTimeSync_Status = ETHTIMESYNC_STATUS_SOCKET_READY;
    }
    else
    {
        EthTimeSync_Status = ETHTIMESYNC_STATUS_TX_ERROR;
    }
#endif
}

void EthTimeSync_MainFunction(uint32 elapsedMs)
{
    uint64 nowNs;

    (void)elapsedMs;

    if (TcpIp_IsLinkAvailable() == 0u)
    {
#if (TIMESYNC_UDP_ENABLE == STD_ON)
        EthTimeSync_NextTxTimeNs = 0ull;
        EthTimeSync_Status = ETHTIMESYNC_STATUS_WAIT_LINK;
#endif
        return;
    }

    nowNs = TimeBase_PlatformGetCounterNs();

#if (TIMESYNC_UDP_ENABLE == STD_ON)
    if (EthTimeSync_Socket == TCPIP_INVALID_SOCKET)
    {
        EthTimeSync_OpenSocket();
    }

    if (EthTimeSync_NextTxTimeNs == 0ull)
    {
        EthTimeSync_NextTxTimeNs = nowNs + ETHTIMESYNC_TX_PERIOD_NS;
    }
    else if (nowNs >= EthTimeSync_NextTxTimeNs)
    {
        EthTimeSync_SendPacket();
        EthTimeSync_UpdateNextTxDeadline(nowNs);
    }
#endif

}

uint8 EthTimeSync_GetStatus(void)
{
    return EthTimeSync_Status;
}

uint32 EthTimeSync_GetTxCounter(void)
{
    return EthTimeSync_TxCounter;
}
