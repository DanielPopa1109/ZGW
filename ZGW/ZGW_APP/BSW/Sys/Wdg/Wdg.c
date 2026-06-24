#include "Wdg.h"
#include "Wdg_Cfg.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "Bsp.h"

void Wdg_DeInitializeSafetyWatchdog(void);
void Wdg_DeInitializeCpu0Watchdog(void);
void Wdg_DeInitializeCpu1Watchdog(void);
void Wdg_DeInitializeCpu2Watchdog(void);

void Wdg_DeInitializeSafetyWatchdog(void)
{
    uint16 safetyWdtPw = IfxScuWdt_getSafetyWatchdogPassword();
    IfxScuWdt_changeSafetyWatchdogReload(safetyWdtPw, WDG_RELOAD_VALUE);
    IfxScuWdt_enableSafetyWatchdog(safetyWdtPw);
}

void Wdg_DeInitializeCpu0Watchdog(void)
{
    uint16 passwd;
    passwd = IfxScuWdt_getCpuWatchdogPassword();
    IfxScuWdt_changeCpuWatchdogReload(passwd, WDG_RELOAD_VALUE);
    IfxScuWdt_enableCpuWatchdog(passwd);
}

void Wdg_DeInitializeCpu1Watchdog(void)
{
    uint16 passwd;
    passwd = IfxScuWdt_getCpuWatchdogPassword();
    IfxScuWdt_changeCpuWatchdogReload(passwd, WDG_RELOAD_VALUE);
    IfxScuWdt_enableCpuWatchdog(passwd);
}

void Wdg_DeInitializeCpu2Watchdog(void)
{
    uint16 passwd;
    passwd = IfxScuWdt_getCpuWatchdogPassword();
    IfxScuWdt_changeCpuWatchdogReload(passwd, WDG_RELOAD_VALUE);
    IfxScuWdt_enableCpuWatchdog(passwd);
}
