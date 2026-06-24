#include "Dem_NvM.h"
#include "Dem_Cfg.h"
#include "NvM.h"
#include <string.h>

static const NvM_BlockDescriptorType *Dem_NvM_FindBlock(uint16 blockId)
{
    uint16 i;

    for (i = 0u; i < NVM_TOTAL_BLOCKS; i++)
    {
        if (NvM_BlockDescriptor[i].blockId == blockId)
        {
            return &NvM_BlockDescriptor[i];
        }
    }

    return NULL_PTR;
}

static Std_ReturnType Dem_NvM_CopyToRamBlock(uint16 blockId, const void *srcPtr)
{
    const NvM_BlockDescriptorType *block;

    if (srcPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    block = Dem_NvM_FindBlock(blockId);
    if ((block == NULL_PTR) || (block->ramBlockDataAddress == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (block->ramBlockDataAddress == srcPtr)
    {
        return E_OK;
    }

    memcpy(block->ramBlockDataAddress, srcPtr, block->nvBlockLength);
    return E_OK;
}

Std_ReturnType Dem_NvM_StartRead(uint16 blockId, void *dstPtr)
{
    if (dstPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    return NvM_ReadBlock(blockId, dstPtr);
}

Std_ReturnType Dem_NvM_StartWrite(uint16 blockId, const void *srcPtr)
{
    if (Dem_NvM_CopyToRamBlock(blockId, srcPtr) != E_OK)
    {
        return E_NOT_OK;
    }

    return NvM_WriteBlock(blockId, NULL_PTR);
}

boolean Dem_NvM_IsIdle(void)
{
    return (NvM_GetStatus() == NVM_IDLE) ? TRUE : FALSE;
}

Dem_NvMRequestResultType Dem_NvM_GetResult(uint16 blockId)
{
    NvM_RequestResultType result;

    if (NvM_GetErrorStatus(blockId, &result) != E_OK)
    {
        return DEM_NVM_REQ_FAILED;
    }

    if (result == NVM_REQ_OK)
    {
        return DEM_NVM_REQ_OK;
    }

    if (result == NVM_REQ_PENDING)
    {
        return DEM_NVM_REQ_PENDING;
    }

    if (result == NVM_REQ_RESTORED_FROM_ROM)
    {
        return DEM_NVM_REQ_OK;
    }

    return DEM_NVM_REQ_FAILED;
}

Std_ReturnType Dem_NvM_UpdateRamBlock(uint16 blockId, const void *srcPtr)
{
    const NvM_BlockDescriptorType *block;

    if (srcPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    block = Dem_NvM_FindBlock(blockId);
    if ((block == NULL_PTR) || (block->ramBlockDataAddress == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (block->ramBlockDataAddress == srcPtr)
    {
        return NvM_SetRamBlockStatus(blockId, TRUE);
    }

    if (memcmp(block->ramBlockDataAddress, srcPtr, block->nvBlockLength) == 0)
    {
        return E_OK;
    }

    memcpy(block->ramBlockDataAddress, srcPtr, block->nvBlockLength);
    return NvM_SetRamBlockStatus(blockId, TRUE);
}

Std_ReturnType Dem_NvM_UpdateRamBlockForced(uint16 blockId, const void *srcPtr)
{
    if (Dem_NvM_CopyToRamBlock(blockId, srcPtr) != E_OK)
    {
        return E_NOT_OK;
    }

    return NvM_ForceRamBlockChanged(blockId);
}

Std_ReturnType Dem_NvM_SetRamBlockStatus(uint16 blockId, boolean changed)
{
    return NvM_SetRamBlockStatus(blockId, changed);
}

