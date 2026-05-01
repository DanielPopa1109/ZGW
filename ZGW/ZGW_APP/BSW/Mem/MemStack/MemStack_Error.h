#ifndef MEMSTACK_ERROR_H
#define MEMSTACK_ERROR_H

#include "Std_Types.h"
#include "MemStack_Cfg.h"

typedef void (*MemStack_ErrorCallbackType)(uint16 moduleId, uint8 apiId, uint8 errorId, uint32 detail);

void MemStack_SetErrorCallback(MemStack_ErrorCallbackType callback);
void MemStack_ReportError(uint16 moduleId, uint8 apiId, uint8 errorId, uint32 detail);

#endif /* MEMSTACK_ERROR_H */
