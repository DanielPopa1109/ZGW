#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxCpu_reg.h"
#include "IfxScuWdt.h"
#include "Os.h"
#include "Wdg.h"
#include "task_core2.h"
#include "SafetyKit_InternalWatchdogs.h"
#include "McuSm.h"
#include "aurix_pin_mappings.h"
#include "SysMgr.h"
#include "EthStack.h"
#include "Cpu/Std/IfxCpu_Intrinsics.h"
#include "UdpNm.h"
#include "EthSM.h"
#include "BSW/Com/Ethernet/EthStartupTiming.h"
#include "BSW/Sys/CpuPerf/CpuPerf.h"

extern volatile uint8 OsInit_C1;
volatile uint8 OsInit_C2 = 0u;
volatile uint32 Core2_MainEnteredCounter = 0u;
volatile uint32 Core2_WaitForCore1LoopCounter = 0u;

#define CPU_COMPAT_SP_MASK               (1u << 4u)

static void Core2_EnableCpuSysconSafetyProtection(void)
{
    uint32 compat = (uint32)__mfcr(CPU_COMPAT);

    if((compat & CPU_COMPAT_SP_MASK) != 0u)
    {
        uint16 cpuWdtPassword = IfxScuWdt_getCpuWatchdogPassword();
        uint16 safetyWdtPassword = IfxScuWdt_getSafetyWatchdogPassword();

        /* TC37x Erratum CPU_TC.H023: clear COMPAT.SP to protect CPU_SYSCON[31:1]. */
        IfxScuWdt_clearCpuEndinit(cpuWdtPassword);
        IfxScuWdt_clearSafetyEndinit(safetyWdtPassword);
        compat = (uint32)__mfcr(CPU_COMPAT);
        __mtcr(CPU_COMPAT, compat & (uint32)~CPU_COMPAT_SP_MASK);
        __isync();
        IfxScuWdt_setSafetyEndinit(safetyWdtPassword);
        IfxScuWdt_setCpuEndinit(cpuWdtPassword);
    }
}

void core2_main(void)
{
    Core2_MainEnteredCounter++;

    initCpuWatchdog(2u);
    CpuPerf_InitCore();
    Core2_EnableCpuSysconSafetyProtection();

    while(OsInit_C1 == 0u)
    {
        Core2_WaitForCore1LoopCounter++;
    }
    __dsync();
    EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_RMII_INIT_ENTER);
    rmii0_init_pins();
    EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_RMII_INIT_COMPLETE);
    Os_Init_C2();
    __dsync();
    OsInit_C2 = 1u;
    __dsync();
    vTaskStartScheduler_core2();
}
