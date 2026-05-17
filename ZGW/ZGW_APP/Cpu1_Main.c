#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "Os.h"
#include "Wdg.h"
#include "task_core1.h"
#include "SafetyKit_InternalWatchdogs.h"
#include "McuSm.h"
#include "Cpu/Std/IfxCpu_Intrinsics.h"

extern volatile uint8 OsInit_C0;
volatile uint8 OsInit_C1 = 0u;
volatile uint32 Core1_WaitForCore0LoopCounter = 0u;
volatile uint32 Core1_WaitForCore0Timeout = 0u;

#define CORE1_WAIT_FOR_CORE0_LOOP_LIMIT 1000000u

void core1_main(void)
{
    uint32 waitLoops = 0u;

    initCpuWatchdog(1u);

    while(OsInit_C0 == 0u)
    {
        Core1_WaitForCore0LoopCounter++;
        waitLoops++;
        serviceCpuWatchdog();

        if (waitLoops >= CORE1_WAIT_FOR_CORE0_LOOP_LIMIT)
        {
            Core1_WaitForCore0Timeout = 1u;
            McuSm_PerformResetHook(383u, 1u);

            for(;;)
            {
                /* Wait for reset or debugger intervention. */
            }
        }
    }
    __dsync();

    Os_Init_C1();

    __dsync();
    OsInit_C1 = 1u;
    __dsync();

    serviceCpuWatchdog();

    vTaskStartScheduler_core1();
}
