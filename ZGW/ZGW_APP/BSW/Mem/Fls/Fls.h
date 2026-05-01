#ifndef FLS_H
#define FLS_H

#include "Std_Types.h"
#include "MemIf.h"
#include "Fls_Cfg.h"

typedef uint32 Fls_AddressType;
typedef uint32 Fls_LengthType;

typedef struct
{
    uint32 baseAddress;
    uint32 totalSize;
    uint32 sectorSize;
    uint32 pageSize;
} Fls_ConfigType;

void Fls_Init(const Fls_ConfigType *ConfigPtr);
Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length);
Std_ReturnType Fls_Write(Fls_AddressType TargetAddress, const uint8 *SourceAddressPtr, Fls_LengthType Length);
Std_ReturnType Fls_Read(Fls_AddressType SourceAddress, uint8 *TargetAddressPtr, Fls_LengthType Length);
void Fls_Cancel(void);
MemIf_StatusType Fls_GetStatus(void);
MemIf_JobResultType Fls_GetJobResult(void);
void Fls_SetMode(MemIf_ModeType Mode);
void Fls_MainFunction(void);
uint32 Fls_GetPhysicalAddress(Fls_AddressType Address);

#endif /* FLS_H */
