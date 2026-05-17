#include <stddef.h>
#include <string.h>
#include "Fee.h"
#include "Fls.h"
#include "Crc.h"
#include "MemStack_Error.h"

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
} Fee_BlockRuntimeType;

typedef struct
{
    boolean headerValid;
    boolean sectorValidMarkerFound;
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
    FEE_STATE_GC_READ_SOURCE_WAIT,
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
} Fee_StateType;

static Fee_StateType Fee_State;
static Fee_BlockRuntimeType Fee_Runtime[FEE_CONFIGURED_BLOCKS];
static uint8 Fee_JobData[FEE_MAX_BLOCK_SIZE + FLS_DFLASH0_PAGE_SIZE];
static uint8 Fee_PendingData[FEE_MAX_BLOCK_SIZE + FLS_DFLASH0_PAGE_SIZE];
static uint8 Fee_RecordData[FEE_MAX_BLOCK_SIZE + FLS_DFLASH0_PAGE_SIZE];
static Fee_RecordHeaderType Fee_RecordHeader;
static Fee_RecordTrailerType Fee_RecordTrailer;
static Fee_SectorHeaderType Fee_SectorHeader;
static uint8 Fee_MarkerBuffer[sizeof(Fee_RecordHeaderType) + sizeof(Fee_RecordTrailerType)];
static uint32 Fee_RecordStartOffset;
static uint32 Fee_RecordTotalLength;
static uint32 Fee_RecordPaddedLength;
static uint32 Fee_NextSequence;

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

static void Fee_CopyFromFlash(uint32 offset, void *dst, uint32 length)
{
    memcpy(dst, (const void *)Fls_GetPhysicalAddress(offset), (size_t)length);
}

static boolean Fee_IsFlashErased(uint32 offset, uint32 length)
{
    const volatile uint8 *ptr = (const volatile uint8 *)Fls_GetPhysicalAddress(offset);
    uint32 i;
    for (i = 0u; i < length; i++)
    {
        if (ptr[i] != FLS_ERASED_VALUE)
        {
            return FALSE;
        }
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
    uint32 i;

    Fee_RecordHeader.magic = FEE_RECORD_MAGIC;
    Fee_RecordHeader.blockNumber = blockNumber;
    Fee_RecordHeader.flags = flags;
    Fee_RecordHeader.length = length;
    Fee_RecordHeader.sequence = sequence;

    Fee_RecordPaddedLength = Fee_Align8(length);
    memset(Fee_RecordData, FLS_ERASED_VALUE, sizeof(Fee_RecordData));
    if ((data != NULL_PTR) && (length > 0u))
    {
        for (i = 0u; i < length; i++)
        {
            Fee_RecordData[i] = data[i];
        }
    }

    Fee_RecordHeader.dataCrc = Crc_CalculateCRC32(Fee_RecordData, length, 0u, TRUE);
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
    Fee_CopyFromFlash(trailerOffset, &trailer, sizeof(trailer));

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

    Fee_CopyFromFlash(recordOffset, header, sizeof(Fee_RecordHeaderType));
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
        crc = Crc_CalculateCRC32((const uint8 *)Fls_GetPhysicalAddress(dataOffset), header->length, 0u, TRUE);
        if (crc != header->dataCrc)
        {
            return FALSE;
        }
    }

    return TRUE;
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
        }
        return;
    }

    if (header->length != block->blockSize)
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
        }
        block->invalid = FALSE;
        block->latestSequence = header->sequence;
        block->copyValid[copyIndex] = TRUE;
        block->copyAddress[copyIndex] = recordOffset + (uint32)sizeof(Fee_RecordHeaderType);
        block->copySequence[copyIndex] = header->sequence;
        block->copyCrc[copyIndex] = header->dataCrc;
    }
}

static void Fee_ScanSector(uint8 sector, Fee_ScanResultType *result)
{
    uint32 cursor;
    uint32 end;
    uint32 totalLength;
    Fee_RecordHeaderType header;
    Fee_SectorHeaderType sectorHeader;

    memset(result, 0, sizeof(*result));
    Fee_RuntimeInit(result->block);
    result->appendOffset = Fee_SectorBase(sector) + (uint32)sizeof(Fee_SectorHeaderType);

    Fee_CopyFromFlash(Fee_SectorBase(sector), &sectorHeader, sizeof(sectorHeader));
    if (Fee_IsSectorHeaderValid(&sectorHeader) == FALSE)
    {
        return;
    }

    result->headerValid = TRUE;
    result->generation = sectorHeader.generation;
    cursor = Fee_SectorBase(sector) + (uint32)sizeof(Fee_SectorHeaderType);
    end = Fee_SectorEnd(sector);

    while ((cursor + sizeof(Fee_RecordHeaderType) + sizeof(Fee_RecordTrailerType)) <= end)
    {
        if (Fee_IsFlashErased(cursor, FLS_DFLASH0_PAGE_SIZE) == TRUE)
        {
            break;
        }

        if (Fee_IsRecordValidAt(cursor, end, &header) == FALSE)
        {
            break;
        }

        if ((header.flags & FEE_FLAG_SECTOR_VALID) != 0u)
        {
            result->sectorValidMarkerFound = TRUE;
        }
        else
        {
            Fee_ApplyRecord(result->block, cursor, &header);
        }

        totalLength = (uint32)sizeof(Fee_RecordHeaderType) + Fee_Align8(header.length) + (uint32)sizeof(Fee_RecordTrailerType);
        cursor += totalLength;
    }

    result->appendOffset = cursor;
}

static boolean Fee_ChooseActiveSector(void)
{
    Fee_ScanResultType scan0;
    Fee_ScanResultType scan1;
    boolean valid0;
    boolean valid1;

    Fee_ScanSector(0u, &scan0);
    Fee_ScanSector(1u, &scan1);
    valid0 = (boolean)(scan0.headerValid && scan0.sectorValidMarkerFound);
    valid1 = (boolean)(scan1.headerValid && scan1.sectorValidMarkerFound);

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
}

static void Fee_SetFailed(uint8 error, uint32 detail)
{
    MemStack_ReportError(MEMSTACK_MODULE_ID_FEE, FEE_API_MAIN, error, detail);
    Fee_SetIdle(MEMIF_JOB_FAILED);
}

static Std_ReturnType Fee_StartHeaderWrite(uint32 address)
{
    return Fls_Write(address, (const uint8 *)&Fee_RecordHeader, (Fls_LengthType)sizeof(Fee_RecordHeaderType));
}

static Std_ReturnType Fee_StartDataWrite(uint32 address)
{
    if (Fee_RecordPaddedLength == 0u)
    {
        return E_OK;
    }
    return Fls_Write(address, Fee_RecordData, Fee_RecordPaddedLength);
}

static Std_ReturnType Fee_StartTrailerWrite(uint32 address)
{
    return Fls_Write(address, (const uint8 *)&Fee_RecordTrailer, (Fls_LengthType)sizeof(Fee_RecordTrailerType));
}

static void Fee_UpdateRuntimeAfterCurrentRecord(void)
{
    Fee_ApplyRecord(Fee_Runtime, Fee_RecordStartOffset, &Fee_RecordHeader);
}

static boolean Fee_SelectReadCopy(uint16 blockIndex, uint8 attempt, uint32 *address, uint32 *crc)
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
            return TRUE;
        }
        if (block->copyValid[1] == TRUE)
        {
            *address = block->copyAddress[1];
            *crc = block->copyCrc[1];
            return TRUE;
        }
    }
    else if (attempt == 1u)
    {
        if ((block->copyValid[1] == TRUE) && (block->copyAddress[1] != block->copyAddress[0]))
        {
            *address = block->copyAddress[1];
            *crc = block->copyCrc[1];
            return TRUE;
        }
    }
    return FALSE;
}

static void Fee_StartGc(void)
{
    uint16 blockSize;

    Fee_State.pendingAfterGc = TRUE;
    Fee_State.pendingJob = Fee_State.userJob;
    Fee_State.pendingBlockNumber = Fee_State.jobBlockNumber;

    if (Fee_State.userJob == FEE_USER_JOB_WRITE)
    {
        blockSize = Fee_BlockConfig[Fee_State.jobBlockIndex].blockSize;
        memcpy(Fee_PendingData, Fee_JobData, blockSize);
    }

    Fee_State.gcOldSector = Fee_State.activeSector;
    Fee_State.gcDestSector = (Fee_State.activeSector == 0u) ? 1u : 0u;
    Fee_State.gcBlockIndex = 0u;
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
    if (Fee_State.userJob == FEE_USER_JOB_WRITE)
    {
        memcpy(Fee_JobData, Fee_PendingData, Fee_BlockConfig[Fee_State.jobBlockIndex].blockSize);
    }

    Fee_State.jobSequence = Fee_NextSequence++;
    Fee_State.copiesToWrite = (Fee_BlockConfig[Fee_State.jobBlockIndex].managementType == FEE_BLOCK_REDUNDANT) ? 2u : 1u;
    Fee_State.currentCopy = 0u;
    Fee_State.pendingAfterGc = FALSE;
    Fee_State.status = MEMIF_BUSY;
    Fee_State.state = FEE_STATE_WRITE_PREPARE;
}

void Fee_Init(const Fee_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;
    memset(&Fee_State, 0, sizeof(Fee_State));
    Fee_RuntimeInit(Fee_Runtime);
    Fls_Init(NULL_PTR);

    if (Fee_ChooseActiveSector() == TRUE)
    {
        Fee_State.status = MEMIF_IDLE;
        Fee_State.result = MEMIF_JOB_OK;
        Fee_State.state = FEE_STATE_IDLE;
    }
    else
    {
        Fee_State.status = MEMIF_BUSY_INTERNAL;
        Fee_State.result = MEMIF_JOB_PENDING;
        Fee_State.activeSector = 0u;
        Fee_State.activeGeneration = 1u;
        Fee_State.activeAppend = FEE_VIRTUAL_SECTOR0_OFFSET + (uint32)sizeof(Fee_SectorHeaderType);
        Fee_State.state = FEE_STATE_FORMAT_ERASE0_START;
        Fee_NextSequence = 1u;
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

static void Fee_ProcessWriteStates(Fee_InternalStateType prepareState,
                                   Fee_InternalStateType headerWaitState,
                                   Fee_InternalStateType dataWaitState,
                                   Fee_InternalStateType trailerWaitState,
                                   Fee_InternalStateType nextPrepareState,
                                   boolean updateRuntime)
{
    uint16 flags;
    uint32 dataLength;
    const uint8 *dataPtr;
    uint32 dataOffset;
    uint32 trailerOffset;

    if (Fee_State.state == prepareState)
    {
        flags = 0u;
        dataLength = Fee_BlockConfig[Fee_State.jobBlockIndex].blockSize;
        dataPtr = Fee_JobData;

        if ((Fee_State.userJob == FEE_USER_JOB_INVALIDATE) || (Fee_State.userJob == FEE_USER_JOB_ERASE_IMMEDIATE))
        {
            flags |= FEE_FLAG_INVALID;
            dataLength = 0u;
            dataPtr = NULL_PTR;
        }
        if ((Fee_BlockConfig[Fee_State.jobBlockIndex].managementType == FEE_BLOCK_REDUNDANT) && (Fee_State.currentCopy == 1u))
        {
            flags |= FEE_FLAG_REDUNDANT_COPY1;
        }

        Fee_RecordStartOffset = Fee_State.activeAppend;
        Fee_PrepareRecord(Fee_State.jobBlockNumber, flags, dataPtr, dataLength, Fee_State.jobSequence);
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

static void Fee_WriteSectorValidMarker(Fee_InternalStateType waitState)
{
    Fee_RecordStartOffset = Fee_State.activeAppend;
    Fee_PrepareRecord(FEE_MARKER_BLOCK_NUMBER, FEE_FLAG_SECTOR_VALID, NULL_PTR, 0u, Fee_State.activeGeneration);
    memcpy(&Fee_MarkerBuffer[0], &Fee_RecordHeader, sizeof(Fee_RecordHeaderType));
    memcpy(&Fee_MarkerBuffer[sizeof(Fee_RecordHeaderType)], &Fee_RecordTrailer, sizeof(Fee_RecordTrailerType));

    if (Fls_Write(Fee_RecordStartOffset, Fee_MarkerBuffer, Fee_RecordTotalLength) != E_OK)
    {
        Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_RecordStartOffset);
        return;
    }
    Fee_State.state = waitState;
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

void Fee_MainFunction(void)
{
    uint32 address;
    uint32 crc;
    uint16 length;
    sint16 idx;

    switch (Fee_State.state)
    {
        case FEE_STATE_UNINIT:
        case FEE_STATE_IDLE:
            break;

        case FEE_STATE_FORMAT_ERASE0_START:
            if (Fls_Erase(FEE_VIRTUAL_SECTOR0_OFFSET, FEE_VIRTUAL_SECTOR_SIZE) != E_OK)
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
            if (Fls_Write(FEE_VIRTUAL_SECTOR0_OFFSET, (const uint8 *)&Fee_SectorHeader, sizeof(Fee_SectorHeader)) != E_OK)
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
            }
            break;

        case FEE_STATE_FORMAT_ERASE1_START:
            if (Fls_Erase(FEE_VIRTUAL_SECTOR1_OFFSET, FEE_VIRTUAL_SECTOR_SIZE) != E_OK)
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
                                   TRUE);
            break;

        case FEE_STATE_READ_START:
            if (Fee_SelectReadCopy(Fee_State.jobBlockIndex, Fee_State.currentCopy, &address, &crc) == FALSE)
            {
                Fee_SetIdle(MEMIF_BLOCK_INVALID);
            }
            else if (Fls_Read(address, Fee_JobData, Fee_BlockConfig[Fee_State.jobBlockIndex].blockSize) != E_OK)
            {
                Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, address);
            }
            else
            {
                Fee_RecordHeader.dataCrc = crc;
                Fee_State.state = FEE_STATE_READ_WAIT;
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
                    length = Fee_BlockConfig[Fee_State.jobBlockIndex].blockSize;
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
            if (Fls_Erase(Fee_SectorBase(Fee_State.gcDestSector), FEE_VIRTUAL_SECTOR_SIZE) != E_OK)
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
            Fee_PrepareSectorHeader(Fee_State.activeGeneration);
            if (Fls_Write(Fee_SectorBase(Fee_State.activeSector), (const uint8 *)&Fee_SectorHeader, sizeof(Fee_SectorHeader)) != E_OK)
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
                Fee_State.state = FEE_STATE_GC_MARKER_START;
            }
            break;

        case FEE_STATE_GC_READ_SOURCE_START:
            idx = (sint16)Fee_State.gcBlockIndex;
            if (Fee_SelectReadCopy((uint16)idx, 0u, &address, &crc) == FALSE)
            {
                Fee_State.gcBlockIndex++;
                Fee_State.state = FEE_STATE_GC_FIND_NEXT;
            }
            else if (Fls_Read(address, Fee_JobData, Fee_BlockConfig[(uint16)idx].blockSize) != E_OK)
            {
                Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, address);
            }
            else
            {
                Fee_RecordHeader.dataCrc = crc;
                Fee_State.state = FEE_STATE_GC_READ_SOURCE_WAIT;
            }
            break;

        case FEE_STATE_GC_READ_SOURCE_WAIT:
            if (Fls_GetStatus() == MEMIF_IDLE)
            {
                if (Fls_GetJobResult() != MEMIF_JOB_OK)
                {
                    Fee_SetFailed(FEE_E_FLASH_JOB_FAILED, Fee_State.gcBlockIndex);
                }
                else
                {
                    length = Fee_BlockConfig[Fee_State.gcBlockIndex].blockSize;
                    crc = Crc_CalculateCRC32(Fee_JobData, length, 0u, TRUE);
                    if (crc != Fee_RecordHeader.dataCrc)
                    {
                        Fee_SetFailed(FEE_E_INTERNAL, Fee_State.gcBlockIndex);
                    }
                    else
                    {
                        Fee_State.jobBlockIndex = Fee_State.gcBlockIndex;
                        Fee_State.jobBlockNumber = Fee_BlockConfig[Fee_State.gcBlockIndex].blockNumber;
                        Fee_State.jobSequence = Fee_Runtime[Fee_State.gcBlockIndex].latestSequence;
                        Fee_State.copiesToWrite = (Fee_BlockConfig[Fee_State.gcBlockIndex].managementType == FEE_BLOCK_REDUNDANT) ? 2u : 1u;
                        Fee_State.currentCopy = 0u;
                        Fee_State.userJob = FEE_USER_JOB_WRITE;
                        Fee_State.state = FEE_STATE_GC_WRITE_PREPARE;
                    }
                }
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
                                   TRUE);
            if (Fee_State.state == FEE_STATE_GC_FIND_NEXT)
            {
                Fee_State.gcBlockIndex++;
            }
            break;

        case FEE_STATE_GC_MARKER_START:
            Fee_WriteSectorValidMarker(FEE_STATE_GC_MARKER_WAIT);
            break;

        case FEE_STATE_GC_MARKER_WAIT:
            Fee_ProcessMarkerWait(FEE_STATE_GC_MARKER_WAIT, FEE_STATE_GC_ERASE_OLD_START);
            if (Fee_State.state == FEE_STATE_GC_ERASE_OLD_START)
            {
                Fee_State.activeAppend += Fee_RecordTotalLength;
            }
            break;

        case FEE_STATE_GC_ERASE_OLD_START:
            if (Fls_Erase(Fee_SectorBase(Fee_State.gcOldSector), FEE_VIRTUAL_SECTOR_SIZE) != E_OK)
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

    Fee_MainFunction_Counter++;
}
