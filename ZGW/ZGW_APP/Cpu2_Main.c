#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "Os.h"
#include "Wdg.h"
#include "task_core2.h"
#include "SafetyKit_InternalWatchdogs.h"
#include "McuSm.h"
#include "aurix_pin_mappings.h"
#include "SysMgr.h"
#include "GatewaySwc.h"
#include "EthStack.h"
#include "Cpu/Std/IfxCpu_Intrinsics.h"

extern volatile uint8 OsInit_C1;
volatile uint8 OsInit_C2 = 0u;
volatile uint32 Core2_WaitForCore1LoopCounter = 0u;
volatile uint32 Core2_WaitForCore1Timeout = 0u;

#define CORE2_WAIT_FOR_CORE1_LOOP_LIMIT 1000000u

void core2_main(void)
{
    uint32 waitLoops = 0u;

    initCpuWatchdog(2u);

    while(OsInit_C1 == 0u)
    {
        Core2_WaitForCore1LoopCounter++;
        waitLoops++;
        serviceCpuWatchdog();

        if (waitLoops >= CORE2_WAIT_FOR_CORE1_LOOP_LIMIT)
        {
            Core2_WaitForCore1Timeout = 1u;
            McuSm_PerformResetHook(383u, 2u);

            for(;;)
            {
                /* Wait for reset or debugger intervention. */
            }
        }
    }
    __dsync();

    rmii0_init_pins();

//    IfxPort_setPinLow(&MODULE_P21, 3u);
//    for (volatile uint32 i = 0u; i < 50000u; i++)
//    {
//        __nop();
//    }
//
//    IfxPort_setPinHigh(&MODULE_P21, 3u);
//    for (volatile uint32 i = 0u; i < 500000u; i++)
//    {
//        __nop();
//    }

    Os_Init_C2();
    
    __dsync();
    OsInit_C2 = 1u;
    __dsync();

    serviceCpuWatchdog();

    SysMgr_EcuState = SYSMGR_STARTUP;

    vTaskStartScheduler_core2();
}
