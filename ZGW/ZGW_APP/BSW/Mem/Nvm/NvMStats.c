#include "BSW/Mem/Nvm/NvMStats.h"

#include <string.h>

#include "BSW/Mem/Nvm/NvM_Cfg.h"
#include "MemIf.h"

#define NVMSTATS_ROUTINE_START                    (0x01u)
#define NVMSTATS_ROUTINE_REQUEST_RESULTS          (0x03u)
#define NVMSTATS_ROUTINE_CLEAR                    (0x04u)
#define NVMSTATS_UINT64_MAX                       (0xFFFFFFFFFFFFFFFFull)
#define NVMSTATS_UINT32_MAX                       (0xFFFFFFFFul)
#define NVMSTATS_SELF_INDEX_INVALID               (0xFFFFu)

uint8 NvMStats_Ram[NVMSTATS_NVM_IMAGE_SIZE];
const uint8 NvMStats_Rom[NVMSTATS_NVM_IMAGE_SIZE] =
{
    0x4Eu, 0x56u, 0x53u, 0x54u
};

static boolean NvMStats_BootCounted = FALSE;
static boolean NvMStats_Dirty = FALSE;
static boolean NvMStats_SuppressDirty = FALSE;
static uint16 NvMStats_SelfIndex = NVMSTATS_SELF_INDEX_INVALID;
static uint32 NvMStats_CurrentGcPayloadBytes = 0u;
static uint32 NvMStats_CurrentGcPhysicalBytes = 0u;
static uint32 NvMStats_CurrentGcBlocks = 0u;

static NvMStats_NvImageType *NvMStats_Image(void)
{
    return (NvMStats_NvImageType *)NvMStats_Ram;
}

static void NvMStats_Touch(void)
{
    if (NvMStats_SuppressDirty == FALSE)
    {
        NvMStats_Dirty = TRUE;
    }
}

static void NvMStats_Inc64(uint64 *value)
{
    if (*value != NVMSTATS_UINT64_MAX)
    {
        (*value)++;
    }
}

static void NvMStats_Add64(uint64 *value, uint64 delta)
{
    if ((NVMSTATS_UINT64_MAX - *value) < delta)
    {
        *value = NVMSTATS_UINT64_MAX;
    }
    else
    {
        *value += delta;
    }
}

static void NvMStats_ResetImage(void)
{
    NvMStats_NvImageType *image = NvMStats_Image();
    (void)memset(image, 0, sizeof(*image));
    image->magic = NVMSTATS_MAGIC;
    image->version = NVMSTATS_VERSION;
    image->length = (uint16)NVMSTATS_NVM_IMAGE_SIZE;
    image->minFeeFreeBytes = NVMSTATS_UINT32_MAX;
}

static void NvMStats_EnsureValid(void)
{
    NvMStats_NvImageType *image = NvMStats_Image();
    if ((image->magic != NVMSTATS_MAGIC) ||
            (image->version != NVMSTATS_VERSION) ||
            (image->length != (uint16)NVMSTATS_NVM_IMAGE_SIZE))
    {
        NvMStats_ResetImage();
        NvMStats_Touch();
    }
}

static void NvMStats_PutU16(uint8 *data, uint16 value)
{
    data[0u] = (uint8)(value >> 8u);
    data[1u] = (uint8)value;
}

static void NvMStats_PutU32(uint8 *data, uint32 value)
{
    data[0u] = (uint8)(value >> 24u);
    data[1u] = (uint8)(value >> 16u);
    data[2u] = (uint8)(value >> 8u);
    data[3u] = (uint8)value;
}

static void NvMStats_PutU64(uint8 *data, uint64 value)
{
    NvMStats_PutU32(&data[0u], (uint32)(value >> 32u));
    NvMStats_PutU32(&data[4u], (uint32)value);
}

static void NvMStats_WriteU64List(uint8 *respData, Dcm_PduLengthType *respLen, const uint64 *values, uint8 count)
{
    uint8 i;
    uint16 offset = 0u;
    for (i = 0u; i < count; i++)
    {
        NvMStats_PutU64(&respData[offset], values[i]);
        offset = (uint16)(offset + 8u);
    }
    *respLen = offset;
}

void NvMStats_Init(void)
{
    uint16 i;
    NvMStats_SuppressDirty = TRUE;
    NvMStats_EnsureValid();
    NvMStats_SuppressDirty = FALSE;
    NvMStats_Dirty = FALSE;
    NvMStats_BootCounted = FALSE;
    NvMStats_SelfIndex = NVMSTATS_SELF_INDEX_INVALID;
    for (i = 0u; i < (uint16)NVM_TOTAL_BLOCKS; i++)
    {
        if (NvM_BlockDescriptor[i].blockId == NVM_BLOCK_ID_NVM_STATS)
        {
            NvMStats_SelfIndex = i;
            break;
        }
    }
}

void NvMStats_OnReadAllComplete(void)
{
    NvMStats_EnsureValid();
    if (NvMStats_BootCounted == FALSE)
    {
        NvMStats_Inc64(&NvMStats_Image()->bootCount);
        NvMStats_BootCounted = TRUE;
        NvMStats_Touch();
    }
}

void NvMStats_PrepareForWriteAllBlock(uint16 blockIndex, uint16 blockId)
{
    NvMStats_EnsureValid();
    if ((blockId == NVM_BLOCK_ID_NVM_STATS) || (blockIndex == NvMStats_SelfIndex))
    {
        /* Suppress dirtying while persisting this image to avoid recursive WriteAll demand. */
        NvMStats_SuppressDirty = TRUE;
        NvMStats_Dirty = FALSE;
    }
}

void NvMStats_MarkCleanAfterOwnWrite(void)
{
    NvMStats_SuppressDirty = FALSE;
    NvMStats_Dirty = FALSE;
}

void NvMStats_AbortOwnWrite(void)
{
    NvMStats_SuppressDirty = FALSE;
    NvMStats_Dirty = TRUE;
}

boolean NvMStats_IsDirty(void)
{
    NvMStats_EnsureValid();
    return NvMStats_Dirty;
}

void NvMStats_RecordNvMError(uint8 api, uint8 error, uint32 detail)
{
    NvMStats_Image()->lastNvMError = ((uint32)api << 24u) | ((uint32)error << 16u) | (detail & 0xFFFFu);
    NvMStats_Touch();
}

void NvMStats_RecordNvMWriteBlockRequest(void) { NvMStats_Inc64(&NvMStats_Image()->nvmWriteBlockRequests); NvMStats_Touch(); }
void NvMStats_RecordNvMWriteBlockAccepted(void) { NvMStats_Inc64(&NvMStats_Image()->nvmWriteBlockAccepted); NvMStats_Touch(); }
void NvMStats_RecordNvMWriteBlockRejected(void) { NvMStats_Inc64(&NvMStats_Image()->nvmWriteBlockRejected); NvMStats_Touch(); }
void NvMStats_RecordNvMReadBlockRequest(void) { NvMStats_Inc64(&NvMStats_Image()->nvmReadBlockRequests); NvMStats_Touch(); }
void NvMStats_RecordNvMReadBlockAccepted(void) { NvMStats_Inc64(&NvMStats_Image()->nvmReadBlockAccepted); NvMStats_Touch(); }
void NvMStats_RecordNvMReadBlockRejected(void) { NvMStats_Inc64(&NvMStats_Image()->nvmReadBlockRejected); NvMStats_Touch(); }
void NvMStats_RecordNvMReadAllAccepted(void) { NvMStats_Inc64(&NvMStats_Image()->nvmReadAllCount); NvMStats_Touch(); }
void NvMStats_RecordNvMWriteAllAccepted(void) { NvMStats_Inc64(&NvMStats_Image()->nvmWriteAllCount); NvMStats_Touch(); }
void NvMStats_RecordNvMBusyRejection(void) { NvMStats_Inc64(&NvMStats_Image()->nvmBusyRejections); NvMStats_Inc64(&NvMStats_Image()->nvmQueueRejections); NvMStats_Touch(); }
void NvMStats_RecordNvMUninitRejection(void) { NvMStats_Inc64(&NvMStats_Image()->nvmUninitRejections); NvMStats_Touch(); }
void NvMStats_RecordNvMInvalidBlockRejection(void) { NvMStats_Inc64(&NvMStats_Image()->nvmInvalidBlockRejections); NvMStats_Touch(); }
void NvMStats_RecordNvMWriteProtectionRejection(void) { NvMStats_Inc64(&NvMStats_Image()->nvmWriteProtectionRejections); NvMStats_Touch(); }

void NvMStats_RecordNvMSuccessfulWrite(uint16 blockIndex, uint16 blockId)
{
    if (blockId == NVM_BLOCK_ID_NVM_STATS)
    {
        return;
    }
    NvMStats_Inc64(&NvMStats_Image()->nvmSuccessfulLogicalWrites);
    if (blockIndex < NVMSTATS_PER_BLOCK_COUNTERS)
    {
        NvMStats_Inc64(&NvMStats_Image()->perBlockSuccessfulWrites[blockIndex]);
        NvMStats_Touch();
    }
    else
    {
        NvMStats_Touch();
    }
}

void NvMStats_RecordNvMFailedWrite(void) { NvMStats_Inc64(&NvMStats_Image()->nvmFailedLogicalWrites); NvMStats_Touch(); }
void NvMStats_RecordNvMSuccessfulRead(void) { NvMStats_Inc64(&NvMStats_Image()->nvmSuccessfulReads); NvMStats_Touch(); }

void NvMStats_RecordNvMReadFailure(uint8 memIfResult)
{
    NvMStats_Image()->lastMemIfResult = (uint32)memIfResult;
    NvMStats_Inc64(&NvMStats_Image()->nvmReadFailures);
    if (memIfResult == MEMIF_BLOCK_INCONSISTENT)
    {
        NvMStats_Inc64(&NvMStats_Image()->nvmCrcFailures);
    }
    else if (memIfResult == MEMIF_BLOCK_INVALID)
    {
        NvMStats_Inc64(&NvMStats_Image()->nvmInvalidBlockReads);
    }
    NvMStats_Touch();
}

void NvMStats_RecordNvMRestoredDefaults(void) { NvMStats_Inc64(&NvMStats_Image()->nvmRestoredDefaults); NvMStats_Touch(); }
void NvMStats_RecordNvMInvalidationRequest(void) { NvMStats_Inc64(&NvMStats_Image()->nvmInvalidationRequests); NvMStats_Touch(); }
void NvMStats_RecordNvMInvalidationSuccess(void) { NvMStats_Inc64(&NvMStats_Image()->nvmInvalidationSuccesses); NvMStats_Touch(); }
void NvMStats_RecordNvMInvalidationFailure(void) { NvMStats_Inc64(&NvMStats_Image()->nvmInvalidationFailures); NvMStats_Touch(); }
void NvMStats_RecordNvMEraseRequest(void) { NvMStats_Inc64(&NvMStats_Image()->nvmEraseRequests); NvMStats_Touch(); }

void NvMStats_RecordFeeError(uint8 error, uint32 detail)
{
    NvMStats_Image()->lastFeeError = ((uint32)error << 24u) | (detail & 0x00FFFFFFu);
    NvMStats_Touch();
}

void NvMStats_RecordFeeWriteRequest(uint16 blockNumber) { (void)blockNumber; NvMStats_Inc64(&NvMStats_Image()->feeWriteRequests); NvMStats_Touch(); }
void NvMStats_RecordFeeWriteSuccess(uint16 blockNumber) { (void)blockNumber; NvMStats_Inc64(&NvMStats_Image()->feeSuccessfulLogicalWrites); NvMStats_Touch(); }
void NvMStats_RecordFeeWriteFailure(void) { NvMStats_Inc64(&NvMStats_Image()->feeFailedLogicalWrites); NvMStats_Touch(); }
void NvMStats_RecordFeeInvalidationRequest(void) { NvMStats_Inc64(&NvMStats_Image()->feeInvalidationRequests); NvMStats_Touch(); }
void NvMStats_RecordFeeInvalidationSuccess(void) { NvMStats_Inc64(&NvMStats_Image()->feeInvalidationSuccesses); NvMStats_Touch(); }

void NvMStats_RecordFeeGcStart(void)
{
    NvMStats_CurrentGcPayloadBytes = 0u;
    NvMStats_CurrentGcPhysicalBytes = 0u;
    NvMStats_CurrentGcBlocks = 0u;
    NvMStats_Inc64(&NvMStats_Image()->feeGcCount);
    NvMStats_Touch();
}

void NvMStats_RecordFeeSectorSwitch(void)
{
    NvMStats_Inc64(&NvMStats_Image()->feeSectorSwitchCount);
    if (NvMStats_CurrentGcBlocks > NvMStats_Image()->maxGcCopiedBlocks) { NvMStats_Image()->maxGcCopiedBlocks = NvMStats_CurrentGcBlocks; }
    if (NvMStats_CurrentGcPayloadBytes > NvMStats_Image()->maxGcCopiedPayloadBytes) { NvMStats_Image()->maxGcCopiedPayloadBytes = NvMStats_CurrentGcPayloadBytes; }
    if (NvMStats_CurrentGcPhysicalBytes > NvMStats_Image()->maxGcCopiedPhysicalBytes) { NvMStats_Image()->maxGcCopiedPhysicalBytes = NvMStats_CurrentGcPhysicalBytes; }
    NvMStats_Touch();
}

void NvMStats_RecordFeeGcCopiedRecord(uint32 payloadBytes, uint32 physicalBytes)
{
    NvMStats_Inc64(&NvMStats_Image()->feeGcCopiedBlocks);
    NvMStats_Add64(&NvMStats_Image()->feeGcCopiedPayloadBytes, payloadBytes);
    NvMStats_Add64(&NvMStats_Image()->feeGcCopiedPhysicalBytes, physicalBytes);
    if ((NVMSTATS_UINT32_MAX - NvMStats_CurrentGcPayloadBytes) < payloadBytes) { NvMStats_CurrentGcPayloadBytes = NVMSTATS_UINT32_MAX; } else { NvMStats_CurrentGcPayloadBytes += payloadBytes; }
    if ((NVMSTATS_UINT32_MAX - NvMStats_CurrentGcPhysicalBytes) < physicalBytes) { NvMStats_CurrentGcPhysicalBytes = NVMSTATS_UINT32_MAX; } else { NvMStats_CurrentGcPhysicalBytes += physicalBytes; }
    if (NvMStats_CurrentGcBlocks != NVMSTATS_UINT32_MAX) { NvMStats_CurrentGcBlocks++; }
    NvMStats_Touch();
}

void NvMStats_RecordFeeIntegrityFailure(void) { NvMStats_Inc64(&NvMStats_Image()->feeIntegrityFailures); NvMStats_Touch(); }
void NvMStats_RecordFeeInvalidMissingBlockRead(void) { NvMStats_Inc64(&NvMStats_Image()->feeInvalidMissingBlockReads); NvMStats_Touch(); }
void NvMStats_RecordFeeScanRecovery(void) { NvMStats_Inc64(&NvMStats_Image()->feeScanRecoveryEvents); NvMStats_Touch(); }
void NvMStats_RecordFeeDeferredFormat(void) { NvMStats_Inc64(&NvMStats_Image()->feeDeferredFormatEvents); NvMStats_Touch(); }

void NvMStats_RecordFeeUtilization(uint32 appendOffset, uint32 freeBytes)
{
    NvMStats_Image()->activeFeeAppendOffset = appendOffset;
    NvMStats_Image()->activeFeeFreeBytes = freeBytes;
    if (appendOffset > NvMStats_Image()->maxFeeAppendOffset) { NvMStats_Image()->maxFeeAppendOffset = appendOffset; }
    if (freeBytes < NvMStats_Image()->minFeeFreeBytes) { NvMStats_Image()->minFeeFreeBytes = freeBytes; }
    NvMStats_Touch();
}

void NvMStats_RecordFlsError(uint8 api, uint8 error, uint32 detail)
{
    NvMStats_Image()->lastFlsError = ((uint32)api << 24u) | ((uint32)error << 16u) | (detail & 0xFFFFu);
    NvMStats_Touch();
}

void NvMStats_RecordFlsPageWrite(uint32 bytes) { NvMStats_Inc64(&NvMStats_Image()->flsPageWriteCount); NvMStats_Add64(&NvMStats_Image()->flsWriteBytes, bytes); NvMStats_Touch(); }
void NvMStats_RecordFlsEraseSector(uint32 bytes) { NvMStats_Inc64(&NvMStats_Image()->flsEraseSectorCount); NvMStats_Add64(&NvMStats_Image()->flsEraseBytes, bytes); NvMStats_Touch(); }

void NvMStats_RecordFlsJobFailure(uint32 job)
{
    NvMStats_Inc64(&NvMStats_Image()->flsFailedJobs);
    if (job == 1u) { NvMStats_Inc64(&NvMStats_Image()->flsEraseFailures); }
    else if (job == 2u) { NvMStats_Inc64(&NvMStats_Image()->flsWriteFailures); }
    else if (job == 3u) { NvMStats_Inc64(&NvMStats_Image()->flsReadFailures); }
    NvMStats_Touch();
}

void NvMStats_RecordFlsDmuBusyReject(void) { NvMStats_Inc64(&NvMStats_Image()->flsDmuBusyRejects); NvMStats_Touch(); }
void NvMStats_RecordFlsDmuTimeout(void) { NvMStats_Inc64(&NvMStats_Image()->flsDmuTimeouts); NvMStats_Touch(); }

boolean NvMStats_IsRoutineId(uint16 routineId)
{
    return (routineId == NVMSTATS_ROUTINE_ID) ? TRUE : FALSE;
}

Dcm_ReturnType NvMStats_HandleRoutineControl(
        Dcm_OpStatusType opStatus,
        uint8 routineControlType,
        uint16 routineId,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    NvMStats_NvImageType *s = NvMStats_Image();
    uint8 selector;
    uint8 start;
    uint8 count;
    uint8 i;
    uint16 offset;
    uint64 values[16];

    (void)opStatus;
    if (routineId != NVMSTATS_ROUTINE_ID) { return DCM_NRC_REQUEST_OUT_OF_RANGE; }
    if ((routineControlType != NVMSTATS_ROUTINE_START) &&
            (routineControlType != NVMSTATS_ROUTINE_REQUEST_RESULTS) &&
            (routineControlType != NVMSTATS_ROUTINE_CLEAR))
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }

    if (routineControlType == NVMSTATS_ROUTINE_CLEAR)
    {
        NvMStats_ResetImage();
        NvMStats_BootCounted = TRUE;
        NvMStats_Inc64(&s->bootCount);
        NvMStats_Touch();
        respData[0u] = NVMSTATS_SELECTOR_CLEAR;
        *respLen = 1u;
        return DCM_E_OK;
    }

    selector = (reqLen >= 1u) ? reqData[0u] : NVMSTATS_SELECTOR_SUMMARY;
    NvMStats_EnsureValid();
    respData[0u] = selector;

    if (selector == NVMSTATS_SELECTOR_SUMMARY)
    {
        NvMStats_PutU32(&respData[1u], s->magic);
        NvMStats_PutU16(&respData[5u], s->version);
        NvMStats_PutU16(&respData[7u], s->length);
        NvMStats_PutU16(&respData[9u], (uint16)NVM_TOTAL_BLOCKS);
        NvMStats_PutU16(&respData[11u], NVMSTATS_COUNTER_WIDTH_BITS);
        NvMStats_PutU64(&respData[13u], s->bootCount);
        *respLen = 21u;
        return DCM_E_OK;
    }

    if (selector == NVMSTATS_SELECTOR_NVM)
    {
        values[0] = s->nvmReadAllCount; values[1] = s->nvmWriteAllCount; values[2] = s->nvmWriteBlockRequests;
        values[3] = s->nvmWriteBlockAccepted; values[4] = s->nvmWriteBlockRejected; values[5] = s->nvmSuccessfulLogicalWrites;
        values[6] = s->nvmFailedLogicalWrites; values[7] = s->nvmQueueRejections; values[8] = s->nvmBusyRejections;
        values[9] = s->nvmUninitRejections; values[10] = s->nvmInvalidBlockRejections; values[11] = s->nvmWriteProtectionRejections;
        NvMStats_WriteU64List(&respData[1u], respLen, values, 12u); *respLen = (Dcm_PduLengthType)(*respLen + 1u); return DCM_E_OK;
    }
    if (selector == NVMSTATS_SELECTOR_READ)
    {
        values[0] = s->nvmReadBlockRequests; values[1] = s->nvmReadBlockAccepted; values[2] = s->nvmReadBlockRejected;
        values[3] = s->nvmSuccessfulReads; values[4] = s->nvmReadFailures; values[5] = s->nvmCrcFailures;
        values[6] = s->nvmInvalidBlockReads; values[7] = s->nvmRestoredDefaults; values[8] = s->feeIntegrityFailures;
        values[9] = s->feeInvalidMissingBlockReads;
        NvMStats_WriteU64List(&respData[1u], respLen, values, 10u); *respLen = (Dcm_PduLengthType)(*respLen + 1u); return DCM_E_OK;
    }
    if (selector == NVMSTATS_SELECTOR_WRITE)
    {
        values[0] = s->nvmInvalidationRequests; values[1] = s->nvmInvalidationSuccesses; values[2] = s->nvmInvalidationFailures;
        values[3] = s->nvmEraseRequests;
        NvMStats_WriteU64List(&respData[1u], respLen, values, 4u); *respLen = (Dcm_PduLengthType)(*respLen + 1u); return DCM_E_OK;
    }
    if (selector == NVMSTATS_SELECTOR_FEE)
    {
        values[0] = s->feeWriteRequests; values[1] = s->feeSuccessfulLogicalWrites; values[2] = s->feeFailedLogicalWrites;
        values[3] = s->feeInvalidationRequests; values[4] = s->feeInvalidationSuccesses; values[5] = s->feeDeferredFormatEvents;
        values[6] = s->feeScanRecoveryEvents;
        NvMStats_WriteU64List(&respData[1u], respLen, values, 7u); *respLen = (Dcm_PduLengthType)(*respLen + 1u); return DCM_E_OK;
    }
    if (selector == NVMSTATS_SELECTOR_GC)
    {
        values[0] = s->feeGcCount; values[1] = s->feeSectorSwitchCount; values[2] = s->feeGcCopiedBlocks;
        values[3] = s->feeGcCopiedPayloadBytes; values[4] = s->feeGcCopiedPhysicalBytes;
        NvMStats_WriteU64List(&respData[1u], respLen, values, 5u);
        offset = (uint16)(1u + *respLen);
        NvMStats_PutU32(&respData[offset], s->maxGcCopiedBlocks); NvMStats_PutU32(&respData[offset + 4u], s->maxGcCopiedPayloadBytes);
        NvMStats_PutU32(&respData[offset + 8u], s->maxGcCopiedPhysicalBytes); *respLen = (Dcm_PduLengthType)(offset + 12u); return DCM_E_OK;
    }
    if (selector == NVMSTATS_SELECTOR_FLS)
    {
        values[0] = s->flsPageWriteCount; values[1] = s->flsWriteBytes; values[2] = s->flsEraseSectorCount;
        values[3] = s->flsEraseBytes; values[4] = s->flsFailedJobs; values[5] = s->flsWriteFailures;
        values[6] = s->flsEraseFailures; values[7] = s->flsReadFailures; values[8] = s->flsDmuBusyRejects; values[9] = s->flsDmuTimeouts;
        NvMStats_WriteU64List(&respData[1u], respLen, values, 10u); *respLen = (Dcm_PduLengthType)(*respLen + 1u); return DCM_E_OK;
    }
    if (selector == NVMSTATS_SELECTOR_ERROR)
    {
        NvMStats_PutU32(&respData[1u], s->lastNvMError); NvMStats_PutU32(&respData[5u], s->lastFeeError);
        NvMStats_PutU32(&respData[9u], s->lastFlsError); NvMStats_PutU32(&respData[13u], s->lastMemIfResult);
        *respLen = 17u; return DCM_E_OK;
    }
    if (selector == NVMSTATS_SELECTOR_UTILIZATION)
    {
        NvMStats_PutU32(&respData[1u], s->activeFeeFreeBytes); NvMStats_PutU32(&respData[5u], s->activeFeeAppendOffset);
        NvMStats_PutU32(&respData[9u], s->maxFeeAppendOffset); NvMStats_PutU32(&respData[13u], s->minFeeFreeBytes);
        *respLen = 17u; return DCM_E_OK;
    }
    if (selector == NVMSTATS_SELECTOR_PER_BLOCK)
    {
        start = (reqLen >= 2u) ? reqData[1u] : 0u;
        count = (reqLen >= 3u) ? reqData[2u] : 8u;
        if (start >= (uint8)NVM_TOTAL_BLOCKS) { return DCM_NRC_REQUEST_OUT_OF_RANGE; }
        if (count > 8u) { count = 8u; }
        if (((uint16)start + (uint16)count) > (uint16)NVM_TOTAL_BLOCKS)
        {
            count = (uint8)((uint16)NVM_TOTAL_BLOCKS - (uint16)start);
        }
        respData[1u] = start; respData[2u] = count; offset = 3u;
        for (i = 0u; i < count; i++)
        {
            uint16 idx = (uint16)start + (uint16)i;
            NvMStats_PutU16(&respData[offset], NvM_BlockDescriptor[idx].blockId);
            respData[offset + 2u] = (NvM_BlockDescriptor[idx].blockId == NVM_BLOCK_ID_NVM_STATS) ? 1u : 0u;
            respData[offset + 3u] = 0u;
            NvMStats_PutU64(&respData[offset + 4u], s->perBlockSuccessfulWrites[idx]);
            offset = (uint16)(offset + 12u);
        }
        *respLen = offset;
        return DCM_E_OK;
    }
    return DCM_NRC_REQUEST_OUT_OF_RANGE;
}
