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
#include "UdpNm.h"
#include "EthSM.h"

extern volatile uint8 OsInit_C1;
volatile uint8 OsInit_C2 = 0u;
volatile uint32 Core2_MainEnteredCounter = 0u;
volatile uint32 Core2_WaitForCore1LoopCounter = 0u;
volatile uint32 Core2_WaitForCore1Timeout = 0u;

#define CORE2_WAIT_FOR_CORE1_LOOP_LIMIT 1000000u

void core2_main(void)
{
    Core2_MainEnteredCounter++;

    initCpuWatchdog(2u);

    while(OsInit_C1 == 0u)
    {
        Core2_WaitForCore1LoopCounter++;
        serviceCpuWatchdog();
    }
    __dsync();
    rmii0_init_pins();
    Os_Init_C2();
    __dsync();
    OsInit_C2 = 1u;
    __dsync();
    vTaskStartScheduler_core2();
}
