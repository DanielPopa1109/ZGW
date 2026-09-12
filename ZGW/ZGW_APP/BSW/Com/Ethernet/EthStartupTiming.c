#include "BSW/Com/Ethernet/EthStartupTiming.h"

#include <string.h>

#include "BSW/Mem/Nvm/Nvm.h"
#include "BSW/Mem/Nvm/NvM_Cfg.h"
#include "IfxCpu.h"
#include "IfxStm.h"

#define ETHSTARTUPTIMING_SPIN_TIMEOUT            (1000u)
#define ETHSTARTUPTIMING_ROUTINE_START           (0x01u)
#define ETHSTARTUPTIMING_ROUTINE_REQUEST_RESULTS (0x03u)
#define ETHSTARTUPTIMING_RESPONSE_HEADER_LEN     (28u)
#define ETHSTARTUPTIMING_RESPONSE_ENTRY_LEN      (20u)
#define ETHSTARTUPTIMING_MAX_RESPONSE_ENTRIES    (11u)
#define ETHSTARTUPTIMING_STATE_IN_PROGRESS       (1u)
#define ETHSTARTUPTIMING_STATE_LINK_READY        (2u)
#define ETHSTARTUPTIMING_STATE_RX_OBSERVED       (3u)
#define ETHSTARTUPTIMING_STATE_TX_OBSERVED       (4u)
#define ETHSTARTUPTIMING_STATE_COMPLETE          (5u)

uint8 NvM_EthStartupTiming_Ram[NVM_BLOCK_ETH_STARTUP_TIMING_LENGTH];
const uint8 NvM_EthStartupTiming_Rom[ETHSTARTUPTIMING_NVM_IMAGE_SIZE] =
{
    0x45u, 0x54u, 0x48u, 0x54u, /* magic: "ETHT" */
    0x00u, 0x02u,               /* version */
    0x00u, ETHSTARTUPTIMING_MAX_EVENTS
};

static IfxCpu_spinLock EthStartupTiming_Lock;
/* The live boot capture must never share storage with the NvM ReadAll target.
 * NvM_EthStartupTiming_Ram therefore remains the previous finalized boot until
 * the current capture is closed and deliberately published to it. */
static EthStartupTiming_NvImageType EthStartupTiming_ActiveImage;
static boolean EthStartupTiming_Initialized;
static boolean EthStartupTiming_NvMReady;
static boolean EthStartupTiming_NvMReadAllComplete;
static boolean EthStartupTiming_CaptureOpen;
static boolean EthStartupTiming_SnapshotPending;
static uint64 EthStartupTiming_FirstTxSubmitTicks;
static uint64 EthStartupTiming_FirstTxCompleteTicks;

static EthStartupTiming_NvImageType *EthStartupTiming_Image(void);
static uint64 EthStartupTiming_ReadFrequency(void);
static void EthStartupTiming_MarkDirty(void);
static void EthStartupTiming_CloseCapture(boolean reinitObserved);
static void EthStartupTiming_PublishSnapshot(void);
static void EthStartupTiming_NormalizeMetadata(EthStartupTiming_NvImageType *image);
static uint32 EthStartupTiming_MaskWord(EthStartupTiming_EventIdType eventId);
static uint32 EthStartupTiming_MaskBit(EthStartupTiming_EventIdType eventId);
static boolean EthStartupTiming_HasEvent(
        const EthStartupTiming_NvImageType *image,
        EthStartupTiming_EventIdType eventId);
static void EthStartupTiming_UpdateMeasurementState(EthStartupTiming_NvImageType *image);
static void EthStartupTiming_PutU16(uint8 *data, uint16 value);
static void EthStartupTiming_PutU32(uint8 *data, uint32 value);
static void EthStartupTiming_PutU64(uint8 *data, uint64 value);

static EthStartupTiming_NvImageType *EthStartupTiming_Image(void)
{
    return &EthStartupTiming_ActiveImage;
}

uint64 EthTiming_ReadStmTicks(void)
{
    return IfxStm_get(&MODULE_STM0);
}

static uint64 EthStartupTiming_ReadFrequency(void)
{
    return (uint64)IfxStm_getFrequency(&MODULE_STM0);
}

static void EthStartupTiming_MarkDirty(void)
{
    /* Captures stay RAM-only. Publishing is intentionally deferred until the
     * boot measurement is closed and NvM_ReadAll can no longer overwrite it. */
    EthStartupTiming_SnapshotPending = TRUE;
    EthStartupTiming_PublishSnapshot();
}

static void EthStartupTiming_PublishSnapshot(void)
{
    if ((EthStartupTiming_SnapshotPending == FALSE) ||
            (EthStartupTiming_CaptureOpen != FALSE) ||
            (EthStartupTiming_NvMReady == FALSE) ||
            (EthStartupTiming_NvMReadAllComplete == FALSE))
    {
        return;
    }

    (void)memcpy(NvM_EthStartupTiming_Ram,
            &EthStartupTiming_ActiveImage,
            sizeof(EthStartupTiming_ActiveImage));
    (void)NvM_SetRamBlockStatus(NVM_BLOCK_ID_ETH_STARTUP_TIMING, TRUE);
    EthStartupTiming_SnapshotPending = FALSE;
}

static void EthStartupTiming_CloseCapture(boolean reinitObserved)
{
    if (EthStartupTiming_CaptureOpen == FALSE)
    {
        return;
    }

    EthStartupTiming_CaptureOpen = FALSE;
    EthStartupTiming_ActiveImage.flags |= ETHSTARTUPTIMING_FLAG_CAPTURE_CLOSED;
    if (reinitObserved != FALSE)
    {
        EthStartupTiming_ActiveImage.flags |= ETHSTARTUPTIMING_FLAG_REINIT_OBSERVED;
    }
    EthStartupTiming_MarkDirty();
    EthStartupTiming_PublishSnapshot();
}

static void EthStartupTiming_NormalizeMetadata(EthStartupTiming_NvImageType *image)
{
    uint16 eventIndex;
    uint64 frequencyHz;
    uint64 referenceTicks;
    boolean changed;

    changed = FALSE;
    frequencyHz = image->stmFrequencyHz;
    referenceTicks = image->referenceTicks;

    if (image->magic != ETHSTARTUPTIMING_MAGIC)
    {
        image->magic = ETHSTARTUPTIMING_MAGIC;
        changed = TRUE;
    }

    if (image->version != ETHSTARTUPTIMING_VERSION)
    {
        image->version = ETHSTARTUPTIMING_VERSION;
        changed = TRUE;
    }

    if (image->eventCapacity != ETHSTARTUPTIMING_MAX_EVENTS)
    {
        image->eventCapacity = ETHSTARTUPTIMING_MAX_EVENTS;
        changed = TRUE;
    }

    if (image->stmFrequencyHz == 0u)
    {
        frequencyHz = EthStartupTiming_ReadFrequency();
        image->stmFrequencyHz = frequencyHz;
        changed = TRUE;
    }

    if (referenceTicks == 0u)
    {
        if ((image->events[ETHSTARTUPTIMING_EVENT_REFERENCE].valid != 0u) &&
                (image->events[ETHSTARTUPTIMING_EVENT_REFERENCE].timestampTicks != 0u))
        {
            referenceTicks = image->events[ETHSTARTUPTIMING_EVENT_REFERENCE].timestampTicks;
        }
        else
        {
            referenceTicks = EthTiming_ReadStmTicks();
        }
        image->referenceTicks = referenceTicks;
        changed = TRUE;
    }

    for (eventIndex = 0u; eventIndex < ETHSTARTUPTIMING_MAX_EVENTS; eventIndex++)
    {
        if (image->events[eventIndex].eventId != eventIndex)
        {
            image->events[eventIndex].eventId = eventIndex;
            changed = TRUE;
        }

        (void)frequencyHz;
        (void)referenceTicks;
    }

    if (changed != FALSE)
    {
        EthStartupTiming_MarkDirty();
    }
}

static uint32 EthStartupTiming_MaskWord(EthStartupTiming_EventIdType eventId)
{
    return ((uint32)eventId >= 32u) ? 1u : 0u;
}

static uint32 EthStartupTiming_MaskBit(EthStartupTiming_EventIdType eventId)
{
    return (1UL << ((uint32)eventId & 31u));
}

static boolean EthStartupTiming_HasEvent(
        const EthStartupTiming_NvImageType *image,
        EthStartupTiming_EventIdType eventId)
{
    return ((image->capturedMask[EthStartupTiming_MaskWord(eventId)] &
            EthStartupTiming_MaskBit(eventId)) != 0u) ? TRUE : FALSE;
}

static void EthStartupTiming_UpdateMeasurementState(EthStartupTiming_NvImageType *image)
{
    /* A transmitted ARP only proves that the MAC/DMA path works. Keep the
     * startup window open until the stack, link-facing interface, DoIP and
     * sockets are ready and real diagnostic traffic has traversed both RX and
     * TX paths. PHY-specific events remain useful but are not mandatory: a
     * first DoIP RX is stronger functional proof of an operational link. */
    if ((EthStartupTiming_HasEvent(image, ETHSTARTUPTIMING_EVENT_ETH_STARTUP_COMPLETE) != FALSE) &&
            (EthStartupTiming_HasEvent(image, ETHSTARTUPTIMING_EVENT_NETIF_LINK_UP) != FALSE) &&
            (EthStartupTiming_HasEvent(image, ETHSTARTUPTIMING_EVENT_DOIP_INIT_COMPLETE) != FALSE) &&
            (EthStartupTiming_HasEvent(image, ETHSTARTUPTIMING_EVENT_SOCKET_READY) != FALSE) &&
            (EthStartupTiming_HasEvent(image, ETHSTARTUPTIMING_EVENT_FIRST_DOIP_RX) != FALSE) &&
            (EthStartupTiming_HasEvent(image, ETHSTARTUPTIMING_EVENT_FIRST_TX_SUCCESS) != FALSE))
    {
        image->measurementState = ETHSTARTUPTIMING_STATE_COMPLETE;
    }
    else if (EthStartupTiming_HasEvent(image,
            ETHSTARTUPTIMING_EVENT_FIRST_DMA_TX_COMPLETE) != FALSE)
    {
        image->measurementState = ETHSTARTUPTIMING_STATE_TX_OBSERVED;
    }
    else if (EthStartupTiming_HasEvent(image,
            ETHSTARTUPTIMING_EVENT_FIRST_APP_RX) != FALSE)
    {
        image->measurementState = ETHSTARTUPTIMING_STATE_RX_OBSERVED;
    }
    else if (EthStartupTiming_HasEvent(image,
            ETHSTARTUPTIMING_EVENT_NETIF_LINK_UP) != FALSE)
    {
        image->measurementState = ETHSTARTUPTIMING_STATE_LINK_READY;
    }
}

static void EthStartupTiming_PutU16(uint8 *data, uint16 value)
{
    data[0u] = (uint8)(value >> 8u);
    data[1u] = (uint8)value;
}

static void EthStartupTiming_PutU32(uint8 *data, uint32 value)
{
    data[0u] = (uint8)(value >> 24u);
    data[1u] = (uint8)(value >> 16u);
    data[2u] = (uint8)(value >> 8u);
    data[3u] = (uint8)value;
}

static void EthStartupTiming_PutU64(uint8 *data, uint64 value)
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

void EthStartupTiming_Init(void)
{
    EthStartupTiming_NvImageType *image;
    uint64 nowTicks;
    uint64 frequencyHz;
    uint16 eventIndex;

    if (EthStartupTiming_Initialized != FALSE)
    {
        return;
    }

    image = EthStartupTiming_Image();
    nowTicks = EthTiming_ReadStmTicks();
    frequencyHz = EthStartupTiming_ReadFrequency();

    (void)memset(image, 0, sizeof(*image));
    image->magic = ETHSTARTUPTIMING_MAGIC;
    image->version = ETHSTARTUPTIMING_VERSION;
    image->eventCapacity = ETHSTARTUPTIMING_MAX_EVENTS;
    image->stmFrequencyHz = frequencyHz;
    image->referenceTicks = nowTicks;
    /* A reset starts a new generation. Each event also carries this marker in
     * its reserved field, allowing a reader to reject mixed-generation data. */
    image->reserved[1u] = (uint32)(nowTicks ^ (nowTicks >> 32u));
    if (frequencyHz == 0u)
    {
        image->flags |= ETHSTARTUPTIMING_FLAG_FREQ_INVALID;
    }

    for (eventIndex = 0u; eventIndex < ETHSTARTUPTIMING_MAX_EVENTS; eventIndex++)
    {
        image->events[eventIndex].eventId = eventIndex;
    }

    image->measurementState = ETHSTARTUPTIMING_STATE_IN_PROGRESS;
    image->events[ETHSTARTUPTIMING_EVENT_REFERENCE].valid = 1u;
    image->events[ETHSTARTUPTIMING_EVENT_REFERENCE].timestampTicks = nowTicks;
    image->events[ETHSTARTUPTIMING_EVENT_REFERENCE].reserved = image->reserved[1u];
    image->capturedMask[0u] = EthStartupTiming_MaskBit(ETHSTARTUPTIMING_EVENT_REFERENCE);
    EthStartupTiming_CaptureOpen = TRUE;
    EthStartupTiming_SnapshotPending = FALSE;
    EthStartupTiming_Initialized = TRUE;
    EthStartupTiming_MarkDirty();
}

void EthStartupTiming_EnableNvMStatus(void)
{
    EthStartupTiming_NvMReady = TRUE;
    EthStartupTiming_PublishSnapshot();
}

void EthStartupTiming_OnNvMReadAllComplete(void)
{
    EthStartupTiming_NvMReadAllComplete = TRUE;
    EthStartupTiming_PublishSnapshot();
}

void EthStartupTiming_NotifyReinitialization(void)
{
    EthStartupTiming_CloseCapture(TRUE);
}

void EthStartupTiming_Capture(EthStartupTiming_EventIdType eventId)
{
    EthStartupTiming_CaptureWithMeta(eventId, 0u);
}

void EthStartupTiming_CaptureWithMeta(EthStartupTiming_EventIdType eventId, uint32 metadata)
{
    EthStartupTiming_NvImageType *image;
    uint64 nowTicks;
    uint32 maskWord;
    uint32 mask;
    boolean captured;

    if (eventId >= ETHSTARTUPTIMING_MAX_EVENTS)
    {
        return;
    }

    if (EthStartupTiming_Initialized == FALSE)
    {
        image = EthStartupTiming_Image();
        image->flags |= ETHSTARTUPTIMING_FLAG_EARLY_CAPTURE;
        image->reserved[0u]++;
        return;
    }

    if (EthStartupTiming_CaptureOpen == FALSE)
    {
        return;
    }

    image = EthStartupTiming_Image();
    maskWord = EthStartupTiming_MaskWord(eventId);
    mask = EthStartupTiming_MaskBit(eventId);
    if ((image->capturedMask[maskWord] & mask) != 0u)
    {
        return;
    }

    nowTicks = EthTiming_ReadStmTicks();
    if (IfxCpu_setSpinLock(&EthStartupTiming_Lock, ETHSTARTUPTIMING_SPIN_TIMEOUT) == FALSE)
    {
        image->missedLockCount++;
        return;
    }

    captured = FALSE;
    if ((image->capturedMask[maskWord] & mask) == 0u)
    {
        image->events[eventId].valid = 1u;
        image->events[eventId].timestampTicks = nowTicks;
        image->events[eventId].metadata = metadata;
        image->events[eventId].reserved = image->reserved[1u];
        if (nowTicks < image->referenceTicks)
        {
            image->flags |= ETH_TIMING_FLAG_TIMESTAMP_INVALID;
        }
        image->capturedMask[maskWord] |= mask;
        EthStartupTiming_UpdateMeasurementState(image);
        captured = TRUE;
    }

    IfxCpu_resetSpinLock(&EthStartupTiming_Lock);
    if (captured != FALSE)
    {
        EthStartupTiming_MarkDirty();
        if (image->measurementState == ETHSTARTUPTIMING_STATE_COMPLETE)
        {
            EthStartupTiming_CloseCapture(FALSE);
        }
    }
}

void EthStartupTiming_CaptureFirstRx(uint32 rxClass)
{
    EthStartupTiming_NvImageType *image;

    if (EthStartupTiming_Initialized == FALSE)
    {
        EthStartupTiming_Image()->flags |= ETHSTARTUPTIMING_FLAG_EARLY_CAPTURE;
        EthStartupTiming_Image()->reserved[0u]++;
        return;
    }

    if (EthStartupTiming_CaptureOpen == FALSE)
    {
        return;
    }

    image = EthStartupTiming_Image();
    if (image->firstRxClass == ETHSTARTUPTIMING_RX_NONE)
    {
        image->firstRxClass = rxClass;
    }
    EthStartupTiming_CaptureWithMeta(ETHSTARTUPTIMING_EVENT_FIRST_APP_RX, rxClass);
}

void EthStartupTiming_CaptureFirstTxSubmit(void)
{
    if ((EthStartupTiming_CaptureOpen != FALSE) &&
            (EthStartupTiming_FirstTxSubmitTicks == 0u))
    {
        EthStartupTiming_FirstTxSubmitTicks = EthTiming_ReadStmTicks();
        EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_FIRST_DMA_TX_SUBMIT);
        EthStartupTiming_Image()->flags |= ETHSTARTUPTIMING_FLAG_TX_SUBMIT_VALID;
        EthStartupTiming_MarkDirty();
    }
}

void EthStartupTiming_CaptureFirstTxComplete(void)
{
    if ((EthStartupTiming_CaptureOpen != FALSE) &&
            (EthStartupTiming_FirstTxCompleteTicks == 0u))
    {
        EthStartupTiming_FirstTxCompleteTicks = EthTiming_ReadStmTicks();
        EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_FIRST_DMA_TX_COMPLETE);
        EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_FIRST_TX_SUCCESS);
        EthStartupTiming_Image()->flags |= ETHSTARTUPTIMING_FLAG_TX_COMPLETE_VALID;
        EthStartupTiming_MarkDirty();
    }
}

boolean EthStartupTiming_IsRoutineId(uint16 routineId)
{
    return (routineId == ETHSTARTUPTIMING_ROUTINE_ID) ? TRUE : FALSE;
}

Dcm_ReturnType EthStartupTiming_HandleRoutineControl(
        Dcm_OpStatusType opStatus,
        uint8 routineControlType,
        uint16 routineId,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    EthStartupTiming_NvImageType *image;
    uint8 startIndex;
    uint8 requestedCount;
    uint8 returnedCount;
    uint8 maxCount;
    uint8 index;
    uint16 offset;

    (void)opStatus;

    if (routineId != ETHSTARTUPTIMING_ROUTINE_ID)
    {
        return DCM_NRC_REQUEST_OUT_OF_RANGE;
    }

    if ((routineControlType != ETHSTARTUPTIMING_ROUTINE_START) &&
        (routineControlType != ETHSTARTUPTIMING_ROUTINE_REQUEST_RESULTS))
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }

    if ((reqLen != 0u) && (reqLen != 1u) && (reqLen != 2u))
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    if (EthStartupTiming_Initialized == FALSE)
    {
        EthStartupTiming_Init();
    }

    image = EthStartupTiming_Image();
    EthStartupTiming_NormalizeMetadata(image);
    startIndex = (reqLen >= 1u) ? reqData[0u] : 0u;
    requestedCount = (reqLen >= 2u) ? reqData[1u] : ETHSTARTUPTIMING_MAX_RESPONSE_ENTRIES;
    if (startIndex >= ETHSTARTUPTIMING_MAX_EVENTS)
    {
        return DCM_NRC_REQUEST_OUT_OF_RANGE;
    }

    maxCount = (requestedCount > ETHSTARTUPTIMING_MAX_RESPONSE_ENTRIES) ?
            ETHSTARTUPTIMING_MAX_RESPONSE_ENTRIES : requestedCount;
    if ((uint16)startIndex + (uint16)maxCount > ETHSTARTUPTIMING_MAX_EVENTS)
    {
        maxCount = (uint8)(ETHSTARTUPTIMING_MAX_EVENTS - startIndex);
    }

    EthStartupTiming_PutU32(&respData[0u], image->magic);
    EthStartupTiming_PutU16(&respData[4u], image->version);
    EthStartupTiming_PutU16(&respData[6u], image->eventCapacity);
    EthStartupTiming_PutU64(&respData[8u], image->stmFrequencyHz);
    EthStartupTiming_PutU64(&respData[16u], image->referenceTicks);
    respData[24u] = startIndex;
    respData[25u] = maxCount;
    respData[26u] = (uint8)(image->flags & 0xFFu);
    respData[27u] = (uint8)(image->missedLockCount & 0xFFu);

    offset = ETHSTARTUPTIMING_RESPONSE_HEADER_LEN;
    returnedCount = 0u;
    for (index = 0u; index < maxCount; index++)
    {
        EthStartupTiming_NvEventType *event = &image->events[(uint16)startIndex + index];
        EthStartupTiming_PutU16(&respData[offset + 0u], event->eventId);
        EthStartupTiming_PutU16(&respData[offset + 2u], event->valid);
        EthStartupTiming_PutU64(&respData[offset + 4u], event->timestampTicks);
        EthStartupTiming_PutU32(&respData[offset + 12u], event->metadata);
        EthStartupTiming_PutU32(&respData[offset + 16u], event->reserved);
        offset = (uint16)(offset + ETHSTARTUPTIMING_RESPONSE_ENTRY_LEN);
        returnedCount++;
    }

    respData[25u] = returnedCount;
    *respLen = offset;
    return DCM_E_OK;
}
