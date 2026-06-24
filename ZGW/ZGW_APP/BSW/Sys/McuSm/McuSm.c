#include "McuSm.h"
#include "IfxScuLbist.h"
#include "IfxCpu_IntrinsicsTasking.h"
#include "IfxCpu_reg.h"
#include "IfxPms_reg.h"
#include "Fls.h"
#include "Fls_Cfg.h"
#include "BSW/Time/TimeBase.h"

uint32 McuSm_AGs[12u];
uint32 McuSm_LastResetReason;
uint32 McuSm_LastResetInformation;
uint32 McuSm_IndexResetHistory;
McuSm_ResetHistory_t McuSm_ResetHistory[20u];
uint8 McuSm_FBL_ResetCounter;
uint8 McuSm_FBL_ProgrammingRequest;
uint8 McuSm_FBL_CommInterface;
uint32 McuSm_SswStartupCounter;
uint32 McuSm_SafetyKitFailureMask;
uint32 McuSm_SafetyKitResetReactionCounter;
uint8 McuSm_SafetyKitResetInhibit;
volatile uint32 McuSm_SafetyKitFwCheckLastSshFail;
volatile McuSm_SswStatusData_t McuSm_SswStatusData;
volatile uint32 McuSm_SafetyKitFwCheckResultMask;
volatile uint32 McuSm_SafetyKitFwCheckSshActualEccd;
volatile uint32 McuSm_SafetyKitFwCheckSshActualFaultsts;
volatile uint32 McuSm_SafetyKitFwCheckSshActualErrinfo;
volatile uint32 McuSm_SafetyKitFwCheckSshExpectedEccd;
volatile uint32 McuSm_SafetyKitFwCheckSshExpectedFaultsts;
volatile uint32 McuSm_SafetyKitFwCheckSshExpectedErrinfo;
volatile uint32 McuSm_SafetyKitFwCheckLastRegFail;
volatile uint32 McuSm_SafetyKitFwCheckRegActual;
volatile uint32 McuSm_SafetyKitFwCheckRegExpected;
volatile uint32 McuSm_SafetyKitFwCheckRegMask;
volatile uint32 McuSm_SafetyKitFwCheckRegResetType;
volatile uint32 McuSm_LastTrapClass;
volatile uint32 McuSm_LastTrapId;
volatile uint32 McuSm_LastTrapCoreId;
volatile uint32 McuSm_LastTrapTAddr;
volatile uint32 McuSm_LastTrapPcxi;
volatile uint32 McuSm_LastTrapFcx;
volatile uint32 McuSm_LastTrapLcx;
volatile uint32 McuSm_LastTrapPsw;
volatile uint32 McuSm_TrapCounter;
volatile uint32 McuSm_EndinitWaitCounter;
volatile uint32 McuSm_EndinitTimeoutCounter;
volatile uint32 McuSm_Trap4Dstr;
volatile uint32 McuSm_Trap4Datr;
volatile uint32 McuSm_Trap4Deadd;
volatile uint32 McuSm_Trap4Diear;
volatile uint32 McuSm_Trap4Dietr;
volatile uint32 McuSm_Trap4Piear;
volatile uint32 McuSm_Trap4Pietr;
volatile uint32 McuSm_Trap4ErrorAddress;
volatile uint32 McuSm_Trap4ZeroFillReaction;
volatile uint32 McuSm_Trap4ZeroFillReactionCounter;
volatile uint32 McuSm_Trap7Dstr;
volatile uint32 McuSm_Trap7Datr;
volatile uint32 McuSm_Trap7Deadd;
volatile uint32 McuSm_Trap7Diear;
volatile uint32 McuSm_Trap7Dietr;
volatile uint32 McuSm_Trap7Piear;
volatile uint32 McuSm_Trap7Pietr;
volatile uint32 McuSm_Trap7AgRaw[12u];
volatile uint32 McuSm_Trap7AgMasked[12u];
volatile uint32 McuSm_Trap7AgRstRsn;
volatile uint32 McuSm_Trap7AgRstInfo;
volatile uint32 McuSm_Trap7DomErrAddr[16u];
volatile uint32 McuSm_Trap7DomErr[16u];
volatile uint32 McuSm_Trap7DomPestat;
volatile uint32 McuSm_Trap7DomTidstat;
volatile uint32 McuSm_Trap7DomActiveSci;
volatile uint32 McuSm_Trap7DomActiveErrAddr;
volatile uint32 McuSm_Trap7DomActiveErr;
uint8 McuSm_Trap4ScrRtcRecord[SCR_TIME_RECORD_LENGTH];
volatile uint8 McuSm_Trap4ScrRtcRecordValid;
volatile uint32 McuSm_Trap4ScrRtcRecordCounter;
volatile uint32 McuSm_DFlashRecoveryRequest;
volatile uint32 McuSm_DFlashRecoveryInfo;
volatile uint32 McuSm_DFlashRecoveryCounter;
volatile uint32 McuSm_DFlashRecoveryAttemptCounter;
volatile uint32 McuSm_DFlashRecoverySuppressCounter;
volatile uint32 McuSm_DFlashRecoveryLastFeeAccessKind;
volatile uint32 McuSm_DFlashRecoveryLastFeePhysicalAddress;
volatile uint32 McuSm_ScrStateStoreCounter;
volatile uint32 McuSm_ScrStateRestoreCounter;
volatile uint32 McuSm_ScrStateInvalidCounter;
volatile uint32 McuSm_ResetHookPerformCounter;

/* NOT retained: the BusMPU hardware is reset by the hardware reset, so these
 * bookkeeping copies must be cleared on every startup so re-configuration runs
 * again. */
volatile uint32 McuSm_BusMpuConfigured;
volatile uint32 McuSm_BusMpuWriteAccessMaskA;
volatile uint32 McuSm_BusMpuWriteAccessMaskB;

void McuSm_InitializeBusMpu(void);
void McuSm_PerformResetHook(uint32 resetReason, uint32 resetInformation);
void McuSm_TRAP2(IfxCpu_Trap trapInfo);
void McuSm_TRAP3(IfxCpu_Trap trapInfo);
void McuSm_TRAP4(IfxCpu_Trap trapInfo);
void McuSm_TRAP7(IfxCpu_Trap trapInfo);

volatile uint8 debugvar = 0;

#define MCUSM_ENDINIT_WAIT_LIMIT 100000u
#define MCUSM_TRAP4_REACTION_NONE 0u
#define MCUSM_TRAP4_REACTION_XRAM 1u
#define MCUSM_DFLASH_RECOVERY_MAX_ATTEMPTS 2u
#define MCUSM_POWERON_RESET_MASK \
        ((uint32)((IFX_SCU_RSTSTAT_STBYR_MSK << IFX_SCU_RSTSTAT_STBYR_OFF) | \
                  (IFX_SCU_RSTSTAT_SWD_MSK << IFX_SCU_RSTSTAT_SWD_OFF) | \
                  (IFX_SCU_RSTSTAT_EVR33_MSK << IFX_SCU_RSTSTAT_EVR33_OFF) | \
                  (IFX_SCU_RSTSTAT_EVRC_MSK << IFX_SCU_RSTSTAT_EVRC_OFF) | \
                  (IFX_SCU_RSTSTAT_PORST_MSK << IFX_SCU_RSTSTAT_PORST_OFF)))
#define MCUSM_WARM_RESET_MASK \
        ((uint32)(IFXSCURCU_APPLICATIONRESET_MASK | \
                  (IFX_SCU_RSTSTAT_CB0_MSK << IFX_SCU_RSTSTAT_CB0_OFF) | \
                  (IFX_SCU_RSTSTAT_CB1_MSK << IFX_SCU_RSTSTAT_CB1_OFF) | \
                  (IFX_SCU_RSTSTAT_CB3_MSK << IFX_SCU_RSTSTAT_CB3_OFF) | \
                  (IFX_SCU_RSTSTAT_LBTERM_MSK << IFX_SCU_RSTSTAT_LBTERM_OFF)))
#define MCUSM_BUSMPU_FULL_ACCESS_MASK       0xFFFFFFFFu
#define MCUSM_BUSMPU_NO_ACCESS_MASK         0x00000000u
#define MCUSM_BUSMPU_WRITE_BASE_MASK_A      0x10001777u
/* DMA resource partitions map to ACCENA bits 0, 4, 8, and 12. */
#define MCUSM_BUSMPU_DMA_WRITE_MASK_A       0x00001111u
#define MCUSM_BUSMPU_WRITE_NON_DMA_MASK_A   (MCUSM_BUSMPU_WRITE_BASE_MASK_A & (~MCUSM_BUSMPU_DMA_WRITE_MASK_A))
#define MCUSM_BUSMPU_DSPR0_LOWER_ADDRESS    0x70000000u
#define MCUSM_BUSMPU_DSPR0_UPPER_ADDRESS    0x7003BFE0u
#define MCUSM_BUSMPU_DSPR1_LOWER_ADDRESS    0x60000000u
#define MCUSM_BUSMPU_DSPR1_UPPER_ADDRESS    0x6003BFE0u
#define MCUSM_BUSMPU_DSPR2_LOWER_ADDRESS    0x50000000u
#define MCUSM_BUSMPU_DSPR2_UPPER_ADDRESS    0x50017FE0u
#define MCUSM_BUSMPU_PSPR0_LOWER_ADDRESS    0x70100000u
#define MCUSM_BUSMPU_PSPR0_UPPER_ADDRESS    0x7010FFE0u
#define MCUSM_BUSMPU_PSPR1_LOWER_ADDRESS    0x60100000u
#define MCUSM_BUSMPU_PSPR1_UPPER_ADDRESS    0x6010FFE0u
#define MCUSM_BUSMPU_PSPR2_LOWER_ADDRESS    0x50100000u
#define MCUSM_BUSMPU_PSPR2_UPPER_ADDRESS    0x5010FFE0u
#define MCUSM_BUSMPU_DLMU0_LOWER_ADDRESS    0xB0000000u
#define MCUSM_BUSMPU_DLMU0_UPPER_ADDRESS    0xB000FFE0u
#define MCUSM_BUSMPU_DLMU1_LOWER_ADDRESS    0xB0010000u
#define MCUSM_BUSMPU_DLMU1_UPPER_ADDRESS    0xB001FFE0u
#define MCUSM_BUSMPU_DLMU2_LOWER_ADDRESS    0xB0020000u
#define MCUSM_BUSMPU_DLMU2_UPPER_ADDRESS    0xB002FFE0u
#ifndef MCUSM_DEBUG_HALT_BEFORE_RESET
#define MCUSM_DEBUG_HALT_BEFORE_RESET 0u
#endif

extern volatile uint32 Fee_DebugLastFlashAccessKind;
extern volatile uint32 Fee_DebugLastFlashAccessPhysicalAddress;

typedef struct
{
    uint32 ags[12u];
    uint32 lastResetReason;
    uint32 lastResetInformation;
    uint32 indexResetHistory;
    McuSm_ResetHistory_t resetHistory[20u];
    uint8 fblResetCounter;
    uint8 fblProgrammingRequest;
    uint8 fblCommInterface;
    uint32 sswStartupCounter;
    uint32 safetyKitFailureMask;
    uint32 safetyKitResetReactionCounter;
    uint8 safetyKitResetInhibit;
    uint32 safetyKitFwCheckLastSshFail;
    McuSm_SswStatusData_t sswStatusData;
    uint32 safetyKitFwCheckResultMask;
    uint32 safetyKitFwCheckSshActualEccd;
    uint32 safetyKitFwCheckSshActualFaultsts;
    uint32 safetyKitFwCheckSshActualErrinfo;
    uint32 safetyKitFwCheckSshExpectedEccd;
    uint32 safetyKitFwCheckSshExpectedFaultsts;
    uint32 safetyKitFwCheckSshExpectedErrinfo;
    uint32 safetyKitFwCheckLastRegFail;
    uint32 safetyKitFwCheckRegActual;
    uint32 safetyKitFwCheckRegExpected;
    uint32 safetyKitFwCheckRegMask;
    uint32 safetyKitFwCheckRegResetType;
    uint32 lastTrapClass;
    uint32 lastTrapId;
    uint32 lastTrapCoreId;
    uint32 lastTrapTAddr;
    uint32 lastTrapPcxi;
    uint32 lastTrapFcx;
    uint32 lastTrapLcx;
    uint32 lastTrapPsw;
    uint32 trapCounter;
    uint32 endinitWaitCounter;
    uint32 endinitTimeoutCounter;
    uint32 trap4Dstr;
    uint32 trap4Datr;
    uint32 trap4Deadd;
    uint32 trap4Diear;
    uint32 trap4Dietr;
    uint32 trap4Piear;
    uint32 trap4Pietr;
    uint32 trap4ErrorAddress;
    uint32 trap4ZeroFillReaction;
    uint32 trap4ZeroFillReactionCounter;
    uint32 trap7Dstr;
    uint32 trap7Datr;
    uint32 trap7Deadd;
    uint32 trap7Diear;
    uint32 trap7Dietr;
    uint32 trap7Piear;
    uint32 trap7Pietr;
    uint32 trap7AgRaw[12u];
    uint32 trap7AgMasked[12u];
    uint32 trap7AgRstRsn;
    uint32 trap7AgRstInfo;
    uint8 trap4ScrRtcRecord[SCR_TIME_RECORD_LENGTH];
    uint8 trap4ScrRtcRecordValid;
    uint32 trap4ScrRtcRecordCounter;
    uint32 dFlashRecoveryRequest;
    uint32 dFlashRecoveryInfo;
    uint32 dFlashRecoveryCounter;
    uint32 dFlashRecoveryAttemptCounter;
    uint32 dFlashRecoverySuppressCounter;
    uint32 dFlashRecoveryLastFeeAccessKind;
    uint32 dFlashRecoveryLastFeePhysicalAddress;
} McuSm_RetainedStateType;

typedef char McuSm_RetainedStateFitsScrXram[
        (sizeof(McuSm_RetainedStateType) <= SCR_MCUSM_PAYLOAD_LENGTH) ? 1 : -1];

static McuSm_RetainedStateType McuSm_RetainedStateWork;

static boolean McuSm_IsAddressInRange(uint32 address, uint32 rangeStart, uint32 rangeEnd);
static boolean McuSm_IsDFlashAddress(uint32 address);
static boolean McuSm_IsScrXramAddress(uint32 address);
static void McuSm_ZeroFillRange(uint32 rangeStart, uint32 rangeEnd);
static uint32 McuSm_LoadU32FromBuffer(const uint8 *buffer, uint16 offset);
static volatile uint8 *McuSm_GetScrRetainedXram(void);
static void McuSm_ScrStoreU8(uint16 offset, uint8 value);
static uint8 McuSm_ScrLoadU8(uint16 offset);
static void McuSm_ScrStoreU16(uint16 offset, uint16 value);
static uint16 McuSm_ScrLoadU16(uint16 offset);
static void McuSm_ScrStoreU32(uint16 offset, uint32 value);
static uint32 McuSm_ScrLoadU32(uint16 offset);
static uint32 McuSm_CalculateChecksum(const uint8 *data, uint16 length);
static void McuSm_CaptureRetainedState(McuSm_RetainedStateType *state);
static void McuSm_ApplyRetainedState(const McuSm_RetainedStateType *state);
static void McuSm_CaptureScrRtcRecord(void);
static uint32 McuSm_GetTrap4ErrorAddress(IfxCpu_Trap trapInfo);
static uint32 McuSm_Trap4ZeroFillIfRecoverable(uint32 errorAddress);
static void McuSm_RequestDFlashRecoveryForAddress(uint32 recoveryInfo, uint32 physicalAddress);
static void McuSm_CaptureTrap7DomError(void);
static void McuSm_RequestDFlashRecoveryFromFee(void);

#define MCUSM_RECORD_TRAP(trapClass, trapInfo)                 \
        do                                                         \
        {                                                          \
            Ifx_CPU_CORE_ID mcuSmCoreIdReg;                        \
            mcuSmCoreIdReg.U = __mfcr(CPU_CORE_ID);                \
            McuSm_LastTrapClass = (trapClass);                     \
            McuSm_LastTrapId = (trapInfo).tId;                     \
            McuSm_LastTrapCoreId = mcuSmCoreIdReg.B.CORE_ID;       \
            McuSm_LastTrapTAddr = (trapInfo).tAddr;                \
            McuSm_LastTrapPcxi = __mfcr(CPU_PCXI);                 \
            McuSm_LastTrapFcx = __mfcr(CPU_FCX);                   \
            McuSm_LastTrapLcx = __mfcr(CPU_LCX);                   \
            McuSm_LastTrapPsw = __mfcr(CPU_PSW);                   \
            McuSm_TrapCounter++;                                   \
        } while (0)

void McuSm_ClearResetDtcTriggerData(void)
{
    McuSm_LastResetReason = 0u;
    McuSm_LastResetInformation = 0u;
    McuSm_SafetyKitFailureMask = 0u;
    McuSm_SafetyKitResetInhibit = 0u;
}

void McuSm_PerformResetHook(uint32 resetReason, uint32 resetInformation)
{
    uint32 cpuDeadd;

    /* Do not clear SMU alarms from the trap/reset path. This can interrupt an
     * ENDINIT-protected sequence and re-enter safety ENDINIT handling. */

    if ((resetReason == 7u) &&
            ((resetInformation == 17u) || (resetInformation == 20u)))
    {
        cpuDeadd = __mfcr(CPU_DEADD);
        if (McuSm_IsDFlashAddress(cpuDeadd) != FALSE)
        {
            McuSm_RequestDFlashRecoveryForAddress(
                    cpuDeadd & 0x00FFFFFFu,
                    cpuDeadd);
        }
        else
        {
            McuSm_RequestDFlashRecoveryFromFee();
        }
    }

    if(McuSm_IndexResetHistory >= 20u)
    {
        McuSm_IndexResetHistory = 0u;
    }

    McuSm_LastResetReason = resetReason;
    McuSm_LastResetInformation = resetInformation;
    McuSm_ResetHistory[McuSm_IndexResetHistory].reason = resetReason;
    McuSm_ResetHistory[McuSm_IndexResetHistory].information = resetInformation;
    McuSm_IndexResetHistory++;
    McuSm_FBL_ResetCounter += 1u;

    if(McuSm_IndexResetHistory >= 20u)
    {
        McuSm_IndexResetHistory = 0u;
    }
    else
    {
        /* Do nothing. */
    }

    McuSm_SaveRetainedStateToScr();
    McuSm_ResetHookPerformCounter++;

    IfxScuRcu_performReset(IfxScuRcu_ResetType_application, 0u);

}

static boolean McuSm_IsAddressInRange(uint32 address, uint32 rangeStart, uint32 rangeEnd)
{
    return ((address >= rangeStart) && (address < rangeEnd)) ? TRUE : FALSE;
}

static boolean McuSm_IsDFlashAddress(uint32 address)
{
    return McuSm_IsAddressInRange(
            address,
            FLS_DFLASH0_BASE_ADDRESS,
            FLS_DFLASH0_BASE_ADDRESS + FLS_DFLASH0_TOTAL_SIZE);
}

static void McuSm_RequestDFlashRecoveryForAddress(uint32 recoveryInfo, uint32 physicalAddress)
{
    if (McuSm_DFlashRecoveryRequest != MCUSM_DFLASH_RECOVERY_MAGIC)
    {
        McuSm_DFlashRecoveryAttemptCounter = 0u;
    }

    McuSm_DFlashRecoveryInfo = recoveryInfo;
    McuSm_DFlashRecoveryLastFeeAccessKind = Fee_DebugLastFlashAccessKind;
    McuSm_DFlashRecoveryLastFeePhysicalAddress = physicalAddress;
    McuSm_DFlashRecoveryRequest = MCUSM_DFLASH_RECOVERY_MAGIC;
    McuSm_DFlashRecoveryCounter++;
}

void McuSm_RequestDFlashRecovery(uint32 recoveryInfo)
{
    McuSm_RequestDFlashRecoveryForAddress(
            recoveryInfo,
            Fee_DebugLastFlashAccessPhysicalAddress);
}

boolean McuSm_IsDFlashRecoveryRequested(void)
{
    if (McuSm_DFlashRecoveryRequest == MCUSM_DFLASH_RECOVERY_MAGIC)
    {
        return McuSm_IsDFlashAddress(McuSm_DFlashRecoveryLastFeePhysicalAddress);
    }

    /*
     * Loop breaker for reset paths where the SMU NMI was raised by the
     * DFlash bus-error alarm before the explicit recovery marker could be
     * trusted on the next boot.  Re-scanning Fee after this reset would repeat
     * the same unsafe DFlash read and stay in cyclic resets.
     */
    if ((McuSm_LastResetReason == 7u) &&
            ((McuSm_LastResetInformation == 17u) || (McuSm_LastResetInformation == 20u)) &&
            (McuSm_IsDFlashAddress(McuSm_DFlashRecoveryLastFeePhysicalAddress) != FALSE))
    {
        return TRUE;
    }

    return FALSE;
}

void McuSm_ClearDFlashRecoveryRequest(void)
{
    McuSm_DFlashRecoveryRequest = 0u;
    McuSm_DFlashRecoveryInfo = 0u;
    McuSm_DFlashRecoveryAttemptCounter = 0u;
    McuSm_DFlashRecoveryLastFeeAccessKind = 0u;
    McuSm_DFlashRecoveryLastFeePhysicalAddress = 0u;
}

boolean McuSm_BeginDFlashRecoveryAttempt(void)
{
    if (McuSm_IsDFlashRecoveryRequested() == FALSE)
    {
        return FALSE;
    }

    if (McuSm_DFlashRecoveryAttemptCounter >= MCUSM_DFLASH_RECOVERY_MAX_ATTEMPTS)
    {
        McuSm_DFlashRecoverySuppressCounter++;
        return FALSE;
    }

    McuSm_DFlashRecoveryAttemptCounter++;
    return TRUE;
}

static void McuSm_RequestDFlashRecoveryFromFee(void)
{
    if (McuSm_IsDFlashAddress(Fee_DebugLastFlashAccessPhysicalAddress) != FALSE)
    {
        McuSm_RequestDFlashRecovery(
                (Fee_DebugLastFlashAccessKind << 24u) |
                (Fee_DebugLastFlashAccessPhysicalAddress & 0x00FFFFFFu));
    }
}

static boolean McuSm_IsScrXramAddress(uint32 address)
{
    uint32 const xramStart = (uint32)PMS_XRAM;
    uint32 const xramEnd = xramStart + (uint32)PMS_XRAM_SIZE;

    return McuSm_IsAddressInRange(address, xramStart, xramEnd);
}

static void McuSm_ZeroFillRange(uint32 rangeStart, uint32 rangeEnd)
{
    volatile uint32 *address = (volatile uint32 *)rangeStart;

    while ((uint32)address < rangeEnd)
    {
        *address = 0u;
        address++;
    }
}

static uint32 McuSm_LoadU32FromBuffer(const uint8 *buffer, uint16 offset)
{
    return ((uint32)buffer[offset] << 24u) |
            ((uint32)buffer[(uint16)(offset + 1u)] << 16u) |
            ((uint32)buffer[(uint16)(offset + 2u)] << 8u) |
            (uint32)buffer[(uint16)(offset + 3u)];
}

static volatile uint8 *McuSm_GetScrRetainedXram(void)
{
    return &((volatile uint8 *)PMS_XRAM)[SCR_MCUSM_XRAM_BASE];
}

static void McuSm_ScrStoreU8(uint16 offset, uint8 value)
{
    volatile uint8 *record = McuSm_GetScrRetainedXram();

    record[offset] = value;
}

static uint8 McuSm_ScrLoadU8(uint16 offset)
{
    volatile uint8 *record = McuSm_GetScrRetainedXram();

    return record[offset];
}

static void McuSm_ScrStoreU16(uint16 offset, uint16 value)
{
    McuSm_ScrStoreU8(offset, (uint8)(value >> 8u));
    McuSm_ScrStoreU8((uint16)(offset + 1u), (uint8)value);
}

static uint16 McuSm_ScrLoadU16(uint16 offset)
{
    return (uint16)(((uint16)McuSm_ScrLoadU8(offset) << 8u) |
            (uint16)McuSm_ScrLoadU8((uint16)(offset + 1u)));
}

static void McuSm_ScrStoreU32(uint16 offset, uint32 value)
{
    McuSm_ScrStoreU8(offset, (uint8)(value >> 24u));
    McuSm_ScrStoreU8((uint16)(offset + 1u), (uint8)(value >> 16u));
    McuSm_ScrStoreU8((uint16)(offset + 2u), (uint8)(value >> 8u));
    McuSm_ScrStoreU8((uint16)(offset + 3u), (uint8)value);
}

static uint32 McuSm_ScrLoadU32(uint16 offset)
{
    return ((uint32)McuSm_ScrLoadU8(offset) << 24u) |
            ((uint32)McuSm_ScrLoadU8((uint16)(offset + 1u)) << 16u) |
            ((uint32)McuSm_ScrLoadU8((uint16)(offset + 2u)) << 8u) |
            (uint32)McuSm_ScrLoadU8((uint16)(offset + 3u));
}

static uint32 McuSm_CalculateChecksum(const uint8 *data, uint16 length)
{
    uint16 index;
    uint32 checksum = 0x811C9DC5u;

    if (data == NULL_PTR)
    {
        return 0u;
    }

    for (index = 0u; index < length; index++)
    {
        checksum ^= data[index];
        checksum *= 16777619u;
    }

    return checksum;
}

static void McuSm_CaptureRetainedState(McuSm_RetainedStateType *state)
{
    uint8 index;

    if (state == NULL_PTR)
    {
        return;
    }

    for (index = 0u; index < 12u; index++)
    {
        state->ags[index] = McuSm_AGs[index];
    }

    state->lastResetReason = McuSm_LastResetReason;
    state->lastResetInformation = McuSm_LastResetInformation;
    state->indexResetHistory = McuSm_IndexResetHistory;

    for (index = 0u; index < 20u; index++)
    {
        state->resetHistory[index] = McuSm_ResetHistory[index];
    }

    state->fblResetCounter = McuSm_FBL_ResetCounter;
    state->fblProgrammingRequest = McuSm_FBL_ProgrammingRequest;
    state->fblCommInterface = McuSm_FBL_CommInterface;
    state->sswStartupCounter = McuSm_SswStartupCounter;
    state->safetyKitFailureMask = McuSm_SafetyKitFailureMask;
    state->safetyKitResetReactionCounter = McuSm_SafetyKitResetReactionCounter;
    state->safetyKitResetInhibit = McuSm_SafetyKitResetInhibit;
    state->safetyKitFwCheckLastSshFail = McuSm_SafetyKitFwCheckLastSshFail;
    state->sswStatusData = McuSm_SswStatusData;
    state->safetyKitFwCheckResultMask = McuSm_SafetyKitFwCheckResultMask;
    state->safetyKitFwCheckSshActualEccd = McuSm_SafetyKitFwCheckSshActualEccd;
    state->safetyKitFwCheckSshActualFaultsts = McuSm_SafetyKitFwCheckSshActualFaultsts;
    state->safetyKitFwCheckSshActualErrinfo = McuSm_SafetyKitFwCheckSshActualErrinfo;
    state->safetyKitFwCheckSshExpectedEccd = McuSm_SafetyKitFwCheckSshExpectedEccd;
    state->safetyKitFwCheckSshExpectedFaultsts = McuSm_SafetyKitFwCheckSshExpectedFaultsts;
    state->safetyKitFwCheckSshExpectedErrinfo = McuSm_SafetyKitFwCheckSshExpectedErrinfo;
    state->safetyKitFwCheckLastRegFail = McuSm_SafetyKitFwCheckLastRegFail;
    state->safetyKitFwCheckRegActual = McuSm_SafetyKitFwCheckRegActual;
    state->safetyKitFwCheckRegExpected = McuSm_SafetyKitFwCheckRegExpected;
    state->safetyKitFwCheckRegMask = McuSm_SafetyKitFwCheckRegMask;
    state->safetyKitFwCheckRegResetType = McuSm_SafetyKitFwCheckRegResetType;
    state->lastTrapClass = McuSm_LastTrapClass;
    state->lastTrapId = McuSm_LastTrapId;
    state->lastTrapCoreId = McuSm_LastTrapCoreId;
    state->lastTrapTAddr = McuSm_LastTrapTAddr;
    state->lastTrapPcxi = McuSm_LastTrapPcxi;
    state->lastTrapFcx = McuSm_LastTrapFcx;
    state->lastTrapLcx = McuSm_LastTrapLcx;
    state->lastTrapPsw = McuSm_LastTrapPsw;
    state->trapCounter = McuSm_TrapCounter;
    state->endinitWaitCounter = McuSm_EndinitWaitCounter;
    state->endinitTimeoutCounter = McuSm_EndinitTimeoutCounter;
    state->trap4Dstr = McuSm_Trap4Dstr;
    state->trap4Datr = McuSm_Trap4Datr;
    state->trap4Deadd = McuSm_Trap4Deadd;
    state->trap4Diear = McuSm_Trap4Diear;
    state->trap4Dietr = McuSm_Trap4Dietr;
    state->trap4Piear = McuSm_Trap4Piear;
    state->trap4Pietr = McuSm_Trap4Pietr;
    state->trap4ErrorAddress = McuSm_Trap4ErrorAddress;
    state->trap4ZeroFillReaction = McuSm_Trap4ZeroFillReaction;
    state->trap4ZeroFillReactionCounter = McuSm_Trap4ZeroFillReactionCounter;
    state->trap7Dstr = McuSm_Trap7Dstr;
    state->trap7Datr = McuSm_Trap7Datr;
    state->trap7Deadd = McuSm_Trap7Deadd;
    state->trap7Diear = McuSm_Trap7Diear;
    state->trap7Dietr = McuSm_Trap7Dietr;
    state->trap7Piear = McuSm_Trap7Piear;
    state->trap7Pietr = McuSm_Trap7Pietr;

    for (index = 0u; index < 12u; index++)
    {
        state->trap7AgRaw[index] = McuSm_Trap7AgRaw[index];
        state->trap7AgMasked[index] = McuSm_Trap7AgMasked[index];
    }

    state->trap7AgRstRsn = McuSm_Trap7AgRstRsn;
    state->trap7AgRstInfo = McuSm_Trap7AgRstInfo;

    for (index = 0u; index < SCR_TIME_RECORD_LENGTH; index++)
    {
        state->trap4ScrRtcRecord[index] = McuSm_Trap4ScrRtcRecord[index];
    }

    state->trap4ScrRtcRecordValid = McuSm_Trap4ScrRtcRecordValid;
    state->trap4ScrRtcRecordCounter = McuSm_Trap4ScrRtcRecordCounter;
    state->dFlashRecoveryRequest = McuSm_DFlashRecoveryRequest;
    state->dFlashRecoveryInfo = McuSm_DFlashRecoveryInfo;
    state->dFlashRecoveryCounter = McuSm_DFlashRecoveryCounter;
    state->dFlashRecoveryAttemptCounter = McuSm_DFlashRecoveryAttemptCounter;
    state->dFlashRecoverySuppressCounter = McuSm_DFlashRecoverySuppressCounter;
    state->dFlashRecoveryLastFeeAccessKind = McuSm_DFlashRecoveryLastFeeAccessKind;
    state->dFlashRecoveryLastFeePhysicalAddress = McuSm_DFlashRecoveryLastFeePhysicalAddress;
}

static void McuSm_ApplyRetainedState(const McuSm_RetainedStateType *state)
{
    uint8 index;

    if (state == NULL_PTR)
    {
        return;
    }

    for (index = 0u; index < 12u; index++)
    {
        McuSm_AGs[index] = state->ags[index];
    }

    McuSm_LastResetReason = state->lastResetReason;
    McuSm_LastResetInformation = state->lastResetInformation;
    McuSm_IndexResetHistory = state->indexResetHistory;
    if (McuSm_IndexResetHistory >= 20u)
    {
        McuSm_IndexResetHistory = 0u;
    }

    for (index = 0u; index < 20u; index++)
    {
        McuSm_ResetHistory[index] = state->resetHistory[index];
    }

    McuSm_FBL_ResetCounter = state->fblResetCounter;
    McuSm_FBL_ProgrammingRequest = state->fblProgrammingRequest;
    McuSm_FBL_CommInterface = state->fblCommInterface;
    McuSm_SswStartupCounter = state->sswStartupCounter;
    McuSm_SafetyKitFailureMask = state->safetyKitFailureMask;
    McuSm_SafetyKitResetReactionCounter = state->safetyKitResetReactionCounter;
    McuSm_SafetyKitResetInhibit = state->safetyKitResetInhibit;
    McuSm_SafetyKitFwCheckLastSshFail = state->safetyKitFwCheckLastSshFail;
    McuSm_SswStatusData = state->sswStatusData;
    McuSm_SafetyKitFwCheckResultMask = state->safetyKitFwCheckResultMask;
    McuSm_SafetyKitFwCheckSshActualEccd = state->safetyKitFwCheckSshActualEccd;
    McuSm_SafetyKitFwCheckSshActualFaultsts = state->safetyKitFwCheckSshActualFaultsts;
    McuSm_SafetyKitFwCheckSshActualErrinfo = state->safetyKitFwCheckSshActualErrinfo;
    McuSm_SafetyKitFwCheckSshExpectedEccd = state->safetyKitFwCheckSshExpectedEccd;
    McuSm_SafetyKitFwCheckSshExpectedFaultsts = state->safetyKitFwCheckSshExpectedFaultsts;
    McuSm_SafetyKitFwCheckSshExpectedErrinfo = state->safetyKitFwCheckSshExpectedErrinfo;
    McuSm_SafetyKitFwCheckLastRegFail = state->safetyKitFwCheckLastRegFail;
    McuSm_SafetyKitFwCheckRegActual = state->safetyKitFwCheckRegActual;
    McuSm_SafetyKitFwCheckRegExpected = state->safetyKitFwCheckRegExpected;
    McuSm_SafetyKitFwCheckRegMask = state->safetyKitFwCheckRegMask;
    McuSm_SafetyKitFwCheckRegResetType = state->safetyKitFwCheckRegResetType;
    McuSm_LastTrapClass = state->lastTrapClass;
    McuSm_LastTrapId = state->lastTrapId;
    McuSm_LastTrapCoreId = state->lastTrapCoreId;
    McuSm_LastTrapTAddr = state->lastTrapTAddr;
    McuSm_LastTrapPcxi = state->lastTrapPcxi;
    McuSm_LastTrapFcx = state->lastTrapFcx;
    McuSm_LastTrapLcx = state->lastTrapLcx;
    McuSm_LastTrapPsw = state->lastTrapPsw;
    McuSm_TrapCounter = state->trapCounter;
    McuSm_EndinitWaitCounter = state->endinitWaitCounter;
    McuSm_EndinitTimeoutCounter = state->endinitTimeoutCounter;
    McuSm_Trap4Dstr = state->trap4Dstr;
    McuSm_Trap4Datr = state->trap4Datr;
    McuSm_Trap4Deadd = state->trap4Deadd;
    McuSm_Trap4Diear = state->trap4Diear;
    McuSm_Trap4Dietr = state->trap4Dietr;
    McuSm_Trap4Piear = state->trap4Piear;
    McuSm_Trap4Pietr = state->trap4Pietr;
    McuSm_Trap4ErrorAddress = state->trap4ErrorAddress;
    McuSm_Trap4ZeroFillReaction = state->trap4ZeroFillReaction;
    McuSm_Trap4ZeroFillReactionCounter = state->trap4ZeroFillReactionCounter;
    McuSm_Trap7Dstr = state->trap7Dstr;
    McuSm_Trap7Datr = state->trap7Datr;
    McuSm_Trap7Deadd = state->trap7Deadd;
    McuSm_Trap7Diear = state->trap7Diear;
    McuSm_Trap7Dietr = state->trap7Dietr;
    McuSm_Trap7Piear = state->trap7Piear;
    McuSm_Trap7Pietr = state->trap7Pietr;

    for (index = 0u; index < 12u; index++)
    {
        McuSm_Trap7AgRaw[index] = state->trap7AgRaw[index];
        McuSm_Trap7AgMasked[index] = state->trap7AgMasked[index];
    }

    McuSm_Trap7AgRstRsn = state->trap7AgRstRsn;
    McuSm_Trap7AgRstInfo = state->trap7AgRstInfo;

    for (index = 0u; index < SCR_TIME_RECORD_LENGTH; index++)
    {
        McuSm_Trap4ScrRtcRecord[index] = state->trap4ScrRtcRecord[index];
    }

    McuSm_Trap4ScrRtcRecordValid = state->trap4ScrRtcRecordValid;
    McuSm_Trap4ScrRtcRecordCounter = state->trap4ScrRtcRecordCounter;
    McuSm_DFlashRecoveryRequest = state->dFlashRecoveryRequest;
    McuSm_DFlashRecoveryInfo = state->dFlashRecoveryInfo;
    McuSm_DFlashRecoveryCounter = state->dFlashRecoveryCounter;
    McuSm_DFlashRecoveryAttemptCounter = state->dFlashRecoveryAttemptCounter;
    McuSm_DFlashRecoverySuppressCounter = state->dFlashRecoverySuppressCounter;
    McuSm_DFlashRecoveryLastFeeAccessKind = state->dFlashRecoveryLastFeeAccessKind;
    McuSm_DFlashRecoveryLastFeePhysicalAddress = state->dFlashRecoveryLastFeePhysicalAddress;
}

void McuSm_SaveRetainedStateToScr(void)
{
    const uint8 *payload;
    uint16 payloadLength;
    uint16 index;

    McuSm_CaptureRetainedState(&McuSm_RetainedStateWork);

    payload = (const uint8 *)(const void *)&McuSm_RetainedStateWork;
    payloadLength = (uint16)sizeof(McuSm_RetainedStateType);

    McuSm_ScrStoreU8(SCR_MCUSM_OFFSET_VALID, 0u);

    for (index = 0u; index < payloadLength; index++)
    {
        McuSm_ScrStoreU8((uint16)(SCR_MCUSM_OFFSET_PAYLOAD + index), payload[index]);
    }

    McuSm_ScrStoreU32(SCR_MCUSM_OFFSET_MAGIC, SCR_MCUSM_MAGIC);
    McuSm_ScrStoreU8(SCR_MCUSM_OFFSET_VERSION, SCR_MCUSM_VERSION);
    McuSm_ScrStoreU16(SCR_MCUSM_OFFSET_PAYLOAD_LENGTH, payloadLength);
    McuSm_ScrStoreU32(
            SCR_MCUSM_OFFSET_CHECKSUM,
            McuSm_CalculateChecksum(payload, payloadLength));

    __dsync();
    McuSm_ScrStoreU8(SCR_MCUSM_OFFSET_VALID, SCR_MCUSM_VALID);
    McuSm_ScrStateStoreCounter++;
}

boolean McuSm_RestoreRetainedStateFromScr(void)
{
    uint8 *payload;
    uint16 payloadLength;
    uint16 index;
    uint32 checksum;

    if ((McuSm_ScrLoadU32(SCR_MCUSM_OFFSET_MAGIC) != SCR_MCUSM_MAGIC) ||
            (McuSm_ScrLoadU8(SCR_MCUSM_OFFSET_VERSION) != SCR_MCUSM_VERSION) ||
            (McuSm_ScrLoadU8(SCR_MCUSM_OFFSET_VALID) != SCR_MCUSM_VALID))
    {
        McuSm_ScrStateInvalidCounter++;
        return FALSE;
    }

    payloadLength = McuSm_ScrLoadU16(SCR_MCUSM_OFFSET_PAYLOAD_LENGTH);
    if ((payloadLength != (uint16)sizeof(McuSm_RetainedStateType)) ||
            (payloadLength > SCR_MCUSM_PAYLOAD_LENGTH))
    {
        McuSm_ScrStateInvalidCounter++;
        return FALSE;
    }

    payload = (uint8 *)(void *)&McuSm_RetainedStateWork;
    for (index = 0u; index < payloadLength; index++)
    {
        payload[index] = McuSm_ScrLoadU8((uint16)(SCR_MCUSM_OFFSET_PAYLOAD + index));
    }

    checksum = McuSm_ScrLoadU32(SCR_MCUSM_OFFSET_CHECKSUM);
    if (McuSm_CalculateChecksum(payload, payloadLength) != checksum)
    {
        McuSm_ScrStateInvalidCounter++;
        return FALSE;
    }

    McuSm_ApplyRetainedState(&McuSm_RetainedStateWork);
    McuSm_ScrStateRestoreCounter++;
    return TRUE;
}

void McuSm_CaptureWakeupImagesFromScr(void)
{
    (void)McuSm_RestoreRetainedStateFromScr();
    McuSm_CaptureScrRtcRecord();
    McuSm_SaveRetainedStateToScr();
}

uint8 dbgcnt = 0;

static void McuSm_CaptureScrRtcRecord(void)
{
    volatile uint8 *scrTime = &((volatile uint8 *)PMS_XRAM)[SCR_TIME_XRAM_BASE];
    uint16 index;

    /*
     * Do not reject the record based on PMS_PMSWCR2.B.SCRECC.  The SCR updates
     * PMS_XRAM through its 8-bit bus while tracking standby elapsed time, which
     * can leave TriCore-side word ECC mismatches even for a valid handoff
     * record.  TimeBase validates the SCR-reported wake/fault status before
     * consuming the captured image.
     */
    if (PMS_PMSWCR2.B.SCRECC != 0u)
    {
        dbgcnt++;
    }

    for (index = 0u; index < SCR_TIME_RECORD_LENGTH; index++)
    {
        McuSm_Trap4ScrRtcRecord[index] = scrTime[index];
    }

    if ((McuSm_LoadU32FromBuffer(McuSm_Trap4ScrRtcRecord, SCR_TIME_OFFSET_MAGIC) == SCR_TIME_MAGIC) &&
            (McuSm_Trap4ScrRtcRecord[SCR_TIME_OFFSET_VERSION] == SCR_TIME_VERSION) &&
            (McuSm_Trap4ScrRtcRecord[SCR_TIME_OFFSET_VALID] == SCR_TIME_VALID))
    {
        McuSm_Trap4ScrRtcRecordValid = TRUE;
        McuSm_Trap4ScrRtcRecordCounter++;
    }
    else
    {
        McuSm_Trap4ScrRtcRecordValid = FALSE;
    }
}

static uint32 McuSm_GetTrap4ErrorAddress(IfxCpu_Trap trapInfo)
{
    uint32 errorAddress;

    switch (trapInfo.tId)
    {
        case IfxCpu_Trap_Bus_Id_dataAccessSynchronousError:
        case IfxCpu_Trap_Bus_Id_dataAccessAsynchronousError:
        {
            errorAddress = McuSm_Trap4Deadd;
            break;
        }

        case IfxCpu_Trap_Bus_Id_dataMemoryIntegrityError:
        {
            errorAddress = McuSm_Trap4Diear;
            break;
        }

        case IfxCpu_Trap_Bus_Id_programMemoryIntegrityError:
        {
            errorAddress = McuSm_Trap4Piear;
            break;
        }

        default:
        {
            errorAddress = trapInfo.tAddr;
            break;
        }
    }

    return errorAddress;
}

static uint32 McuSm_Trap4ZeroFillIfRecoverable(uint32 errorAddress)
{
    uint32 reaction = MCUSM_TRAP4_REACTION_NONE;
    uint32 const xramStart = (uint32)PMS_XRAM;
    uint32 const xramEnd = xramStart + (uint32)PMS_XRAM_SIZE;

    if (McuSm_IsAddressInRange(errorAddress, xramStart, xramEnd) != FALSE)
    {
        McuSm_ZeroFillRange(xramStart, xramEnd);
        reaction = MCUSM_TRAP4_REACTION_XRAM;
    }
    else
    {
        /* No recoverable zero-fill target for this bus-error address. */
    }

    if (reaction != MCUSM_TRAP4_REACTION_NONE)
    {
        McuSm_Trap4ZeroFillReaction = reaction;
        McuSm_Trap4ZeroFillReactionCounter++;
    }

    return reaction;
}

void McuSm_TRAP1(IfxCpu_Trap trapInfo)
{
    MCUSM_RECORD_TRAP(1u, trapInfo);
    McuSm_PerformResetHook(371u, trapInfo.tId);
}

void McuSm_TRAP2(IfxCpu_Trap trapInfo)
{
    MCUSM_RECORD_TRAP(2u, trapInfo);
    McuSm_PerformResetHook(372u, trapInfo.tId);
}

void McuSm_TRAP4(IfxCpu_Trap trapInfo)
{
    McuSm_Trap4Dstr = __mfcr(CPU_DSTR);
    McuSm_Trap4Datr = __mfcr(CPU_DATR);
    McuSm_Trap4Deadd = __mfcr(CPU_DEADD);
    McuSm_Trap4Diear = __mfcr(CPU_DIEAR);
    McuSm_Trap4Dietr = __mfcr(CPU_DIETR);
    McuSm_Trap4Piear = __mfcr(CPU_PIEAR);
    McuSm_Trap4Pietr = __mfcr(CPU_PIETR);

    MCUSM_RECORD_TRAP(4u, trapInfo);
    McuSm_Trap4ErrorAddress = McuSm_GetTrap4ErrorAddress(trapInfo);

    if ((trapInfo.tId == IfxCpu_Trap_Bus_Id_dataMemoryIntegrityError) &&
            (McuSm_IsDFlashAddress(Fee_DebugLastFlashAccessPhysicalAddress) != FALSE))
    {
        McuSm_RequestDFlashRecoveryFromFee();
    }

    if (McuSm_IsScrXramAddress(McuSm_Trap4ErrorAddress) == FALSE)
    {
        TimeBase_CaptureStandbyRtcBeforeScrReset();
    }

    if (McuSm_IsScrXramAddress(McuSm_Trap4ErrorAddress) == FALSE)
    {
        McuSm_CaptureScrRtcRecord();
    }

    if (McuSm_Trap4ZeroFillIfRecoverable(McuSm_Trap4ErrorAddress) == MCUSM_TRAP4_REACTION_XRAM)
    {
        McuSm_Trap4ScrRtcRecordValid = FALSE;
    }
    McuSm_PerformResetHook(374u, trapInfo.tId);
}

/*
 * DOM0 (base 0xF8700000) is the LMU "Online Data Acquisition" (OLDA) bridge -
 * an emulation/calibration-only peripheral (BRCON.OLDAEN gates it).  On this
 * production TC37x the OLDA/EMEM bridge is not populated, so any read of the DOM
 * register space raises an SRI data-access synchronous error (TRAP4, tId=2 DSE).
 *
 * This capture used to run at the very top of McuSm_TRAP7, so that DSE aborted
 * EVERY NMI before the SMU alarm could be classified and before DFlash recovery
 * could be requested - the NMI silently turned into a 374/2 (TRAP4/DSE) reset
 * loop and the existing DFlash-recovery path never ran.
 *
 * The DOM registers are post-mortem-only and nothing depends on them, so we no
 * longer touch the hardware.  The diagnostic fields are left at "not captured"
 * sentinels; the DFlash error address is recovered from the Fee access recorder
 * and CPU_DEADD instead.  Do not reintroduce DOM reads unless OLDA is actually
 * enabled on the target device.
 */
static void McuSm_CaptureTrap7DomError(void)
{
    uint8 index;

    McuSm_Trap7DomPestat = 0u;
    McuSm_Trap7DomTidstat = 0u;
    McuSm_Trap7DomActiveSci = 0xFFFFFFFFu;
    McuSm_Trap7DomActiveErrAddr = 0u;
    McuSm_Trap7DomActiveErr = 0u;

    for (index = 0u; index < 16u; index++)
    {
        McuSm_Trap7DomErrAddr[index] = 0u;
        McuSm_Trap7DomErr[index] = 0u;
    }
}

void McuSm_TRAP7(IfxCpu_Trap trapInfo)
{
    uint32 const volatile* ag;
    uint32 agMasked;
    uint32 agRstRsn = 0u;
    uint32 agRstInfo = 0u;

    McuSm_Trap7Dstr = __mfcr(CPU_DSTR);
    McuSm_Trap7Datr = __mfcr(CPU_DATR);
    McuSm_Trap7Deadd = __mfcr(CPU_DEADD);
    McuSm_Trap7Diear = __mfcr(CPU_DIEAR);
    McuSm_Trap7Dietr = __mfcr(CPU_DIETR);
    McuSm_Trap7Piear = __mfcr(CPU_PIEAR);
    McuSm_Trap7Pietr = __mfcr(CPU_PIETR);
    McuSm_CaptureTrap7DomError();

    MCUSM_RECORD_TRAP(7u, trapInfo);

    McuSm_AGs[0u] = SMU_AGCF0_0.U & SMU_AGCF0_2.U & ~(SMU_AGCF0_1.U);
    McuSm_AGs[1u] = SMU_AGCF1_0.U & SMU_AGCF1_2.U & ~(SMU_AGCF1_1.U);
    McuSm_AGs[2u] = SMU_AGCF2_0.U & SMU_AGCF2_2.U & ~(SMU_AGCF2_1.U);
    McuSm_AGs[6u] = SMU_AGCF6_0.U & SMU_AGCF6_2.U & ~(SMU_AGCF6_1.U);
    McuSm_AGs[7u] = SMU_AGCF7_0.U & SMU_AGCF7_2.U & ~(SMU_AGCF7_1.U);
    McuSm_AGs[8u] = SMU_AGCF8_0.U & SMU_AGCF8_2.U & ~(SMU_AGCF8_1.U);
    McuSm_AGs[9u] = SMU_AGCF9_0.U & SMU_AGCF9_2.U & ~(SMU_AGCF9_1.U);
    McuSm_AGs[10u] = SMU_AGCF10_0.U & SMU_AGCF10_2.U & ~(SMU_AGCF10_1.U);
    McuSm_AGs[11u] = SMU_AGCF11_0.U & SMU_AGCF11_2.U & ~(SMU_AGCF11_1.U);

    ag = (uint32 const volatile*)(&SMU_AG0);

    for(sint8 i = 0u; i < 12u; i++)
    {
        McuSm_Trap7AgRaw[(uint8)i] = ag[i];
        agMasked = McuSm_Trap7AgRaw[(uint8)i] & McuSm_AGs[(uint8)i];
        McuSm_Trap7AgMasked[(uint8)i] = agMasked;

        if(0u != agMasked)
        {
            agRstRsn = i;
            agRstInfo = (sint8)(31u - (uint8)__clz(agMasked));
            break;
        }
        else
        {
            /* Do nothing. */
        }
    }

    McuSm_Trap7AgRstRsn = agRstRsn;
    McuSm_Trap7AgRstInfo = agRstInfo;
    /*
     * SMU alarms 7/17 (XBAR0 SRI bus error) and 7/20 (SPB bus error) can both
     * be raised by DFlash access. Classify DFlash recovery from the Fee access
     * recorder when available, otherwise fall back to CPU_DEADD.  (The DOM0
     * ERRADDR fallback was removed: that peripheral is not accessible on this
     * device and reading it aborted the handler before reaching this point.)
     */
    if ((agRstRsn == 7u) &&
            ((agRstInfo == 17u) || (agRstInfo == 20u)) &&
            ((McuSm_IsDFlashAddress(Fee_DebugLastFlashAccessPhysicalAddress) != FALSE) ||
                    (McuSm_IsDFlashAddress(McuSm_Trap7Deadd) != FALSE)))
    {
        if (McuSm_IsDFlashAddress(Fee_DebugLastFlashAccessPhysicalAddress) != FALSE)
        {
            McuSm_RequestDFlashRecoveryFromFee();
        }
        else
        {
            McuSm_RequestDFlashRecoveryForAddress(
                    McuSm_Trap7Deadd & 0x00FFFFFFu,
                    McuSm_Trap7Deadd);
        }
    }

    McuSm_PerformResetHook(agRstRsn, agRstInfo);
}

void McuSm_TRAP3(IfxCpu_Trap trapInfo)
{
    uint32 index;
    uint16 password;
    Ifx_SCU_WDTS *watchdog = &MODULE_SCU.WDTS;
    Ifx_CPU_CORE_ID reg;
    uint8 coreId;

    MCUSM_RECORD_TRAP(3u, trapInfo);

    reg.U = __mfcr(CPU_CORE_ID);
    coreId =  (IfxCpu_ResourceCpu)reg.B.CORE_ID;
    Ifx_SCU_WDTCPU *cpuwdg = &MODULE_SCU.WDTCPU[coreId];

    if(McuSm_IndexResetHistory >= 20u)
    {
        McuSm_IndexResetHistory = 0u;
    }

    McuSm_LastResetReason = 373u;
    McuSm_LastResetInformation = trapInfo.tId;
    McuSm_ResetHistory[McuSm_IndexResetHistory].reason = 373u;
    McuSm_ResetHistory[McuSm_IndexResetHistory].information = trapInfo.tId;
    McuSm_IndexResetHistory++;

    if(McuSm_IndexResetHistory >= 20u)
    {
        McuSm_IndexResetHistory = 0u;
    }
    else
    {
        /* Do nothing. */
    }

    password  = watchdog->CON0.B.PW;
    password ^= 0x003Fu;

    if (SCU_WDTS_CON0.B.LCK)
    {
        SCU_WDTS_CON0.U = (1u << IFX_SCU_WDTS_CON0_ENDINIT_OFF) |
                (0u << IFX_SCU_WDTS_CON0_LCK_OFF) |
                (password << IFX_SCU_WDTS_CON0_PW_OFF) |
                (SCU_WDTS_CON0.B.REL << IFX_SCU_WDTS_CON0_REL_OFF);
    }
    else
    {
        /* Do nothing. */
    }

    SCU_WDTS_CON0.U = (0u << IFX_SCU_WDTS_CON0_ENDINIT_OFF) |
            (1u << IFX_SCU_WDTS_CON0_LCK_OFF) |
            (password << IFX_SCU_WDTS_CON0_PW_OFF) |
            (SCU_WDTS_CON0.B.REL << IFX_SCU_WDTS_CON0_REL_OFF);

    index = 0u;
    while ((SCU_WDTS_CON0.B.ENDINIT == 1u) && (index < MCUSM_ENDINIT_WAIT_LIMIT))
    {
        index++;
    }
    McuSm_EndinitWaitCounter += index;
    if (SCU_WDTS_CON0.B.ENDINIT == 1u)
    {
        McuSm_EndinitTimeoutCounter++;
    }

    MODULE_SCU.RSTCON.B.SW = 2u;
    password  = cpuwdg->CON0.B.PW;
    password ^= 0x003Fu;

    if (cpuwdg->CON0.B.LCK)
    {
        cpuwdg->CON0.U = (1u << IFX_SCU_WDTCPU_CON0_ENDINIT_OFF) |
                (0u << IFX_SCU_WDTCPU_CON0_LCK_OFF) |
                (password << IFX_SCU_WDTCPU_CON0_PW_OFF) |
                (cpuwdg->CON0.B.REL << IFX_SCU_WDTCPU_CON0_REL_OFF);
    }
    else
    {
        /* Do nothing. */
    }

    cpuwdg->CON0.U = (0u << IFX_SCU_WDTCPU_CON0_ENDINIT_OFF) |
            (1u << IFX_SCU_WDTCPU_CON0_LCK_OFF) |
            (password << IFX_SCU_WDTCPU_CON0_PW_OFF) |
            (cpuwdg->CON0.B.REL << IFX_SCU_WDTCPU_CON0_REL_OFF);

    index = 0u;
    while ((cpuwdg->CON0.B.ENDINIT == 1u) && (index < MCUSM_ENDINIT_WAIT_LIMIT))
    {
        index++;
    }
    McuSm_EndinitWaitCounter += index;
    if (cpuwdg->CON0.B.ENDINIT == 1u)
    {
        McuSm_EndinitTimeoutCounter++;
    }

    if (cpuwdg->CON0.B.LCK)
    {
        cpuwdg->CON0.U = (1u << IFX_SCU_WDTCPU_CON0_ENDINIT_OFF) |
                (0u << IFX_SCU_WDTCPU_CON0_LCK_OFF) |
                (password << IFX_SCU_WDTCPU_CON0_PW_OFF) |
                (cpuwdg->CON0.B.REL << IFX_SCU_WDTCPU_CON0_REL_OFF);
    }

    cpuwdg->CON0.U = (0u << IFX_SCU_WDTCPU_CON0_ENDINIT_OFF) |
            (1u << IFX_SCU_WDTCPU_CON0_LCK_OFF) |
            (password << IFX_SCU_WDTCPU_CON0_PW_OFF) |
            (cpuwdg->CON0.B.REL << IFX_SCU_WDTCPU_CON0_REL_OFF);

    index = 0u;
    while ((cpuwdg->CON0.B.ENDINIT == 1u) && (index < MCUSM_ENDINIT_WAIT_LIMIT))
    {
        index++;
    }
    McuSm_EndinitWaitCounter += index;
    if (cpuwdg->CON0.B.ENDINIT == 1u)
    {
        McuSm_EndinitTimeoutCounter++;
    }

    MODULE_SCU.RSTCON2.B.USRINFO = 34u;
    McuSm_SaveRetainedStateToScr();
    MODULE_SCU.SWRSTCON.B.SWRSTREQ = 1U;

    for (index = 0U; index < (uint32)90000U; index++){}
}

void McuSm_InitializeBusMpu(void)
{
    IfxScuWdt_clearSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());

    CPU0_SPR_SPROT_RGNLA0.U = MCUSM_BUSMPU_DSPR0_LOWER_ADDRESS;
    CPU0_SPR_SPROT_RGNUA0.U = MCUSM_BUSMPU_DSPR0_UPPER_ADDRESS;
    CPU0_SPR_SPROT_RGNACCENA0_W.U = MCUSM_BUSMPU_WRITE_NON_DMA_MASK_A;
    CPU0_SPR_SPROT_RGNACCENB0_W.U = MCUSM_BUSMPU_NO_ACCESS_MASK;
    CPU0_SPR_SPROT_RGNACCENA0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU0_SPR_SPROT_RGNACCENB0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;

    CPU0_SPR_SPROT_RGNLA1.U = MCUSM_BUSMPU_PSPR0_LOWER_ADDRESS;
    CPU0_SPR_SPROT_RGNUA1.U = MCUSM_BUSMPU_PSPR0_UPPER_ADDRESS;
    CPU0_SPR_SPROT_RGNACCENA1_W.U = MCUSM_BUSMPU_WRITE_NON_DMA_MASK_A;
    CPU0_SPR_SPROT_RGNACCENB1_W.U = MCUSM_BUSMPU_NO_ACCESS_MASK;
    CPU0_SPR_SPROT_RGNACCENA1_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU0_SPR_SPROT_RGNACCENB1_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;

    CPU1_SPR_SPROT_RGNLA0.U = MCUSM_BUSMPU_DSPR1_LOWER_ADDRESS;
    CPU1_SPR_SPROT_RGNUA0.U = MCUSM_BUSMPU_DSPR1_UPPER_ADDRESS;
    CPU1_SPR_SPROT_RGNACCENA0_W.U = MCUSM_BUSMPU_WRITE_NON_DMA_MASK_A;
    CPU1_SPR_SPROT_RGNACCENB0_W.U = MCUSM_BUSMPU_NO_ACCESS_MASK;
    CPU1_SPR_SPROT_RGNACCENA0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU1_SPR_SPROT_RGNACCENB0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;

    CPU1_SPR_SPROT_RGNLA1.U = MCUSM_BUSMPU_PSPR1_LOWER_ADDRESS;
    CPU1_SPR_SPROT_RGNUA1.U = MCUSM_BUSMPU_PSPR1_UPPER_ADDRESS;
    CPU1_SPR_SPROT_RGNACCENA1_W.U = MCUSM_BUSMPU_WRITE_NON_DMA_MASK_A;
    CPU1_SPR_SPROT_RGNACCENB1_W.U = MCUSM_BUSMPU_NO_ACCESS_MASK;
    CPU1_SPR_SPROT_RGNACCENA1_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU1_SPR_SPROT_RGNACCENB1_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;

    CPU2_SPR_SPROT_RGNLA0.U = MCUSM_BUSMPU_DSPR2_LOWER_ADDRESS;
    CPU2_SPR_SPROT_RGNUA0.U = MCUSM_BUSMPU_DSPR2_UPPER_ADDRESS;
    CPU2_SPR_SPROT_RGNACCENA0_W.U = MCUSM_BUSMPU_WRITE_NON_DMA_MASK_A;
    CPU2_SPR_SPROT_RGNACCENB0_W.U = MCUSM_BUSMPU_NO_ACCESS_MASK;
    CPU2_SPR_SPROT_RGNACCENA0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU2_SPR_SPROT_RGNACCENB0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;

    CPU2_SPR_SPROT_RGNLA1.U = MCUSM_BUSMPU_PSPR2_LOWER_ADDRESS;
    CPU2_SPR_SPROT_RGNUA1.U = MCUSM_BUSMPU_PSPR2_UPPER_ADDRESS;
    CPU2_SPR_SPROT_RGNACCENA1_W.U = MCUSM_BUSMPU_WRITE_NON_DMA_MASK_A;
    CPU2_SPR_SPROT_RGNACCENB1_W.U = MCUSM_BUSMPU_NO_ACCESS_MASK;
    CPU2_SPR_SPROT_RGNACCENA1_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU2_SPR_SPROT_RGNACCENB1_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;

    CPU0_DLMU_SPROT_RGNLA0.U = MCUSM_BUSMPU_DLMU0_LOWER_ADDRESS;
    CPU0_DLMU_SPROT_RGNUA0.U = MCUSM_BUSMPU_DLMU0_UPPER_ADDRESS;
    CPU0_DLMU_SPROT_RGNACCENA0_W.U = MCUSM_BUSMPU_WRITE_NON_DMA_MASK_A;
    CPU0_DLMU_SPROT_RGNACCENB0_W.U = MCUSM_BUSMPU_NO_ACCESS_MASK;
    CPU0_DLMU_SPROT_RGNACCENA0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU0_DLMU_SPROT_RGNACCENB0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;

    CPU1_DLMU_SPROT_RGNLA0.U = MCUSM_BUSMPU_DLMU1_LOWER_ADDRESS;
    CPU1_DLMU_SPROT_RGNUA0.U = MCUSM_BUSMPU_DLMU1_UPPER_ADDRESS;
    CPU1_DLMU_SPROT_RGNACCENA0_W.U = MCUSM_BUSMPU_WRITE_NON_DMA_MASK_A;
    CPU1_DLMU_SPROT_RGNACCENB0_W.U = MCUSM_BUSMPU_NO_ACCESS_MASK;
    CPU1_DLMU_SPROT_RGNACCENA0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU1_DLMU_SPROT_RGNACCENB0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;

    CPU2_DLMU_SPROT_RGNLA0.U = MCUSM_BUSMPU_DLMU2_LOWER_ADDRESS;
    CPU2_DLMU_SPROT_RGNUA0.U = MCUSM_BUSMPU_DLMU2_UPPER_ADDRESS;
    CPU2_DLMU_SPROT_RGNACCENA0_W.U = MCUSM_BUSMPU_WRITE_NON_DMA_MASK_A;
    CPU2_DLMU_SPROT_RGNACCENB0_W.U = MCUSM_BUSMPU_NO_ACCESS_MASK;
    CPU2_DLMU_SPROT_RGNACCENA0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU2_DLMU_SPROT_RGNACCENB0_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;

    CPU0_LPB_SPROT_ACCENA_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU1_LPB_SPROT_ACCENA_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU2_LPB_SPROT_ACCENA_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU0_LPB_SPROT_ACCENB_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU1_LPB_SPROT_ACCENB_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;
    CPU2_LPB_SPROT_ACCENB_R.U = MCUSM_BUSMPU_FULL_ACCESS_MASK;

    McuSm_BusMpuWriteAccessMaskA = MCUSM_BUSMPU_WRITE_NON_DMA_MASK_A;
    McuSm_BusMpuWriteAccessMaskB = MCUSM_BUSMPU_NO_ACCESS_MASK;
    McuSm_BusMpuConfigured++;
    IfxScuWdt_setSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());
}
