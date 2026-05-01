#ifndef MEMIF_H
#define MEMIF_H

#include "Std_Types.h"
#include "MemStack_Cfg.h"

typedef enum
{
    MEMIF_UNINIT = 0,
    MEMIF_IDLE,
    MEMIF_BUSY,
    MEMIF_BUSY_INTERNAL
} MemIf_StatusType;

typedef enum
{
    MEMIF_JOB_OK = 0,
    MEMIF_JOB_FAILED,
    MEMIF_JOB_PENDING,
    MEMIF_JOB_CANCELED,
    MEMIF_BLOCK_INCONSISTENT,
    MEMIF_BLOCK_INVALID
} MemIf_JobResultType;

typedef enum
{
    MEMIF_MODE_SLOW = 0,
    MEMIF_MODE_FAST
} MemIf_ModeType;

#define MEMIF_BROADCAST_ID                  (0xFFu)
#define MEMIF_FEE_DEVICE_INDEX              (0u)

void MemIf_SetMode(MemIf_ModeType Mode);
Std_ReturnType MemIf_Read(uint8 DeviceIndex, uint16 BlockNumber, uint16 BlockOffset, uint8 *DataBufferPtr, uint16 Length);
Std_ReturnType MemIf_Write(uint8 DeviceIndex, uint16 BlockNumber, const uint8 *DataBufferPtr);
Std_ReturnType MemIf_InvalidateBlock(uint8 DeviceIndex, uint16 BlockNumber);
Std_ReturnType MemIf_EraseImmediateBlock(uint8 DeviceIndex, uint16 BlockNumber);
void MemIf_Cancel(uint8 DeviceIndex);
MemIf_StatusType MemIf_GetStatus(uint8 DeviceIndex);
MemIf_JobResultType MemIf_GetJobResult(uint8 DeviceIndex);

#endif /* MEMIF_H */
