#include <string.h>
#include "NvM.h"
#include "MemIf.h"
#include "Fee.h"
#include "Fls.h"

#define NVM_API_INIT                    (0x00u)
#define NVM_API_READ_BLOCK              (0x06u)
#define NVM_API_WRITE_BLOCK             (0x07u)
#define NVM_API_RESTORE_DEFAULTS        (0x08u)
#define NVM_API_READ_ALL                (0x0Cu)
#define NVM_API_WRITE_ALL               (0x0Du)
#define NVM_API_CANCEL                  (0x10u)
#define NVM_API_GET_ERROR_STATUS        (0x04u)
#define NVM_API_SET_RAM_STATUS          (0x05u)
#define NVM_API_INVALIDATE              (0x0Bu)
#define NVM_API_ERASE                   (0x09u)
#define NVM_API_MAIN                    (0x0Eu)

#define NVM_E_NOT_INITIALIZED           (0x01u)
#define NVM_E_BLOCK_PENDING             (0x02u)
#define NVM_E_PARAM_BLOCK_ID            (0x03u)
#define NVM_E_PARAM_ADDRESS             (0x04u)
#define NVM_E_QUEUE_FULL                (0x05u)
#define NVM_E_LOWER_LAYER               (0x06u)

typedef enum
{
    NVM_SERVICE_NONE = 0,
    NVM_SERVICE_READ_BLOCK,
    NVM_SERVICE_WRITE_BLOCK,
    NVM_SERVICE_RESTORE_DEFAULTS,
    NVM_SERVICE_READ_ALL,
    NVM_SERVICE_WRITE_ALL,
    NVM_SERVICE_INVALIDATE_BLOCK,
    NVM_SERVICE_ERASE_BLOCK,
    NVM_SERVICE_INIT_WAIT
} NvM_ServiceType;

typedef enum
{
    NVM_STATE_UNINIT = 0,
    NVM_STATE_IDLE,
    NVM_STATE_INIT_WAIT,
    NVM_STATE_READ_START,
    NVM_STATE_READ_WAIT,
    NVM_STATE_WRITE_START,
    NVM_STATE_WRITE_WAIT,
    NVM_STATE_RESTORE_START,
    NVM_STATE_READALL_NEXT,
    NVM_STATE_READALL_WAIT,
    NVM_STATE_WRITEALL_NEXT,
    NVM_STATE_WRITEALL_WAIT,
    NVM_STATE_INVALIDATE_START,
    NVM_STATE_INVALIDATE_WAIT
} NvM_InternalStateType;

typedef struct
{
    NvM_StatusType status;
    NvM_InternalStateType state;
    NvM_ServiceType service;
    uint16 currentIndex;
    uint16 activeIndex;
    void *activePtr;
} NvM_StateType;

typedef struct
{
    NvM_RequestResultType result;
    boolean ramValid;
    boolean ramChanged;
    boolean writeProtected;
} NvM_BlockAdminType;

static NvM_StateType NvM_State =
{
    NVM_UNINIT,
    NVM_STATE_UNINIT,
    NVM_SERVICE_NONE,
    0u,
    0u,
    NULL_PTR
};

static NvM_BlockAdminType NvM_Admin[NVM_TOTAL_BLOCKS];

volatile uint32 NvM_WriteAllRequestCounter = 0u;
volatile uint32 NvM_WriteAllPlannedBytes = 0u;
volatile uint32 NvM_WriteAllWriteStartedBytes = 0u;
volatile uint32 NvM_WriteAllWrittenBytes = 0u;
volatile uint32 NvM_WriteAllFailedBytes = 0u;
volatile uint32 NvM_WriteAllSkippedBytes = 0u;
volatile uint16 NvM_WriteAllBlocksPlanned = 0u;
volatile uint16 NvM_WriteAllBlocksStarted = 0u;
volatile uint16 NvM_WriteAllBlocksWritten = 0u;
volatile uint16 NvM_WriteAllBlocksFailed = 0u;
volatile uint16 NvM_WriteAllBlocksSkipped = 0u;
volatile uint16 NvM_WriteAllActiveIndex = 0xFFFFu;
volatile NvM_BlockIdType NvM_WriteAllActiveBlockId = 0u;
volatile uint32 NvM_WriteAllBlockLength[NVM_TOTAL_BLOCKS];
volatile uint8 NvM_WriteAllBlockPlanned[NVM_TOTAL_BLOCKS];
volatile uint8 NvM_WriteAllBlockWritten[NVM_TOTAL_BLOCKS];
volatile uint8 NvM_WriteAllBlockResult[NVM_TOTAL_BLOCKS];

static void NvM_ResetWriteAllDebug(void)
{
    uint16 i;

    NvM_WriteAllPlannedBytes = 0u;
    NvM_WriteAllWriteStartedBytes = 0u;
    NvM_WriteAllWrittenBytes = 0u;
    NvM_WriteAllFailedBytes = 0u;
    NvM_WriteAllSkippedBytes = 0u;
    NvM_WriteAllBlocksPlanned = 0u;
    NvM_WriteAllBlocksStarted = 0u;
    NvM_WriteAllBlocksWritten = 0u;
    NvM_WriteAllBlocksFailed = 0u;
    NvM_WriteAllBlocksSkipped = 0u;
    NvM_WriteAllActiveIndex = 0xFFFFu;
    NvM_WriteAllActiveBlockId = 0u;

    for (i = 0u; i < (uint16)NVM_TOTAL_BLOCKS; i++)
    {
        NvM_WriteAllBlockLength[i] = NvM_BlockDescriptor[i].nvBlockLength;
        NvM_WriteAllBlockPlanned[i] = 0u;
        NvM_WriteAllBlockWritten[i] = 0u;
        NvM_WriteAllBlockResult[i] = 0u;

        if ((NvM_Admin[i].ramValid == TRUE) &&
            (NvM_Admin[i].ramChanged == TRUE))
        {
            NvM_WriteAllBlockPlanned[i] = 1u;
            NvM_WriteAllPlannedBytes += NvM_BlockDescriptor[i].nvBlockLength;
            NvM_WriteAllBlocksPlanned++;
        }
        else
        {
            NvM_WriteAllSkippedBytes += NvM_BlockDescriptor[i].nvBlockLength;
            NvM_WriteAllBlocksSkipped++;
        }
    }
}

static sint16 NvM_FindBlockIndex(NvM_BlockIdType BlockId)
{
    uint16 i;
    for (i = 0u; i < NVM_TOTAL_BLOCKS; i++)
    {
        if (NvM_BlockDescriptor[i].blockId == BlockId)
        {
            return (sint16)i;
        }
    }
    return -1;
}

static uint8 *NvM_GetRamPtr(uint16 idx)
{
    return NvM_BlockDescriptor[idx].ramBlockDataAddress;
}

static const uint8 *NvM_GetRomPtr(uint16 idx)
{
    return NvM_BlockDescriptor[idx].romBlockDataAddress;
}

static void NvM_SetIdle(void)
{
    NvM_State.status = NVM_IDLE;
    NvM_State.state = NVM_STATE_IDLE;
    NvM_State.service = NVM_SERVICE_NONE;
    NvM_State.activePtr = NULL_PTR;
}

static void NvM_SetBusy(NvM_ServiceType service, NvM_InternalStateType state)
{
    NvM_State.status = NVM_BUSY;
    NvM_State.service = service;
    NvM_State.state = state;
}

static void NvM_Report(uint8 api, uint8 error, uint32 detail)
{
    MemStack_ReportError(MEMSTACK_MODULE_ID_NVM, api, error, detail);
}

static void NvM_RestoreDefaultsByIndex(uint16 idx, void *dest)
{
    uint8 *target;
    const uint8 *romPtr;

    target = (dest != NULL_PTR) ? (uint8 *)dest : NvM_GetRamPtr(idx);
    if (target == NULL_PTR)
    {
        return;
    }

    romPtr = NvM_GetRomPtr(idx);
    if (romPtr != NULL_PTR)
    {
        memcpy(target, romPtr, NvM_BlockDescriptor[idx].nvBlockLength);
    }
    else
    {
        memset(target, 0, NvM_BlockDescriptor[idx].nvBlockLength);
    }
}

static void NvM_RestoreReadDefaults(uint16 idx)
{
    uint8 *ramPtr;

    ramPtr = NvM_GetRamPtr(idx);
    NvM_RestoreDefaultsByIndex(idx, ramPtr);
    if ((NvM_State.activePtr != NULL_PTR) && (NvM_State.activePtr != ramPtr))
    {
        NvM_RestoreDefaultsByIndex(idx, NvM_State.activePtr);
    }
}

void NvM_Init(const NvM_ConfigType *ConfigPtr)
{
    uint16 i;

    (void)ConfigPtr;

    for (i = 0u; i < NVM_TOTAL_BLOCKS; i++)
    {
        NvM_Admin[i].result = NVM_REQ_PENDING;
        NvM_Admin[i].ramValid = FALSE;
        NvM_Admin[i].ramChanged = FALSE;
        NvM_Admin[i].writeProtected = FALSE;
        NvM_RestoreDefaultsByIndex(i, NULL_PTR);
    }

    NvM_ResetWriteAllDebug();

    Fee_Init(NULL_PTR);
    NvM_State.status = NVM_BUSY_INTERNAL;
    NvM_State.state = NVM_STATE_INIT_WAIT;
    NvM_State.service = NVM_SERVICE_INIT_WAIT;
}

Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void *NvM_DstPtr)
{
    sint16 idx;

    if (NvM_State.status == NVM_UNINIT)
    {
        NvM_Report(NVM_API_READ_BLOCK, NVM_E_NOT_INITIALIZED, BlockId);
        return E_NOT_OK;
    }
    if (NvM_State.status != NVM_IDLE)
    {
        NvM_Report(NVM_API_READ_BLOCK, NVM_E_BLOCK_PENDING, BlockId);
        return E_NOT_OK;
    }

    idx = NvM_FindBlockIndex(BlockId);
    if (idx < 0)
    {
        NvM_Report(NVM_API_READ_BLOCK, NVM_E_PARAM_BLOCK_ID, BlockId);
        return E_NOT_OK;
    }
    if ((NvM_DstPtr == NULL_PTR) && (NvM_GetRamPtr((uint16)idx) == NULL_PTR))
    {
        NvM_Report(NVM_API_READ_BLOCK, NVM_E_PARAM_ADDRESS, BlockId);
        return E_NOT_OK;
    }

    NvM_State.activeIndex = (uint16)idx;
    NvM_State.activePtr = (NvM_DstPtr != NULL_PTR) ? NvM_DstPtr : NvM_GetRamPtr((uint16)idx);
    NvM_Admin[(uint16)idx].result = NVM_REQ_PENDING;
    NvM_SetBusy(NVM_SERVICE_READ_BLOCK, NVM_STATE_READ_START);
    return E_OK;
}

Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void *NvM_SrcPtr)
{
    sint16 idx;

    if (NvM_State.status == NVM_UNINIT)
    {
        NvM_Report(NVM_API_WRITE_BLOCK, NVM_E_NOT_INITIALIZED, BlockId);
        return E_NOT_OK;
    }
    if (NvM_State.status != NVM_IDLE)
    {
        NvM_Report(NVM_API_WRITE_BLOCK, NVM_E_BLOCK_PENDING, BlockId);
        return E_NOT_OK;
    }

    idx = NvM_FindBlockIndex(BlockId);
    if (idx < 0)
    {
        NvM_Report(NVM_API_WRITE_BLOCK, NVM_E_PARAM_BLOCK_ID, BlockId);
        return E_NOT_OK;
    }
    if (NvM_Admin[(uint16)idx].writeProtected == TRUE)
    {
        NvM_Admin[(uint16)idx].result = NVM_REQ_BLOCK_SKIPPED;
        return E_NOT_OK;
    }
    if ((NvM_SrcPtr == NULL_PTR) && (NvM_GetRamPtr((uint16)idx) == NULL_PTR))
    {
        NvM_Report(NVM_API_WRITE_BLOCK, NVM_E_PARAM_ADDRESS, BlockId);
        return E_NOT_OK;
    }
    if (NvM_SrcPtr != NULL_PTR)
    {
        memcpy(NvM_GetRamPtr((uint16)idx), NvM_SrcPtr, NvM_BlockDescriptor[(uint16)idx].nvBlockLength);
    }

    NvM_State.activeIndex = (uint16)idx;
    NvM_Admin[(uint16)idx].ramValid = TRUE;
    NvM_Admin[(uint16)idx].ramChanged = TRUE;
    NvM_Admin[(uint16)idx].result = NVM_REQ_PENDING;
    NvM_SetBusy(NVM_SERVICE_WRITE_BLOCK, NVM_STATE_WRITE_START);
    return E_OK;
}

Std_ReturnType NvM_RestoreBlockDefaults(NvM_BlockIdType BlockId, void *NvM_DestPtr)
{
    sint16 idx;

    if (NvM_State.status == NVM_UNINIT)
    {
        NvM_Report(NVM_API_RESTORE_DEFAULTS, NVM_E_NOT_INITIALIZED, BlockId);
        return E_NOT_OK;
    }
    if (NvM_State.status != NVM_IDLE)
    {
        NvM_Report(NVM_API_RESTORE_DEFAULTS, NVM_E_BLOCK_PENDING, BlockId);
        return E_NOT_OK;
    }

    idx = NvM_FindBlockIndex(BlockId);
    if (idx < 0)
    {
        NvM_Report(NVM_API_RESTORE_DEFAULTS, NVM_E_PARAM_BLOCK_ID, BlockId);
        return E_NOT_OK;
    }

    NvM_State.activeIndex = (uint16)idx;
    NvM_State.activePtr = NvM_DestPtr;
    NvM_Admin[(uint16)idx].result = NVM_REQ_PENDING;
    NvM_SetBusy(NVM_SERVICE_RESTORE_DEFAULTS, NVM_STATE_RESTORE_START);
    return E_OK;
}

Std_ReturnType NvM_EraseNvBlock(NvM_BlockIdType BlockId)
{
    return NvM_InvalidateNvBlock(BlockId);
}

Std_ReturnType NvM_InvalidateNvBlock(NvM_BlockIdType BlockId)
{
    sint16 idx;

    if (NvM_State.status == NVM_UNINIT)
    {
        NvM_Report(NVM_API_INVALIDATE, NVM_E_NOT_INITIALIZED, BlockId);
        return E_NOT_OK;
    }
    if (NvM_State.status != NVM_IDLE)
    {
        NvM_Report(NVM_API_INVALIDATE, NVM_E_BLOCK_PENDING, BlockId);
        return E_NOT_OK;
    }
    idx = NvM_FindBlockIndex(BlockId);
    if (idx < 0)
    {
        NvM_Report(NVM_API_INVALIDATE, NVM_E_PARAM_BLOCK_ID, BlockId);
        return E_NOT_OK;
    }

    NvM_State.activeIndex = (uint16)idx;
    NvM_Admin[(uint16)idx].result = NVM_REQ_PENDING;
    NvM_SetBusy(NVM_SERVICE_INVALIDATE_BLOCK, NVM_STATE_INVALIDATE_START);
    return E_OK;
}

Std_ReturnType NvM_ReadAll(void)
{
    if (NvM_State.status == NVM_UNINIT)
    {
        NvM_Report(NVM_API_READ_ALL, NVM_E_NOT_INITIALIZED, 0u);
        return E_NOT_OK;
    }
    if (NvM_State.status != NVM_IDLE)
    {
        NvM_Report(NVM_API_READ_ALL, NVM_E_BLOCK_PENDING, 0u);
        return E_NOT_OK;
    }

    NvM_State.currentIndex = 0u;
    NvM_SetBusy(NVM_SERVICE_READ_ALL, NVM_STATE_READALL_NEXT);
    return E_OK;
}

Std_ReturnType NvM_WriteAll(void)
{
    if (NvM_State.status == NVM_UNINIT)
    {
        NvM_Report(NVM_API_WRITE_ALL, NVM_E_NOT_INITIALIZED, 0u);
        return E_NOT_OK;
    }
    if (NvM_State.status != NVM_IDLE)
    {
        NvM_Report(NVM_API_WRITE_ALL, NVM_E_BLOCK_PENDING, 0u);
        return E_NOT_OK;
    }

    NvM_State.currentIndex = 0u;
    NvM_WriteAllRequestCounter++;
    NvM_ResetWriteAllDebug();
    NvM_SetBusy(NVM_SERVICE_WRITE_ALL, NVM_STATE_WRITEALL_NEXT);
    return E_OK;
}

Std_ReturnType NvM_CancelJobs(void)
{
    if (NvM_State.status == NVM_UNINIT)
    {
        NvM_Report(NVM_API_CANCEL, NVM_E_NOT_INITIALIZED, 0u);
        return E_NOT_OK;
    }
    MemIf_Cancel(MEMIF_FEE_DEVICE_INDEX);
    NvM_SetIdle();
    return E_OK;
}

Std_ReturnType NvM_SetRamBlockStatus(NvM_BlockIdType BlockId, boolean BlockChanged)
{
    sint16 idx;

    if (NvM_State.status == NVM_UNINIT)
    {
        NvM_Report(NVM_API_SET_RAM_STATUS, NVM_E_NOT_INITIALIZED, BlockId);
        return E_NOT_OK;
    }

    idx = NvM_FindBlockIndex(BlockId);
    if (idx < 0)
    {
        NvM_Report(NVM_API_SET_RAM_STATUS, NVM_E_PARAM_BLOCK_ID, BlockId);
        return E_NOT_OK;
    }

    NvM_Admin[(uint16)idx].ramChanged = BlockChanged;
    if (BlockChanged == TRUE)
    {
        NvM_Admin[(uint16)idx].ramValid = TRUE;
    }

    if ((BlockChanged == TRUE) && (NvM_BlockDescriptor[(uint16)idx].immediateData == TRUE))
    {
        return NvM_WriteBlock(BlockId, NULL_PTR);
    }
    return E_OK;
}

Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType BlockId, NvM_RequestResultType *RequestResultPtr)
{
    sint16 idx;

    if (RequestResultPtr == NULL_PTR)
    {
        NvM_Report(NVM_API_GET_ERROR_STATUS, NVM_E_PARAM_ADDRESS, BlockId);
        return E_NOT_OK;
    }
    idx = NvM_FindBlockIndex(BlockId);
    if (idx < 0)
    {
        NvM_Report(NVM_API_GET_ERROR_STATUS, NVM_E_PARAM_BLOCK_ID, BlockId);
        return E_NOT_OK;
    }
    *RequestResultPtr = NvM_Admin[(uint16)idx].result;
    return E_OK;
}

NvM_StatusType NvM_GetStatus(void)
{
    return NvM_State.status;
}

void NvM_SetErrorCallback(MemStack_ErrorCallbackType callback)
{
    MemStack_SetErrorCallback(callback);
}

static void NvM_HandleReadCompletion(uint16 idx)
{
    MemIf_JobResultType result;

    result = MemIf_GetJobResult(MEMIF_FEE_DEVICE_INDEX);
    if (result == MEMIF_JOB_OK)
    {
        NvM_Admin[idx].result = NVM_REQ_OK;
        NvM_Admin[idx].ramValid = TRUE;
        NvM_Admin[idx].ramChanged = FALSE;
    }
    else if (result == MEMIF_BLOCK_INVALID)
    {
        NvM_RestoreReadDefaults(idx);
        NvM_Admin[idx].result = NVM_REQ_NV_INVALIDATED;
        NvM_Admin[idx].ramValid = TRUE;
        NvM_Admin[idx].ramChanged = TRUE;
    }
    else if (result == MEMIF_BLOCK_INCONSISTENT)
    {
        NvM_RestoreReadDefaults(idx);
        NvM_Admin[idx].result = NVM_REQ_INTEGRITY_FAILED;
        NvM_Admin[idx].ramValid = TRUE;
        NvM_Admin[idx].ramChanged = TRUE;
    }
    else
    {
        NvM_RestoreReadDefaults(idx);
        NvM_Admin[idx].result = NVM_REQ_NOT_OK;
        NvM_Admin[idx].ramValid = TRUE;
        NvM_Admin[idx].ramChanged = TRUE;
    }
}

long long NvM_MainFunction_Counter = 0;

void NvM_MainFunction(void)
{
    uint16 idx;

    switch (NvM_State.state)
    {
        case NVM_STATE_UNINIT:
        case NVM_STATE_IDLE:
            break;

        case NVM_STATE_INIT_WAIT:
            if (Fee_GetStatus() == MEMIF_IDLE)
            {
                if (Fee_GetJobResult() == MEMIF_JOB_OK)
                {
                    NvM_SetIdle();
                }
                else
                {
                    NvM_Report(NVM_API_INIT, NVM_E_LOWER_LAYER, Fee_GetJobResult());
                    NvM_State.status = NVM_IDLE;
                    NvM_State.state = NVM_STATE_IDLE;
                }
            }
            break;

        case NVM_STATE_READ_START:
            idx = NvM_State.activeIndex;
            if (MemIf_Read(MEMIF_FEE_DEVICE_INDEX,
                           NvM_BlockDescriptor[idx].blockId,
                           0u,
                           (uint8 *)NvM_State.activePtr,
                           NvM_BlockDescriptor[idx].nvBlockLength) != E_OK)
            {
                NvM_RestoreReadDefaults(idx);
                NvM_Admin[idx].result = NVM_REQ_NOT_OK;
                NvM_Admin[idx].ramValid = TRUE;
                NvM_Admin[idx].ramChanged = TRUE;
                NvM_Report(NVM_API_MAIN, NVM_E_LOWER_LAYER, NvM_BlockDescriptor[idx].blockId);
                NvM_SetIdle();
            }
            else
            {
                NvM_State.state = NVM_STATE_READ_WAIT;
            }
            break;

        case NVM_STATE_READ_WAIT:
            if (MemIf_GetStatus(MEMIF_FEE_DEVICE_INDEX) == MEMIF_IDLE)
            {
                idx = NvM_State.activeIndex;
                NvM_HandleReadCompletion(idx);
                NvM_SetIdle();
            }
            break;

        case NVM_STATE_WRITE_START:
            idx = NvM_State.activeIndex;
            NvM_Admin[idx].ramChanged = FALSE;
            if (MemIf_Write(MEMIF_FEE_DEVICE_INDEX, NvM_BlockDescriptor[idx].blockId, NvM_GetRamPtr(idx)) != E_OK)
            {
                NvM_Admin[idx].result = NVM_REQ_NOT_OK;
                NvM_Admin[idx].ramChanged = TRUE;
                NvM_Report(NVM_API_MAIN, NVM_E_LOWER_LAYER, NvM_BlockDescriptor[idx].blockId);
                NvM_SetIdle();
            }
            else
            {
                NvM_State.state = NVM_STATE_WRITE_WAIT;
            }
            break;

        case NVM_STATE_WRITE_WAIT:
            if (MemIf_GetStatus(MEMIF_FEE_DEVICE_INDEX) == MEMIF_IDLE)
            {
                idx = NvM_State.activeIndex;
                if (MemIf_GetJobResult(MEMIF_FEE_DEVICE_INDEX) == MEMIF_JOB_OK)
                {
                    NvM_Admin[idx].result = NVM_REQ_OK;
                    NvM_Admin[idx].ramValid = TRUE;
                    if (NvM_BlockDescriptor[idx].writeOnce == TRUE)
                    {
                        NvM_Admin[idx].writeProtected = TRUE;
                    }
                }
                else
                {
                    NvM_Admin[idx].result = NVM_REQ_NOT_OK;
                    NvM_Admin[idx].ramChanged = TRUE;
                }
                NvM_SetIdle();
            }
            break;

        case NVM_STATE_RESTORE_START:
            idx = NvM_State.activeIndex;
            NvM_RestoreDefaultsByIndex(idx, NvM_State.activePtr);
            NvM_Admin[idx].result = NVM_REQ_RESTORED_FROM_ROM;
            NvM_Admin[idx].ramValid = TRUE;
            NvM_Admin[idx].ramChanged = TRUE;
            NvM_SetIdle();
            break;

        case NVM_STATE_READALL_NEXT:
            if (NvM_State.currentIndex >= NVM_TOTAL_BLOCKS)
            {
                NvM_WriteAllActiveIndex = 0xFFFFu;
                NvM_WriteAllActiveBlockId = 0u;
                NvM_SetIdle();
            }
            else
            {
                idx = NvM_State.currentIndex;
                NvM_State.activeIndex = idx;
                NvM_State.activePtr = NvM_GetRamPtr(idx);
                NvM_Admin[idx].result = NVM_REQ_PENDING;
                if (MemIf_Read(MEMIF_FEE_DEVICE_INDEX,
                               NvM_BlockDescriptor[idx].blockId,
                               0u,
                               NvM_GetRamPtr(idx),
                               NvM_BlockDescriptor[idx].nvBlockLength) != E_OK)
                {
                    NvM_RestoreDefaultsByIndex(idx, NULL_PTR);
                    NvM_Admin[idx].result = NVM_REQ_NOT_OK;
                    NvM_Admin[idx].ramValid = TRUE;
                    NvM_Admin[idx].ramChanged = TRUE;
                    NvM_State.currentIndex++;
                }
                else
                {
                    NvM_State.state = NVM_STATE_READALL_WAIT;
                }
            }
            break;

        case NVM_STATE_READALL_WAIT:
            if (MemIf_GetStatus(MEMIF_FEE_DEVICE_INDEX) == MEMIF_IDLE)
            {
                idx = NvM_State.currentIndex;
                NvM_State.activePtr = NvM_GetRamPtr(idx);
                NvM_HandleReadCompletion(idx);
                NvM_State.currentIndex++;
                NvM_State.state = NVM_STATE_READALL_NEXT;
            }
            break;

        case NVM_STATE_WRITEALL_NEXT:
            if (NvM_State.currentIndex >= NVM_TOTAL_BLOCKS)
            {
                NvM_SetIdle();
            }
            else
            {
                idx = NvM_State.currentIndex;
                if ((NvM_Admin[idx].ramValid == TRUE) &&
                    (NvM_Admin[idx].ramChanged == TRUE))
                {
                    NvM_State.activeIndex = idx;
                    NvM_WriteAllActiveIndex = idx;
                    NvM_WriteAllActiveBlockId = NvM_BlockDescriptor[idx].blockId;
                    NvM_Admin[idx].result = NVM_REQ_PENDING;
                    NvM_Admin[idx].ramChanged = FALSE;
                    if (MemIf_Write(MEMIF_FEE_DEVICE_INDEX, NvM_BlockDescriptor[idx].blockId, NvM_GetRamPtr(idx)) != E_OK)
                    {
                        NvM_Admin[idx].result = NVM_REQ_NOT_OK;
                        NvM_Admin[idx].ramChanged = TRUE;
                        NvM_WriteAllFailedBytes += NvM_BlockDescriptor[idx].nvBlockLength;
                        NvM_WriteAllBlocksFailed++;
                        NvM_WriteAllBlockResult[idx] = 3u;
                        NvM_State.currentIndex++;
                    }
                    else
                    {
                        NvM_WriteAllWriteStartedBytes += NvM_BlockDescriptor[idx].nvBlockLength;
                        NvM_WriteAllBlocksStarted++;
                        NvM_WriteAllBlockResult[idx] = 1u;
                        NvM_State.state = NVM_STATE_WRITEALL_WAIT;
                    }
                }
                else
                {
                    NvM_Admin[idx].result = NVM_REQ_BLOCK_SKIPPED;
                    NvM_WriteAllBlockResult[idx] = 4u;
                    NvM_State.currentIndex++;
                }
            }
            break;

        case NVM_STATE_WRITEALL_WAIT:
            if (MemIf_GetStatus(MEMIF_FEE_DEVICE_INDEX) == MEMIF_IDLE)
            {
                idx = NvM_State.currentIndex;
                if (MemIf_GetJobResult(MEMIF_FEE_DEVICE_INDEX) == MEMIF_JOB_OK)
                {
                    NvM_Admin[idx].result = NVM_REQ_OK;
                    NvM_Admin[idx].ramValid = TRUE;
                    NvM_WriteAllWrittenBytes += NvM_BlockDescriptor[idx].nvBlockLength;
                    NvM_WriteAllBlocksWritten++;
                    NvM_WriteAllBlockWritten[idx] = 1u;
                    NvM_WriteAllBlockResult[idx] = 2u;
                }
                else
                {
                    NvM_Admin[idx].result = NVM_REQ_NOT_OK;
                    NvM_Admin[idx].ramChanged = TRUE;
                    NvM_WriteAllFailedBytes += NvM_BlockDescriptor[idx].nvBlockLength;
                    NvM_WriteAllBlocksFailed++;
                    NvM_WriteAllBlockResult[idx] = 3u;
                }
                NvM_State.currentIndex++;
                NvM_State.state = NVM_STATE_WRITEALL_NEXT;
            }
            break;

        case NVM_STATE_INVALIDATE_START:
            idx = NvM_State.activeIndex;
            if (MemIf_InvalidateBlock(MEMIF_FEE_DEVICE_INDEX, NvM_BlockDescriptor[idx].blockId) != E_OK)
            {
                NvM_Admin[idx].result = NVM_REQ_NOT_OK;
                NvM_SetIdle();
            }
            else
            {
                NvM_State.state = NVM_STATE_INVALIDATE_WAIT;
            }
            break;

        case NVM_STATE_INVALIDATE_WAIT:
            if (MemIf_GetStatus(MEMIF_FEE_DEVICE_INDEX) == MEMIF_IDLE)
            {
                idx = NvM_State.activeIndex;
                if (MemIf_GetJobResult(MEMIF_FEE_DEVICE_INDEX) == MEMIF_JOB_OK)
                {
                    NvM_Admin[idx].result = NVM_REQ_NV_INVALIDATED;
                    NvM_Admin[idx].ramValid = FALSE;
                    NvM_Admin[idx].ramChanged = FALSE;
                }
                else
                {
                    NvM_Admin[idx].result = NVM_REQ_NOT_OK;
                }
                NvM_SetIdle();
            }
            break;

        default:
            NvM_Report(NVM_API_MAIN, NVM_E_LOWER_LAYER, (uint32)NvM_State.state);
            NvM_SetIdle();
            break;
    }

    NvM_MainFunction_Counter++;
}
