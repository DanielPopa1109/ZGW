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
#include "APP/GatewaySwc/GatewaySwc.h"
#include "APP/ParallelFlashSwc/ParallelFlashSwc.h"
#include "BSW/Time/TimeBase.h"

AURIX_SHARED_NC volatile uint8 OsInit_C0;
volatile uint32 Core0_WaitForNvMIdleLoopCounter = 0u;
volatile uint32 Core0_ServiceDemNvMLoopCounter = 0u;
volatile uint32 Core0_WaitForNvMIdleTimeoutCounter = 0u;
volatile uint32 Core0_WaitForNvMIdleLastFailureReason = 0u;
volatile uint32 Core0_WaitForNvMIdleLastNvMStatus = 0u;
volatile uint32 Core0_WaitForNvMIdleLastFeeStatus = 0u;
volatile uint32 Core0_WaitForNvMIdleLastFlsStatus = 0u;
volatile uint32 Core0_ServiceDemNvMTimeoutCounter = 0u;
volatile uint32 Core0_ServiceDemNvMLastFailureReason = 0u;
volatile uint32 Core0_ServiceDemNvMLastNvMStatus = 0u;
volatile uint32 Core0_ServiceDemNvMLastFeeStatus = 0u;
volatile uint32 Core0_ServiceDemNvMLastFlsStatus = 0u;

#define CORE0_NVM_IDLE_WAIT_LOOP_LIMIT      1000000u

#define CORE0_NVM_WAIT_REASON_INIT                  1u
#define CORE0_NVM_WAIT_REASON_READ_ALL              2u
#define CORE0_NVM_WAIT_REASON_TIME_LOAD             3u
#define CORE0_NVM_WAIT_REASON_TIME_STANDBY          4u
#define CORE0_NVM_WAIT_REASON_DEM_INIT              5u
#define CORE0_NVM_WAIT_REASON_OPERATION_CYCLE_START 6u

extern void Ssw_StartCores(void);

static void Core0_ServiceMemoryStack(void)
{
    Fls_MainFunction();

    Fee_MainFunction();

    NvM_MainFunction();
}

static void Core0_WaitForNvMIdle(uint32 FailureReason)
{
    uint32 waitLoops = 0u;
    NvM_StatusType nvmStatus;

    while ((nvmStatus = NvM_GetStatus()) != NVM_IDLE)
    {
        Core0_WaitForNvMIdleLoopCounter++;

        waitLoops++;

        Core0_ServiceMemoryStack();

        if (waitLoops >= CORE0_NVM_IDLE_WAIT_LOOP_LIMIT)
        {
            nvmStatus = NvM_GetStatus();
            Core0_WaitForNvMIdleTimeoutCounter++;
            Core0_WaitForNvMIdleLastFailureReason = FailureReason;
            Core0_WaitForNvMIdleLastNvMStatus = (uint32)nvmStatus;
            Core0_WaitForNvMIdleLastFeeStatus = (uint32)Fee_GetStatus();
            Core0_WaitForNvMIdleLastFlsStatus = (uint32)Fls_GetStatus();
            (void)NvM_CancelJobs();
            break;
        }
    }
}

static void Core0_ServiceDemNvM(uint32 FailureReason)
{
    uint32 waitLoops = 0u;
    NvM_StatusType nvmStatus;

    do
    {
        Core0_ServiceDemNvMLoopCounter++;

        waitLoops++;

        Core0_ServiceMemoryStack();

        Dem_MainFunction();

        if (waitLoops >= CORE0_NVM_IDLE_WAIT_LOOP_LIMIT)
        {
            nvmStatus = NvM_GetStatus();
            Core0_ServiceDemNvMTimeoutCounter++;
            Core0_ServiceDemNvMLastFailureReason = FailureReason;
            Core0_ServiceDemNvMLastNvMStatus = (uint32)nvmStatus;
            Core0_ServiceDemNvMLastFeeStatus = (uint32)Fee_GetStatus();
            Core0_ServiceDemNvMLastFlsStatus = (uint32)Fls_GetStatus();
            (void)NvM_CancelJobs();
            break;
        }

        nvmStatus = NvM_GetStatus();

    } while (nvmStatus != NVM_IDLE);
}

void Core0_InitWatchdog(void)
{
    initCpuWatchdog(0u);

    initSafetyWatchdog();
}

void Core0_HandleScrStartup(void)
{
    IfxScuWdt_clearCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());

    IfxScuWdt_clearSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());

    SysMgr_CaptureScrFaultBeforeScrReset();

    /* Capture retained SCR XRAM records before the SCR image is reset and copied.
     * The SCR writes the time handoff through its 8-bit PMS_XRAM path, so
     * PMS_PMSWCR2.SCRECC is not a reliable validity gate here. */
    McuSm_CaptureWakeupImagesFromScr();

    IfxScr_init(0u);

    IfxScr_copyProgram();

    IfxScr_disableSCR();

    IfxScuWdt_setCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());

    IfxScuWdt_setSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());
}

void Core0_ExecuteSwSafetyKit(void)
{
    runSafeAppSwStartup();

    McuSm_InitializeBusMpu();

    initSafetyKit();

    Ssw_StartCores();
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

    LinTp_Init(1);

    LinSM_RequestComMode(0u, LINSM_FULL_COMMUNICATION);

    Nm_Init();

    ComM_Init();
}

void Core0_DemNvMInit(void)
{
    Dem_PreInit();

    NvM_Init(NULL_PTR);

    Core0_WaitForNvMIdle(CORE0_NVM_WAIT_REASON_INIT);

    NvM_ReadAll();

    Core0_WaitForNvMIdle(CORE0_NVM_WAIT_REASON_READ_ALL);

    (void)TimeBase_LoadUtcFromNvM();

    Core0_WaitForNvMIdle(CORE0_NVM_WAIT_REASON_TIME_LOAD);

    (void)TimeBase_PrepareStandbyRtc();

    Core0_WaitForNvMIdle(CORE0_NVM_WAIT_REASON_TIME_STANDBY);

    CodingApp_Init();

    ParallelFlashSwc_Init();

    GatewaySwc_Init();

    Dem_Init(&Dem_Config);

    Core0_ServiceDemNvM(CORE0_NVM_WAIT_REASON_DEM_INIT);

    Dem_SetOperationCycleState(DEM_DEFAULT_OPERATION_CYCLE, DEM_CYCLE_STATE_START);

    Core0_ServiceDemNvM(CORE0_NVM_WAIT_REASON_OPERATION_CYCLE_START);
}

void Core0_InitSequence(void)
{
    Core0_InitWatchdog();

    Core0_HandleScrStartup();

    Core0_ExecuteSwSafetyKit();

    if (McuSm_Trap4ScrRtcRecordValid != FALSE)
    {
        (void)TimeBase_RestoreStandbyRtcCapture(
                McuSm_Trap4ScrRtcRecord,
                (uint16)SCR_TIME_RECORD_LENGTH);
        McuSm_Trap4ScrRtcRecordValid = FALSE;
    }

    Core0_PinInit();

    TimeBase_Init();

    Core0_ComInit();

    Core0_DemNvMInit();

    Os_Init_C0();

    __dsync();

    OsInit_C0 = 1u;

    Ifx__dsync();
}

void core0_main(void)
{
    //while(0x40000000 == SCU_RSTSTAT.U){__debug();}

    Core0_InitSequence();

    vTaskStartScheduler_core0();
}
