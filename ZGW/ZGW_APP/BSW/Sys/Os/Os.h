#ifndef OS_H
#define OS_H

#include "Ifx_Types.h"

#define OS_CPU_LOAD_CORE_COUNT          3u
#define OS_CPU_LOAD_INVALID_PERCENT     0xFFu
#define OS_CPU_LOAD_INVALID_PERMILLE    0xFFFFu

typedef struct
{
    uint8 valid;
    uint8 percent;
    uint16 permille;
    uint32 runtimeDelta;
    uint32 idleRuntimeDelta;
} Os_CpuLoadType;

extern void Os_Init_C0(void);
extern void Os_Init_C1(void);
extern void Os_Init_C2(void);

extern volatile long long OS_Counter_core0;
extern volatile long long OS_Counter_core1;
extern volatile long long OS_Counter_core2;
extern volatile long long IDLE_Counter_core0;
extern volatile long long IDLE_Counter_core1;
extern volatile long long IDLE_Counter_core2;

extern volatile uint8 Os_CpuLoadPercent_core0;
extern volatile uint8 Os_CpuLoadPercent_core1;
extern volatile uint8 Os_CpuLoadPercent_core2;

extern volatile uint16 Os_CpuLoadPermille_core0;
extern volatile uint16 Os_CpuLoadPermille_core1;
extern volatile uint16 Os_CpuLoadPermille_core2;
extern volatile uint8 Os_EthStackInitialized;
extern volatile uint8 Os_EthNetifReadyBeforeStackInit;
extern volatile uint32 Os_EthNetifWaitLoops;
extern volatile uint32 Os_Core2AsilApplStackHighWater;
extern volatile uint32 Os_Core2QmBswStackHighWater;

uint8 Os_GetCpuLoadPercent(uint8 CoreId);
uint16 Os_GetCpuLoadPermille(uint8 CoreId);
void Os_GetCpuLoadSnapshot(uint8 CoreId, Os_CpuLoadType *CpuLoad);

#endif /* OS_H */
