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
#include "SafetyKit_InternalWatchdogs.h"
#include "IfxCpu_Intrinsics.h"
#include "IfxPmsPm.h"
#include "IfxScuWdt.h"
#include "IfxStm.h"
#include "IfxPort.h"
#include "IfxPort_reg.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "Dcm.h"
#include "SafetyKit_Main.h"
#include "IfxAsclin_Lin.h"
#include "IfxGeth.h"
#include "Fee.h"
#include "Fls.h"
#include "SCR.h"
#include "IfxPms_reg.h"
#include "APP/TimeSync/TimeBase.h"
#include "../../../SCR/scr_time_shared.h"

#ifdef SAFETYKIT_PMS_ERRATA_STATUS_FAILED
#define SYSMGR_PMS_ERRATA_FEATURE_ENABLED 1u
#else
#define SYSMGR_PMS_ERRATA_FEATURE_ENABLED 0u
#endif

#define SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS 1000u
#define SYSMGR_KEEP_AWAKE_WHILE_FULL_COM  1u
/*
 * Backstop iteration cap for the GoSleep NvM drain loops.  GoSleep runs with
 * interrupts disabled, so the normal ASIL_BSW watchdog service never runs while
 * these loops pump the mem stack - the loops now feed both watchdogs themselves
 * (see below).  The cap must be comfortably ABOVE a healthy worst-case WriteAll:
 * a WriteAll can trigger a Fee garbage collection = two 128 KiB virtual-sector
 * erases (64 x 4 KiB logical-sector erases) plus the record copies.  A single
 * stuck DFlash erase is already bounded by the Fls per-erase DMU poll limit
 * (FLS_DMU_ERASE_BUSY_POLL_LIMIT = 1e6 consecutive busy polls -> job FAILED ->
 * Fls/Fee/NvM return to IDLE and the loop exits), so 64 erases can legitimately
 * absorb up to ~64e6 pump iterations before the lower layer itself declares a
 * stall.  The old 1e6 cap was BELOW even a single healthy GC, so a sector switch
 * at sleep hit SYSMGR_FAIL_NVM_POST_WRITEALL_IDLE and reset the ECU mid-write.
 * 1e8 sits above the lower-layer guarantee and only fires on a genuine
 * lower-layer fault (double failure), as a true last resort.
 */
#define SYSMGR_NVM_IDLE_WAIT_LOOP_LIMIT   100000000u
#define SYSMGR_FAIL_NVM_PRE_WRITEALL_IDLE 1u
#define SYSMGR_FAIL_NVM_POST_WRITEALL_IDLE 2u
#define SYSMGR_FAIL_DEM_CYCLE_END         3u
#define SYSMGR_FAIL_DEM_SHUTDOWN          4u
#define SYSMGR_FAIL_NVM_WRITEALL_REQ      5u
#define SYSMGR_FAIL_NVM_DEM_WRITEALL      6u
#define SYSMGR_FAIL_P33_PCSR_SCR_OWNER    7u
#define SYSMGR_SCR_FAULT_ECC_DBE          0x01u
#define SYSMGR_SCR_FAULT_WDT              0x02u
#define SYSMGR_MCUSM_SOURCE_RESET         0x01u
#define SYSMGR_MCUSM_SOURCE_SCR_ECC_DBE   0x02u
#define SYSMGR_MCUSM_SOURCE_SCR_WDT       0x04u
#define SYSMGR_MCUSM_SOURCE_SAFETYKIT     0x08u
#define SYSMGR_MCUSM_CAPTURE_VERSION      2u
#define SYSMGR_MCUSM_SNAPSHOT_DATA_HEADER_SIZE 64u
#define SYSMGR_MCUSM_SNAPSHOT_DATA_DETAIL_SIZE 160u
#define SYSMGR_MCUSM_SNAPSHOT_DATA_TIME_OFFSET \
    (SYSMGR_MCUSM_SNAPSHOT_DATA_HEADER_SIZE + SYSMGR_MCUSM_SNAPSHOT_DATA_DETAIL_SIZE)
#define SYSMGR_MCUSM_SNAPSHOT_DATA_SIZE \
    (SYSMGR_MCUSM_SNAPSHOT_DATA_TIME_OFFSET + DEM_DTC_TIMESTAMP_DATA_SIZE)
#define SYSMGR_PMS_ERRATA_SNAPSHOT_DATA_VERSION 2u
#define SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET  76u
#define SYSMGR_PMS_ERRATA_SNAPSHOT_COMMON_TIME_OFFSET 108u
#define SYSMGR_PMS_ERRATA_SNAPSHOT_DATA_SIZE \
    (SYSMGR_PMS_ERRATA_SNAPSHOT_COMMON_TIME_OFFSET + DEM_DTC_TIMESTAMP_DATA_SIZE)
#define SYSMGR_PMSWSTATCLR_SCRSTCLR_MASK 0x00010000u
#define SYSMGR_P33_PCSR_CLASSIC_RX_MASK   (1u << 5u)
#define SYSMGR_P33_PCSR_LOCK_WAIT_LIMIT   10000u

uint32 SysMgr_MainCounter = 0u;
static boolean SysMgr_ResetDtcProcessed = FALSE;
uint32 SysMgr_RunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
volatile uint32 SysMgr_BusActivityCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
volatile uint32 SysMgr_GoSleepCounter = 0u;
volatile uint32 SysMgr_NvMPreWriteAllWaitCounter = 0u;
volatile uint32 SysMgr_NvMPostWriteAllWaitCounter = 0u;
volatile uint32 SysMgr_NvMTimeoutCounter = 0u;
volatile uint32 SysMgr_PcsrLockWaitCounter = 0u;
volatile uint32 SysMgr_PcsrLockTimeoutCounter = 0u;
volatile uint32 SysMgr_PcsrBeforeScrOwner = 0u;
volatile uint32 SysMgr_PcsrAfterScrOwner = 0u;
volatile uint32 SysMgr_PcsrLockTimeoutValue = 0u;
volatile SysMgr_EcuState_t SysMgr_EcuState = SYSMGR_STARTUP;
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

extern void getPmsVoltageMeasurements(void);

void SysMgr_ProcessResetDtc(void);
void SysMgr_EcuStateMachine(void);
void SysMgr_MainFunction(void);
void SysMgr_GoSleep(void);

static boolean SysMgr_IsFullComActive(void);
static void SysMgr_KeepRunState(void);
static void SysMgr_GoSleepFailure(uint32 FailureInformation);
static boolean SysMgr_WaitP33PcsrUnlocked(void);
static boolean SysMgr_SetClassicRxPinOwnerScr(void);
static uint8 SysMgr_ClearScrResetStatusIfSet(void);
static uint8 SysMgr_ReadScrXramU8(uint16 offset);
static void SysMgr_WriteScrXramU8(uint16 offset, uint8 value);
static void SysMgr_CaptureScrFaultStatus(void);
static void SysMgr_ClearScrFaultStatus(void);
static void SysMgr_ClearScrFaultTriggerData(void);
static boolean SysMgr_HasScrFaultError(void);
static void SysMgr_StoreU16(uint8 *buffer, uint16 offset, uint16 value);
static void SysMgr_StoreU32(uint8 *buffer, uint16 offset, uint32 value);
static void SysMgr_StoreU64(uint8 *buffer, uint16 offset, uint64 value);
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
    /* ComM is gated by SysMgr_BusActivityCounter; controller mode alone is
     * never a reason to postpone sleep. */
    return FALSE;
}

static void SysMgr_KeepRunState(void)
{
    SysMgr_RunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;

    if (SysMgr_EcuState == SYSMGR_GOSLEEP)
    {
        SysMgr_EcuState = SYSMGR_RUN;
    }
}

static boolean SysMgr_WaitP33PcsrUnlocked(void)
{
    uint32 waitCount = SYSMGR_P33_PCSR_LOCK_WAIT_LIMIT;

    while(P33_PCSR.B.LCK != 0u)
    {
        if(waitCount == 0u)
        {
            SysMgr_PcsrLockTimeoutCounter++;
            SysMgr_PcsrLockTimeoutValue = P33_PCSR.U;
            return FALSE;
        }

        SysMgr_PcsrLockWaitCounter++;
        serviceCpuWatchdog();
        serviceSafetyWatchdog();
        waitCount--;
    }

    return TRUE;
}

static boolean SysMgr_SetClassicRxPinOwnerScr(void)
{
    uint16 safetyWdtPw;
    uint8 safetyEndinitWasSet;

    SysMgr_PcsrBeforeScrOwner = P33_PCSR.U;

    if(SysMgr_WaitP33PcsrUnlocked() == FALSE)
    {
        SysMgr_PcsrAfterScrOwner = P33_PCSR.U;
        return FALSE;
    }

    safetyWdtPw = IfxScuWdt_getSafetyWatchdogPassword();
    safetyEndinitWasSet = (IfxScuWdt_getSafetyWatchdogEndInit() != 0u) ? 1u : 0u;

    if(safetyEndinitWasSet != 0u)
    {
        IfxScuWdt_clearSafetyEndinit(safetyWdtPw);
    }
    __ldmst(&P33_PCSR.U, SYSMGR_P33_PCSR_CLASSIC_RX_MASK, SYSMGR_P33_PCSR_CLASSIC_RX_MASK);
    __dsync();
    if(safetyEndinitWasSet != 0u)
    {
        IfxScuWdt_setSafetyEndinit(safetyWdtPw);
    }

    if(SysMgr_WaitP33PcsrUnlocked() == FALSE)
    {
        SysMgr_PcsrAfterScrOwner = P33_PCSR.U;
        return FALSE;
    }

    SysMgr_PcsrAfterScrOwner = P33_PCSR.U;
    return TRUE;
}

static uint8 SysMgr_ClearScrResetStatusIfSet(void)
{
    if (PMS_PMSWSTAT.B.SCRST != 0u)
    {
        uint16 safetyWdtPw = IfxScuWdt_getSafetyWatchdogPassword();
        uint8 safetyEndinitWasSet = (IfxScuWdt_getSafetyWatchdogEndInit() != 0u) ? 1u : 0u;

        if (safetyEndinitWasSet != 0u)
        {
            IfxScuWdt_clearSafetyEndinit(safetyWdtPw);
        }
        PMS_PMSWSTATCLR.U = SYSMGR_PMSWSTATCLR_SCRSTCLR_MASK;
        __dsync();
        if (safetyEndinitWasSet != 0u)
        {
            IfxScuWdt_setSafetyEndinit(safetyWdtPw);
        }
        return 1u;
    }

    return 0u;
}

static uint8 SysMgr_ReadScrXramU8(uint16 offset)
{
    volatile uint8 *xram = (volatile uint8 *)PMS_XRAM;
    uint8 value;

    do
    {
        (void)SysMgr_ClearScrResetStatusIfSet();
        /* TC37x Erratum SCR_TC.019: retry SCR XRAM read if SCR reset overlaps it. */
        value = xram[(uint16)(SCR_TIME_XRAM_BASE + offset)];
    } while (SysMgr_ClearScrResetStatusIfSet() != 0u);

    return value;
}

static void SysMgr_WriteScrXramU8(uint16 offset, uint8 value)
{
    volatile uint8 *xram = (volatile uint8 *)PMS_XRAM;

    do
    {
        (void)SysMgr_ClearScrResetStatusIfSet();
        /* TC37x Erratum SCR_TC.019: retry SCR XRAM write if SCR reset overlaps it. */
        xram[(uint16)(SCR_TIME_XRAM_BASE + offset)] = value;
    } while (SysMgr_ClearScrResetStatusIfSet() != 0u);
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

static void SysMgr_StoreU64(uint8 *buffer, uint16 offset, uint64 value)
{
    buffer[offset] = (uint8)((value >> 56u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)((value >> 48u) & 0xFFu);
    buffer[(uint16)(offset + 2u)] = (uint8)((value >> 40u) & 0xFFu);
    buffer[(uint16)(offset + 3u)] = (uint8)((value >> 32u) & 0xFFu);
    buffer[(uint16)(offset + 4u)] = (uint8)((value >> 24u) & 0xFFu);
    buffer[(uint16)(offset + 5u)] = (uint8)((value >> 16u) & 0xFFu);
    buffer[(uint16)(offset + 6u)] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 7u)] = (uint8)(value & 0xFFu);
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
    uint8 *timeData;

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
    SysMgr_StoreU32(buffer, 52u, 0u);
    SysMgr_StoreU32(buffer, 56u, 0u);
    SysMgr_StoreU32(buffer, 60u, 0u);

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
    SysMgr_StoreU32(detail, 132u, 0u);
    SysMgr_StoreU32(detail, 136u, 0u);
    SysMgr_StoreU32(detail, 140u, 0u);
    SysMgr_StoreU32(detail, 144u, 0u);
    SysMgr_StoreU32(detail, 148u, 0u);
    SysMgr_StoreU32(detail, 152u, 0u);
    SysMgr_StoreU32(detail, 156u, 0u);

    timeData = &buffer[SYSMGR_MCUSM_SNAPSHOT_DATA_TIME_OFFSET];
    if (Dem_Cfg_CaptureTimestampTemperatureData(timeData, length,
            DEM_SNAPSHOT_KIND_COMMON) != E_OK)
    {
        return E_NOT_OK;
    }

    *length = SYSMGR_MCUSM_SNAPSHOT_DATA_SIZE;
    return E_OK;
}

Std_ReturnType SysMgr_CapturePmsErrataSnapshotData(
    Dem_EventIdType eventId,
    uint8 *buffer,
    uint16 *length
)
{
    uint16 i;
    uint8 *timeData;
    TimeBase_TimestampSnapshotType timestamp;
    TimeBase_DateTimeType dateTime;

    if ((eventId != DEM_EVENT_ID_PMS_ERRATA_STARTUP) ||
            (buffer == NULL_PTR) ||
            (length == NULL_PTR) ||
            (*length < SYSMGR_PMS_ERRATA_SNAPSHOT_DATA_SIZE))
    {
        return E_NOT_OK;
    }

    for (i = 0u; i < SYSMGR_PMS_ERRATA_SNAPSHOT_DATA_SIZE; i++)
    {
        buffer[i] = 0u;
    }

    SysMgr_StoreU16(buffer, 0u, eventId);
    buffer[2] = SYSMGR_PMS_ERRATA_SNAPSHOT_DATA_VERSION;
    buffer[3] = McuSm_SswStatusData.wakeupFromStandby;
    SysMgr_StoreU32(buffer, 4u, McuSm_LastResetReason);
    SysMgr_StoreU32(buffer, 8u, McuSm_LastResetInformation);
    SysMgr_StoreU32(buffer, 12u, McuSm_SafetyKitFailureMask);
#if (SYSMGR_PMS_ERRATA_FEATURE_ENABLED != 0u)
    SysMgr_StoreU32(buffer, 16u, g_SafetyKitStatus.voltStatus.pmsErrataFailureMask);
    SysMgr_StoreU32(buffer, 20u, g_SafetyKitStatus.voltStatus.pmsErrataStandbyWakeIgnoredMask);
    SysMgr_StoreU32(buffer, 24u, g_SafetyKitStatus.voltStatus.pmsErrataTc007SampleCount);
    SysMgr_StoreU32(buffer, 28u, g_SafetyKitStatus.voltStatus.pmsErrataTc007RefreshTimeoutCount);
    SysMgr_StoreU32(buffer, 32u, g_SafetyKitStatus.voltStatus.pmsErrataCheckedEvrStat);
    SysMgr_StoreU32(buffer, 36u, g_SafetyKitStatus.voltStatus.pmsErrataCheckedEvrMonStat1);
    SysMgr_StoreU32(buffer, 40u, g_SafetyKitStatus.voltStatus.pmsErrataEvrStat);
    SysMgr_StoreU32(buffer, 44u, g_SafetyKitStatus.voltStatus.pmsErrataEvrAdcStat);
    SysMgr_StoreU32(buffer, 48u, g_SafetyKitStatus.voltStatus.pmsErrataEvrMonStat1);
    SysMgr_StoreU32(buffer, 52u, g_SafetyKitStatus.voltStatus.pmsErrataEvrRstCon);
    SysMgr_StoreU32(buffer, 56u, g_SafetyKitStatus.voltStatus.pmsErrataEvrOvMon2);
    SysMgr_StoreU32(buffer, 60u, g_SafetyKitStatus.voltStatus.pmsErrataEvrUvMon2);
#endif
    SysMgr_StoreU32(buffer, 64u, McuSm_SswStatusData.resetType);
    SysMgr_StoreU32(buffer, 68u, McuSm_SswStatusData.resetTrigger);
    SysMgr_StoreU16(buffer, 72u, McuSm_SswStatusData.resetReason);
#if (SYSMGR_PMS_ERRATA_FEATURE_ENABLED != 0u)
    buffer[74] = g_SafetyKitStatus.voltStatus.pmsErrataCheckStatus;
#else
    buffer[74] = 0u;
#endif
    buffer[75] = McuSm_SswStatusData.mcuFwcheckStatus;

    TimeBase_GetTimestampSnapshot(&timestamp);
    SysMgr_StoreU64(buffer, SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET, timestamp.vehicle_time_ns);
    SysMgr_StoreU64(buffer, (uint16)(SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET + 8u), timestamp.utc_time_ns);
    buffer[(uint16)(SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET + 16u)] =
            (timestamp.utc_valid != FALSE) ? 1u : 0u;
    buffer[(uint16)(SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET + 17u)] = timestamp.time_source;
    SysMgr_StoreU32(buffer, (uint16)(SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET + 18u),
            timestamp.sync_status);

    if ((timestamp.utc_valid != FALSE) &&
            (TimeBase_ConvertUtcNsToDateTime(timestamp.utc_time_ns, &dateTime) == E_OK))
    {
        SysMgr_StoreU16(buffer, (uint16)(SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET + 22u),
                dateTime.year);
        buffer[(uint16)(SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET + 24u)] = dateTime.month;
        buffer[(uint16)(SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET + 25u)] = dateTime.day;
        buffer[(uint16)(SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET + 26u)] = dateTime.hour;
        buffer[(uint16)(SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET + 27u)] = dateTime.minute;
        buffer[(uint16)(SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET + 28u)] = dateTime.second;
        SysMgr_StoreU16(buffer, (uint16)(SYSMGR_PMS_ERRATA_SNAPSHOT_TIME_OFFSET + 29u),
                dateTime.millisecond);
    }

    timeData = &buffer[SYSMGR_PMS_ERRATA_SNAPSHOT_COMMON_TIME_OFFSET];
    if (Dem_Cfg_CaptureTimestampTemperatureData(timeData, length,
            DEM_SNAPSHOT_KIND_COMMON) != E_OK)
    {
        return E_NOT_OK;
    }

    *length = SYSMGR_PMS_ERRATA_SNAPSHOT_DATA_SIZE;
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

        /* Interrupts are disabled for the whole GoSleep sequence, so the
         * periodic ASIL_BSW watchdog service never runs. A WriteAll that
         * garbage-collects (two 128 KiB erases) can take hundreds of ms - well
         * past the 1 s watchdog window - so feed both watchdogs here or the ECU
         * resets mid-write (torn record / failed sleep). The watchdogs stay
         * armed; they are only kept fed across this bounded, known-long flash
         * operation. */
        serviceCpuWatchdog();
        serviceSafetyWatchdog();

        if (waitLoops >= SYSMGR_NVM_IDLE_WAIT_LOOP_LIMIT)
        {
            SysMgr_GoSleepFailure(SYSMGR_FAIL_NVM_PRE_WRITEALL_IDLE);
        }
    }

    McuSm_ClearResetDataForCleanSleep();
    SysMgr_ClearScrFaultTriggerData();
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

        /* Same as the pre-WriteAll drain: interrupts are off, and this is the
         * loop that actually carries the WriteAll (and any GC sector switch) to
         * completion, so it must keep both watchdogs fed across the erases. */
        serviceCpuWatchdog();
        serviceSafetyWatchdog();

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

    if(SysMgr_SetClassicRxPinOwnerScr() == FALSE)
    {
        SysMgr_GoSleepFailure(SYSMGR_FAIL_P33_PCSR_SCR_OWNER);
    }

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

    //McuSm_ArmStandbyWakeLatch();
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
    if(SysMgr_ResetDtcProcessed == FALSE)
    {
        boolean scrFaultDetected = SysMgr_HasScrFaultError();
        boolean safetyKitFaultDetected = ((McuSm_SafetyKitFailureMask != 0u) ||
                (McuSm_LastResetReason == MCUSM_RESET_REASON_SAFETYKIT_TEST)) ? TRUE : FALSE;
        Std_ReturnType resetDtcStatus;

        if ((Dem_IsReady() == FALSE) ||
                (Dem_IsDtcSettingEnabled() == FALSE) ||
                (TimeBase_IsUtcRestoredFromNvM() == FALSE))
        {
            return;
        }

        if((0u != McuSm_LastResetReason) || (scrFaultDetected != FALSE) || (safetyKitFaultDetected != FALSE))
        {
            resetDtcStatus = Dem_SetEventStatus(DEM_EVENT_ID_MCUSM_SW_ERROR, DEM_EVENT_STATUS_FAILED);

            if (resetDtcStatus == E_OK)
            {
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
        }
        else
        {
            resetDtcStatus = Dem_SetEventStatus(DEM_EVENT_ID_MCUSM_SW_ERROR, DEM_EVENT_STATUS_PASSED);
        }

        if (resetDtcStatus != E_OK)
        {
            return;
        }

        SysMgr_ResetDtcProcessed = TRUE;

#if (SYSMGR_PMS_ERRATA_FEATURE_ENABLED != 0u)
        if (g_SafetyKitStatus.voltStatus.pmsErrataFailureMask != 0u)
        {
            Dem_SetEventStatus(DEM_EVENT_ID_PMS_ERRATA_STARTUP, DEM_EVENT_STATUS_FAILED);
        }
        else
        {
            Dem_SetEventStatus(DEM_EVENT_ID_PMS_ERRATA_STARTUP, DEM_EVENT_STATUS_PASSED);
        }
#else
        Dem_SetEventStatus(DEM_EVENT_ID_PMS_ERRATA_STARTUP, DEM_EVENT_STATUS_PASSED);
#endif
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
        SysMgr_RunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
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
                SysMgr_EcuState = SYSMGR_GOSLEEP;
            }
        }
        else
        {
            SysMgr_RunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
            SysMgr_EcuState = SYSMGR_RUN;
        }
    }

    if(SYSMGR_GOSLEEP == SysMgr_EcuState)
    {
        if(0u != SysMgr_NoBusActivity)
        {
            SysMgr_GoSleep();
        }
        else
        {
            SysMgr_RunCounter = SYSMGR_BUS_ACTIVITY_TIMEOUT_TICKS;
            SysMgr_EcuState = SYSMGR_RUN;
        }
    }
}

void SysMgr_MainFunction(void)
{
    SysMgr_EcuStateMachine();
    SysMgr_ProcessResetDtc();

    SysMgr_McuTemperature = g_SafetyKitStatus.dieTempStatus.dieTemperatureCore;
    getPmsVoltageMeasurements();
    SysMgr_MainCounter++;
}
