#include "Dem_NvM.h"
#include "Dem_Cfg.h"

#if (DEM_NVM_ENABLED == 1u)

#if (DEM_NVM_INCLUDE_NVM_H == 1u)
#include "NvM.h"
#else
typedef uint8 NvM_RequestResultType;

#ifndef NVM_REQ_OK
#define NVM_REQ_OK                 0u
#endif

#ifndef NVM_REQ_PENDING
#define NVM_REQ_PENDING            1u
#endif

#ifndef NVM_REQ_NOT_OK
#define NVM_REQ_NOT_OK             2u
#endif

#ifndef NVM_REQ_INTEGRITY_FAILED
#define NVM_REQ_INTEGRITY_FAILED   3u
#endif

#ifndef NVM_REQ_BLOCK_SKIPPED
#define NVM_REQ_BLOCK_SKIPPED      4u
#endif

#ifndef NVM_REQ_NV_INVALIDATED
#define NVM_REQ_NV_INVALIDATED     5u
#endif

#ifndef NVM_REQ_RESTORED_FROM_ROM
#define NVM_REQ_RESTORED_FROM_ROM  6u
#endif

extern Std_ReturnType NvM_ReadBlock(uint16 BlockId, void *NvM_DstPtr);
extern Std_ReturnType NvM_WriteBlock(uint16 BlockId, const void *NvM_SrcPtr);
extern Std_ReturnType NvM_GetErrorStatus(uint16 BlockId, NvM_RequestResultType *RequestResultPtr);
extern Std_ReturnType NvM_SetRamBlockStatus(uint16 BlockId, boolean BlockChanged);
#endif

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
    if (srcPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    (void)NvM_SetRamBlockStatus(blockId, TRUE);
    return NvM_WriteBlock(blockId, srcPtr);
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

Std_ReturnType Dem_NvM_SetRamBlockStatus(uint16 blockId, boolean changed)
{
    return NvM_SetRamBlockStatus(blockId, changed);
}

#else

Std_ReturnType Dem_NvM_StartRead(uint16 blockId, void *dstPtr)
{
    (void)blockId;
    (void)dstPtr;
    return E_NOT_OK;
}

Std_ReturnType Dem_NvM_StartWrite(uint16 blockId, const void *srcPtr)
{
    (void)blockId;
    (void)srcPtr;
    return E_NOT_OK;
}

Dem_NvMRequestResultType Dem_NvM_GetResult(uint16 blockId)
{
    (void)blockId;
    return DEM_NVM_REQ_FAILED;
}

Std_ReturnType Dem_NvM_SetRamBlockStatus(uint16 blockId, boolean changed)
{
    (void)blockId;
    (void)changed;
    return E_NOT_OK;
}

#endif
