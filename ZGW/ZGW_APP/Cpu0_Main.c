#include "Nvm.h"
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "IfxMtu.h"
#include "Os.h"
#include "Can.h"
#include "Dem.h"
#include "Dcm.h"
#include "McuSm.h"
#include "SysMgr.h"
#include "Wdg.h"
#include "Smu.h"
#include "Crc.h"
#include "task_core0.h"
#include "SafetyKit_SSW.h"
#include "SafetyKit_Main.h"
#include "aurix_pin_mappings.h"
#include "SCR.h"
#include "CanIf.h"
#include "CanTp.h"
#include "Dcm_Cfg.h"
#include "LinIf.h"
#include "LinSM.h"
#include "PduR.h"
#include "Com.h"
#include "CanSM.h"
#include "LinTp.h"
#include "EthStack.h"
#include "Dem.h"
#include "Fls.h"
#include "Fee.h"
#include "SafetyKit_InternalWatchdogs.h"
#include "IfxCpu_Intrinsics.h"
#include "Nm.h"
#include "ComM.h"
#include "APP/CodingApp/CodingApp.h"
#include "BSW/Time/TimeBase.h"

volatile uint8 OsInit_C0 = 0u;
volatile uint32 Core0_WaitForNvMIdleLoopCounter = 0u;
volatile uint32 Core0_ServiceDemNvMLoopCounter = 0u;

#define CORE0_NVM_IDLE_WAIT_LOOP_LIMIT      1000000u

extern void Ssw_StartCores(void);

static void Core0_ServiceWatchdogs(void)
{
    serviceCpuWatchdog();

    serviceSafetyWatchdog();
}

static void Core0_ServiceMemoryStack(void)
{
    Fls_MainFunction();

    Fee_MainFunction();

    NvM_MainFunction();

    Core0_ServiceWatchdogs();
}

static void Core0_WaitForNvMIdle(uint32 FailureReason)
{
    uint32 waitLoops = 0u;

    while (NvM_GetStatus() != NVM_IDLE)
    {
        Core0_WaitForNvMIdleLoopCounter++;

        waitLoops++;

        Core0_ServiceMemoryStack();

        if (waitLoops >= CORE0_NVM_IDLE_WAIT_LOOP_LIMIT)
        {
            break;
        }
    }
}

static void Core0_ServiceDemNvM(uint32 FailureReason)
{
    uint32 waitLoops = 0u;

    do
    {
        Core0_ServiceDemNvMLoopCounter++;

        waitLoops++;

        Core0_ServiceMemoryStack();

        Dem_MainFunction();

        if (waitLoops >= CORE0_NVM_IDLE_WAIT_LOOP_LIMIT)
        {
            break;
        }

    } while (NvM_GetStatus() != NVM_IDLE);
}

void Core0_InitWatchdog(void)
{
    initCpuWatchdog(0u);

    initSafetyWatchdog();

    Core0_ServiceWatchdogs();
}

void Core0_HandleScrStartup(void)
{
    IfxScuWdt_clearCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());

    IfxScuWdt_clearSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());

    SysMgr_CaptureScrFaultBeforeScrReset();

    TimeBase_CaptureStandbyRtcBeforeScrReset();

    IfxScr_init(0u);

    IfxScr_copyProgram();

    IfxScr_disableSCR();

    IfxScuWdt_setCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());

    IfxScuWdt_setSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());
}

void Core0_ExecuteSwSafetyKit(void)
{
    Core0_ServiceWatchdogs();

    runSafeAppSwStartup();

    Core0_ServiceWatchdogs();

    McuSm_InitializeBusMpu();

    Core0_ServiceWatchdogs();

    initSafetyKit();

    Ssw_StartCores();

    Core0_ServiceWatchdogs();
}

void Core0_PinInit(void)
{
    gpio_init_pins();

    can0_node0_init_pins();

    can1_node3_init_pins();

    asclin1_init_pins();
}

void Core0_ComInit(void)
{
    Can_Init();

    CanIf_Init(&CanIf_Config);

    PduR_Init();

    Com_Init();

    CanTp_Init(&CanTp_Config);

    CanSM_Init();

    CanSM_RequestComMode(CAN_CONTROLLER_CLASSIC, CANSM_COMM_FULL_COMMUNICATION);

    CanSM_RequestComMode(CAN_CONTROLLER_FD, CANSM_COMM_FULL_COMMUNICATION);

    Dcm_Init(&Dcm_Config);

    LinSM_Init();

    LinIf_Init();

    LinTp_Init(1);

    LinSM_RequestComMode(0u, LINSM_FULL_COMMUNICATION);

    Nm_Init();

    ComM_Init();
}

void Core0_DemNvMInit(void)
{
    Dem_PreInit();

    NvM_Init(NULL_PTR);

    Core0_WaitForNvMIdle(0);

    NvM_ReadAll();

    Core0_WaitForNvMIdle(0);

    (void)TimeBase_LoadUtcFromNvM();

    CodingApp_Init();

    Dem_Init(&Dem_Config);

    Core0_ServiceDemNvM(0);

    Dem_SetOperationCycleState(DEM_DEFAULT_OPERATION_CYCLE, DEM_CYCLE_STATE_START);

    Core0_ServiceDemNvM(0);
}

void Core0_InitSequence(void)
{
    Core0_InitWatchdog();

    Core0_HandleScrStartup();

    Core0_ExecuteSwSafetyKit();

    Core0_PinInit();

    TimeBase_Init();

    Core0_ComInit();

    Core0_DemNvMInit();

    Os_Init_C0();

    __dsync();

    OsInit_C0 = 1u;

    __dsync();
}

void core0_main(void)
{
    Core0_InitSequence();

    vTaskStartScheduler_core0();
}
