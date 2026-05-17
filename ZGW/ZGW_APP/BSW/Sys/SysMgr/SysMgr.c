#include "Nvm.h"
#include "SysMgr.h"
#include "McuSm.h"
#include "IfxCpu.h"
#include "Can.h"
#include "CanSM.h"
#include "LinSM.h"
#include "Crc.h"
#include "task_core0.h"
#include "FreeRTOSConfig_core0.h"
#include "Wdg.h"
#include "IfxPmsPm.h"
#include "IfxStm.h"
#include "IfxPort.h"
#include "IfxPort_reg.h"
#include "Dem.h"
#include "SafetyKit_InternalWatchdogs.h"
#include "SafetyKit_Main.h"
#include "IfxAsclin_Lin.h"
#include "IfxGeth.h"
#include "Fee.h"
#include "Fls.h"
#include "SCR.h"

#define SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS 200u
#define SYSMGR_KEEP_AWAKE_WHILE_FULL_COM  1u
#define SYSMGR_NVM_IDLE_WAIT_LOOP_LIMIT   1000000u
#define SYSMGR_FAIL_NVM_PRE_WRITEALL_IDLE 1u
#define SYSMGR_FAIL_NVM_POST_WRITEALL_IDLE 2u
#define SYSMGR_FAIL_DEM_CYCLE_END         3u
#define SYSMGR_FAIL_DEM_SHUTDOWN          4u
#define SYSMGR_FAIL_NVM_WRITEALL_REQ      5u

uint32 SysMgr_MainCounter = 0u;
uint32 SysMgr_RunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
uint32 SysMgr_PostRunCounter = 1u;
volatile uint32 SysMgr_BusActivityCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
volatile uint32 SysMgr_GoSleepCounter = 0u;
volatile uint32 SysMgr_NvMPreWriteAllWaitCounter = 0u;
volatile uint32 SysMgr_NvMPostWriteAllWaitCounter = 0u;
volatile uint32 SysMgr_NvMTimeoutCounter = 0u;
volatile uint32 SysMgr_PcsrLockWaitCounter = 0u;
volatile SysMgr_EcuState_t SysMgr_EcuState = SYSMGR_INIT;
volatile uint8 SysMgr_NoBusActivity = 0u;
float SysMgr_McuTemperature = 0u;

void SysMgr_ProcessResetDtc(void);
void SysMgr_EcuStateMachine(void);
void SysMgr_MainFunction(void);
void SysMgr_GoSleep(void);

static boolean SysMgr_IsFullComActive(void);
static void SysMgr_KeepRunState(void);
static void SysMgr_GoSleepFailure(uint32 FailureInformation);

void SysMgr_NotifyBusActivity(void)
{
    SysMgr_BusActivityCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
    SysMgr_NoBusActivity = 0u;
}

static boolean SysMgr_IsFullComActive(void)
{
#if (SYSMGR_KEEP_AWAKE_WHILE_FULL_COM == 1u)
    CanSM_ComModeType canMode;

    if ((CanSM_GetCurrentComMode(CAN_CONTROLLER_CLASSIC, &canMode) == E_OK) &&
            (canMode == CANSM_COMM_FULL_COMMUNICATION))
    {
        return TRUE;
    }

    if ((CanSM_GetCurrentComMode(CAN_CONTROLLER_FD, &canMode) == E_OK) &&
            (canMode == CANSM_COMM_FULL_COMMUNICATION))
    {
        return TRUE;
    }

    if (LinSM_GetState(0u) == LINSM_FULL_COMMUNICATION)
    {
        return TRUE;
    }
#endif

    return FALSE;
}

static void SysMgr_KeepRunState(void)
{
    SysMgr_RunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
    SysMgr_PostRunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;

    if ((SysMgr_EcuState == SYSMGR_POSTRUN) ||
            (SysMgr_EcuState == SYSMGR_SLEEP))
    {
        SysMgr_EcuState = SYSMGR_RUN;
    }
}

void SysMgr_GoSleep(void)
{
    uint16 cpuWdtPw;
    uint16 safetyWdtPw;
    Ifx_PMS_PMSWCR0 pmswcr0;
    uint32 waitLoops;

    SysMgr_GoSleepCounter++;

    (void)IfxCpu_disableInterrupts();

    if (Dem_SetOperationCycleState(DEM_DEFAULT_OPERATION_CYCLE,
            DEM_CYCLE_STATE_END) != E_OK)
    {
        SysMgr_GoSleepFailure(SYSMGR_FAIL_DEM_CYCLE_END);
    }

    if (Dem_Shutdown() != E_OK)
    {
        SysMgr_GoSleepFailure(SYSMGR_FAIL_DEM_SHUTDOWN);
    }

    waitLoops = 0u;
    while (NvM_GetStatus() != NVM_IDLE)
    {
        SysMgr_NvMPreWriteAllWaitCounter++;
        waitLoops++;
        Fls_MainFunction();
        Fee_MainFunction();
        NvM_MainFunction();
        Dem_MainFunction();
        serviceCpuWatchdog();
        serviceSafetyWatchdog();

        if (waitLoops >= SYSMGR_NVM_IDLE_WAIT_LOOP_LIMIT)
        {
            SysMgr_GoSleepFailure(SYSMGR_FAIL_NVM_PRE_WRITEALL_IDLE);
        }
    }

    if (NvM_WriteAll() != E_OK)
    {
        SysMgr_GoSleepFailure(SYSMGR_FAIL_NVM_WRITEALL_REQ);
    }

    waitLoops = 0u;
    while (NvM_GetStatus() != NVM_IDLE)
    {
        SysMgr_NvMPostWriteAllWaitCounter++;
        waitLoops++;
        Fls_MainFunction();
        Fee_MainFunction();
        NvM_MainFunction();
        serviceCpuWatchdog();
        serviceSafetyWatchdog();

        if (waitLoops >= SYSMGR_NVM_IDLE_WAIT_LOOP_LIMIT)
        {
            SysMgr_GoSleepFailure(SYSMGR_FAIL_NVM_POST_WRITEALL_IDLE);
        }
    }

    IfxAsclin_disableModule((Ifx_ASCLIN *)(void *)&MODULE_ASCLIN1);
    //IfxCan_disableModule(&MODULE_CAN0);
    //IfxCan_disableModule(&MODULE_CAN1);
    IfxGeth_disableModule(&MODULE_GETH);
    vTaskSuspendAll_core0();
    vTaskEndScheduler_core0();
    IfxStm_disableModule(&MODULE_STM0);
    SRC_STM0SR0.B.SRE = 0u;
    SRC_STM0SR1.B.SRE = 0u;
    SRC_STM1SR0.B.SRE = 0u;
    SRC_STM1SR1.B.SRE = 0u;
    SRC_STM2SR0.B.SRE = 0u;
    SRC_STM2SR1.B.SRE = 0u;
    SRC_BCUSPB.B.SRE = 0u;
    SRC_MTUDONE.B.SRE = 0U;
    SRC_PMSDTS.B.SRE = 0U;
    SRC_CAN0INT0.B.SRE = 0u;
    SRC_CAN0INT1.B.SRE = 0u;
    SRC_CAN0INT2.B.SRE = 0u;
    SRC_CAN0INT3.B.SRE = 0u;
    SRC_CAN0INT4.B.SRE = 0u;
    SRC_CAN0INT5.B.SRE = 0u;
    SRC_CAN0INT6.B.SRE = 0u;
    SRC_CAN0INT7.B.SRE = 0u;
    SRC_CAN0INT8.B.SRE = 0u;
    SRC_CAN0INT9.B.SRE = 0u;
    SRC_CAN0INT10.B.SRE = 0u;
    SRC_CAN0INT11.B.SRE = 0u;
    SRC_CAN0INT12.B.SRE = 0u;
    SRC_CAN0INT13.B.SRE = 0u;
    SRC_CAN0INT14.B.SRE = 0u;
    SRC_CAN0INT15.B.SRE = 0u;
    SRC_CAN1INT0.B.SRE = 0u;
    SRC_CAN1INT1.B.SRE = 0u;
    SRC_CAN1INT2.B.SRE = 0u;
    SRC_CAN1INT3.B.SRE = 0u;
    SRC_CAN1INT4.B.SRE = 0u;
    SRC_CAN1INT5.B.SRE = 0u;
    SRC_CAN1INT6.B.SRE = 0u;
    SRC_CAN1INT7.B.SRE = 0u;
    SRC_CAN1INT8.B.SRE = 0u;
    SRC_CAN1INT9.B.SRE = 0u;
    SRC_CAN1INT10.B.SRE = 0u;
    SRC_CAN1INT11.B.SRE = 0u;
    SRC_CAN1INT12.B.SRE = 0u;
    SRC_CAN1INT13.B.SRE = 0u;
    SRC_CAN1INT14.B.SRE = 0u;
    SRC_CAN1INT15.B.SRE = 0u;
    SRC_STM0SR0.B.CLRR = 1U;
    SRC_STM0SR1.B.CLRR = 1U;
    SRC_STM1SR0.B.CLRR = 1U;
    SRC_STM1SR1.B.CLRR = 1U;
    SRC_STM2SR0.B.CLRR = 1U;
    SRC_STM2SR1.B.CLRR = 1U;
    SRC_BCUSPB.B.CLRR = 1U;
    SRC_MTUDONE.B.CLRR = 1U;
    SRC_PMSDTS.B.CLRR = 1U;
    SRC_CAN0INT0.B.CLRR = 1;
    SRC_CAN0INT1.B.CLRR = 1;
    SRC_CAN0INT2.B.CLRR = 1;
    SRC_CAN0INT3.B.CLRR = 1;
    SRC_CAN0INT4.B.CLRR = 1;
    SRC_CAN0INT5.B.CLRR = 1;
    SRC_CAN0INT6.B.CLRR = 1;
    SRC_CAN0INT7.B.CLRR = 1;
    SRC_CAN0INT8.B.CLRR = 1;
    SRC_CAN0INT9.B.CLRR = 1;
    SRC_CAN0INT10.B.CLRR = 1;
    SRC_CAN0INT11.B.CLRR = 1;
    SRC_CAN0INT12.B.CLRR = 1;
    SRC_CAN0INT13.B.CLRR = 1;
    SRC_CAN0INT14.B.CLRR = 1;
    SRC_CAN0INT15.B.CLRR = 1;
    SRC_CAN1INT0.B.CLRR = 1;
    SRC_CAN1INT1.B.CLRR = 1;
    SRC_CAN1INT2.B.CLRR = 1;
    SRC_CAN1INT3.B.CLRR = 1;
    SRC_CAN1INT4.B.CLRR = 1;
    SRC_CAN1INT5.B.CLRR = 1;
    SRC_CAN1INT6.B.CLRR = 1;
    SRC_CAN1INT7.B.CLRR = 1;
    SRC_CAN1INT8.B.CLRR = 1;
    SRC_CAN1INT9.B.CLRR = 1;
    SRC_CAN1INT10.B.CLRR = 1;
    SRC_CAN1INT11.B.CLRR = 1;
    SRC_CAN1INT12.B.CLRR = 1;
    SRC_CAN1INT13.B.CLRR = 1;
    SRC_CAN1INT14.B.CLRR = 1;
    SRC_CAN1INT15.B.CLRR = 1;
    SRC_STM0SR0.B.IOVCLR = 1;
    SRC_STM0SR1.B.IOVCLR = 1;
    SRC_STM1SR0.B.IOVCLR = 1;
    SRC_STM1SR1.B.IOVCLR = 1;
    SRC_STM2SR0.B.IOVCLR = 1;
    SRC_STM2SR1.B.IOVCLR = 1;
    SRC_BCUSPB.B.IOVCLR = 1;
    SRC_MTUDONE.B.IOVCLR = 1;
    SRC_PMSDTS.B.IOVCLR = 1;
    SRC_CAN0INT0.B.IOVCLR = 1;
    SRC_CAN0INT1.B.IOVCLR = 1;
    SRC_CAN0INT2.B.IOVCLR = 1;
    SRC_CAN0INT3.B.IOVCLR = 1;
    SRC_CAN0INT4.B.IOVCLR = 1;
    SRC_CAN0INT5.B.IOVCLR = 1;
    SRC_CAN0INT6.B.IOVCLR = 1;
    SRC_CAN0INT7.B.IOVCLR = 1;
    SRC_CAN0INT8.B.IOVCLR = 1;
    SRC_CAN0INT9.B.IOVCLR = 1;
    SRC_CAN0INT10.B.IOVCLR = 1;
    SRC_CAN0INT11.B.IOVCLR = 1;
    SRC_CAN0INT12.B.IOVCLR = 1;
    SRC_CAN0INT13.B.IOVCLR = 1;
    SRC_CAN0INT14.B.IOVCLR = 1;
    SRC_CAN0INT15.B.IOVCLR = 1;
    SRC_CAN1INT0.B.IOVCLR = 1;
    SRC_CAN1INT1.B.IOVCLR = 1;
    SRC_CAN1INT2.B.IOVCLR = 1;
    SRC_CAN1INT3.B.IOVCLR = 1;
    SRC_CAN1INT4.B.IOVCLR = 1;
    SRC_CAN1INT5.B.IOVCLR = 1;
    SRC_CAN1INT6.B.IOVCLR = 1;
    SRC_CAN1INT7.B.IOVCLR = 1;
    SRC_CAN1INT8.B.IOVCLR = 1;
    SRC_CAN1INT9.B.IOVCLR = 1;
    SRC_CAN1INT10.B.IOVCLR = 1;
    SRC_CAN1INT11.B.IOVCLR = 1;
    SRC_CAN1INT12.B.IOVCLR = 1;
    SRC_CAN1INT13.B.IOVCLR = 1;
    SRC_CAN1INT14.B.IOVCLR = 1;
    SRC_CAN1INT15.B.IOVCLR = 1;
    IfxCpu_setAllIdleExceptMasterCpu(IfxCpu_getCoreIndex());

    /* Give SCR ownership of CANFD RX before starting WCAN wake detection. */
    IfxScuWdt_clearSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());
    while(P33_PCSR.B.LCK)
    {
        SysMgr_PcsrLockWaitCounter++;
    }
    P33_PCSR.B.SEL5 = 1;
    IfxScuWdt_setSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());

    IfxScuWdt_clearSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());
    IfxScr_enableSCR();
    IfxScr_copyProgram();
    IfxScr_init(1);
    IfxScuWdt_setSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());
    if(SCU_RSTSTAT.B.STBYR)
    {
        IfxScuWdt_clearCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());
        SCU_RSTCON2.B.CLRC = 1;    /* A write access to this register is Endinit protected;
                                          the protection has to be removed and set again afterward. */
        IfxScuWdt_setCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());
    }

    while(SCU_RSTSTAT.B.STBYR)    /* Wait until Reset status is finally reset */
    {
    }

    __dsync();
    cpuWdtPw = IfxScuWdt_getCpuWatchdogPassword();
    safetyWdtPw = IfxScuWdt_getSafetyWatchdogPassword();
    IfxScuWdt_clearSafetyEndinit(safetyWdtPw);
    IfxScuWdt_clearCpuEndinit(cpuWdtPw);

    PMS_PMSWSTATCLR.U = 0xFFFFFFFFu;
    pmswcr0.U = PMS_PMSWCR0.U;
    pmswcr0.B.STBYRAMSEL = 7u;
    pmswcr0.B.SCRWKEN = 1u;
    pmswcr0.B.PORSTWKEN = 1u;
    PMS_PMSWCR0.U = pmswcr0.U;
    SCU_PMSWCR1.B.IRADIS = 1u;

    __dsync();
    SCU_PMCSR0.B.REQSLP = 0x03u;;
    __dsync();

    IfxScuWdt_setSafetyEndinit(safetyWdtPw);
    IfxScuWdt_setCpuEndinit(cpuWdtPw);

    McuSm_PerformResetHook(254, 253);
}

static void SysMgr_GoSleepFailure(uint32 FailureInformation)
{
    SysMgr_NvMTimeoutCounter++;
    McuSm_PerformResetHook(384u, FailureInformation);

    for(;;)
    {
        /* Wait for reset or debugger intervention. */
    }
}

void SysMgr_ProcessResetDtc(void)
{
    if(0u == SysMgr_MainCounter)
    {
        if(0u != McuSm_LastResetReason)
        {
            Dem_SetEventStatus(DEM_EVENT_ID_MCUSM_SW_ERROR, DEM_EVENT_STATUS_FAILED);
        }
        else
        {
            Dem_SetEventStatus(DEM_EVENT_ID_MCUSM_SW_ERROR, DEM_EVENT_STATUS_PASSED);
        }
    }
    else
    {
        /* Do nothing. */
    }
}

void SysMgr_EcuStateMachine(void)
{
    if(SYSMGR_STARTUP == SysMgr_EcuState)
    {
        SysMgr_EcuState = SYSMGR_RUN;
        SysMgr_ProcessResetDtc();
        SysMgr_RunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
        SysMgr_PostRunCounter = 1u;
    }

    if (SysMgr_IsFullComActive() != FALSE)
    {
        SysMgr_KeepRunState();
    }

    if(0u < SysMgr_BusActivityCounter)
    {
        SysMgr_BusActivityCounter--;
        SysMgr_NoBusActivity = 0u;
    }
    else
    {
        SysMgr_NoBusActivity = 1u;
    }

    if(SYSMGR_RUN == SysMgr_EcuState)
    {
        if(0u != SysMgr_NoBusActivity)
        {
            SysMgr_RunCounter--;

            if(0u == SysMgr_RunCounter)
            {
                SysMgr_EcuState = SYSMGR_POSTRUN;
            }
        }
        else
        {
            SysMgr_RunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
            SysMgr_PostRunCounter = 1u;
            SysMgr_EcuState = SYSMGR_RUN;
        }
    }

    if(SYSMGR_POSTRUN == SysMgr_EcuState)
    {
        if(0u != SysMgr_NoBusActivity)
        {
            SysMgr_PostRunCounter--;

            if(0u == SysMgr_PostRunCounter)
            {
                SysMgr_EcuState = SYSMGR_SLEEP;
            }
        }
        else
        {
            SysMgr_RunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
            SysMgr_PostRunCounter = 1u;
            SysMgr_EcuState = SYSMGR_RUN;
        }
    }

    if(SYSMGR_SLEEP == SysMgr_EcuState)
    {
        if(0u != SysMgr_NoBusActivity)
        {
            SysMgr_GoSleep();
        }
        else
        {
            SysMgr_RunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
            SysMgr_PostRunCounter = 1u;
            SysMgr_EcuState = SYSMGR_RUN;
        }
    }
}

void SysMgr_MainFunction(void)
{
    SysMgr_EcuStateMachine();

    serviceCpuWatchdog();
    serviceSafetyWatchdog();

    SysMgr_McuTemperature = g_SafetyKitStatus.dieTempStatus.dieTemperatureCore;
    SysMgr_MainCounter++;
}
