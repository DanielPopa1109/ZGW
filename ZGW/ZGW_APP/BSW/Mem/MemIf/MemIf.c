#include "MemIf.h"
#include "Fee.h"
#include "MemStack_Error.h"

#define MEMIF_API_SET_MODE              (0x01u)
#define MEMIF_API_READ                  (0x02u)
#define MEMIF_API_WRITE                 (0x03u)
#define MEMIF_API_CANCEL                (0x04u)
#define MEMIF_API_GET_STATUS            (0x05u)
#define MEMIF_API_GET_JOB_RESULT        (0x06u)
#define MEMIF_API_INVALIDATE            (0x07u)
#define MEMIF_API_ERASE_IMMEDIATE       (0x08u)
#define MEMIF_E_PARAM_DEVICE            (0x01u)

static boolean MemIf_IsDeviceValid(uint8 DeviceIndex)
{
    return (boolean)((DeviceIndex == MEMIF_FEE_DEVICE_INDEX) || (DeviceIndex == MEMIF_BROADCAST_ID));
}

void MemIf_SetMode(MemIf_ModeType Mode)
{
    Fee_SetMode(Mode);
}

Std_ReturnType MemIf_Read(uint8 DeviceIndex, uint16 BlockNumber, uint16 BlockOffset, uint8 *DataBufferPtr, uint16 Length)
{
    if (DeviceIndex != MEMIF_FEE_DEVICE_INDEX)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_MEMIF, MEMIF_API_READ, MEMIF_E_PARAM_DEVICE, DeviceIndex);
        return E_NOT_OK;
    }
    return Fee_Read(BlockNumber, BlockOffset, DataBufferPtr, Length);
}

Std_ReturnType MemIf_Write(uint8 DeviceIndex, uint16 BlockNumber, const uint8 *DataBufferPtr)
{
    if (DeviceIndex != MEMIF_FEE_DEVICE_INDEX)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_MEMIF, MEMIF_API_WRITE, MEMIF_E_PARAM_DEVICE, DeviceIndex);
        return E_NOT_OK;
    }
    return Fee_Write(BlockNumber, DataBufferPtr);
}

Std_ReturnType MemIf_InvalidateBlock(uint8 DeviceIndex, uint16 BlockNumber)
{
    if (DeviceIndex != MEMIF_FEE_DEVICE_INDEX)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_MEMIF, MEMIF_API_INVALIDATE, MEMIF_E_PARAM_DEVICE, DeviceIndex);
        return E_NOT_OK;
    }
    return Fee_InvalidateBlock(BlockNumber);
}

Std_ReturnType MemIf_EraseImmediateBlock(uint8 DeviceIndex, uint16 BlockNumber)
{
    if (DeviceIndex != MEMIF_FEE_DEVICE_INDEX)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_MEMIF, MEMIF_API_ERASE_IMMEDIATE, MEMIF_E_PARAM_DEVICE, DeviceIndex);
        return E_NOT_OK;
    }
    return Fee_EraseImmediateBlock(BlockNumber);
}

void MemIf_Cancel(uint8 DeviceIndex)
{
    if (MemIf_IsDeviceValid(DeviceIndex) == FALSE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_MEMIF, MEMIF_API_CANCEL, MEMIF_E_PARAM_DEVICE, DeviceIndex);
        return;
    }
    Fee_Cancel();
}

MemIf_StatusType MemIf_GetStatus(uint8 DeviceIndex)
{
    if (MemIf_IsDeviceValid(DeviceIndex) == FALSE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_MEMIF, MEMIF_API_GET_STATUS, MEMIF_E_PARAM_DEVICE, DeviceIndex);
        return MEMIF_UNINIT;
    }
    return Fee_GetStatus();
}

MemIf_JobResultType MemIf_GetJobResult(uint8 DeviceIndex)
{
    if (MemIf_IsDeviceValid(DeviceIndex) == FALSE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_MEMIF, MEMIF_API_GET_JOB_RESULT, MEMIF_E_PARAM_DEVICE, DeviceIndex);
        return MEMIF_JOB_FAILED;
    }
    return Fee_GetJobResult();
}
