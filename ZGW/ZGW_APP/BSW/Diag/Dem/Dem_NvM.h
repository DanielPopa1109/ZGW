#ifndef DEM_NVM_H_
#define DEM_NVM_H_

#include "Dem_Types.h"

typedef enum
{
    DEM_NVM_REQ_IDLE = 0,
    DEM_NVM_REQ_PENDING,
    DEM_NVM_REQ_OK,
    DEM_NVM_REQ_FAILED
} Dem_NvMRequestResultType;

Std_ReturnType Dem_NvM_StartRead(uint16 blockId, void *dstPtr);
Std_ReturnType Dem_NvM_StartWrite(uint16 blockId, const void *srcPtr);
Dem_NvMRequestResultType Dem_NvM_GetResult(uint16 blockId);
Std_ReturnType Dem_NvM_SetRamBlockStatus(uint16 blockId, boolean changed);

#endif
