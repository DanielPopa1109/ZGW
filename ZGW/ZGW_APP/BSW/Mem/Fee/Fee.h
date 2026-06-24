#ifndef FEE_H
#define FEE_H

#include "Std_Types.h"
#include "MemIf.h"
#include "Fee_Cfg.h"

typedef struct
{
    const Fee_BlockConfigType *blockConfig;
    uint16 blockCount;
    uint32 virtualSectorSize;
} Fee_ConfigType;

extern volatile uint32 Fee_NextWriteDFlashAddress;
extern volatile uint32 Fee_DebugLastFlashPhysicalAddress;
extern volatile uint32 Fee_DebugLastFlashAccessPhysicalAddress;

void Fee_Init(const Fee_ConfigType *ConfigPtr);
void Fee_SetMode(MemIf_ModeType Mode);
Std_ReturnType Fee_Read(uint16 BlockNumber, uint16 BlockOffset, uint8 *DataBufferPtr, uint16 Length);
Std_ReturnType Fee_Write(uint16 BlockNumber, const uint8 *DataBufferPtr);
Std_ReturnType Fee_InvalidateBlock(uint16 BlockNumber);
Std_ReturnType Fee_EraseImmediateBlock(uint16 BlockNumber);
void Fee_Cancel(void);
MemIf_StatusType Fee_GetStatus(void);
MemIf_JobResultType Fee_GetJobResult(void);
boolean Fee_IsDeferredFormatPending(void);
Std_ReturnType Fee_StartDeferredFormat(void);
void Fee_MainFunction(void);

#endif /* FEE_H */
