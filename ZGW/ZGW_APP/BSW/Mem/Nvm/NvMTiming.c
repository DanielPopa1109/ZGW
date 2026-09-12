#include "BSW/Mem/Nvm/NvMTiming.h"

#include <string.h>

#include "IfxCpu.h"
#include "IfxStm.h"

#define NVMTIMING_SPIN_TIMEOUT                  (1000u)
#define NVMTIMING_ROUTINE_START                 (0x01u)
#define NVMTIMING_ROUTINE_REQUEST_RESULTS       (0x03u)
#define NVMTIMING_SELECTOR_SUMMARY              (0x00u)
#define NVMTIMING_SELECTOR_STARTUP              (0x01u)
#define NVMTIMING_SELECTOR_READALL              (0x02u)
#define NVMTIMING_SELECTOR_WRITEALL             (0x03u)
#define NVMTIMING_SELECTOR_HISTORY_META         (0x04u)
#define NVMTIMING_SELECTOR_HISTORY_ENTRY_BASE   (0x10u)
#define NVMTIMING_RESPONSE_ENTRY_LEN            (16u)
#define NVMTIMING_RESPONSE_MAX_EVENTS           (14u)
#define NVMTIMING_FLAG_FREQ_INVALID             (0x00000001u)
#define NVMTIMING_FLAG_WRITEALL_POST_PERSISTED  (0x00000002u)
#define NVMTIMING_FLAG_WRITEALL_PREVIOUS_BOOT   (0x00000004u)
#define NVMTIMING_SB_FLAG_ACCEPTED              (0x00000001u)
#define NVMTIMING_SB_FLAG_FIRST_MAIN            (0x00000002u)
#define NVMTIMING_SB_FLAG_PROCESSING            (0x00000004u)
#define NVMTIMING_SB_FLAG_MEMIF                 (0x00000008u)
#define NVMTIMING_SB_FLAG_FEE                   (0x00000010u)
#define NVMTIMING_SB_FLAG_FLS                   (0x00000020u)
#define NVMTIMING_SB_FLAG_FLS_COMPLETE          (0x00000040u)
#define NVMTIMING_SB_FLAG_COMPLETE              (0x00000080u)
#define NVMTIMING_MB_FLAG_FIRST_MAIN            (0x00000001u)
#define NVMTIMING_MB_FLAG_FIRST_BLOCK           (0x00000002u)
#define NVMTIMING_MB_FLAG_FIRST_MEMIF           (0x00000004u)
#define NVMTIMING_MB_FLAG_FIRST_FEE             (0x00000008u)
#define NVMTIMING_MB_FLAG_FIRST_FLS             (0x00000010u)
#define NVMTIMING_MB_FLAG_LAST_FLS_COMPLETE     (0x00000020u)
#define NVMTIMING_MB_FLAG_COMPLETE              (0x00000040u)

uint8 NvM_NvMTiming_Ram[NVMTIMING_NVM_IMAGE_SIZE];
const uint8 NvM_NvMTiming_Rom[NVMTIMING_NVM_IMAGE_SIZE] =
{
    0x4Eu, 0x56u, 0x4Du, 0x54u
};

static IfxCpu_spinLock NvMTiming_Lock;
static uint16 NvMTiming_ActiveSingleIndex = 0xFFFFu;
static uint8 NvMTiming_ActiveSingleOperation;
static uint16 NvMTiming_ActiveSingleBlockId;
static uint8 NvMTiming_ActiveMultiOperation;
static uint32 NvMTiming_NvMMainCounter;
static uint32 NvMTiming_FeeMainCounter;
static uint32 NvMTiming_FlsMainCounter;
static NvMTiming_NvImageType NvMTiming_ActiveImage;

static NvMTiming_NvImageType *NvMTiming_Image(void);
static uint64 NvMTiming_Now(void);
static uint64 NvMTiming_Frequency(void);
static uint32 NvMTiming_ElapsedUs(uint64 ticks);
static void NvMTiming_Touch(void);
static void NvMTiming_LockEnter(void);
static void NvMTiming_LockExit(void);
static void NvMTiming_PutU16(uint8 *data, uint16 value);
static void NvMTiming_PutU32(uint8 *data, uint32 value);
static void NvMTiming_PutU64(uint8 *data, uint64 value);
static void NvMTiming_RecordStartup(uint16 eventId);
static NvMTiming_MultiBlockType *NvMTiming_Multi(uint8 operation);
static NvMTiming_SingleBlockType *NvMTiming_ActiveSingle(void);
static void NvMTiming_UpdateStats(uint8 operation, uint64 totalTicks);
static Dcm_ReturnType NvMTiming_WriteEventPage(const uint8 *reqData, Dcm_PduLengthType reqLen, uint8 *respData, Dcm_PduLengthType *respLen);
static void NvMTiming_WriteMulti(const NvMTiming_MultiBlockType *multi, uint8 *respData, Dcm_PduLengthType *respLen);
static void NvMTiming_WriteSingle(const NvMTiming_SingleBlockType *single, uint8 *respData, Dcm_PduLengthType *respLen);

static NvMTiming_NvImageType *NvMTiming_Image(void)
{
    return &NvMTiming_ActiveImage;
}

static uint64 NvMTiming_Now(void)
{
    return IfxStm_get(&MODULE_STM0);
}

static uint64 NvMTiming_Frequency(void)
{
    return (uint64)IfxStm_getFrequency(&MODULE_STM0);
}

static uint32 NvMTiming_ElapsedUs(uint64 ticks)
{
    NvMTiming_NvImageType *image = NvMTiming_Image();
    uint64 delta;
    uint64 us;

    if ((image->stmFrequencyHz == 0u) || (ticks < image->bootReferenceTicks))
    {
        return 0u;
    }
    delta = ticks - image->bootReferenceTicks;
    us = (delta * 1000000u) / image->stmFrequencyHz;
    return (us > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32)us;
}

static void NvMTiming_Touch(void)
{
    NvMTiming_Image()->dirtyCounter++;
}

static void NvMTiming_LockEnter(void)
{
    (void)IfxCpu_setSpinLock(&NvMTiming_Lock, NVMTIMING_SPIN_TIMEOUT);
}

static void NvMTiming_LockExit(void)
{
    IfxCpu_resetSpinLock(&NvMTiming_Lock);
}

static void NvMTiming_PutU16(uint8 *data, uint16 value)
{
    data[0u] = (uint8)(value >> 8u);
    data[1u] = (uint8)value;
}

static void NvMTiming_PutU32(uint8 *data, uint32 value)
{
    data[0u] = (uint8)(value >> 24u);
    data[1u] = (uint8)(value >> 16u);
    data[2u] = (uint8)(value >> 8u);
    data[3u] = (uint8)value;
}

static void NvMTiming_PutU64(uint8 *data, uint64 value)
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

void NvMTiming_BootReference(void)
{
    NvMTiming_NvImageType *image = NvMTiming_Image();
    uint64 now = NvMTiming_Now();
    uint64 freq = NvMTiming_Frequency();
    uint16 i;

    NvMTiming_LockEnter();
    (void)memset(image, 0, sizeof(*image));
    image->magic = NVMTIMING_MAGIC;
    image->version = NVMTIMING_VERSION;
    image->historySize = NVMTIMING_HISTORY_SIZE;
    image->stmFrequencyHz = freq;
    image->bootReferenceTicks = now;
    image->flags = 0u;
    if (freq == 0u)
    {
        image->flags |= NVMTIMING_FLAG_FREQ_INVALID;
    }
    for (i = 0u; i < NVMTIMING_STARTUP_EVENT_COUNT; i++)
    {
        image->startup[i].eventId = i;
    }
    image->startup[NVMTIMING_STARTUP_BOOT_REFERENCE].valid = 1u;
    image->startup[NVMTIMING_STARTUP_BOOT_REFERENCE].ticks = now;
    image->startup[NVMTIMING_STARTUP_UNINIT_STATE].eventId = NVMTIMING_STARTUP_UNINIT_STATE;
    image->startup[NVMTIMING_STARTUP_UNINIT_STATE].valid = 1u;
    image->startup[NVMTIMING_STARTUP_UNINIT_STATE].ticks = now;
    NvMTiming_Touch();
    NvMTiming_LockExit();
}

static void NvMTiming_RecordStartup(uint16 eventId)
{
    NvMTiming_NvImageType *image = NvMTiming_Image();
    uint64 now;

    if (eventId >= NVMTIMING_STARTUP_EVENT_COUNT)
    {
        return;
    }
    now = NvMTiming_Now();
    NvMTiming_LockEnter();
    if (image->magic != NVMTIMING_MAGIC)
    {
        NvMTiming_LockExit();
        NvMTiming_BootReference();
        NvMTiming_LockEnter();
    }
    if (image->startup[eventId].valid == 0u)
    {
        image->startup[eventId].eventId = eventId;
        image->startup[eventId].valid = 1u;
        image->startup[eventId].ticks = now;
        image->startup[eventId].elapsedUs = NvMTiming_ElapsedUs(now);
        NvMTiming_Touch();
    }
    NvMTiming_LockExit();
}

void NvMTiming_StartupEvent(uint16 eventId)
{
    NvMTiming_RecordStartup(eventId);
}

void NvMTiming_InitEnter(void) { NvMTiming_RecordStartup(NVMTIMING_STARTUP_INIT_ENTER); }
void NvMTiming_InitExit(void) { NvMTiming_RecordStartup(NVMTIMING_STARTUP_INIT_EXIT); }
void NvMTiming_SetNvMReady(void) { NvMTiming_RecordStartup(NVMTIMING_STARTUP_NVM_READY); }
void NvMTiming_ReadAllRequest(void) { NvMTiming_RecordStartup(NVMTIMING_STARTUP_READALL_REQUEST); }
void NvMTiming_WriteAllRequest(void) { NvMTiming_RecordStartup(NVMTIMING_STARTUP_WRITEALL_REQUEST); }

static NvMTiming_MultiBlockType *NvMTiming_Multi(uint8 operation)
{
    NvMTiming_NvImageType *image = NvMTiming_Image();
    if (operation == NVMTIMING_OP_READ_ALL)
    {
        return &image->lastReadAll;
    }
    if (operation == NVMTIMING_OP_WRITE_ALL)
    {
        return &image->lastWriteAll;
    }
    return NULL_PTR;
}

void NvMTiming_MultiProcessing(uint8 operation, uint16 blockId)
{
    NvMTiming_MultiBlockType *multi = NvMTiming_Multi(operation);
    uint64 now = NvMTiming_Now();
    if (multi == NULL_PTR) { return; }
    NvMTiming_LockEnter();
    if ((multi->flags & NVMTIMING_MB_FLAG_FIRST_MAIN) == 0u)
    {
        multi->firstMainTicks = now;
        multi->nvmMainCycles = NvMTiming_NvMMainCounter;
        multi->flags |= NVMTIMING_MB_FLAG_FIRST_MAIN;
    }
    if ((multi->flags & NVMTIMING_MB_FLAG_FIRST_BLOCK) == 0u)
    {
        multi->firstBlockTicks = now;
        multi->activeBlockId = blockId;
        multi->flags |= NVMTIMING_MB_FLAG_FIRST_BLOCK;
    }
    NvMTiming_LockExit();
}

void NvMTiming_MultiMemIfRequest(uint8 operation, uint16 blockId)
{
    NvMTiming_MultiBlockType *multi = NvMTiming_Multi(operation);
    uint64 now = NvMTiming_Now();
    if (multi == NULL_PTR) { return; }
    NvMTiming_LockEnter();
    if ((multi->flags & NVMTIMING_MB_FLAG_FIRST_MEMIF) == 0u)
    {
        multi->firstMemIfTicks = now;
        multi->activeBlockId = blockId;
        multi->flags |= NVMTIMING_MB_FLAG_FIRST_MEMIF;
    }
    NvMTiming_LockExit();
}

void NvMTiming_MultiCounters(
        uint8 operation,
        uint16 planned,
        uint16 started,
        uint16 done,
        uint16 failed,
        uint16 skipped)
{
    NvMTiming_MultiBlockType *multi = NvMTiming_Multi(operation);
    if (multi == NULL_PTR) { return; }
    NvMTiming_LockEnter();
    multi->blocksPlanned = planned;
    multi->blocksStarted = started;
    multi->blocksDone = done;
    multi->blocksFailed = failed;
    multi->blocksSkipped = skipped;
    NvMTiming_LockExit();
}

void NvMTiming_MultiComplete(uint8 operation, uint8 result, uint8 memIfResult)
{
    NvMTiming_MultiBlockType *multi = NvMTiming_Multi(operation);
    uint64 now = NvMTiming_Now();
    if (multi == NULL_PTR) { return; }
    NvMTiming_LockEnter();
    multi->completionTicks = now;
    multi->result = result;
    multi->memIfResult = memIfResult;
    multi->nvmMainCycles = NvMTiming_NvMMainCounter - multi->nvmMainCycles;
    multi->feeMainCycles = NvMTiming_FeeMainCounter - multi->feeMainCycles;
    multi->flsMainCycles = NvMTiming_FlsMainCounter - multi->flsMainCycles;
    multi->flags |= NVMTIMING_MB_FLAG_COMPLETE;
    if (NvMTiming_ActiveMultiOperation == operation)
    {
        NvMTiming_ActiveMultiOperation = NVMTIMING_OP_NONE;
    }
    NvMTiming_Touch();
    NvMTiming_LockExit();
}

void NvMTiming_StartSingle(uint8 operation, uint16 blockId, Std_ReturnType accepted, uint8 result)
{
    NvMTiming_NvImageType *image = NvMTiming_Image();
    NvMTiming_SingleBlockType *entry;
    uint16 index;

    NvMTiming_LockEnter();
    index = (uint16)(image->historyWriteIndex % NVMTIMING_HISTORY_SIZE);
    entry = &image->history[index];
    (void)memset(entry, 0, sizeof(*entry));
    entry->operation = operation;
    entry->blockId = blockId;
    entry->result = result;
    entry->sequence = (uint16)(++image->sequence);
    entry->requestTicks = NvMTiming_Now();
    if (accepted == E_OK)
    {
        entry->acceptedTicks = entry->requestTicks;
        entry->flags |= NVMTIMING_SB_FLAG_ACCEPTED;
        NvMTiming_ActiveSingleIndex = index;
        NvMTiming_ActiveSingleOperation = operation;
        NvMTiming_ActiveSingleBlockId = blockId;
    }
    else
    {
        entry->completionTicks = entry->requestTicks;
        entry->flags |= NVMTIMING_SB_FLAG_COMPLETE;
        image->historyWriteIndex++;
    }
    NvMTiming_Touch();
    NvMTiming_LockExit();
}

static NvMTiming_SingleBlockType *NvMTiming_ActiveSingle(void)
{
    NvMTiming_NvImageType *image = NvMTiming_Image();
    if (NvMTiming_ActiveSingleIndex >= NVMTIMING_HISTORY_SIZE)
    {
        return NULL_PTR;
    }
    return &image->history[NvMTiming_ActiveSingleIndex];
}

void NvMTiming_SingleProcessing(uint16 blockId)
{
    NvMTiming_SingleBlockType *entry = NvMTiming_ActiveSingle();
    if ((entry == NULL_PTR) || (entry->blockId != blockId)) { return; }
    NvMTiming_LockEnter();
    if ((entry->flags & NVMTIMING_SB_FLAG_FIRST_MAIN) == 0u)
    {
        entry->firstMainTicks = NvMTiming_Now();
        entry->nvmMainCycles = NvMTiming_NvMMainCounter;
        entry->feeMainCycles = NvMTiming_FeeMainCounter;
        entry->flsMainCycles = NvMTiming_FlsMainCounter;
        entry->flags |= NVMTIMING_SB_FLAG_FIRST_MAIN;
    }
    if ((entry->flags & NVMTIMING_SB_FLAG_PROCESSING) == 0u)
    {
        entry->processingTicks = entry->firstMainTicks;
        entry->flags |= NVMTIMING_SB_FLAG_PROCESSING;
    }
    NvMTiming_LockExit();
}

void NvMTiming_SingleMemIfRequest(uint16 blockId)
{
    NvMTiming_SingleBlockType *entry = NvMTiming_ActiveSingle();
    if ((entry == NULL_PTR) || (entry->blockId != blockId)) { return; }
    NvMTiming_LockEnter();
    if ((entry->flags & NVMTIMING_SB_FLAG_MEMIF) == 0u)
    {
        entry->memIfRequestTicks = NvMTiming_Now();
        entry->flags |= NVMTIMING_SB_FLAG_MEMIF;
    }
    NvMTiming_LockExit();
}

void NvMTiming_SingleComplete(uint16 blockId, uint8 result, uint8 memIfResult)
{
    NvMTiming_NvImageType *image = NvMTiming_Image();
    NvMTiming_SingleBlockType *entry = NvMTiming_ActiveSingle();
    uint64 totalTicks;
    if ((entry == NULL_PTR) || (entry->blockId != blockId)) { return; }
    NvMTiming_LockEnter();
    entry->completionTicks = NvMTiming_Now();
    entry->result = result;
    entry->memIfResult = memIfResult;
    entry->nvmMainCycles = NvMTiming_NvMMainCounter - entry->nvmMainCycles;
    entry->feeMainCycles = NvMTiming_FeeMainCounter - entry->feeMainCycles;
    entry->flsMainCycles = NvMTiming_FlsMainCounter - entry->flsMainCycles;
    entry->flags |= NVMTIMING_SB_FLAG_COMPLETE;
    totalTicks = entry->completionTicks - entry->requestTicks;
    NvMTiming_UpdateStats(entry->operation, totalTicks);
    image->historyWriteIndex++;
    NvMTiming_ActiveSingleIndex = 0xFFFFu;
    NvMTiming_ActiveSingleOperation = NVMTIMING_OP_NONE;
    NvMTiming_ActiveSingleBlockId = 0u;
    NvMTiming_Touch();
    NvMTiming_LockExit();
}

void NvMTiming_SetRamStatus(uint16 blockId, boolean changed)
{
    /* This API is synchronous bookkeeping, not an NvM job. Recording every
     * call displaced all useful asynchronous read/write history entries. */
    (void)blockId;
    (void)changed;
}

void NvMTiming_FeeRequest(uint8 operation, uint16 blockId)
{
    uint64 now = NvMTiming_Now();
    NvMTiming_SingleBlockType *single = NvMTiming_ActiveSingle();
    NvMTiming_MultiBlockType *multi = NvMTiming_Multi(NvMTiming_ActiveMultiOperation);
    NvMTiming_LockEnter();
    if ((single != NULL_PTR) && (single->blockId == blockId) && ((single->flags & NVMTIMING_SB_FLAG_FEE) == 0u))
    {
        single->feeRequestTicks = now;
        single->flags |= NVMTIMING_SB_FLAG_FEE;
    }
    if ((multi != NULL_PTR) && ((multi->flags & NVMTIMING_MB_FLAG_FIRST_FEE) == 0u))
    {
        multi->firstFeeTicks = now;
        multi->flags |= NVMTIMING_MB_FLAG_FIRST_FEE;
    }
    NvMTiming_LockExit();
    if (NvMTiming_ActiveMultiOperation == NVMTIMING_OP_READ_ALL)
    {
        NvMTiming_RecordStartup(NVMTIMING_STARTUP_READALL_FIRST_FEE);
    }
    (void)operation;
}

void NvMTiming_FeeComplete(uint8 memIfResult)
{
    NvMTiming_MultiBlockType *multi = NvMTiming_Multi(NvMTiming_ActiveMultiOperation);
    if (multi != NULL_PTR)
    {
        multi->lastFeeCompleteTicks = NvMTiming_Now();
        if (NvMTiming_ActiveMultiOperation == NVMTIMING_OP_READ_ALL)
        {
            NvMTiming_RecordStartup(NVMTIMING_STARTUP_READALL_LAST_FEE_COMPLETE);
        }
    }
    (void)memIfResult;
}

void NvMTiming_FlsRequest(uint8 operation, uint32 address, uint32 length)
{
    uint64 now = NvMTiming_Now();
    NvMTiming_SingleBlockType *single = NvMTiming_ActiveSingle();
    NvMTiming_MultiBlockType *multi = NvMTiming_Multi(NvMTiming_ActiveMultiOperation);
    NvMTiming_LockEnter();
    if ((single != NULL_PTR) && ((single->flags & NVMTIMING_SB_FLAG_FLS) == 0u))
    {
        single->flsRequestTicks = now;
        single->flags |= NVMTIMING_SB_FLAG_FLS;
    }
    if ((multi != NULL_PTR) && ((multi->flags & NVMTIMING_MB_FLAG_FIRST_FLS) == 0u))
    {
        multi->firstFlsTicks = now;
        multi->flags |= NVMTIMING_MB_FLAG_FIRST_FLS;
    }
    NvMTiming_LockExit();
    if (NvMTiming_ActiveMultiOperation == NVMTIMING_OP_READ_ALL)
    {
        NvMTiming_RecordStartup(NVMTIMING_STARTUP_READALL_FIRST_FLS);
    }
    (void)operation;
    (void)address;
    (void)length;
}

void NvMTiming_FlsComplete(uint8 memIfResult)
{
    uint64 now = NvMTiming_Now();
    NvMTiming_SingleBlockType *single = NvMTiming_ActiveSingle();
    NvMTiming_MultiBlockType *multi = NvMTiming_Multi(NvMTiming_ActiveMultiOperation);
    NvMTiming_LockEnter();
    if ((single != NULL_PTR) && ((single->flags & NVMTIMING_SB_FLAG_FLS_COMPLETE) == 0u))
    {
        single->flsCompleteTicks = now;
        single->flags |= NVMTIMING_SB_FLAG_FLS_COMPLETE;
    }
    if (multi != NULL_PTR)
    {
        multi->lastFlsCompleteTicks = now;
        multi->flags |= NVMTIMING_MB_FLAG_LAST_FLS_COMPLETE;
    }
    NvMTiming_LockExit();
    if (NvMTiming_ActiveMultiOperation == NVMTIMING_OP_READ_ALL)
    {
        NvMTiming_RecordStartup(NVMTIMING_STARTUP_READALL_LAST_FLS_COMPLETE);
    }
    (void)memIfResult;
}

void NvMTiming_NvMMainCycle(void) { NvMTiming_NvMMainCounter++; }
void NvMTiming_FeeMainCycle(void) { NvMTiming_FeeMainCounter++; }
void NvMTiming_FlsMainCycle(void) { NvMTiming_FlsMainCounter++; }
boolean NvMTiming_IsDirty(void) { return (NvMTiming_Image()->dirtyCounter != 0u) ? TRUE : FALSE; }
void NvMTiming_ClearDirty(void) { NvMTiming_Image()->dirtyCounter = 0u; }

void NvMTiming_LoadPersistedWriteAll(void)
{
    const NvMTiming_NvImageType *persisted =
            (const NvMTiming_NvImageType *)NvM_NvMTiming_Ram;

    if ((persisted->magic == NVMTIMING_MAGIC) &&
            (persisted->version == NVMTIMING_VERSION) &&
            (persisted->historySize == NVMTIMING_HISTORY_SIZE) &&
            (persisted->lastWriteAll.operation == NVMTIMING_OP_WRITE_ALL) &&
            ((persisted->lastWriteAll.flags & NVMTIMING_MB_FLAG_COMPLETE) != 0u) &&
            (NvMTiming_Image()->lastWriteAll.operation != NVMTIMING_OP_WRITE_ALL))
    {
        NvMTiming_Image()->lastWriteAll = persisted->lastWriteAll;
        NvMTiming_Image()->flags |= (NVMTIMING_FLAG_WRITEALL_POST_PERSISTED |
                NVMTIMING_FLAG_WRITEALL_PREVIOUS_BOOT);
    }
}

void NvMTiming_PreparePersistedImage(void)
{
    NvMTiming_NvImageType *image = NvMTiming_Image();

    image->flags &= ~NVMTIMING_FLAG_WRITEALL_PREVIOUS_BOOT;
    image->flags |= NVMTIMING_FLAG_WRITEALL_POST_PERSISTED;
    image->dirtyCounter = 0u;
    (void)memcpy(NvM_NvMTiming_Ram, image, sizeof(*image));
}

void NvMTiming_PersistFailed(void)
{
    NvMTiming_Image()->flags &= ~NVMTIMING_FLAG_WRITEALL_POST_PERSISTED;
    NvMTiming_Touch();
}

void NvMTiming_BeginMulti(uint8 operation)
{
    NvMTiming_MultiBlockType *multi = NvMTiming_Multi(operation);
    if (multi == NULL_PTR) { return; }
    NvMTiming_LockEnter();
    (void)memset(multi, 0, sizeof(*multi));
    multi->operation = operation;
    multi->sequence = (uint16)(++NvMTiming_Image()->sequence);
    multi->requestTicks = NvMTiming_Now();
    multi->nvmMainCycles = NvMTiming_NvMMainCounter;
    multi->feeMainCycles = NvMTiming_FeeMainCounter;
    multi->flsMainCycles = NvMTiming_FlsMainCounter;
    NvMTiming_ActiveMultiOperation = operation;
    NvMTiming_Touch();
    NvMTiming_LockExit();
}

static void NvMTiming_UpdateStats(uint8 operation, uint64 totalTicks)
{
    NvMTiming_StatsType *stats = NULL_PTR;
    if (operation == NVMTIMING_OP_READ_BLOCK)
    {
        stats = &NvMTiming_Image()->readBlockStats;
    }
    else if (operation == NVMTIMING_OP_WRITE_BLOCK)
    {
        stats = &NvMTiming_Image()->writeBlockStats;
    }
    if (stats == NULL_PTR) { return; }
    if ((stats->count == 0u) || (totalTicks < stats->minTicks)) { stats->minTicks = totalTicks; }
    if (totalTicks > stats->maxTicks) { stats->maxTicks = totalTicks; }
    stats->totalTicks += totalTicks;
    stats->count++;
}

boolean NvMTiming_IsRoutineId(uint16 routineId)
{
    return (routineId == NVMTIMING_ROUTINE_ID) ? TRUE : FALSE;
}

static Dcm_ReturnType NvMTiming_WriteEventPage(const uint8 *reqData, Dcm_PduLengthType reqLen, uint8 *respData, Dcm_PduLengthType *respLen)
{
    NvMTiming_NvImageType *image = NvMTiming_Image();
    uint8 start = (reqLen >= 2u) ? reqData[1u] : 0u;
    uint8 count = (reqLen >= 3u) ? reqData[2u] : NVMTIMING_RESPONSE_MAX_EVENTS;
    uint8 returned = 0u;
    uint16 offset = 14u;

    if (start >= NVMTIMING_STARTUP_EVENT_COUNT) { return DCM_NRC_REQUEST_OUT_OF_RANGE; }
    if (count > NVMTIMING_RESPONSE_MAX_EVENTS) { count = NVMTIMING_RESPONSE_MAX_EVENTS; }
    if ((uint16)start + count > NVMTIMING_STARTUP_EVENT_COUNT)
    {
        count = (uint8)(NVMTIMING_STARTUP_EVENT_COUNT - start);
    }
    NvMTiming_PutU16(&respData[0u], image->version);
    NvMTiming_PutU16(&respData[2u], NVMTIMING_STARTUP_EVENT_COUNT);
    NvMTiming_PutU64(&respData[4u], image->stmFrequencyHz);
    respData[12u] = start;
    respData[13u] = count;
    for (returned = 0u; returned < count; returned++)
    {
        NvMTiming_EventType *event = &image->startup[start + returned];
        respData[offset + 0u] = (uint8)event->eventId;
        respData[offset + 1u] = (uint8)((event->valid != 0u) ? 1u : 0u);
        NvMTiming_PutU16(&respData[offset + 2u], event->valid);
        NvMTiming_PutU64(&respData[offset + 4u], event->ticks);
        NvMTiming_PutU32(&respData[offset + 12u], event->elapsedUs);
        offset = (uint16)(offset + NVMTIMING_RESPONSE_ENTRY_LEN);
    }
    respData[13u] = returned;
    *respLen = offset;
    return DCM_E_OK;
}

static void NvMTiming_WriteMulti(const NvMTiming_MultiBlockType *m, uint8 *r, Dcm_PduLengthType *len)
{
    r[0u] = m->operation; r[1u] = m->result; r[2u] = m->memIfResult; r[3u] = 0u;
    NvMTiming_PutU16(&r[4u], m->sequence); NvMTiming_PutU16(&r[6u], m->activeBlockId);
    NvMTiming_PutU64(&r[8u], m->requestTicks); NvMTiming_PutU64(&r[16u], m->firstMainTicks);
    NvMTiming_PutU64(&r[24u], m->firstBlockTicks); NvMTiming_PutU64(&r[32u], m->firstMemIfTicks);
    NvMTiming_PutU64(&r[40u], m->firstFeeTicks); NvMTiming_PutU64(&r[48u], m->firstFlsTicks);
    NvMTiming_PutU64(&r[56u], m->lastFlsCompleteTicks); NvMTiming_PutU64(&r[64u], m->lastFeeCompleteTicks);
    NvMTiming_PutU64(&r[72u], m->completionTicks);
    NvMTiming_PutU32(&r[80u], m->nvmMainCycles); NvMTiming_PutU32(&r[84u], m->feeMainCycles);
    NvMTiming_PutU32(&r[88u], m->flsMainCycles);
    NvMTiming_PutU16(&r[92u], m->blocksPlanned); NvMTiming_PutU16(&r[94u], m->blocksStarted);
    NvMTiming_PutU16(&r[96u], m->blocksDone); NvMTiming_PutU16(&r[98u], m->blocksFailed);
    NvMTiming_PutU16(&r[100u], m->blocksSkipped); NvMTiming_PutU16(&r[102u], 0u);
    NvMTiming_PutU32(&r[104u], m->flags);
    *len = 108u;
}

static void NvMTiming_WriteSingle(const NvMTiming_SingleBlockType *s, uint8 *r, Dcm_PduLengthType *len)
{
    r[0u] = s->operation; r[1u] = s->result; r[2u] = s->memIfResult; r[3u] = 0u;
    NvMTiming_PutU16(&r[4u], s->blockId); NvMTiming_PutU16(&r[6u], s->sequence);
    NvMTiming_PutU64(&r[8u], s->requestTicks); NvMTiming_PutU64(&r[16u], s->acceptedTicks);
    NvMTiming_PutU64(&r[24u], s->firstMainTicks); NvMTiming_PutU64(&r[32u], s->processingTicks);
    NvMTiming_PutU64(&r[40u], s->memIfRequestTicks); NvMTiming_PutU64(&r[48u], s->feeRequestTicks);
    NvMTiming_PutU64(&r[56u], s->flsRequestTicks); NvMTiming_PutU64(&r[64u], s->flsCompleteTicks);
    NvMTiming_PutU64(&r[72u], s->completionTicks);
    NvMTiming_PutU32(&r[80u], s->nvmMainCycles); NvMTiming_PutU32(&r[84u], s->feeMainCycles);
    NvMTiming_PutU32(&r[88u], s->flsMainCycles); NvMTiming_PutU32(&r[92u], s->flags);
    *len = 96u;
}

Dcm_ReturnType NvMTiming_HandleRoutineControl(
        Dcm_OpStatusType opStatus,
        uint8 routineControlType,
        uint16 routineId,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    NvMTiming_NvImageType *image = NvMTiming_Image();
    uint8 selector;
    uint8 index;

    (void)opStatus;
    if (routineId != NVMTIMING_ROUTINE_ID) { return DCM_NRC_REQUEST_OUT_OF_RANGE; }
    if ((routineControlType != NVMTIMING_ROUTINE_START) && (routineControlType != NVMTIMING_ROUTINE_REQUEST_RESULTS))
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }
    selector = (reqLen >= 1u) ? reqData[0u] : NVMTIMING_SELECTOR_SUMMARY;
    if (image->magic != NVMTIMING_MAGIC)
    {
        NvMTiming_BootReference();
    }
    respData[0u] = selector;
    if (selector == NVMTIMING_SELECTOR_SUMMARY)
    {
        NvMTiming_PutU32(&respData[1u], image->magic);
        NvMTiming_PutU16(&respData[5u], image->version);
        NvMTiming_PutU16(&respData[7u], image->historySize);
        NvMTiming_PutU64(&respData[9u], image->stmFrequencyHz);
        NvMTiming_PutU64(&respData[17u], image->bootReferenceTicks);
        NvMTiming_PutU32(&respData[25u], image->flags);
        NvMTiming_PutU32(&respData[29u], image->historyWriteIndex);
        NvMTiming_PutU32(&respData[33u], image->readBlockStats.count);
        NvMTiming_PutU64(&respData[37u], image->readBlockStats.minTicks);
        NvMTiming_PutU64(&respData[45u], image->readBlockStats.maxTicks);
        NvMTiming_PutU64(&respData[53u], image->readBlockStats.totalTicks);
        NvMTiming_PutU32(&respData[61u], image->writeBlockStats.count);
        NvMTiming_PutU64(&respData[65u], image->writeBlockStats.minTicks);
        NvMTiming_PutU64(&respData[73u], image->writeBlockStats.maxTicks);
        NvMTiming_PutU64(&respData[81u], image->writeBlockStats.totalTicks);
        *respLen = 89u;
        return DCM_E_OK;
    }
    if (selector == NVMTIMING_SELECTOR_STARTUP)
    {
        Dcm_ReturnType ret = NvMTiming_WriteEventPage(reqData, reqLen, &respData[1u], respLen);
        if (ret == DCM_E_OK)
        {
            *respLen = (Dcm_PduLengthType)(*respLen + 1u);
        }
        return ret;
    }
    if (selector == NVMTIMING_SELECTOR_READALL)
    {
        NvMTiming_WriteMulti(&image->lastReadAll, &respData[1u], respLen);
        *respLen = (Dcm_PduLengthType)(*respLen + 1u);
        return DCM_E_OK;
    }
    if (selector == NVMTIMING_SELECTOR_WRITEALL)
    {
        NvMTiming_WriteMulti(&image->lastWriteAll, &respData[1u], respLen);
        *respLen = (Dcm_PduLengthType)(*respLen + 1u);
        return DCM_E_OK;
    }
    if (selector == NVMTIMING_SELECTOR_HISTORY_META)
    {
        respData[1u] = NVMTIMING_HISTORY_SIZE;
        NvMTiming_PutU32(&respData[2u], image->historyWriteIndex);
        *respLen = 6u;
        return DCM_E_OK;
    }
    if (selector >= NVMTIMING_SELECTOR_HISTORY_ENTRY_BASE)
    {
        index = (uint8)(selector - NVMTIMING_SELECTOR_HISTORY_ENTRY_BASE);
        if (index >= NVMTIMING_HISTORY_SIZE) { return DCM_NRC_REQUEST_OUT_OF_RANGE; }
        NvMTiming_WriteSingle(&image->history[index], &respData[1u], respLen);
        *respLen = (Dcm_PduLengthType)(*respLen + 1u);
        return DCM_E_OK;
    }
    return DCM_NRC_REQUEST_OUT_OF_RANGE;
}
