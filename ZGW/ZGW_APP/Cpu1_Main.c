#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "Os.h"
#include "Wdg.h"
#include "task_core1.h"
#include "SafetyKit_InternalWatchdogs.h"
#include "McuSm.h"
#include "IfxCpu_Intrinsics.h"

extern volatile uint8 OsInit_C0;
AURIX_SHARED_NC volatile uint8 OsInit_C1;
volatile uint32 Core1_MainEnteredCounter = 0u;
volatile uint32 Core1_WaitForCore0LoopCounter = 0u;

void core1_main(void)
{
    Core1_MainEnteredCounter++;

    initCpuWatchdog(1u);
    while(OsInit_C0 == 0u)
    {
        Core1_WaitForCore0LoopCounter++;
        serviceCpuWatchdog();
    }
    __dsync();
    Os_Init_C1();
    __dsync();
    OsInit_C1 = 1u;
    Ifx__dsync();
    vTaskStartScheduler_core1();
}
