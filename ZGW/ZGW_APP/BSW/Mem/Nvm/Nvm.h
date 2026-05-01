#ifndef NVM_H
#define NVM_H

#include "Std_Types.h"
#include "MemIf.h"
#include "NvM_Cfg.h"
#include "MemStack_Error.h"

typedef enum
{
    NVM_REQ_OK = 0,
    NVM_REQ_NOT_OK,
    NVM_REQ_PENDING,
    NVM_REQ_INTEGRITY_FAILED,
    NVM_REQ_BLOCK_SKIPPED,
    NVM_REQ_NV_INVALIDATED,
    NVM_REQ_RESTORED_FROM_ROM
} NvM_RequestResultType;

typedef enum
{
    NVM_UNINIT = 0,
    NVM_IDLE,
    NVM_BUSY,
    NVM_BUSY_INTERNAL
} NvM_StatusType;

typedef struct
{
    const NvM_BlockDescriptorType *blockDescriptor;
    uint16 blockCount;
} NvM_ConfigType;

void NvM_Init(const NvM_ConfigType *ConfigPtr);
Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void *NvM_DstPtr);
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void *NvM_SrcPtr);
Std_ReturnType NvM_RestoreBlockDefaults(NvM_BlockIdType BlockId, void *NvM_DestPtr);
Std_ReturnType NvM_EraseNvBlock(NvM_BlockIdType BlockId);
Std_ReturnType NvM_InvalidateNvBlock(NvM_BlockIdType BlockId);
Std_ReturnType NvM_ReadAll(void);
Std_ReturnType NvM_WriteAll(void);
Std_ReturnType NvM_CancelJobs(void);
Std_ReturnType NvM_SetRamBlockStatus(NvM_BlockIdType BlockId, boolean BlockChanged);
Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType BlockId, NvM_RequestResultType *RequestResultPtr);
NvM_StatusType NvM_GetStatus(void);
void NvM_MainFunction(void);
void NvM_SetErrorCallback(MemStack_ErrorCallbackType callback);

#endif /* NVM_H */
