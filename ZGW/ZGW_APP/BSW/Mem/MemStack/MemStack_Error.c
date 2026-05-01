#include "MemStack_Error.h"

static MemStack_ErrorCallbackType MemStack_ErrorCallback = NULL_PTR;

void MemStack_SetErrorCallback(MemStack_ErrorCallbackType callback)
{
    MemStack_ErrorCallback = callback;
}

void MemStack_ReportError(uint16 moduleId, uint8 apiId, uint8 errorId, uint32 detail)
{
#if (MEMSTACK_ERROR_CALLBACK_ENABLED == STD_ON)
    if (MemStack_ErrorCallback != NULL_PTR)
    {
        MemStack_ErrorCallback(moduleId, apiId, errorId, detail);
    }
#else
    (void)moduleId;
    (void)apiId;
    (void)errorId;
    (void)detail;
#endif
}
