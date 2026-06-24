#include "Nvm.h"
#include "SysMgr.h"
#include "McuSm.h"
#include "IfxCpu.h"
#include "Can.h"
#include "CanSM.h"
#include "EthSM.h"
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
#include "SafetyKit_Main.h"
#include "IfxAsclin_Lin.h"
#include "IfxGeth.h"
#include "Fee.h"
#include "Fls.h"
#include "SCR.h"
#include "IfxPms_reg.h"
#include "BSW/Time/TimeBase.h"
#include "../../../SCR/scr_time_shared.h"

#define SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS 200u
#define SYSMGR_KEEP_AWAKE_WHILE_FULL_COM  1u
#define SYSMGR_NVM_IDLE_WAIT_LOOP_LIMIT   1000000u
#define SYSMGR_FAIL_NVM_PRE_WRITEALL_IDLE 1u
#define SYSMGR_FAIL_NVM_POST_WRITEALL_IDLE 2u
#define SYSMGR_FAIL_DEM_CYCLE_END         3u
#define SYSMGR_FAIL_DEM_SHUTDOWN          4u
#define SYSMGR_FAIL_NVM_WRITEALL_REQ      5u
#define SYSMGR_FAIL_NVM_DEM_WRITEALL      6u
#define SYSMGR_SCR_FAULT_ECC_DBE          0x01u
#define SYSMGR_SCR_FAULT_WDT              0x02u
#define SYSMGR_MCUSM_SOURCE_RESET         0x01u
#define SYSMGR_MCUSM_SOURCE_SCR_ECC_DBE   0x02u
#define SYSMGR_MCUSM_SOURCE_SCR_WDT       0x04u
#define SYSMGR_MCUSM_SOURCE_SAFETYKIT     0x08u
#define SYSMGR_MCUSM_CAPTURE_VERSION      2u
#define SYSMGR_MCUSM_SNAPSHOT_DATA_HEADER_SIZE 64u
#define SYSMGR_MCUSM_SNAPSHOT_DATA_DETAIL_SIZE 160u
#define SYSMGR_MCUSM_SNAPSHOT_DATA_SIZE \
    (SYSMGR_MCUSM_SNAPSHOT_DATA_HEADER_SIZE + SYSMGR_MCUSM_SNAPSHOT_DATA_DETAIL_SIZE)

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
volatile uint32 SysMgr_LastPmsWcr2Status = 0u;
volatile uint32 SysMgr_LastPmsWstat2Status = 0u;
volatile uint8 SysMgr_LastScrWakeReason = 0u;
volatile uint8 SysMgr_LastScrFaultStatus = 0u;
volatile uint8 SysMgr_LastScrNmiStatus = 0u;
volatile uint8 SysMgr_LastScrRstStatus = 0u;
volatile uint8 SysMgr_ScrFaultPending = 0u;
volatile uint32 SysMgr_ScrFaultWakeCounter = 0u;

void SysMgr_ProcessResetDtc(void);
void SysMgr_EcuStateMachine(void);
void SysMgr_MainFunction(void);
void SysMgr_GoSleep(void);

static boolean SysMgr_IsFullComActive(void);
static void SysMgr_KeepRunState(void);
static void SysMgr_GoSleepFailure(uint32 FailureInformation);
static uint8 SysMgr_ReadScrXramU8(uint16 offset);
static void SysMgr_WriteScrXramU8(uint16 offset, uint8 value);
static void SysMgr_CaptureScrFaultStatus(void);
static void SysMgr_ClearScrFaultStatus(void);
static void SysMgr_ClearScrFaultTriggerData(void);
static boolean SysMgr_HasScrFaultError(void);
static void SysMgr_StoreU16(uint8 *buffer, uint16 offset, uint16 value);
static void SysMgr_StoreU32(uint8 *buffer, uint16 offset, uint32 value);
static uint8 SysMgr_GetMcuSmFaultSource(void);
static uint32 SysMgr_GetLastTrapRegister(uint32 trap4Value, uint32 trap7Value);
static uint32 SysMgr_GetTrap7AgRaw(uint32 group);
static uint32 SysMgr_GetTrap7AgMasked(uint32 group);

void SysMgr_NotifyBusActivity(void)
{
    SysMgr_BusActivityCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
    SysMgr_NoBusActivity = 0u;
}

static boolean SysMgr_IsFullComActive(void)
{
#if (SYSMGR_KEEP_AWAKE_WHILE_FULL_COM == 1u)
    CanSM_ComModeType canMode;
    EthSM_ComModeType ethMode;

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

    if ((EthSM_GetCurrentComMode(0u, &ethMode) == E_OK) &&
            (ethMode == ETHSM_FULL_COMMUNICATION))
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

static uint8 SysMgr_ReadScrXramU8(uint16 offset)
{
    volatile uint8 *xram = (volatile uint8 *)PMS_XRAM;

    return xram[(uint16)(SCR_TIME_XRAM_BASE + offset)];
}

static void SysMgr_WriteScrXramU8(uint16 offset, uint8 value)
{
    volatile uint8 *xram = (volatile uint8 *)PMS_XRAM;

    xram[(uint16)(SCR_TIME_XRAM_BASE + offset)] = value;
}

static void SysMgr_StoreU16(uint8 *buffer, uint16 offset, uint16 value)
{
    buffer[offset] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)(value & 0xFFu);
}

static void SysMgr_StoreU32(uint8 *buffer, uint16 offset, uint32 value)
{
    buffer[offset] = (uint8)((value >> 24u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)((value >> 16u) & 0xFFu);
    buffer[(uint16)(offset + 2u)] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 3u)] = (uint8)(value & 0xFFu);
}

static uint32 SysMgr_GetLastTrapRegister(uint32 trap4Value, uint32 trap7Value)
{
    if (McuSm_LastTrapClass == 4u)
    {
        return trap4Value;
    }

    if (McuSm_LastTrapClass == 7u)
    {
        return trap7Value;
    }

    return 0u;
}

static uint32 SysMgr_GetTrap7AgRaw(uint32 group)
{
    if (group < 12u)
    {
        return McuSm_Trap7AgRaw[(uint8)group];
    }

    return 0u;
}

static uint32 SysMgr_GetTrap7AgMasked(uint32 group)
{
    if (group < 12u)
    {
        return McuSm_Trap7AgMasked[(uint8)group];
    }

    return 0u;
}

static void SysMgr_CaptureScrFaultStatus(void)
{
    uint8 pending = 0u;
    uint8 newPending;

    SysMgr_LastPmsWcr2Status = PMS_PMSWCR2.U;
    SysMgr_LastPmsWstat2Status = PMS_PMSWSTAT2.U;
    SysMgr_LastScrWakeReason = SysMgr_ReadScrXramU8(SCR_TIME_OFFSET_WAKE_REASON);
    SysMgr_LastScrFaultStatus = SysMgr_ReadScrXramU8(SCR_TIME_OFFSET_SCR_FAULT_STATUS);
    SysMgr_LastScrNmiStatus = SysMgr_ReadScrXramU8(SCR_TIME_OFFSET_SCR_NMI_STATUS);
    SysMgr_LastScrRstStatus = SysMgr_ReadScrXramU8(SCR_TIME_OFFSET_SCR_RST_STATUS);

    if ((PMS_PMSWCR2.B.SCRECC != 0u) ||
            ((SysMgr_LastScrWakeReason & SCR_TIME_WAKE_REASON_ECC_DBE) != 0u) ||
            ((SysMgr_LastScrFaultStatus & SCR_TIME_FAULT_STATUS_ECC_DBE) != 0u))
    {
        pending |= SYSMGR_SCR_FAULT_ECC_DBE;
    }

    if ((PMS_PMSWCR2.B.SCRWDT != 0u) ||
            ((SysMgr_LastScrWakeReason & SCR_TIME_WAKE_REASON_WDT) != 0u) ||
            ((SysMgr_LastScrFaultStatus & SCR_TIME_FAULT_STATUS_WDT) != 0u))
    {
        pending |= SYSMGR_SCR_FAULT_WDT;
    }

    if (pending != 0u)
    {
        newPending = (uint8)(pending & (uint8)~SysMgr_ScrFaultPending);

        SysMgr_ScrFaultPending |= pending;

        if (newPending != 0u)
        {
            SysMgr_ScrFaultWakeCounter++;
        }
    }
}

void SysMgr_CaptureScrFaultBeforeScrReset(void)
{
    SysMgr_CaptureScrFaultStatus();
}

static boolean SysMgr_HasScrFaultError(void)
{
    SysMgr_CaptureScrFaultStatus();

    return (SysMgr_ScrFaultPending != 0u) ? TRUE : FALSE;
}

static void SysMgr_ClearScrFaultStatus(void)
{
    uint16 safetyWdtPw;
    Ifx_PMS_PMSWCR2 pmswcr2Clear;
    Ifx_PMS_PMSWSTATCLR pmswstatClear;

    safetyWdtPw = IfxScuWdt_getSafetyWatchdogPassword();
    IfxScuWdt_clearSafetyEndinit(safetyWdtPw);

    pmswcr2Clear.U = 0u;
    pmswcr2Clear.B.TCINT = PMS_PMSWCR2.B.TCINT;
    if ((SysMgr_ScrFaultPending & SYSMGR_SCR_FAULT_ECC_DBE) != 0u)
    {
        pmswcr2Clear.B.SCRECC = 1u;
    }
    if ((SysMgr_ScrFaultPending & SYSMGR_SCR_FAULT_WDT) != 0u)
    {
        pmswcr2Clear.B.SCRWDT = 1u;
    }
    PMS_PMSWCR2.U = pmswcr2Clear.U;

    pmswstatClear.U = 0u;
    pmswstatClear.B.SCRWKPCLR = 1u;
    pmswstatClear.B.SCROVRUNCLR = 1u;
    PMS_PMSWSTATCLR.U = pmswstatClear.U;

    IfxScuWdt_setSafetyEndinit(safetyWdtPw);
}

static void SysMgr_ClearScrFaultTriggerData(void)
{
    uint16 safetyWdtPw;
    Ifx_PMS_PMSWCR2 pmswcr2Clear;
    Ifx_PMS_PMSWSTATCLR pmswstatClear;
    uint8 wakeReason;

    safetyWdtPw = IfxScuWdt_getSafetyWatchdogPassword();
    IfxScuWdt_clearSafetyEndinit(safetyWdtPw);

    pmswcr2Clear.U = 0u;
    pmswcr2Clear.B.TCINT = PMS_PMSWCR2.B.TCINT;
    pmswcr2Clear.B.SCRECC = 1u;
    pmswcr2Clear.B.SCRWDT = 1u;
    PMS_PMSWCR2.U = pmswcr2Clear.U;

    pmswstatClear.U = 0u;
    pmswstatClear.B.SCRWKPCLR = 1u;
    pmswstatClear.B.SCROVRUNCLR = 1u;
    PMS_PMSWSTATCLR.U = pmswstatClear.U;

    IfxScuWdt_setSafetyEndinit(safetyWdtPw);

    wakeReason = SysMgr_ReadScrXramU8(SCR_TIME_OFFSET_WAKE_REASON);
    wakeReason &= (uint8)~(SCR_TIME_WAKE_REASON_ECC_DBE | SCR_TIME_WAKE_REASON_WDT);
    SysMgr_WriteScrXramU8(SCR_TIME_OFFSET_WAKE_REASON, wakeReason);
    SysMgr_WriteScrXramU8(SCR_TIME_OFFSET_SCR_FAULT_STATUS, 0u);
    SysMgr_WriteScrXramU8(SCR_TIME_OFFSET_SCR_NMI_STATUS, 0u);
    SysMgr_WriteScrXramU8(SCR_TIME_OFFSET_SCR_RST_STATUS, 0u);

    SysMgr_LastPmsWcr2Status = 0u;
    SysMgr_LastPmsWstat2Status = 0u;
    SysMgr_LastScrWakeReason = wakeReason;
    SysMgr_LastScrFaultStatus = 0u;
    SysMgr_LastScrNmiStatus = 0u;
    SysMgr_LastScrRstStatus = 0u;
    SysMgr_ScrFaultPending = 0u;
}

void SysMgr_ClearMcuSmSwErrorTriggerData(void)
{
    McuSm_ClearResetDtcTriggerData();
    SysMgr_ClearScrFaultTriggerData();
}

void SysMgr_OnDemEventCleared(Dem_EventIdType eventId)
{
    if (eventId == DEM_EVENT_ID_MCUSM_SW_ERROR)
    {
        SysMgr_ClearMcuSmSwErrorTriggerData();
        McuSm_SaveRetainedStateToScr();
    }
}

static uint8 SysMgr_GetMcuSmFaultSource(void)
{
    uint8 source = 0u;

    if (McuSm_LastResetReason != 0u)
    {
        source |= SYSMGR_MCUSM_SOURCE_RESET;
    }

    if ((SysMgr_ScrFaultPending & SYSMGR_SCR_FAULT_ECC_DBE) != 0u)
    {
        source |= SYSMGR_MCUSM_SOURCE_SCR_ECC_DBE;
    }

    if ((SysMgr_ScrFaultPending & SYSMGR_SCR_FAULT_WDT) != 0u)
    {
        source |= SYSMGR_MCUSM_SOURCE_SCR_WDT;
    }

    if ((McuSm_SafetyKitFailureMask != 0u) ||
            (McuSm_LastResetReason == MCUSM_RESET_REASON_SAFETYKIT_TEST))
    {
        source |= SYSMGR_MCUSM_SOURCE_SAFETYKIT;
    }

    return source;
}

Std_ReturnType SysMgr_CaptureMcuSmSnapshotData(
    Dem_EventIdType eventId,
    uint8 *buffer,
    uint16 *length
)
{
    uint16 i;
    uint8 *detail;

    if ((eventId != DEM_EVENT_ID_MCUSM_SW_ERROR) ||
            (buffer == NULL_PTR) ||
            (length == NULL_PTR) ||
            (*length < SYSMGR_MCUSM_SNAPSHOT_DATA_SIZE))
    {
        return E_NOT_OK;
    }

    SysMgr_CaptureScrFaultStatus();

    for (i = 0u; i < SYSMGR_MCUSM_SNAPSHOT_DATA_SIZE; i++)
    {
        buffer[i] = 0u;
    }

    SysMgr_StoreU16(buffer, 0u, eventId);
    buffer[2] = SysMgr_GetMcuSmFaultSource();
    buffer[3] = SysMgr_LastScrWakeReason;
    SysMgr_StoreU32(buffer, 4u, McuSm_LastResetReason);
    if ((McuSm_LastResetReason == 0u) && (McuSm_SafetyKitFailureMask != 0u))
    {
        SysMgr_StoreU32(buffer, 8u, McuSm_SafetyKitFailureMask);
    }
    else
    {
        SysMgr_StoreU32(buffer, 8u, McuSm_LastResetInformation);
    }
    SysMgr_StoreU32(buffer, 12u, SysMgr_LastPmsWcr2Status);
    SysMgr_StoreU32(buffer, 16u, SysMgr_LastPmsWstat2Status);
    buffer[20] = SysMgr_LastScrFaultStatus;
    buffer[21] = SysMgr_LastScrNmiStatus;
    buffer[22] = SysMgr_LastScrRstStatus;
    buffer[23] = SysMgr_ScrFaultPending;
    SysMgr_StoreU32(buffer, 24u, SysMgr_ScrFaultWakeCounter);
    SysMgr_StoreU32(buffer, 28u, SysMgr_GoSleepCounter);

    buffer[32u] = SYSMGR_MCUSM_CAPTURE_VERSION;
    buffer[33u] = (uint8)(McuSm_LastTrapClass & 0xFFu);
    buffer[34u] = (uint8)(McuSm_LastTrapId & 0xFFu);
    buffer[35u] = (uint8)(McuSm_LastTrapCoreId & 0xFFu);
    SysMgr_StoreU32(buffer, 36u, McuSm_LastTrapTAddr);
    SysMgr_StoreU32(buffer, 40u, McuSm_Trap4ErrorAddress);
    SysMgr_StoreU32(buffer, 44u, McuSm_Trap7AgRstRsn);
    SysMgr_StoreU32(buffer, 48u, McuSm_Trap7AgRstInfo);
    SysMgr_StoreU32(buffer, 52u, McuSm_DFlashRecoveryRequest);
    SysMgr_StoreU32(buffer, 56u, McuSm_DFlashRecoveryInfo);
    SysMgr_StoreU32(buffer, 60u, McuSm_DFlashRecoveryLastFeePhysicalAddress);

    detail = &buffer[SYSMGR_MCUSM_SNAPSHOT_DATA_HEADER_SIZE];
    SysMgr_StoreU16(detail, 0u, eventId);
    detail[2] = SysMgr_GetMcuSmFaultSource();
    detail[3] = SysMgr_LastScrWakeReason;
    SysMgr_StoreU32(detail, 4u, SysMgr_LastPmsWcr2Status);
    SysMgr_StoreU32(detail, 8u, SysMgr_LastPmsWstat2Status);
    detail[12] = SysMgr_LastScrFaultStatus;
    detail[13] = SysMgr_LastScrNmiStatus;
    detail[14] = SysMgr_LastScrRstStatus;
    detail[15] = SysMgr_ScrFaultPending;

    detail[16u] = SYSMGR_MCUSM_CAPTURE_VERSION;
    detail[17u] = McuSm_SafetyKitResetInhibit;
    detail[18u] = McuSm_FBL_ResetCounter;
    detail[19u] = McuSm_FBL_ProgrammingRequest;
    SysMgr_StoreU32(detail, 20u, McuSm_LastResetReason);
    SysMgr_StoreU32(detail, 24u, McuSm_LastResetInformation);
    SysMgr_StoreU32(detail, 28u, McuSm_SafetyKitFailureMask);
    SysMgr_StoreU32(detail, 32u, McuSm_SafetyKitFwCheckResultMask);
    SysMgr_StoreU32(detail, 36u, McuSm_SswStatusData.resetType);
    SysMgr_StoreU32(detail, 40u, McuSm_SswStatusData.resetTrigger);
    SysMgr_StoreU32(detail, 44u, McuSm_SswStatusData.rstStat);
    SysMgr_StoreU16(detail, 48u, McuSm_SswStatusData.resetReason);
    detail[50u] = McuSm_SswStatusData.lbistAppSwReq;
    detail[51u] = McuSm_SswStatusData.lbistRuns;
    detail[52u] = McuSm_SswStatusData.mcuFwcheckRuns;
    detail[53u] = McuSm_SswStatusData.lbistStatus;
    detail[54u] = McuSm_SswStatusData.monbistStatus;
    detail[55u] = McuSm_SswStatusData.mcuFwcheckStatus;
    detail[56u] = McuSm_SswStatusData.mcuStartupStatus;
    detail[57u] = McuSm_SswStatusData.aliveAlarmTestStatus;
    detail[58u] = McuSm_SswStatusData.regMonitorTestStatus;
    detail[59u] = McuSm_SswStatusData.mbistStatus;
    detail[60u] = McuSm_SswStatusData.smuCoreKeysTestSts;
    detail[61u] = McuSm_SswStatusData.smuCoreKeysTestClearSts;
    detail[62u] = McuSm_SswStatusData.smuCoreInitSts;
    detail[63u] = McuSm_SswStatusData.wakeupFromStandby;
    SysMgr_StoreU32(detail, 64u, McuSm_SafetyKitFwCheckLastSshFail);
    SysMgr_StoreU32(detail, 68u, McuSm_SafetyKitFwCheckSshActualEccd);
    SysMgr_StoreU32(detail, 72u, McuSm_SafetyKitFwCheckSshActualFaultsts);
    SysMgr_StoreU32(detail, 76u, McuSm_SafetyKitFwCheckSshActualErrinfo);
    SysMgr_StoreU32(detail, 80u, McuSm_SafetyKitFwCheckSshExpectedEccd);
    SysMgr_StoreU32(detail, 84u, McuSm_SafetyKitFwCheckSshExpectedFaultsts);
    SysMgr_StoreU32(detail, 88u, McuSm_SafetyKitFwCheckSshExpectedErrinfo);
    SysMgr_StoreU32(detail, 92u, McuSm_TrapCounter);
    SysMgr_StoreU32(detail, 96u, SysMgr_GetLastTrapRegister(McuSm_Trap4Dstr, McuSm_Trap7Dstr));
    SysMgr_StoreU32(detail, 100u, SysMgr_GetLastTrapRegister(McuSm_Trap4Datr, McuSm_Trap7Datr));
    SysMgr_StoreU32(detail, 104u, SysMgr_GetLastTrapRegister(McuSm_Trap4Deadd, McuSm_Trap7Deadd));
    SysMgr_StoreU32(detail, 108u, SysMgr_GetLastTrapRegister(McuSm_Trap4Diear, McuSm_Trap7Diear));
    SysMgr_StoreU32(detail, 112u, SysMgr_GetLastTrapRegister(McuSm_Trap4Dietr, McuSm_Trap7Dietr));
    SysMgr_StoreU32(detail, 116u, SysMgr_GetLastTrapRegister(McuSm_Trap4Piear, McuSm_Trap7Piear));
    SysMgr_StoreU32(detail, 120u, SysMgr_GetLastTrapRegister(McuSm_Trap4Pietr, McuSm_Trap7Pietr));
    SysMgr_StoreU32(detail, 124u, SysMgr_GetTrap7AgRaw(McuSm_Trap7AgRstRsn));
    SysMgr_StoreU32(detail, 128u, SysMgr_GetTrap7AgMasked(McuSm_Trap7AgRstRsn));
    SysMgr_StoreU32(detail, 132u, McuSm_DFlashRecoveryCounter);
    SysMgr_StoreU32(detail, 136u, McuSm_DFlashRecoveryAttemptCounter);
    SysMgr_StoreU32(detail, 140u, McuSm_DFlashRecoverySuppressCounter);
    SysMgr_StoreU32(detail, 144u, McuSm_DFlashRecoveryLastFeeAccessKind);
    SysMgr_StoreU32(detail, 148u, McuSm_DFlashRecoveryLastFeePhysicalAddress);
    SysMgr_StoreU32(detail, 152u, McuSm_DFlashRecoveryRequest);
    SysMgr_StoreU32(detail, 156u, McuSm_DFlashRecoveryInfo);

    *length = SYSMGR_MCUSM_SNAPSHOT_DATA_SIZE;
    return E_OK;
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

        if (waitLoops >= SYSMGR_NVM_IDLE_WAIT_LOOP_LIMIT)
        {
            SysMgr_GoSleepFailure(SYSMGR_FAIL_NVM_PRE_WRITEALL_IDLE);
        }
    }

    SysMgr_ClearMcuSmSwErrorTriggerData();
    (void)TimeBase_PrepareStandbyRtc();
    McuSm_SaveRetainedStateToScr();

    if (Dem_Shutdown() != E_OK)
    {
        SysMgr_GoSleepFailure(SYSMGR_FAIL_DEM_SHUTDOWN);
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

        if (waitLoops >= SYSMGR_NVM_IDLE_WAIT_LOOP_LIMIT)
        {
            SysMgr_GoSleepFailure(SYSMGR_FAIL_NVM_POST_WRITEALL_IDLE);
        }
    }

    if ((NvM_WriteAllBlockPlanned[0u] != 0u) &&
            ((NvM_WriteAllBlockWritten[0u] == 0u) ||
             (NvM_WriteAllBlockResult[0u] != 2u)))
    {
        SysMgr_GoSleepFailure(SYSMGR_FAIL_NVM_DEM_WRITEALL);
    }

    IfxAsclin_disableModule((Ifx_ASCLIN *)(void *)&MODULE_ASCLIN1);
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
    IfxMtu_clearSram((IfxMtu_MbistSel)77);
    IfxMtu_clearSram((IfxMtu_MbistSel)78);
    IfxScr_copyProgram();
    (void)TimeBase_RearmStandbyRtc();
    McuSm_SaveRetainedStateToScr();
    IfxScr_init(1);
    IfxScr_enableSCR();
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
    PMS_PMSIEN.B.SCRINT = 1u;
    PMS_PMSIEN.B.SCRECC = 1u;
    PMS_PMSIEN.B.SCRWDT = 1u;
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
        boolean scrFaultDetected = SysMgr_HasScrFaultError();
        boolean safetyKitFaultDetected = ((McuSm_SafetyKitFailureMask != 0u) ||
                (McuSm_LastResetReason == MCUSM_RESET_REASON_SAFETYKIT_TEST)) ? TRUE : FALSE;

        if((0u != McuSm_LastResetReason) || (scrFaultDetected != FALSE) || (safetyKitFaultDetected != FALSE))
        {
            Dem_SetEventStatus(DEM_EVENT_ID_MCUSM_SW_ERROR, DEM_EVENT_STATUS_FAILED);

            if (scrFaultDetected != FALSE)
            {
                SysMgr_ClearScrFaultStatus();
            }

            if (safetyKitFaultDetected != FALSE)
            {
                if (McuSm_LastResetReason == MCUSM_RESET_REASON_SAFETYKIT_TEST)
                {
                    McuSm_SafetyKitResetInhibit = 1u;
                }
                else
                {
                    McuSm_SafetyKitResetInhibit = 0u;
                }
            }
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
    if(SYSMGR_INIT == SysMgr_EcuState)
    {
        SysMgr_EcuState = SYSMGR_STARTUP;
    }

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
            SysMgr_PostRunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
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
            SysMgr_PostRunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
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
            SysMgr_PostRunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
            SysMgr_EcuState = SYSMGR_RUN;
        }
    }
}

void SysMgr_MainFunction(void)
{
    SysMgr_EcuStateMachine();

    SysMgr_McuTemperature = g_SafetyKitStatus.dieTempStatus.dieTemperatureCore;
    SysMgr_MainCounter++;
}
