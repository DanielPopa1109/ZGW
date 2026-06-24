#include <stddef.h>
#include <string.h>
#include "Fee.h"
#include "Fls.h"
#include "Crc.h"
#include "MemStack_Error.h"
#include "McuSm.h"

#define FEE_API_INIT                    (0x00u)
#define FEE_API_SET_MODE                (0x01u)
#define FEE_API_READ                    (0x02u)
#define FEE_API_WRITE                   (0x03u)
#define FEE_API_CANCEL                  (0x04u)
#define FEE_API_GET_STATUS              (0x05u)
#define FEE_API_GET_JOB_RESULT          (0x06u)
#define FEE_API_INVALIDATE              (0x07u)
#define FEE_API_ERASE_IMMEDIATE         (0x08u)
#define FEE_API_MAIN                    (0x12u)

#define FEE_E_UNINIT                    (0x01u)
#define FEE_E_BUSY                      (0x02u)
#define FEE_E_INVALID_BLOCK             (0x03u)
#define FEE_E_INVALID_OFFSET            (0x04u)
#define FEE_E_INVALID_POINTER           (0x05u)
#define FEE_E_FLASH_JOB_FAILED          (0x06u)
#define FEE_E_INTERNAL                  (0x07u)
#define FEE_E_NO_SPACE                  (0x08u)

#define FEE_SECTOR_MAGIC                (0xFEE0A5A5u)
#define FEE_RECORD_MAGIC                (0xFEE15A5Au)
#define FEE_RECORD_COMMIT_MAGIC         (0xC0DEC0DEu)
#define FEE_MARKER_BLOCK_NUMBER         (0xFFFFu)
#define FEE_FLAG_INVALID                (0x0001u)
#define FEE_FLAG_REDUNDANT_COPY1        (0x0002u)
#define FEE_FLAG_SECTOR_VALID           (0x8000u)

#define FEE_INVALID_ADDRESS             (0xFFFFFFFFu)
#define FEE_HEADER_CRC_SIZE             ((uint32)offsetof(Fee_RecordHeaderType, headerCrc))
#define FEE_SECTOR_HEADER_CRC_SIZE      ((uint32)offsetof(Fee_SectorHeaderType, headerCrc))
#define FEE_DEBUG_FLASH_ACCESS_COPY     (1u)
#define FEE_DEBUG_FLASH_ACCESS_ERASE    (2u)
#define FEE_DEBUG_FLASH_ACCESS_FLS_READ (4u)
#define FEE_DEBUG_FLASH_ACCESS_FLS_WRITE (5u)
#define FEE_INVALID_SECTOR              (0xFFu)
#define FEE_MAIN_STEP_BUDGET            (16u)

typedef struct
{
    uint32 magic;
    uint32 generation;
    uint32 sectorSize;
    uint32 headerCrc;
} Fee_SectorHeaderType;

typedef struct
{
    uint32 magic;
    uint16 blockNumber;
    uint16 flags;
    uint32 length;
    uint32 sequence;
    uint32 dataCrc;
    uint32 headerCrc;
} Fee_RecordHeaderType;

typedef struct
{
    uint32 commitMagic;
    uint32 commitMagicInv;
} Fee_RecordTrailerType;

typedef struct
{
    boolean configured;
    boolean invalid;
    uint16 blockNumber;
    uint16 blockSize;
    Fee_BlockManagementType managementType;
    uint32 latestSequence;
    boolean copyValid[2];
    uint32 copyAddress[2];
    uint32 copySequence[2];
    uint32 copyCrc[2];
    uint32 copyLength[2];
} Fee_BlockRuntimeType;

typedef struct
{
    boolean headerValid;
    boolean sectorValidMarkerFound;
    boolean scanComplete;
    uint32 generation;
    uint32 appendOffset;
    Fee_BlockRuntimeType block[FEE_CONFIGURED_BLOCKS];
} Fee_ScanResultType;

typedef enum
{
    FEE_STATE_UNINIT = 0,
    FEE_STATE_IDLE,
    FEE_STATE_FORMAT_ERASE0_START,
    FEE_STATE_FORMAT_ERASE0_WAIT,
    FEE_STATE_FORMAT_HEADER0_START,
    FEE_STATE_FORMAT_HEADER0_WAIT,
    FEE_STATE_FORMAT_MARKER0_START,
    FEE_STATE_FORMAT_MARKER0_WAIT,
    FEE_STATE_FORMAT_ERASE1_START,
    FEE_STATE_FORMAT_ERASE1_WAIT,
    FEE_STATE_WRITE_PREPARE,
    FEE_STATE_WRITE_HEADER_WAIT,
    FEE_STATE_WRITE_DATA_WAIT,
    FEE_STATE_WRITE_TRAILER_WAIT,
    FEE_STATE_READ_START,
    FEE_STATE_READ_WAIT,
    FEE_STATE_GC_ERASE_DEST_START,
    FEE_STATE_GC_ERASE_DEST_WAIT,
    FEE_STATE_GC_HEADER_START,
    FEE_STATE_GC_HEADER_WAIT,
    FEE_STATE_GC_FIND_NEXT,
    FEE_STATE_GC_READ_SOURCE_START,
    FEE_STATE_GC_WRITE_PREPARE,
    FEE_STATE_GC_WRITE_HEADER_WAIT,
    FEE_STATE_GC_WRITE_DATA_WAIT,
    FEE_STATE_GC_WRITE_TRAILER_WAIT,
    FEE_STATE_GC_MARKER_START,
    FEE_STATE_GC_MARKER_WAIT,
    FEE_STATE_GC_ERASE_OLD_START,
    FEE_STATE_GC_ERASE_OLD_WAIT
} Fee_InternalStateType;

typedef enum
{
    FEE_USER_JOB_NONE = 0,
    FEE_USER_JOB_READ,
    FEE_USER_JOB_WRITE,
    FEE_USER_JOB_INVALIDATE,
    FEE_USER_JOB_ERASE_IMMEDIATE
} Fee_UserJobType;

typedef struct
{
    MemIf_StatusType status;
    MemIf_JobResultType result;
    Fee_InternalStateType state;
    Fee_UserJobType userJob;
    uint8 activeSector;
    uint32 activeGeneration;
    uint32 activeAppend;
    uint16 jobBlockNumber;
    uint16 jobBlockIndex;
    uint16 jobOffset;
    uint16 jobLength;
    uint8 *jobReadPtr;
    uint32 jobSequence;
    uint8 copiesToWrite;
    uint8 currentCopy;
    boolean pendingAfterGc;
    Fee_UserJobType pendingJob;
    uint16 pendingBlockNumber;
    uint8 gcOldSector;
    uint8 gcDestSector;
    uint16 gcBlockIndex;
    uint32 gcSourceAddress;
    uint32 gcSourceCrc;
    uint32 gcSourceLength;
    uint32 jobRecordLength;
} Fee_StateType;

static Fee_StateType Fee_State;
static Fee_BlockRuntimeType Fee_Runtime[FEE_CONFIGURED_BLOCKS];
static uint8 Fee_JobData[FEE_MAX_BLOCK_SIZE + FLS_DFLASH0_PAGE_SIZE];
static const uint8 *Fee_RecordDataPtr;
static Fee_RecordHeaderType Fee_RecordHeader;
static Fee_RecordTrailerType Fee_RecordTrailer;
static Fee_SectorHeaderType Fee_SectorHeader;
static uint8 Fee_MarkerBuffer[sizeof(Fee_RecordHeaderType) + sizeof(Fee_RecordTrailerType)];

/*
 * Compile-time guards for the "big sector" layout and full data-flash usage.
 *
 *  - The two virtual sectors must tile the entire DF0 (start..end) with nothing
 *    left over (req: all of the data flash is used).
 *  - Each virtual sector must be a whole number of physically erasable 4 KiB
 *    logical sectors and start sector 1 immediately after sector 0.
 *  - Even the largest block, written redundantly (two records), must fit in a
 *    freshly formatted virtual sector together with the sector header and the
 *    reserved sector-valid marker slot - otherwise garbage collection could not
 *    relocate it and the sector switch would dead-end.
 *  - The job buffer must cover the largest record payload.
 */
#define FEE_ASSERT_CONCAT_(a, b) a##b
#define FEE_ASSERT_CONCAT(a, b)  FEE_ASSERT_CONCAT_(a, b)
#define FEE_STATIC_ASSERT(cond) \
    typedef char FEE_ASSERT_CONCAT(Fee_StaticAssert_, __LINE__)[(cond) ? 1 : -1]

FEE_STATIC_ASSERT((FEE_VIRTUAL_SECTOR_COUNT * FEE_VIRTUAL_SECTOR_SIZE) == FLS_DFLASH0_TOTAL_SIZE);
FEE_STATIC_ASSERT(FEE_DATAFLASH_SIZE == FLS_DFLASH0_TOTAL_SIZE);
FEE_STATIC_ASSERT((FEE_VIRTUAL_SECTOR_SIZE % FLS_DFLASH0_SECTOR_SIZE) == 0u);
FEE_STATIC_ASSERT(FEE_VIRTUAL_SECTOR0_OFFSET == 0u);
FEE_STATIC_ASSERT(FEE_VIRTUAL_SECTOR1_OFFSET == FEE_VIRTUAL_SECTOR_SIZE);
FEE_STATIC_ASSERT((FEE_MAX_BLOCK_SIZE % FLS_DFLASH0_PAGE_SIZE) == 0u);
FEE_STATIC_ASSERT(sizeof(Fee_JobData) >= FEE_MAX_BLOCK_SIZE);
FEE_STATIC_ASSERT(
        ((uint32)sizeof(Fee_SectorHeaderType) +
         (uint32)sizeof(Fee_RecordHeaderType) + (uint32)sizeof(Fee_RecordTrailerType) +
         (2u * ((uint32)sizeof(Fee_RecordHeaderType) + FEE_MAX_BLOCK_SIZE +
                (uint32)sizeof(Fee_RecordTrailerType)))) <= FEE_VIRTUAL_SECTOR_SIZE);
static uint32 Fee_RecordStartOffset;
static uint32 Fee_RecordTotalLength;
static uint32 Fee_RecordPaddedLength;
static uint32 Fee_NextSequence;
static boolean Fee_DeferredFormatPending = FALSE;
volatile uint32 Fee_NextWriteDFlashAddress = FLS_DFLASH0_BASE_ADDRESS;
volatile uint32 Fee_DebugLastFlashAccessKind = 0u;
volatile uint32 Fee_DebugLastFlashOffset = 0u;
volatile uint32 Fee_DebugLastFlashPhysicalAddress = 0u;
volatile uint32 Fee_DebugLastFlashAccessPhysicalAddress = 0u;
volatile uint32 Fee_DebugLastFlashLength = 0u;
volatile uint32 Fee_DebugLastScanSector = 0u;
volatile uint32 Fee_DebugLastScanCursor = 0u;
volatile uint32 Fee_DebugLastScanEnd = 0u;
volatile uint32 Fee_DebugScanStepCounter = 0u;
volatile uint32 Fee_DebugRecoveryFormatRequestSeen = 0u;
volatile uint32 Fee_DebugRecoveryFormatStartCounter = 0u;
volatile uint32 Fee_DebugRecoveryFormatDoneCounter = 0u;
volatile uint32 Fee_DebugRecoveryFormatSuppressedCounter = 0u;

static void Fee_RecordFlashAccess(uint32 kind, uint32 offset, uint32 length)
{
    Fee_DebugLastFlashAccessKind = kind;
    Fee_DebugLastFlashOffset = offset;
    Fee_DebugLastFlashAccessPhysicalAddress = Fls_GetPhysicalAddress(offset);
    Fee_DebugLastFlashLength = length;
}

static void Fee_UpdateNextWriteDFlashAddress(void)
{
    Fee_NextWriteDFlashAddress = Fls_GetPhysicalAddress(Fee_State.activeAppend);
    Fee_DebugLastFlashPhysicalAddress = Fee_NextWriteDFlashAddress;
}

static uint32 Fee_Align8(uint32 value)
{
    return (value + (FLS_DFLASH0_PAGE_SIZE - 1u)) & ~(FLS_DFLASH0_PAGE_SIZE - 1u);
}

static uint32 Fee_SectorBase(uint8 sector)
{
    return (sector == 0u) ? FEE_VIRTUAL_SECTOR0_OFFSET : FEE_VIRTUAL_SECTOR1_OFFSET;
}

static uint32 Fee_SectorEnd(uint8 sector)
{
    return Fee_SectorBase(sector) + FEE_VIRTUAL_SECTOR_SIZE;
}

static boolean Fee_IsPhysicalAddressInSector(uint32 physicalAddress, uint8 sector)
{
    uint32 sectorStart = Fls_GetPhysicalAddress(Fee_SectorBase(sector));
    uint32 sectorEnd = sectorStart + FEE_VIRTUAL_SECTOR_SIZE;

    return (boolean)((physicalAddress >= sectorStart) && (physicalAddress < sectorEnd));
}

static uint8 Fee_GetRecoverySkipSector(void)
{
    if (McuSm_IsDFlashRecoveryRequested() == FALSE)
    {
        return FEE_INVALID_SECTOR;
    }

    if (Fee_IsPhysicalAddressInSector(McuSm_DFlashRecoveryLastFeePhysicalAddress, 0u) != FALSE)
    {
        return 0u;
    }

    if (Fee_IsPhysicalAddressInSector(McuSm_DFlashRecoveryLastFeePhysicalAddress, 1u) != FALSE)
    {
        return 1u;
    }

    return FEE_INVALID_SECTOR;
}

/* The sector-valid marker lives at a fixed slot immediately after the sector
 * header (this is the only place Fee_ScanSector looks for it). The first data
 * record always starts just past that reserved slot. */
static uint32 Fee_MarkerRecordSize(void)
{
    return (uint32)sizeof(Fee_RecordHeaderType) + (uint32)sizeof(Fee_RecordTrailerType);
}

static uint32 Fee_SectorMarkerOffset(uint8 sector)
{
    return Fee_SectorBase(sector) + (uint32)sizeof(Fee_SectorHeaderType);
}

static uint32 Fee_SectorFirstRecordOffset(uint8 sector)
{
    return Fee_SectorMarkerOffset(sector) + Fee_MarkerRecordSize();
}

static Std_ReturnType Fee_CopyFromFlash(uint32 offset, void *dst, uint32 length)
{
    Fee_RecordFlashAccess(FEE_DEBUG_FLASH_ACCESS_COPY, offset, length);
    return Fls_ReadImmediate(offset, (uint8 *)dst, (Fls_LengthType)length);
}

static boolean Fee_IsFlashErased(uint32 offset, uint32 length)
{
    uint8 page[FLS_DFLASH0_PAGE_SIZE];
    uint32 byteIndex;
    uint32 checkLength;
    uint32 current;

    Fee_RecordFlashAccess(FEE_DEBUG_FLASH_ACCESS_ERASE, offset, length);
    checkLength = Fee_Align8(length);
    current = offset;

    while (checkLength > 0u)
    {
        if (Fls_ReadImmediate(current, page, FLS_DFLASH0_PAGE_SIZE) != E_OK)
        {
            return FALSE;
        }

        for (byteIndex = 0u; byteIndex < FLS_DFLASH0_PAGE_SIZE; byteIndex++)
        {
            if (page[byteIndex] != FLS_ERASED_VALUE)
            {
                return FALSE;
            }
        }

        current += FLS_DFLASH0_PAGE_SIZE;
        checkLength -= FLS_DFLASH0_PAGE_SIZE;
    }

    return TRUE;
}

static void Fee_RuntimeInit(Fee_BlockRuntimeType *runtime)
{
    uint16 i;
    for (i = 0u; i < FEE_CONFIGURED_BLOCKS; i++)
    {
        runtime[i].configured = TRUE;
        runtime[i].invalid = FALSE;
        runtime[i].blockNumber = Fee_BlockConfig[i].blockNumber;
        runtime[i].blockSize = Fee_BlockConfig[i].blockSize;
        runtime[i].managementType = Fee_BlockConfig[i].managementType;
        runtime[i].latestSequence = 0u;
        runtime[i].copyValid[0] = FALSE;
        runtime[i].copyValid[1] = FALSE;
        runtime[i].copyAddress[0] = FEE_INVALID_ADDRESS;
        runtime[i].copyAddress[1] = FEE_INVALID_ADDRESS;
        runtime[i].copySequence[0] = 0u;
        runtime[i].copySequence[1] = 0u;
        runtime[i].copyCrc[0] = 0u;
        runtime[i].copyCrc[1] = 0u;
        runtime[i].copyLength[0] = 0u;
        runtime[i].copyLength[1] = 0u;
    }
}

static sint16 Fee_FindBlockIndex(uint16 blockNumber)
{
    uint16 i;
    for (i = 0u; i < FEE_CONFIGURED_BLOCKS; i++)
    {
        if (Fee_BlockConfig[i].blockNumber == blockNumber)
        {
            return (sint16)i;
        }
    }
    return -1;
}

static boolean Fee_IsAcceptedRecordLength(const Fee_BlockRuntimeType *block, const Fee_RecordHeaderType *header)
{
    if (header->length == block->blockSize)
    {
        return TRUE;
    }

    if ((block->blockNumber == FEE_BLOCK_APP_DATA_LEGACY_NUMBER) &&
            (header->length == FEE_BLOCK_APP_DATA_LEGACY_SIZE) &&
            ((uint32)block->blockSize <= FEE_BLOCK_APP_DATA_LEGACY_SIZE))
    {
        return TRUE;
    }

    return FALSE;
}

static uint32 Fee_CalcHeaderCrc(Fee_RecordHeaderType *header)
{
    return Crc_CalculateCRC32((const uint8 *)header, FEE_HEADER_CRC_SIZE, 0u, TRUE);
}

static uint32 Fee_CalcSectorHeaderCrc(Fee_SectorHeaderType *header)
{
    return Crc_CalculateCRC32((const uint8 *)header, FEE_SECTOR_HEADER_CRC_SIZE, 0u, TRUE);
}

static boolean Fee_IsSectorHeaderValid(const Fee_SectorHeaderType *header)
{
    Fee_SectorHeaderType temp;

    if ((header->magic != FEE_SECTOR_MAGIC) || (header->sectorSize != FEE_VIRTUAL_SECTOR_SIZE))
    {
        return FALSE;
    }
    temp = *header;
    return (boolean)(Fee_CalcSectorHeaderCrc(&temp) == header->headerCrc);
}

static void Fee_PrepareSectorHeader(uint32 generation)
{
    Fee_SectorHeader.magic = FEE_SECTOR_MAGIC;
    Fee_SectorHeader.generation = generation;
    Fee_SectorHeader.sectorSize = FEE_VIRTUAL_SECTOR_SIZE;
    Fee_SectorHeader.headerCrc = Fee_CalcSectorHeaderCrc(&Fee_SectorHeader);
}

static void Fee_PrepareRecord(uint16 blockNumber, uint16 flags, const uint8 *data, uint32 length, uint32 sequence)
{
    Fee_RecordHeader.magic = FEE_RECORD_MAGIC;
    Fee_RecordHeader.blockNumber = blockNumber;
    Fee_RecordHeader.flags = flags;
    Fee_RecordHeader.length = length;
    Fee_RecordHeader.sequence = sequence;

    Fee_RecordPaddedLength = Fee_Align8(length);
    Fee_RecordDataPtr = NULL_PTR;
    if (length > 0u)
    {
        if (data == NULL_PTR)
        {
            memset(Fee_JobData, FLS_ERASED_VALUE, Fee_RecordPaddedLength);
        }
        else if (data != Fee_JobData)
        {
            memcpy(Fee_JobData, data, length);
        }

        if (Fee_RecordPaddedLength > length)
        {
            memset(&Fee_JobData[length], FLS_ERASED_VALUE, (size_t)(Fee_RecordPaddedLength - length));
        }
        Fee_RecordDataPtr = Fee_JobData;
    }

    Fee_RecordHeader.dataCrc = Crc_CalculateCRC32(Fee_JobData, length, 0u, TRUE);
    Fee_RecordHeader.headerCrc = Fee_CalcHeaderCrc(&Fee_RecordHeader);
    Fee_RecordTrailer.commitMagic = FEE_RECORD_COMMIT_MAGIC;
    Fee_RecordTrailer.commitMagicInv = ~FEE_RECORD_COMMIT_MAGIC;
    Fee_RecordTotalLength = (uint32)sizeof(Fee_RecordHeaderType) + Fee_RecordPaddedLength + (uint32)sizeof(Fee_RecordTrailerType);
}

static void Fee_PrepareRecordFromFlash(uint16 blockNumber,
                                       uint16 flags,
                                       uint32 sourceAddress,
                                       uint32 length,
                                       uint32 sequence,
                                       uint32 dataCrc)
{
    Fee_RecordHeader.magic = FEE_RECORD_MAGIC;
    Fee_RecordHeader.blockNumber = blockNumber;
    Fee_RecordHeader.flags = flags;
    Fee_RecordHeader.length = length;
    Fee_RecordHeader.sequence = sequence;

    Fee_RecordPaddedLength = Fee_Align8(length);
    Fee_RecordDataPtr = NULL_PTR;
    if (length > 0u)
    {
        Fee_RecordDataPtr = (const uint8 *)Fls_GetPhysicalAddress(sourceAddress);
    }
    Fee_RecordHeader.dataCrc = dataCrc;
    Fee_RecordHeader.headerCrc = Fee_CalcHeaderCrc(&Fee_RecordHeader);
    Fee_RecordTrailer.commitMagic = FEE_RECORD_COMMIT_MAGIC;
    Fee_RecordTrailer.commitMagicInv = ~FEE_RECORD_COMMIT_MAGIC;
    Fee_RecordTotalLength = (uint32)sizeof(Fee_RecordHeaderType) + Fee_RecordPaddedLength + (uint32)sizeof(Fee_RecordTrailerType);
}

static boolean Fee_IsRecordCommitted(uint32 recordOffset, const Fee_RecordHeaderType *header)
{
    Fee_RecordTrailerType trailer;
    uint32 trailerOffset;
    uint32 paddedLength;

    paddedLength = Fee_Align8(header->length);
    trailerOffset = recordOffset + (uint32)sizeof(Fee_RecordHeaderType) + paddedLength;

    /* A reset can leave a valid header with an erased, uncommitted trailer.
     * Do not use DFlash erase-verify as a predicate here: a programmed trailer
     * reports verify failure through the same SMU path as a real flash fault. */
    if (Fee_IsFlashErased(trailerOffset, (uint32)sizeof(trailer)) != FALSE)
    {
        return FALSE;
    }
    if (Fee_CopyFromFlash(trailerOffset, &trailer, sizeof(trailer)) != E_OK)
    {
        return FALSE;
    }

    return (boolean)((trailer.commitMagic == FEE_RECORD_COMMIT_MAGIC) &&
                     (trailer.commitMagicInv == ~FEE_RECORD_COMMIT_MAGIC));
}

static boolean Fee_IsRecordHeaderValid(const Fee_RecordHeaderType *header)
{
    Fee_RecordHeaderType temp;

    if (header->magic != FEE_RECORD_MAGIC)
    {
        return FALSE;
    }
    if (header->length > FEE_MAX_BLOCK_SIZE)
    {
        return FALSE;
    }
    temp = *header;
    return (boolean)(Fee_CalcHeaderCrc(&temp) == header->headerCrc);
}

static boolean Fee_IsRecordValidAt(uint32 recordOffset, uint32 sectorEnd, Fee_RecordHeaderType *header)
{
    uint32 totalLength;
    uint32 dataOffset;
    uint32 crc;

    if ((recordOffset + sizeof(Fee_RecordHeaderType) + sizeof(Fee_RecordTrailerType)) > sectorEnd)
    {
        return FALSE;
    }

    if (Fee_CopyFromFlash(recordOffset, header, sizeof(Fee_RecordHeaderType)) != E_OK)
    {
        return FALSE;
    }
    if (Fee_IsRecordHeaderValid(header) == FALSE)
    {
        return FALSE;
    }

    totalLength = (uint32)sizeof(Fee_RecordHeaderType) + Fee_Align8(header->length) + (uint32)sizeof(Fee_RecordTrailerType);
    if ((recordOffset + totalLength) > sectorEnd)
    {
        return FALSE;
    }

    if (Fee_IsRecordCommitted(recordOffset, header) == FALSE)
    {
        return FALSE;
    }

    if (((header->flags & FEE_FLAG_INVALID) == 0u) && ((header->flags & FEE_FLAG_SECTOR_VALID) == 0u))
    {
        dataOffset = recordOffset + (uint32)sizeof(Fee_RecordHeaderType);
        if (Fee_CopyFromFlash(dataOffset, Fee_JobData, header->length) != E_OK)
        {
            return FALSE;
        }
        crc = Crc_CalculateCRC32(Fee_JobData, header->length, 0u, TRUE);
        if (crc != header->dataCrc)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static boolean Fee_IsSectorMarkerValidAt(uint32 recordOffset, uint32 sectorEnd, Fee_RecordHeaderType *header)
{
    uint32 totalLength;

    if ((recordOffset + sizeof(Fee_RecordHeaderType) + sizeof(Fee_RecordTrailerType)) > sectorEnd)
    {
        return FALSE;
    }

    if (Fee_CopyFromFlash(recordOffset, header, sizeof(Fee_RecordHeaderType)) != E_OK)
    {
        return FALSE;
    }
    if (Fee_IsRecordHeaderValid(header) == FALSE)
    {
        return FALSE;
    }

    if ((header->flags & FEE_FLAG_SECTOR_VALID) == 0u)
    {
        return FALSE;
    }
    if ((header->blockNumber != FEE_MARKER_BLOCK_NUMBER) || (header->length != 0u))
    {
        return FALSE;
    }

    totalLength = (uint32)sizeof(Fee_RecordHeaderType) + (uint32)sizeof(Fee_RecordTrailerType);
    if ((recordOffset + totalLength) > sectorEnd)
    {
        return FALSE;
    }

    return Fee_IsRecordCommitted(recordOffset, header);
}

static void Fee_ApplyRecord(Fee_BlockRuntimeType *runtime, uint32 recordOffset, const Fee_RecordHeaderType *header)
{
    sint16 idx;
    uint8 copyIndex;
    Fee_BlockRuntimeType *block;

    if ((header->flags & FEE_FLAG_SECTOR_VALID) != 0u)
    {
        return;
    }

    idx = Fee_FindBlockIndex(header->blockNumber);
    if (idx < 0)
    {
        return;
    }

    block = &runtime[(uint16)idx];

    if ((header->flags & FEE_FLAG_INVALID) != 0u)
    {
        if (header->sequence >= block->latestSequence)
        {
            block->invalid = TRUE;
            block->latestSequence = header->sequence;
            block->copyValid[0] = FALSE;
            block->copyValid[1] = FALSE;
            block->copyAddress[0] = FEE_INVALID_ADDRESS;
            block->copyAddress[1] = FEE_INVALID_ADDRESS;
            block->copyLength[0] = 0u;
            block->copyLength[1] = 0u;
        }
        return;
    }

    if (Fee_IsAcceptedRecordLength(block, header) == FALSE)
    {
        return;
    }

    copyIndex = ((header->flags & FEE_FLAG_REDUNDANT_COPY1) != 0u) ? 1u : 0u;
    if (block->managementType == FEE_BLOCK_NATIVE)
    {
        copyIndex = 0u;
    }

    if (header->sequence >= block->latestSequence)
    {
        if (header->sequence > block->latestSequence)
        {
            block->copyValid[0] = FALSE;
            block->copyValid[1] = FALSE;
            block->copyAddress[0] = FEE_INVALID_ADDRESS;
            block->copyAddress[1] = FEE_INVALID_ADDRESS;
            block->copyLength[0] = 0u;
            block->copyLength[1] = 0u;
        }
        block->invalid = FALSE;
        block->latestSequence = header->sequence;
        block->copyValid[copyIndex] = TRUE;
        block->copyAddress[copyIndex] = recordOffset + (uint32)sizeof(Fee_RecordHeaderType);
        block->copySequence[copyIndex] = header->sequence;
        block->copyCrc[copyIndex] = header->dataCrc;
        block->copyLength[copyIndex] = header->length;
    }
}

static void Fee_ScanSector(uint8 sector, Fee_ScanResultType *result)
{
    uint32 cursor;
    uint32 end;
    Fee_RecordHeaderType header;
    Fee_SectorHeaderType sectorHeader;

    memset(result, 0, sizeof(*result));
    Fee_RuntimeInit(result->block);
    result->appendOffset = Fee_SectorBase(sector) + (uint32)sizeof(Fee_SectorHeaderType);

    if (Fee_IsFlashErased(Fee_SectorBase(sector), FLS_DFLASH0_PAGE_SIZE) != FALSE)
    {
        return;
    }

    if (Fee_CopyFromFlash(Fee_SectorBase(sector), &sectorHeader, sizeof(sectorHeader)) != E_OK)
    {
        return;
    }
    if (Fee_IsSectorHeaderValid(&sectorHeader) == FALSE)
    {
        return;
    }

    result->headerValid = TRUE;
    result->generation = sectorHeader.generation;
    cursor = Fee_SectorBase(sector) + (uint32)sizeof(Fee_SectorHeaderType);
    end = Fee_SectorEnd(sector);

    if (Fee_IsFlashErased(cursor, FLS_DFLASH0_PAGE_SIZE) == TRUE)
    {
        return;
    }

    if (Fee_IsSectorMarkerValidAt(cursor, end, &header) == FALSE)
    {
        return;
    }

    result->sectorValidMarkerFound = TRUE;
    cursor += (uint32)sizeof(Fee_RecordHeaderType) +
            (uint32)sizeof(Fee_RecordTrailerType);
    result->appendOffset = cursor;

    while ((cursor + (uint32)sizeof(Fee_RecordHeaderType) +
            (uint32)sizeof(Fee_RecordTrailerType)) <= end)
    {
        Fee_DebugLastScanSector = sector;
        Fee_DebugLastScanCursor = cursor;
        Fee_DebugLastScanEnd = end;
        Fee_DebugScanStepCounter++;

        if (Fee_IsFlashErased(cursor, FLS_DFLASH0_PAGE_SIZE) == TRUE)
        {
            result->appendOffset = cursor;
            result->scanComplete = TRUE;
            return;
        }

        if (Fee_IsRecordValidAt(cursor, end, &header) == FALSE)
        {
            return;
        }

        Fee_ApplyRecord(result->block, cursor, &header);
        cursor += (uint32)sizeof(Fee_RecordHeaderType) +
                Fee_Align8(header.length) +
                (uint32)sizeof(Fee_RecordTrailerType);
        result->appendOffset = cursor;
    }

    result->scanComplete = TRUE;
    return;
}

static boolean Fee_ChooseActiveSector(uint8 skipSector)
{
    Fee_ScanResultType scan0;
    Fee_ScanResultType scan1;
    boolean valid0;
    boolean valid1;

    memset(&scan0, 0, sizeof(scan0));
    memset(&scan1, 0, sizeof(scan1));

    if (skipSector != 0u)
    {
        Fee_ScanSector(0u, &scan0);
    }
    if (skipSector != 1u)
    {
        Fee_ScanSector(1u, &scan1);
    }

    valid0 = (boolean)(scan0.headerValid && scan0.sectorValidMarkerFound && scan0.scanComplete);
    valid1 = (boolean)(scan1.headerValid && scan1.sectorValidMarkerFound && scan1.scanComplete);

    if ((valid0 == FALSE) && (valid1 == FALSE))
    {
        return FALSE;
    }

    if ((valid1 == TRUE) && ((valid0 == FALSE) || (scan1.generation > scan0.generation)))
    {
        Fee_State.activeSector = 1u;
        Fee_State.activeGeneration = scan1.generation;
        Fee_State.activeAppend = scan1.appendOffset;
        memcpy(Fee_Runtime, scan1.block, sizeof(Fee_Runtime));
    }
    else
    {
        Fee_State.activeSector = 0u;
        Fee_State.activeGeneration = scan0.generation;
        Fee_State.activeAppend = scan0.appendOffset;
        memcpy(Fee_Runtime, scan0.block, sizeof(Fee_Runtime));
    }
    Fee_UpdateNextWriteDFlashAddress();

    Fee_NextSequence = 1u;
    for (uint16 i = 0u; i < FEE_CONFIGURED_BLOCKS; i++)
    {
        if (Fee_Runtime[i].latestSequence >= Fee_NextSequence)
        {
            Fee_NextSequence = Fee_Runtime[i].latestSequence + 1u;
        }
    }
    return TRUE;
}

static uint32 Fee_FreeBytesInActiveSector(void)
{
    if (Fee_State.activeAppend >= Fee_SectorEnd(Fee_State.activeSector))
    {
        return 0u;
    }
    return Fee_SectorEnd(Fee_State.activeSector) - Fee_State.activeAppend;
}

static uint32 Fee_RecordLengthForBlock(uint16 length, Fee_BlockManagementType type)
{
    uint32 oneRecord = (uint32)sizeof(Fee_RecordHeaderType) + Fee_Align8(length) + (uint32)sizeof(Fee_RecordTrailerType);
    return (type == FEE_BLOCK_REDUNDANT) ? (2u * oneRecord) : oneRecord;
}

static uint32 Fee_MinimumRequiredForCurrentJob(void)
{
    const Fee_BlockConfigType *cfg = &Fee_BlockConfig[Fee_State.jobBlockIndex];

    if ((Fee_State.userJob == FEE_USER_JOB_INVALIDATE) || (Fee_State.userJob == FEE_USER_JOB_ERASE_IMMEDIATE))
    {
        return (uint32)sizeof(Fee_RecordHeaderType) + (uint32)sizeof(Fee_RecordTrailerType);
    }
    return Fee_RecordLengthForBlock(cfg->blockSize, cfg->managementType);
}

static boolean Fee_HasSpaceForCurrentJob(void)
{
    return (boolean)(Fee_FreeBytesInActiveSector() >= Fee_MinimumRequiredForCurrentJob());
}

static void Fee_SetIdle(MemIf_JobResultType result)
{
    Fee_State.result = result;
    Fee_State.status = MEMIF_IDLE;
    Fee_State.state = FEE_STATE_IDLE;
    Fee_State.userJob = FEE_USER_JOB_NONE;
    Fee_State.pendingAfterGc = FALSE;
    Fee_State.jobRecordLength = 0u;
    Fee_State.gcSourceLength = 0u;
}

static void Fee_SetFailed(uint8 error, uint32 detail)
{
    MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_MAIN, error, detail);
    Fee_SetIdle(MEMIF_JOB_FAILED);
}

static void Fee_StartFormat(void)
{
    Fls_ClearDFlashSmuBusError();
    Fee_State.status = MEMIF_BUSY_INTERNAL;
    Fee_State.result = MEMIF_JOB_PENDING;
    Fee_State.activeSector = 0u;
    Fee_State.activeGeneration = 1u;
    Fee_State.activeAppend = FEE_VIRTUAL_SECTOR0_OFFSET + (uint32)sizeof(Fee_SectorHeaderType);
    Fee_UpdateNextWriteDFlashAddress();
    Fee_State.state = FEE_STATE_FORMAT_ERASE0_START;
    Fee_NextSequence = 1u;
    Fee_DebugRecoveryFormatStartCounter++;
}

static Std_ReturnType Fee_StartErase(uint32 address, uint32 length)
{
    Fee_RecordFlashAccess(FEE_DEBUG_FLASH_ACCESS_ERASE, address, length);
    return Fls_Erase(address, (Fls_LengthType)length);
}

static Std_ReturnType Fee_StartRawWrite(uint32 address, const uint8 *data, uint32 length)
{
    Fee_NextWriteDFlashAddress = Fls_GetPhysicalAddress(address);
    Fee_DebugLastFlashPhysicalAddress = Fee_NextWriteDFlashAddress;
    Fee_RecordFlashAccess(FEE_DEBUG_FLASH_ACCESS_FLS_WRITE, address, length);
    return Fls_Write(address, data, (Fls_LengthType)length);
}

static Std_ReturnType Fee_StartHeaderWrite(uint32 address)
{
    return Fee_StartRawWrite(address, (const uint8 *)&Fee_RecordHeader, (uint32)sizeof(Fee_RecordHeaderType));
}

static Std_ReturnType Fee_StartDataWrite(uint32 address)
{
    if (Fee_RecordPaddedLength == 0u)
    {
        return E_OK;
    }
    if (Fee_RecordDataPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }
    return Fee_StartRawWrite(address, Fee_RecordDataPtr, Fee_RecordPaddedLength);
}

static Std_ReturnType Fee_StartTrailerWrite(uint32 address)
{
    return Fee_StartRawWrite(address, (const uint8 *)&Fee_RecordTrailer, (uint32)sizeof(Fee_RecordTrailerType));
}

static void Fee_UpdateRuntimeAfterCurrentRecord(void)
{
    Fee_ApplyRecord(Fee_Runtime, Fee_RecordStartOffset, &Fee_RecordHeader);
}

static boolean Fee_SelectReadCopy(uint16 blockIndex, uint8 attempt, uint32 *address, uint32 *crc, uint32 *length)
{
    Fee_BlockRuntimeType *block = &Fee_Runtime[blockIndex];

    if ((block->invalid == TRUE) || ((block->copyValid[0] == FALSE) && (block->copyValid[1] == FALSE)))
    {
        return FALSE;
    }

    if (block->managementType == FEE_BLOCK_NATIVE)
    {
        if ((attempt == 0u) && (block->copyValid[0] == TRUE))
        {
            *address = block->copyAddress[0];
            *crc = block->copyCrc[0];
            *length = block->copyLength[0];
            return TRUE;
        }
        return FALSE;
    }

    if (attempt == 0u)
    {
        if (block->copyValid[0] == TRUE)
        {
            *address = block->copyAddress[0];
            *crc = block->copyCrc[0];
            *length = block->copyLength[0];
            return TRUE;
        }
        if (block->copyValid[1] == TRUE)
        {
            *address = block->copyAddress[1];
            *crc = block->copyCrc[1];
            *length = block->copyLength[1];
            return TRUE;
        }
    }
    else if (attempt == 1u)
    {
        if ((block->copyValid[1] == TRUE) && (block->copyAddress[1] != block->copyAddress[0]))
        {
            *address = block->copyAddress[1];
            *crc = block->copyCrc[1];
            *length = block->copyLength[1];
            return TRUE;
        }
    }
    return FALSE;
}

static void Fee_StartGc(void)
{
    Fee_State.pendingAfterGc = TRUE;
    Fee_State.pendingJob = Fee_State.userJob;
    Fee_State.pendingBlockNumber = Fee_State.jobBlockNumber;

    Fee_State.gcOldSector = Fee_State.activeSector;
    Fee_State.gcDestSector = (Fee_State.activeSector == 0u) ? 1u : 0u;
    Fee_State.gcBlockIndex = 0u;
    Fee_State.gcSourceLength = 0u;
    Fee_State.status = MEMIF_BUSY_INTERNAL;
    Fee_State.state = FEE_STATE_GC_ERASE_DEST_START;
}

static void Fee_RestorePendingJobAfterGc(void)
{
    sint16 idx;

    Fee_State.userJob = Fee_State.pendingJob;
    Fee_State.jobBlockNumber = Fee_State.pendingBlockNumber;
    idx = Fee_FindBlockIndex(Fee_State.pendingBlockNumber);
    if (idx < 0)
    {
        Fee_SetFailed(FEE_E_INVALID_BLOCK, Fee_State.pendingBlockNumber);
        return;
    }

    Fee_State.jobBlockIndex = (uint16)idx;

    Fee_State.jobSequence = Fee_NextSequence++;
    Fee_State.copiesToWrite = (Fee_BlockConfig[Fee_State.jobBlockIndex].managementType == FEE_BLOCK_REDUNDANT) ? 2u : 1u;
    Fee_State.currentCopy = 0u;
    Fee_State.pendingAfterGc = FALSE;
    Fee_State.status = MEMIF_BUSY;
    Fee_State.state = FEE_STATE_WRITE_PREPARE;
}

void Fee_Init(const Fee_ConfigType *ConfigPtr)
{
    boolean recoveryFormatRequested;
    boolean tryExistingSectors;
    uint8 recoverySkipSector;

    (void)ConfigPtr;
    memset(&Fee_State, 0, sizeof(Fee_State));
    Fee_RuntimeInit(Fee_Runtime);
    Fee_UpdateNextWriteDFlashAddress();
    Fee_DeferredFormatPending = FALSE;
    Fls_Init(NULL_PTR);

    recoveryFormatRequested = McuSm_IsDFlashRecoveryRequested();
    recoverySkipSector = Fee_GetRecoverySkipSector();
    tryExistingSectors = TRUE;
    if (recoveryFormatRequested != FALSE)
    {
        Fee_DebugRecoveryFormatRequestSeen++;
        if (McuSm_BeginDFlashRecoveryAttempt() == FALSE)
        {
            /* Do not keep re-scanning forever if the recovery-attempt limiter
             * trips. At that point the retained marker is treated as unrecovered
             * corruption and Fee falls back to formatting below. */
            McuSm_ClearDFlashRecoveryRequest();
            tryExistingSectors = FALSE;
            Fee_DebugRecoveryFormatSuppressedCounter++;
        }
    }

    if ((tryExistingSectors != FALSE) && (Fee_ChooseActiveSector(recoverySkipSector) == TRUE))
    {
        if (recoveryFormatRequested != FALSE)
        {
            McuSm_ClearDFlashRecoveryRequest();
        }
        Fee_State.status = MEMIF_IDLE;
        Fee_State.result = MEMIF_JOB_OK;
        Fee_State.state = FEE_STATE_IDLE;
    }
    else
    {
        Fee_State.status = MEMIF_IDLE;
        Fee_State.result = MEMIF_JOB_OK;
        Fee_State.state = FEE_STATE_IDLE;
        Fee_NextSequence = 1u;
        Fee_DeferredFormatPending = TRUE;
    }
}

void Fee_SetMode(MemIf_ModeType Mode)
{
    Fls_SetMode(Mode);
}

Std_ReturnType Fee_Read(uint16 BlockNumber, uint16 BlockOffset, uint8 *DataBufferPtr, uint16 Length)
{
    sint16 idx;

    if (Fee_State.status == MEMIF_UNINIT)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_READ, FEE_E_UNINIT, 0u);
        return E_NOT_OK;
    }
    if (Fee_State.status != MEMIF_IDLE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_READ, FEE_E_BUSY, 0u);
        return E_NOT_OK;
    }
    if (DataBufferPtr == NULL_PTR)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_READ, FEE_E_INVALID_POINTER, 0u);
        return E_NOT_OK;
    }
    idx = Fee_FindBlockIndex(BlockNumber);
    if (idx < 0)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_READ, FEE_E_INVALID_BLOCK, BlockNumber);
        return E_NOT_OK;
    }
    if (((uint32)BlockOffset + (uint32)Length) > Fee_BlockConfig[(uint16)idx].blockSize)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_READ, FEE_E_INVALID_OFFSET, BlockOffset);
        return E_NOT_OK;
    }

    Fee_State.status = MEMIF_BUSY;
    Fee_State.result = MEMIF_JOB_PENDING;
    Fee_State.userJob = FEE_USER_JOB_READ;
    Fee_State.jobBlockNumber = BlockNumber;
    Fee_State.jobBlockIndex = (uint16)idx;
    Fee_State.jobOffset = BlockOffset;
    Fee_State.jobLength = Length;
    Fee_State.jobReadPtr = DataBufferPtr;
    Fee_State.currentCopy = 0u;
    Fee_State.state = FEE_STATE_READ_START;
    return E_OK;
}

Std_ReturnType Fee_Write(uint16 BlockNumber, const uint8 *DataBufferPtr)
{
    sint16 idx;

    if (Fee_State.status == MEMIF_UNINIT)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_WRITE, FEE_E_UNINIT, 0u);
        return E_NOT_OK;
    }
    if (Fee_State.status != MEMIF_IDLE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_WRITE, FEE_E_BUSY, 0u);
        return E_NOT_OK;
    }
    if (DataBufferPtr == NULL_PTR)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_WRITE, FEE_E_INVALID_POINTER, 0u);
        return E_NOT_OK;
    }
    idx = Fee_FindBlockIndex(BlockNumber);
    if (idx < 0)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_WRITE, FEE_E_INVALID_BLOCK, BlockNumber);
        return E_NOT_OK;
    }

    memcpy(Fee_JobData, DataBufferPtr, Fee_BlockConfig[(uint16)idx].blockSize);
    Fee_State.status = MEMIF_BUSY;
    Fee_State.result = MEMIF_JOB_PENDING;
    Fee_State.userJob = FEE_USER_JOB_WRITE;
    Fee_State.jobBlockNumber = BlockNumber;
    Fee_State.jobBlockIndex = (uint16)idx;
    Fee_State.jobSequence = Fee_NextSequence++;
    Fee_State.copiesToWrite = (Fee_BlockConfig[(uint16)idx].managementType == FEE_BLOCK_REDUNDANT) ? 2u : 1u;
    Fee_State.currentCopy = 0u;

    if (Fee_HasSpaceForCurrentJob() == FALSE)
    {
        Fee_StartGc();
    }
    else
    {
        Fee_State.state = FEE_STATE_WRITE_PREPARE;
    }
    return E_OK;
}

Std_ReturnType Fee_InvalidateBlock(uint16 BlockNumber)
{
    sint16 idx;

    if (Fee_State.status == MEMIF_UNINIT)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_INVALIDATE, FEE_E_UNINIT, 0u);
        return E_NOT_OK;
    }
    if (Fee_State.status != MEMIF_IDLE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_INVALIDATE, FEE_E_BUSY, 0u);
        return E_NOT_OK;
    }
    idx = Fee_FindBlockIndex(BlockNumber);
    if (idx < 0)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_INVALIDATE, FEE_E_INVALID_BLOCK, BlockNumber);
        return E_NOT_OK;
    }

    Fee_State.status = MEMIF_BUSY;
    Fee_State.result = MEMIF_JOB_PENDING;
    Fee_State.userJob = FEE_USER_JOB_INVALIDATE;
    Fee_State.jobBlockNumber = BlockNumber;
    Fee_State.jobBlockIndex = (uint16)idx;
    Fee_State.jobSequence = Fee_NextSequence++;
    Fee_State.copiesToWrite = 1u;
    Fee_State.currentCopy = 0u;

    if (Fee_HasSpaceForCurrentJob() == FALSE)
    {
        Fee_StartGc();
    }
    else
    {
        Fee_State.state = FEE_STATE_WRITE_PREPARE;
    }
    return E_OK;
}

Std_ReturnType Fee_EraseImmediateBlock(uint16 BlockNumber)
{
    return Fee_InvalidateBlock(BlockNumber);
}

void Fee_Cancel(void)
{
    if ((Fee_State.status == MEMIF_BUSY) || (Fee_State.status == MEMIF_BUSY_INTERNAL))
    {
        Fls_Cancel();
        Fee_State.result = MEMIF_JOB_CANCELED;
        Fee_State.status = MEMIF_IDLE;
        Fee_State.state = FEE_STATE_IDLE;
        Fee_State.userJob = FEE_USER_JOB_NONE;
    }
}

MemIf_StatusType Fee_GetStatus(void)
{
    return Fee_State.status;
}

MemIf_JobResultType Fee_GetJobResult(void)
{
    return Fee_State.result;
}

boolean Fee_IsDeferredFormatPending(void)
{
    return Fee_DeferredFormatPending;
}

Std_ReturnType Fee_StartDeferredFormat(void)
{
    if (Fee_State.status == MEMIF_UNINIT)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_INIT, FEE_E_UNINIT, 0u);
        return E_NOT_OK;
    }

    if (Fee_DeferredFormatPending == FALSE)
    {
        return E_NOT_OK;
    }

    if (Fee_State.status != MEMIF_IDLE)
    {
        return E_NOT_OK;
    }

    Fee_DeferredFormatPending = FALSE;
    Fee_StartFormat();
    return E_OK;
}

static void Fee_ProcessWriteStates(Fee_InternalStateType prepareState,
                                   Fee_InternalStateType headerWaitState,
                                   Fee_InternalStateType dataWaitState,
                                   Fee_InternalStateType trailerWaitState,
                                   Fee_InternalStateType nextPrepareState,
                                   boolean updateRuntime,
                                   boolean dataFromFlash)
{
    uint16 flags;
    uint32 dataLength;
    const uint8 *dataPtr;
    const Fee_BlockConfigType *cfg;
    uint32 dataOffset;
    uint32 trailerOffset;

    if (Fee_State.state == prepareState)
    {
        cfg = &Fee_BlockConfig[Fee_State.jobBlockIndex];
        flags = 0u;
        dataLength = cfg->blockSize;
        dataPtr = Fee_JobData;

        if ((Fee_State.userJob == FEE_USER_JOB_INVALIDATE) || (Fee_State.userJob == FEE_USER_JOB_ERASE_IMMEDIATE))
        {
            flags |= FEE_FLAG_INVALID;
            dataLength = 0u;
            dataPtr = NULL_PTR;
        }
        if ((cfg->managementType == FEE_BLOCK_REDUNDANT) && (Fee_State.currentCopy == 1u))
        {
            flags |= FEE_FLAG_REDUNDANT_COPY1;
        }

        Fee_RecordStartOffset = Fee_State.activeAppend;
        if ((dataFromFlash != FALSE) && (dataLength > 0u))
        {
            if (Fee_State.gcSourceLength == dataLength)
            {
                Fee_PrepareRecordFromFlash(Fee_State.jobBlockNumber,
                                           flags,
                                           Fee_State.gcSourceAddress,
                                           dataLength,
                                           Fee_State.jobSequence,
                                           Fee_State.gcSourceCrc);
            }
            else if ((Fee_State.gcSourceLength > dataLength) &&
                    (Fee_State.gcSourceLength <= FEE_MAX_BLOCK_SIZE))
            {
                if (Fee_CopyFromFlash(Fee_State.gcSourceAddress, Fee_JobData, dataLength) != E_OK)
                {
                    Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_State.gcSourceAddress);
                    return;
                }
                Fee_PrepareRecord(Fee_State.jobBlockNumber, flags, Fee_JobData, dataLength, Fee_State.jobSequence);
            }
            else
            {
                Fee_SetFailed(FEE_E_INTERNAL, Fee_State.gcSourceLength);
                return;
            }
        }
        else
        {
            Fee_PrepareRecord(Fee_State.jobBlockNumber, flags, dataPtr, dataLength, Fee_State.jobSequence);
        }
        if ((Fee_RecordStartOffset + Fee_RecordTotalLength) > Fee_SectorEnd(Fee_State.activeSector))
        {
            Fee_SetFailed(FEE_E_NO_SPACE, Fee_RecordStartOffset);
            return;
        }
        if (Fee_StartHeaderWrite(Fee_RecordStartOffset) != E_OK)
        {
            Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_RecordStartOffset);
            return;
        }
        Fee_State.state = headerWaitState;
        return;
    }

    if (Fee_State.state == headerWaitState)
    {
        if (Fls_GetStatus() != MEMIF_IDLE)
        {
            return;
        }
        if (Fls_GetJobResult() != MEMIF_JOB_OK)
        {
            Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_RecordStartOffset);
            return;
        }
        dataOffset = Fee_RecordStartOffset + (uint32)sizeof(Fee_RecordHeaderType);
        if (Fee_StartDataWrite(dataOffset) != E_OK)
        {
            trailerOffset = dataOffset;
            if (Fee_RecordPaddedLength != 0u)
            {
                Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, dataOffset);
                return;
            }
            if (Fee_StartTrailerWrite(trailerOffset) != E_OK)
            {
                Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, trailerOffset);
                return;
            }
            Fee_State.state = trailerWaitState;
        }
        else
        {
            Fee_State.state = dataWaitState;
        }
        return;
    }

    if (Fee_State.state == dataWaitState)
    {
        if (Fls_GetStatus() != MEMIF_IDLE)
        {
            return;
        }
        if (Fls_GetJobResult() != MEMIF_JOB_OK)
        {
            Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_RecordStartOffset);
            return;
        }
        trailerOffset = Fee_RecordStartOffset + (uint32)sizeof(Fee_RecordHeaderType) + Fee_RecordPaddedLength;
        if (Fee_StartTrailerWrite(trailerOffset) != E_OK)
        {
            Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, trailerOffset);
            return;
        }
        Fee_State.state = trailerWaitState;
        return;
    }

    if (Fee_State.state == trailerWaitState)
    {
        if (Fls_GetStatus() != MEMIF_IDLE)
        {
            return;
        }
        if (Fls_GetJobResult() != MEMIF_JOB_OK)
        {
            Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_RecordStartOffset);
            return;
        }

        if (updateRuntime == TRUE)
        {
            Fee_UpdateRuntimeAfterCurrentRecord();
        }
        Fee_State.activeAppend += Fee_RecordTotalLength;
        Fee_UpdateNextWriteDFlashAddress();
        Fee_State.currentCopy++;

        if (Fee_State.currentCopy < Fee_State.copiesToWrite)
        {
            Fee_State.state = nextPrepareState;
        }
        else
        {
            if (Fee_State.status == MEMIF_BUSY_INTERNAL)
            {
                Fee_State.state = FEE_STATE_GC_FIND_NEXT;
            }
            else
            {
                Fee_SetIdle(MEMIF_JOB_OK);
            }
        }
    }
}

static void Fee_WriteSectorValidMarkerAt(uint32 markerOffset, Fee_InternalStateType waitState)
{
    Fee_RecordStartOffset = markerOffset;
    Fee_PrepareRecord(FEE_MARKER_BLOCK_NUMBER, FEE_FLAG_SECTOR_VALID, NULL_PTR, 0u, Fee_State.activeGeneration);
    memcpy(&Fee_MarkerBuffer[0], &Fee_RecordHeader, sizeof(Fee_RecordHeaderType));
    memcpy(&Fee_MarkerBuffer[sizeof(Fee_RecordHeaderType)], &Fee_RecordTrailer, sizeof(Fee_RecordTrailerType));

    if (Fee_StartRawWrite(Fee_RecordStartOffset, Fee_MarkerBuffer, Fee_RecordTotalLength) != E_OK)
    {
        Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_RecordStartOffset);
        return;
    }
    Fee_State.state = waitState;
}

static void Fee_WriteSectorValidMarker(Fee_InternalStateType waitState)
{
    Fee_WriteSectorValidMarkerAt(Fee_State.activeAppend, waitState);
}

static void Fee_ProcessMarkerWait(Fee_InternalStateType waitState, Fee_InternalStateType nextState)
{
    if (Fee_State.state != waitState)
    {
        return;
    }

    if (Fls_GetStatus() != MEMIF_IDLE)
    {
        return;
    }
    if (Fls_GetJobResult() != MEMIF_JOB_OK)
    {
        Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_RecordStartOffset);
        return;
    }

    Fee_State.state = nextState;
}

long long Fee_MainFunction_Counter =0;

static void Fee_MainFunctionStep(void)
{
    uint32 address;
    uint32 crc;
    uint32 length;
    sint16 idx;

    switch (Fee_State.state)
    {
        case FEE_STATE_UNINIT:
        case FEE_STATE_IDLE:
            break;

        case FEE_STATE_FORMAT_ERASE0_START:
            if (Fee_StartErase(FEE_VIRTUAL_SECTOR0_OFFSET, FEE_VIRTUAL_SECTOR_SIZE) != E_OK)
            {
                Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, FEE_VIRTUAL_SECTOR0_OFFSET);
            }
            else
            {
                Fee_State.state = FEE_STATE_FORMAT_ERASE0_WAIT;
            }
            break;

        case FEE_STATE_FORMAT_ERASE0_WAIT:
            if (Fls_GetStatus() == MEMIF_IDLE)
            {
                if (Fls_GetJobResult() != MEMIF_JOB_OK)
                {
                    Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, FEE_VIRTUAL_SECTOR0_OFFSET);
                }
                else
                {
                    Fee_State.state = FEE_STATE_FORMAT_HEADER0_START;
                }
            }
            break;

        case FEE_STATE_FORMAT_HEADER0_START:
            Fee_PrepareSectorHeader(1u);
            if (Fee_StartRawWrite(FEE_VIRTUAL_SECTOR0_OFFSET, (const uint8 *)&Fee_SectorHeader, (uint32)sizeof(Fee_SectorHeader)) != E_OK)
            {
                Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, FEE_VIRTUAL_SECTOR0_OFFSET);
            }
            else
            {
                Fee_State.state = FEE_STATE_FORMAT_HEADER0_WAIT;
            }
            break;

        case FEE_STATE_FORMAT_HEADER0_WAIT:
            if (Fls_GetStatus() == MEMIF_IDLE)
            {
                if (Fls_GetJobResult() != MEMIF_JOB_OK)
                {
                    Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, FEE_VIRTUAL_SECTOR0_OFFSET);
                }
                else
                {
                    Fee_State.activeAppend = FEE_VIRTUAL_SECTOR0_OFFSET + (uint32)sizeof(Fee_SectorHeaderType);
                    Fee_UpdateNextWriteDFlashAddress();
                    Fee_State.state = FEE_STATE_FORMAT_MARKER0_START;
                }
            }
            break;

        case FEE_STATE_FORMAT_MARKER0_START:
            Fee_WriteSectorValidMarker(FEE_STATE_FORMAT_MARKER0_WAIT);
            break;

        case FEE_STATE_FORMAT_MARKER0_WAIT:
            Fee_ProcessMarkerWait(FEE_STATE_FORMAT_MARKER0_WAIT, FEE_STATE_FORMAT_ERASE1_START);
            if (Fee_State.state == FEE_STATE_FORMAT_ERASE1_START)
            {
                Fee_State.activeAppend += Fee_RecordTotalLength;
                Fee_UpdateNextWriteDFlashAddress();
            }
            break;

        case FEE_STATE_FORMAT_ERASE1_START:
            if (Fee_StartErase(FEE_VIRTUAL_SECTOR1_OFFSET, FEE_VIRTUAL_SECTOR_SIZE) != E_OK)
            {
                Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, FEE_VIRTUAL_SECTOR1_OFFSET);
            }
            else
            {
                Fee_State.state = FEE_STATE_FORMAT_ERASE1_WAIT;
            }
            break;

        case FEE_STATE_FORMAT_ERASE1_WAIT:
            if (Fls_GetStatus() == MEMIF_IDLE)
            {
                if (Fls_GetJobResult() != MEMIF_JOB_OK)
                {
                    Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, FEE_VIRTUAL_SECTOR1_OFFSET);
                }
                else
                {
                    Fee_State.activeSector = 0u;
                    Fee_State.activeGeneration = 1u;
                    McuSm_ClearDFlashRecoveryRequest();
                    Fee_DebugRecoveryFormatDoneCounter++;
                    Fee_SetIdle(MEMIF_JOB_OK);
                }
            }
            break;

        case FEE_STATE_WRITE_PREPARE:
        case FEE_STATE_WRITE_HEADER_WAIT:
        case FEE_STATE_WRITE_DATA_WAIT:
        case FEE_STATE_WRITE_TRAILER_WAIT:
            Fee_ProcessWriteStates(FEE_STATE_WRITE_PREPARE,
                                   FEE_STATE_WRITE_HEADER_WAIT,
                                   FEE_STATE_WRITE_DATA_WAIT,
                                   FEE_STATE_WRITE_TRAILER_WAIT,
                                   FEE_STATE_WRITE_PREPARE,
                                   TRUE,
                                   FALSE);
            break;

        case FEE_STATE_READ_START:
            if (Fee_SelectReadCopy(Fee_State.jobBlockIndex, Fee_State.currentCopy, &address, &crc, &length) == FALSE)
            {
                Fee_SetIdle(MEMIF_BLOCK_INVALID);
            }
            else if ((((uint32)Fee_State.jobOffset + (uint32)Fee_State.jobLength) > length) ||
                    (length > FEE_MAX_BLOCK_SIZE))
            {
                Fee_SetIdle(MEMIF_BLOCK_INCONSISTENT);
            }
            else
            {
                Fee_RecordFlashAccess(FEE_DEBUG_FLASH_ACCESS_FLS_READ,
                        address,
                        length);
                if (Fls_Read(address, Fee_JobData, length) != E_OK)
                {
                    Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, address);
                }
                else
                {
                    Fee_RecordHeader.dataCrc = crc;
                    Fee_State.jobRecordLength = length;
                    Fee_State.state = FEE_STATE_READ_WAIT;
                }
            }
            break;

        case FEE_STATE_READ_WAIT:
            if (Fls_GetStatus() == MEMIF_IDLE)
            {
                if (Fls_GetJobResult() != MEMIF_JOB_OK)
                {
                    Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_State.jobBlockNumber);
                }
                else
                {
                    length = Fee_State.jobRecordLength;
                    crc = Crc_CalculateCRC32(Fee_JobData, length, 0u, TRUE);
                    if (crc == Fee_RecordHeader.dataCrc)
                    {
                        memcpy(Fee_State.jobReadPtr, &Fee_JobData[Fee_State.jobOffset], Fee_State.jobLength);
                        Fee_SetIdle(MEMIF_JOB_OK);
                    }
                    else
                    {
                        Fee_State.currentCopy++;
                        if (Fee_State.currentCopy < 2u)
                        {
                            Fee_State.state = FEE_STATE_READ_START;
                        }
                        else
                        {
                            Fee_SetIdle(MEMIF_BLOCK_INCONSISTENT);
                        }
                    }
                }
            }
            break;

        case FEE_STATE_GC_ERASE_DEST_START:
            if (Fee_StartErase(Fee_SectorBase(Fee_State.gcDestSector), FEE_VIRTUAL_SECTOR_SIZE) != E_OK)
            {
                Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_SectorBase(Fee_State.gcDestSector));
            }
            else
            {
                Fee_State.state = FEE_STATE_GC_ERASE_DEST_WAIT;
            }
            break;

        case FEE_STATE_GC_ERASE_DEST_WAIT:
            if (Fls_GetStatus() == MEMIF_IDLE)
            {
                if (Fls_GetJobResult() != MEMIF_JOB_OK)
                {
                    Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_SectorBase(Fee_State.gcDestSector));
                }
                else
                {
                    Fee_State.state = FEE_STATE_GC_HEADER_START;
                }
            }
            break;

        case FEE_STATE_GC_HEADER_START:
            Fee_State.activeSector = Fee_State.gcDestSector;
            Fee_State.activeGeneration++;
            Fee_State.activeAppend = Fee_SectorBase(Fee_State.activeSector) + (uint32)sizeof(Fee_SectorHeaderType);
            Fee_UpdateNextWriteDFlashAddress();
            Fee_PrepareSectorHeader(Fee_State.activeGeneration);
            if (Fee_StartRawWrite(Fee_SectorBase(Fee_State.activeSector), (const uint8 *)&Fee_SectorHeader, (uint32)sizeof(Fee_SectorHeader)) != E_OK)
            {
                Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_SectorBase(Fee_State.activeSector));
            }
            else
            {
                Fee_State.state = FEE_STATE_GC_HEADER_WAIT;
            }
            break;

        case FEE_STATE_GC_HEADER_WAIT:
            if (Fls_GetStatus() == MEMIF_IDLE)
            {
                if (Fls_GetJobResult() != MEMIF_JOB_OK)
                {
                    Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_SectorBase(Fee_State.activeSector));
                }
                else
                {
                    /* Reserve the fixed marker slot right after the sector header
                     * (the only position Fee_ScanSector looks for the marker) but
                     * do NOT write it yet. Copy the live blocks behind the reserved
                     * slot first, then seal the sector by writing the marker into
                     * that slot as the very last step before erasing the old sector
                     * (see FEE_STATE_GC_MARKER_START). This keeps the on-flash
                     * layout the scan expects ([header][marker][data...]) while
                     * making the marker a true completion seal: if the GC is
                     * interrupted mid-copy, the marker slot stays erased, so the
                     * half-built destination is rejected on the next init and the
                     * intact source sector is kept - instead of the
                     * higher-generation-but-incomplete destination being chosen and
                     * the source erased (silent loss of every not-yet-copied
                     * block). */
                    Fee_State.activeAppend = Fee_SectorFirstRecordOffset(Fee_State.activeSector);
                    Fee_UpdateNextWriteDFlashAddress();
                    Fee_State.gcBlockIndex = 0u;
                    Fee_State.state = FEE_STATE_GC_FIND_NEXT;
                }
            }
            break;

        case FEE_STATE_GC_FIND_NEXT:
            while (Fee_State.gcBlockIndex < FEE_CONFIGURED_BLOCKS)
            {
                if ((Fee_Runtime[Fee_State.gcBlockIndex].invalid == FALSE) &&
                    ((Fee_Runtime[Fee_State.gcBlockIndex].copyValid[0] == TRUE) || (Fee_Runtime[Fee_State.gcBlockIndex].copyValid[1] == TRUE)))
                {
                    Fee_State.state = FEE_STATE_GC_READ_SOURCE_START;
                    break;
                }
                Fee_State.gcBlockIndex++;
            }
            if (Fee_State.gcBlockIndex >= FEE_CONFIGURED_BLOCKS)
            {
                /* All live blocks copied. Seal the destination by writing the
                 * sector-valid marker into the slot reserved after the header,
                 * then retire the old sector. */
                Fee_State.state = FEE_STATE_GC_MARKER_START;
            }
            break;

        case FEE_STATE_GC_READ_SOURCE_START:
            idx = (sint16)Fee_State.gcBlockIndex;
            if (Fee_SelectReadCopy((uint16)idx, 0u, &address, &crc, &length) == FALSE)
            {
                Fee_State.gcBlockIndex++;
                Fee_State.state = FEE_STATE_GC_FIND_NEXT;
            }
            else
            {
                Fee_State.gcSourceAddress = address;
                Fee_State.gcSourceCrc = crc;
                Fee_State.gcSourceLength = length;
                Fee_State.jobBlockIndex = Fee_State.gcBlockIndex;
                Fee_State.jobBlockNumber = Fee_BlockConfig[Fee_State.gcBlockIndex].blockNumber;
                Fee_State.jobSequence = Fee_Runtime[Fee_State.gcBlockIndex].latestSequence;
                Fee_State.copiesToWrite = (Fee_BlockConfig[Fee_State.gcBlockIndex].managementType == FEE_BLOCK_REDUNDANT) ? 2u : 1u;
                Fee_State.currentCopy = 0u;
                Fee_State.userJob = FEE_USER_JOB_WRITE;
                Fee_State.state = FEE_STATE_GC_WRITE_PREPARE;
            }
            break;

        case FEE_STATE_GC_WRITE_PREPARE:
        case FEE_STATE_GC_WRITE_HEADER_WAIT:
        case FEE_STATE_GC_WRITE_DATA_WAIT:
        case FEE_STATE_GC_WRITE_TRAILER_WAIT:
            Fee_ProcessWriteStates(FEE_STATE_GC_WRITE_PREPARE,
                                   FEE_STATE_GC_WRITE_HEADER_WAIT,
                                   FEE_STATE_GC_WRITE_DATA_WAIT,
                                   FEE_STATE_GC_WRITE_TRAILER_WAIT,
                                   FEE_STATE_GC_WRITE_PREPARE,
                                   TRUE,
                                   TRUE);
            if (Fee_State.state == FEE_STATE_GC_FIND_NEXT)
            {
                Fee_State.gcBlockIndex++;
            }
            break;

        case FEE_STATE_GC_MARKER_START:
            /* Seal the destination: write the marker into the reserved slot right
             * after the header. activeAppend already points past the last copied
             * block and must stay there for subsequent appends, so write at the
             * fixed slot offset rather than at activeAppend. */
            Fee_WriteSectorValidMarkerAt(Fee_SectorMarkerOffset(Fee_State.activeSector),
                                         FEE_STATE_GC_MARKER_WAIT);
            break;

        case FEE_STATE_GC_MARKER_WAIT:
            /* Once the seal commits the destination is a complete, scannable
             * sector; now retire the old one. */
            Fee_ProcessMarkerWait(FEE_STATE_GC_MARKER_WAIT, FEE_STATE_GC_ERASE_OLD_START);
            break;

        case FEE_STATE_GC_ERASE_OLD_START:
            if (Fee_StartErase(Fee_SectorBase(Fee_State.gcOldSector), FEE_VIRTUAL_SECTOR_SIZE) != E_OK)
            {
                Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_SectorBase(Fee_State.gcOldSector));
            }
            else
            {
                Fee_State.state = FEE_STATE_GC_ERASE_OLD_WAIT;
            }
            break;

        case FEE_STATE_GC_ERASE_OLD_WAIT:
            if (Fls_GetStatus() == MEMIF_IDLE)
            {
                if (Fls_GetJobResult() != MEMIF_JOB_OK)
                {
                    Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_SectorBase(Fee_State.gcOldSector));
                }
                else if (Fee_State.pendingAfterGc == TRUE)
                {
                    Fee_RestorePendingJobAfterGc();
                }
                else
                {
                    Fee_SetIdle(MEMIF_JOB_OK);
                }
            }
            break;

        default:
            Fee_SetFailed(FEE_E_INTERNAL, (uint32)Fee_State.state);
            break;
    }

}

void Fee_MainFunction(void)
{
    uint8 stepBudget = FEE_MAIN_STEP_BUDGET;
    MemIf_StatusType statusBefore;
    Fee_InternalStateType stateBefore;
    Fee_UserJobType userJobBefore;
    uint8 currentCopyBefore;
    uint32 activeAppendBefore;

    do
    {
        statusBefore = Fee_State.status;
        stateBefore = Fee_State.state;
        userJobBefore = Fee_State.userJob;
        currentCopyBefore = Fee_State.currentCopy;
        activeAppendBefore = Fee_State.activeAppend;

        Fee_MainFunctionStep();

        if ((Fee_State.state == FEE_STATE_IDLE) || (Fee_State.state == FEE_STATE_UNINIT))
        {
            break;
        }

        if ((statusBefore == Fee_State.status) &&
                (stateBefore == Fee_State.state) &&
                (userJobBefore == Fee_State.userJob) &&
                (currentCopyBefore == Fee_State.currentCopy) &&
                (activeAppendBefore == Fee_State.activeAppend))
        {
            break;
        }

        stepBudget--;
    } while (stepBudget > 0u);

    Fee_MainFunction_Counter++;
}
