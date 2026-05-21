#include "Gptp_Lab.h"
#include "TimeBase.h"
#include "TimeSync_Cfg.h"

#include <string.h>

#define GPTP_LAB_ETH_HEADER_LENGTH             14u
#define GPTP_LAB_PTP_HEADER_LENGTH             34u
#define GPTP_LAB_PTP_PAYLOAD_OFFSET            (GPTP_LAB_ETH_HEADER_LENGTH + GPTP_LAB_PTP_HEADER_LENGTH)
#define GPTP_LAB_ETHERTYPE_PTP                 0x88F7u
#define GPTP_LAB_TRANSPORT_SPECIFIC            0x10u
#define GPTP_LAB_VERSION_PTP2                  0x02u
#define GPTP_LAB_SYNC_FRAME_LENGTH             58u
#define GPTP_LAB_FOLLOW_UP_FRAME_LENGTH        58u
#define GPTP_LAB_ANNOUNCE_FRAME_LENGTH         78u
#define GPTP_LAB_DELAY_REQ_FRAME_LENGTH        58u
#define GPTP_LAB_DELAY_RESP_FRAME_LENGTH       68u
#define GPTP_LAB_CONTROL_SYNC                  0x00u
#define GPTP_LAB_CONTROL_FOLLOW_UP             0x02u
#define GPTP_LAB_CONTROL_OTHER                 0x05u

static const uint8 Gptp_Lab_DestinationMac[6] = { 0x01u, 0x80u, 0xC2u, 0x00u, 0x00u, 0x0Eu };
static const uint8 Gptp_Lab_SourceMac[6] = { 0x02u, 0x54u, 0x43u, 0x33u, 0x37u, 0x35u };
static const uint8 Gptp_Lab_ClockIdentity[GPTP_LAB_CLOCK_IDENTITY_LENGTH] =
{
    0x02u, 0x54u, 0x43u, 0xFFu, 0xFEu, 0x33u, 0x75u, 0x19u
};

static Gptp_Lab_StatusType Gptp_Lab_Status;
static Gptp_Lab_DatasetType Gptp_Lab_LocalDataset;
static uint64 Gptp_Lab_NextSyncTimeNs = 0ull;
static uint64 Gptp_Lab_NextAnnounceTimeNs = 0ull;

static void Gptp_Lab_PutU16(uint8 *data, uint16 value);
static void Gptp_Lab_PutU32(uint8 *data, uint32 value);
static void Gptp_Lab_PutU48(uint8 *data, uint64 value);
static void Gptp_Lab_PutU64(uint8 *data, uint64 value);
static uint16 Gptp_Lab_GetU16(const uint8 *data);
static uint64 Gptp_Lab_SecondsFromNs(uint64 timestampNs);
static uint32 Gptp_Lab_NanosecondsRemainder(uint64 timestampNs);
static void Gptp_Lab_WriteTimestamp(uint8 *data, uint64 timestampNs);
static void Gptp_Lab_WriteEthernetHeader(uint8 *frame);
static void Gptp_Lab_WritePtpHeader(uint8 *frame, uint8 messageType, uint16 messageLength, uint16 sequenceId, uint8 controlField, sint8 logInterval);
static void Gptp_Lab_InitLocalDataset(void);
static void Gptp_Lab_SendSyncAndFollowUp(void);
static void Gptp_Lab_SendAnnounce(void);
#if (TIMESYNC_GPTP_ENABLE == STD_ON)
static void Gptp_Lab_UpdateNextDeadline(uint64 *deadlineNs, uint64 nowNs, uint64 periodNs);
#endif

#if (TIMESYNC_GPTP_ENABLE == STD_ON)
static void Gptp_Lab_UpdateNextDeadline(uint64 *deadlineNs, uint64 nowNs, uint64 periodNs)
{
    *deadlineNs = nowNs + periodNs;
}
#endif

static void Gptp_Lab_PutU16(uint8 *data, uint16 value)
{
    data[0u] = (uint8)(value >> 8u);
    data[1u] = (uint8)value;
}

static void Gptp_Lab_PutU32(uint8 *data, uint32 value)
{
    data[0u] = (uint8)(value >> 24u);
    data[1u] = (uint8)(value >> 16u);
    data[2u] = (uint8)(value >> 8u);
    data[3u] = (uint8)value;
}

static void Gptp_Lab_PutU48(uint8 *data, uint64 value)
{
    data[0u] = (uint8)(value >> 40u);
    data[1u] = (uint8)(value >> 32u);
    data[2u] = (uint8)(value >> 24u);
    data[3u] = (uint8)(value >> 16u);
    data[4u] = (uint8)(value >> 8u);
    data[5u] = (uint8)value;
}

static void Gptp_Lab_PutU64(uint8 *data, uint64 value)
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

static uint16 Gptp_Lab_GetU16(const uint8 *data)
{
    return (uint16)(((uint16)data[0u] << 8u) | (uint16)data[1u]);
}

static uint64 Gptp_Lab_SecondsFromNs(uint64 timestampNs)
{
    return timestampNs / TIMEBASE_NS_PER_S;
}

static uint32 Gptp_Lab_NanosecondsRemainder(uint64 timestampNs)
{
    return (uint32)(timestampNs % TIMEBASE_NS_PER_S);
}

static void Gptp_Lab_WriteTimestamp(uint8 *data, uint64 timestampNs)
{
    Gptp_Lab_PutU48(&data[0u], Gptp_Lab_SecondsFromNs(timestampNs));
    Gptp_Lab_PutU32(&data[6u], Gptp_Lab_NanosecondsRemainder(timestampNs));
}

static void Gptp_Lab_WriteEthernetHeader(uint8 *frame)
{
    memcpy(&frame[0u], Gptp_Lab_DestinationMac, sizeof(Gptp_Lab_DestinationMac));
    memcpy(&frame[6u], Gptp_Lab_SourceMac, sizeof(Gptp_Lab_SourceMac));
    Gptp_Lab_PutU16(&frame[12u], GPTP_LAB_ETHERTYPE_PTP);
}

static void Gptp_Lab_WritePtpHeader(uint8 *frame, uint8 messageType, uint16 messageLength, uint16 sequenceId, uint8 controlField, sint8 logInterval)
{
    uint8 *ptp;

    ptp = &frame[GPTP_LAB_ETH_HEADER_LENGTH];
    ptp[0u] = (uint8)(GPTP_LAB_TRANSPORT_SPECIFIC | (messageType & 0x0Fu));
    ptp[1u] = GPTP_LAB_VERSION_PTP2;
    Gptp_Lab_PutU16(&ptp[2u], messageLength);
    ptp[4u] = 0u;
    ptp[5u] = 0u;
    Gptp_Lab_PutU16(&ptp[6u], 0u);
    Gptp_Lab_PutU64(&ptp[8u], 0ull);
    Gptp_Lab_PutU32(&ptp[16u], 0u);
    memcpy(&ptp[20u], Gptp_Lab_ClockIdentity, GPTP_LAB_CLOCK_IDENTITY_LENGTH);
    Gptp_Lab_PutU16(&ptp[28u], 1u);
    Gptp_Lab_PutU16(&ptp[30u], sequenceId);
    ptp[32u] = controlField;
    ptp[33u] = (uint8)logInterval;
}

static void Gptp_Lab_InitLocalDataset(void)
{
    Gptp_Lab_LocalDataset.priority1 = 248u;
    Gptp_Lab_LocalDataset.clockClass = 248u;
    Gptp_Lab_LocalDataset.clockAccuracy = 0xFEu;
    Gptp_Lab_LocalDataset.offsetScaledLogVariance = 0xFFFFu;
    Gptp_Lab_LocalDataset.priority2 = 248u;
    memcpy(Gptp_Lab_LocalDataset.clockIdentity, Gptp_Lab_ClockIdentity, GPTP_LAB_CLOCK_IDENTITY_LENGTH);
}

void Gptp_Lab_Init(void)
{
    Gptp_Lab_InitLocalDataset();
    Gptp_Lab_NextSyncTimeNs = 0ull;
    Gptp_Lab_NextAnnounceTimeNs = 0ull;
    Gptp_Lab_Status.txSequenceId = 0u;
    Gptp_Lab_Status.syncTxCounter = 0u;
    Gptp_Lab_Status.followUpTxCounter = 0u;
    Gptp_Lab_Status.announceTxCounter = 0u;
    Gptp_Lab_Status.rxAnnounceCounter = 0u;
    Gptp_Lab_Status.platformTxDropCounter = 0u;

#if (TIMESYNC_GPTP_ENABLE == STD_ON)
    Gptp_Lab_Status.enabled = 1u;
    Gptp_Lab_Status.forceMaster = (TIMESYNC_GPTP_FORCE_MASTER == STD_ON) ? 1u : 0u;
    Gptp_Lab_Status.state = GPTP_LAB_STATE_MASTER;
#else
    Gptp_Lab_Status.enabled = 0u;
    Gptp_Lab_Status.forceMaster = (TIMESYNC_GPTP_FORCE_MASTER == STD_ON) ? 1u : 0u;
    Gptp_Lab_Status.state = GPTP_LAB_STATE_DISABLED;
#endif
}

sint32 Gptp_Lab_CompareDatasets(const Gptp_Lab_DatasetType *left, const Gptp_Lab_DatasetType *right)
{
    uint8 i;

    if ((left == NULL_PTR) || (right == NULL_PTR))
    {
        return 0;
    }

#define GPTP_LAB_COMPARE_FIELD(field) \
    if (left->field < right->field) { return -1; } \
    if (left->field > right->field) { return 1; }

    GPTP_LAB_COMPARE_FIELD(priority1)
    GPTP_LAB_COMPARE_FIELD(clockClass)
    GPTP_LAB_COMPARE_FIELD(clockAccuracy)
    GPTP_LAB_COMPARE_FIELD(offsetScaledLogVariance)
    GPTP_LAB_COMPARE_FIELD(priority2)

#undef GPTP_LAB_COMPARE_FIELD

    for (i = 0u; i < GPTP_LAB_CLOCK_IDENTITY_LENGTH; i++)
    {
        if (left->clockIdentity[i] < right->clockIdentity[i])
        {
            return -1;
        }
        if (left->clockIdentity[i] > right->clockIdentity[i])
        {
            return 1;
        }
    }

    return 0;
}

Std_ReturnType Gptp_Lab_BuildSync(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 originTimestampNs)
{
    if ((frame == NULL_PTR) || (outLen == NULL_PTR) || (frameLen < GPTP_LAB_SYNC_FRAME_LENGTH))
    {
        return E_NOT_OK;
    }

    memset(frame, 0, GPTP_LAB_SYNC_FRAME_LENGTH);
    Gptp_Lab_WriteEthernetHeader(frame);
    Gptp_Lab_WritePtpHeader(frame, GPTP_LAB_MESSAGE_SYNC, GPTP_LAB_SYNC_FRAME_LENGTH - GPTP_LAB_ETH_HEADER_LENGTH, sequenceId, GPTP_LAB_CONTROL_SYNC, -3);
    Gptp_Lab_WriteTimestamp(&frame[GPTP_LAB_PTP_PAYLOAD_OFFSET], originTimestampNs);
    *outLen = GPTP_LAB_SYNC_FRAME_LENGTH;

    return E_OK;
}

Std_ReturnType Gptp_Lab_BuildFollowUp(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 originTimestampNs)
{
    if ((frame == NULL_PTR) || (outLen == NULL_PTR) || (frameLen < GPTP_LAB_FOLLOW_UP_FRAME_LENGTH))
    {
        return E_NOT_OK;
    }

    memset(frame, 0, GPTP_LAB_FOLLOW_UP_FRAME_LENGTH);
    Gptp_Lab_WriteEthernetHeader(frame);
    Gptp_Lab_WritePtpHeader(frame, GPTP_LAB_MESSAGE_FOLLOW_UP, GPTP_LAB_FOLLOW_UP_FRAME_LENGTH - GPTP_LAB_ETH_HEADER_LENGTH, sequenceId, GPTP_LAB_CONTROL_FOLLOW_UP, -3);
    Gptp_Lab_WriteTimestamp(&frame[GPTP_LAB_PTP_PAYLOAD_OFFSET], originTimestampNs);
    *outLen = GPTP_LAB_FOLLOW_UP_FRAME_LENGTH;

    return E_OK;
}

Std_ReturnType Gptp_Lab_BuildAnnounce(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 originTimestampNs)
{
    uint8 *payload;

    if ((frame == NULL_PTR) || (outLen == NULL_PTR) || (frameLen < GPTP_LAB_ANNOUNCE_FRAME_LENGTH))
    {
        return E_NOT_OK;
    }

    memset(frame, 0, GPTP_LAB_ANNOUNCE_FRAME_LENGTH);
    Gptp_Lab_WriteEthernetHeader(frame);
    Gptp_Lab_WritePtpHeader(frame, GPTP_LAB_MESSAGE_ANNOUNCE, GPTP_LAB_ANNOUNCE_FRAME_LENGTH - GPTP_LAB_ETH_HEADER_LENGTH, sequenceId, GPTP_LAB_CONTROL_OTHER, 0);

    payload = &frame[GPTP_LAB_PTP_PAYLOAD_OFFSET];
    Gptp_Lab_WriteTimestamp(&payload[0u], originTimestampNs);
    Gptp_Lab_PutU16(&payload[10u], 0u);
    payload[12u] = 0u;
    payload[13u] = Gptp_Lab_LocalDataset.priority1;
    payload[14u] = Gptp_Lab_LocalDataset.clockClass;
    payload[15u] = Gptp_Lab_LocalDataset.clockAccuracy;
    Gptp_Lab_PutU16(&payload[16u], Gptp_Lab_LocalDataset.offsetScaledLogVariance);
    payload[18u] = Gptp_Lab_LocalDataset.priority2;
    memcpy(&payload[19u], Gptp_Lab_LocalDataset.clockIdentity, GPTP_LAB_CLOCK_IDENTITY_LENGTH);
    Gptp_Lab_PutU16(&payload[27u], 0u);
    payload[29u] = 0xA0u;
    *outLen = GPTP_LAB_ANNOUNCE_FRAME_LENGTH;

    return E_OK;
}

Std_ReturnType Gptp_Lab_BuildDelayReq(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 originTimestampNs)
{
    if ((frame == NULL_PTR) || (outLen == NULL_PTR) || (frameLen < GPTP_LAB_DELAY_REQ_FRAME_LENGTH))
    {
        return E_NOT_OK;
    }

    memset(frame, 0, GPTP_LAB_DELAY_REQ_FRAME_LENGTH);
    Gptp_Lab_WriteEthernetHeader(frame);
    Gptp_Lab_WritePtpHeader(frame, GPTP_LAB_MESSAGE_DELAY_REQ, GPTP_LAB_DELAY_REQ_FRAME_LENGTH - GPTP_LAB_ETH_HEADER_LENGTH, sequenceId, GPTP_LAB_CONTROL_OTHER, 0);
    Gptp_Lab_WriteTimestamp(&frame[GPTP_LAB_PTP_PAYLOAD_OFFSET], originTimestampNs);
    *outLen = GPTP_LAB_DELAY_REQ_FRAME_LENGTH;

    return E_OK;
}

Std_ReturnType Gptp_Lab_BuildDelayResp(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 receiveTimestampNs)
{
    uint8 *payload;

    if ((frame == NULL_PTR) || (outLen == NULL_PTR) || (frameLen < GPTP_LAB_DELAY_RESP_FRAME_LENGTH))
    {
        return E_NOT_OK;
    }

    memset(frame, 0, GPTP_LAB_DELAY_RESP_FRAME_LENGTH);
    Gptp_Lab_WriteEthernetHeader(frame);
    Gptp_Lab_WritePtpHeader(frame, GPTP_LAB_MESSAGE_DELAY_RESP, GPTP_LAB_DELAY_RESP_FRAME_LENGTH - GPTP_LAB_ETH_HEADER_LENGTH, sequenceId, GPTP_LAB_CONTROL_OTHER, 0);
    payload = &frame[GPTP_LAB_PTP_PAYLOAD_OFFSET];
    Gptp_Lab_WriteTimestamp(&payload[0u], receiveTimestampNs);
    memcpy(&payload[10u], Gptp_Lab_ClockIdentity, GPTP_LAB_CLOCK_IDENTITY_LENGTH);
    Gptp_Lab_PutU16(&payload[18u], 1u);
    *outLen = GPTP_LAB_DELAY_RESP_FRAME_LENGTH;

    return E_OK;
}

Std_ReturnType Gptp_Lab_ParseAnnounce(const uint8 *frame, uint16 frameLen, Gptp_Lab_DatasetType *dataset)
{
    const uint8 *ptp;
    const uint8 *payload;

    if ((frame == NULL_PTR) || (dataset == NULL_PTR) || (frameLen < GPTP_LAB_ANNOUNCE_FRAME_LENGTH))
    {
        return E_NOT_OK;
    }

    if (Gptp_Lab_GetU16(&frame[12u]) != GPTP_LAB_ETHERTYPE_PTP)
    {
        return E_NOT_OK;
    }

    ptp = &frame[GPTP_LAB_ETH_HEADER_LENGTH];
    if ((ptp[0u] & 0x0Fu) != GPTP_LAB_MESSAGE_ANNOUNCE)
    {
        return E_NOT_OK;
    }

    payload = &frame[GPTP_LAB_PTP_PAYLOAD_OFFSET];
    dataset->priority1 = payload[13u];
    dataset->clockClass = payload[14u];
    dataset->clockAccuracy = payload[15u];
    dataset->offsetScaledLogVariance = Gptp_Lab_GetU16(&payload[16u]);
    dataset->priority2 = payload[18u];
    memcpy(dataset->clockIdentity, &payload[19u], GPTP_LAB_CLOCK_IDENTITY_LENGTH);

    return E_OK;
}

static void Gptp_Lab_SendSyncAndFollowUp(void)
{
#if (TIMESYNC_GPTP_ENABLE == STD_ON)
    uint8 frame[GPTP_LAB_FRAME_BUFFER_LENGTH];
    uint16 frameLength;
    uint16 sequenceId;
    uint64 timestampNs;

    sequenceId = Gptp_Lab_Status.txSequenceId++;
    timestampNs = TimeBase_GetVehicleTimeNs();
    (void)Gptp_Lab_PlatformGetTxTimestamp(&timestampNs);

    if (Gptp_Lab_BuildSync(frame, sizeof(frame), &frameLength, sequenceId, timestampNs) == E_OK)
    {
        if (Gptp_Lab_PlatformTransmitFrame(frame, frameLength) == E_OK)
        {
            Gptp_Lab_Status.syncTxCounter++;
        }
        else
        {
            Gptp_Lab_Status.platformTxDropCounter++;
        }
    }

    if (Gptp_Lab_BuildFollowUp(frame, sizeof(frame), &frameLength, sequenceId, timestampNs) == E_OK)
    {
        if (Gptp_Lab_PlatformTransmitFrame(frame, frameLength) == E_OK)
        {
            Gptp_Lab_Status.followUpTxCounter++;
        }
        else
        {
            Gptp_Lab_Status.platformTxDropCounter++;
        }
    }
#endif
}

static void Gptp_Lab_SendAnnounce(void)
{
#if (TIMESYNC_GPTP_ENABLE == STD_ON)
    uint8 frame[GPTP_LAB_FRAME_BUFFER_LENGTH];
    uint16 frameLength;
    uint16 sequenceId;
    uint64 timestampNs;

    sequenceId = Gptp_Lab_Status.txSequenceId++;
    timestampNs = TimeBase_GetVehicleTimeNs();

    if (Gptp_Lab_BuildAnnounce(frame, sizeof(frame), &frameLength, sequenceId, timestampNs) == E_OK)
    {
        if (Gptp_Lab_PlatformTransmitFrame(frame, frameLength) == E_OK)
        {
            Gptp_Lab_Status.announceTxCounter++;
        }
        else
        {
            Gptp_Lab_Status.platformTxDropCounter++;
        }
    }
#endif
}

void Gptp_Lab_MainFunction(uint32 elapsedMs)
{
#if (TIMESYNC_GPTP_ENABLE == STD_ON)
    uint64 nowNs;
    uint64 syncPeriodNs;
    uint64 announcePeriodNs;

    (void)elapsedMs;

    if ((Gptp_Lab_Status.state != GPTP_LAB_STATE_MASTER) &&
        (Gptp_Lab_Status.forceMaster == 0u))
    {
        Gptp_Lab_NextSyncTimeNs = 0ull;
        Gptp_Lab_NextAnnounceTimeNs = 0ull;
        return;
    }

    nowNs = TimeBase_PlatformGetCounterNs();
    syncPeriodNs = (uint64)TIMESYNC_GPTP_SYNC_PERIOD_MS * TIMEBASE_NS_PER_MS;
    announcePeriodNs = (uint64)TIMESYNC_GPTP_ANNOUNCE_PERIOD_MS * TIMEBASE_NS_PER_MS;

    if (Gptp_Lab_NextSyncTimeNs == 0ull)
    {
        Gptp_Lab_NextSyncTimeNs = nowNs + syncPeriodNs;
    }
    else if (nowNs >= Gptp_Lab_NextSyncTimeNs)
    {
        Gptp_Lab_SendSyncAndFollowUp();
        Gptp_Lab_UpdateNextDeadline(&Gptp_Lab_NextSyncTimeNs, nowNs, syncPeriodNs);
    }

    if (Gptp_Lab_NextAnnounceTimeNs == 0ull)
    {
        Gptp_Lab_NextAnnounceTimeNs = nowNs + announcePeriodNs;
    }
    else if (nowNs >= Gptp_Lab_NextAnnounceTimeNs)
    {
        Gptp_Lab_SendAnnounce();
        Gptp_Lab_UpdateNextDeadline(&Gptp_Lab_NextAnnounceTimeNs, nowNs, announcePeriodNs);
    }
#else
    (void)elapsedMs;
#endif
}

void Gptp_Lab_RxIndication(const uint8 *frame, uint16 frameLen, uint64 rxTimestampNs)
{
#if (TIMESYNC_GPTP_ENABLE == STD_ON)
    Gptp_Lab_DatasetType remoteDataset;
    sint32 compareResult;

    (void)rxTimestampNs;

    if (Gptp_Lab_ParseAnnounce(frame, frameLen, &remoteDataset) != E_OK)
    {
        return;
    }

    Gptp_Lab_Status.rxAnnounceCounter++;

    if (Gptp_Lab_Status.forceMaster != 0u)
    {
        Gptp_Lab_Status.state = GPTP_LAB_STATE_MASTER;
        return;
    }

    compareResult = Gptp_Lab_CompareDatasets(&remoteDataset, &Gptp_Lab_LocalDataset);
    if (compareResult < 0)
    {
        Gptp_Lab_Status.state = GPTP_LAB_STATE_SLAVE;
    }
    else
    {
        Gptp_Lab_Status.state = GPTP_LAB_STATE_MASTER;
    }
#else
    (void)frame;
    (void)frameLen;
    (void)rxTimestampNs;
#endif
}

void Gptp_Lab_GetStatus(Gptp_Lab_StatusType *status)
{
    if (status == NULL_PTR)
    {
        return;
    }

    *status = Gptp_Lab_Status;
}

__attribute__((weak)) Std_ReturnType Gptp_Lab_PlatformTransmitFrame(const uint8 *frame, uint16 frameLen)
{
    (void)frame;
    (void)frameLen;
    /*
     * TODO: connect this to a raw Ethernet 0x88F7 TX path when the Ethernet
     * driver exposes one. The current lwIP socket wrapper is UDP/IP only.
     */
    return E_NOT_OK;
}

__attribute__((weak)) Std_ReturnType Gptp_Lab_PlatformGetTxTimestamp(uint64 *timestampNs)
{
    (void)timestampNs;
    return E_NOT_OK;
}

__attribute__((weak)) Std_ReturnType Gptp_Lab_PlatformGetRxTimestamp(uint64 *timestampNs)
{
    (void)timestampNs;
    return E_NOT_OK;
}
