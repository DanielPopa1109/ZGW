#include <string.h>
#include "Fls.h"
#include "MemStack_Error.h"
#include "IfxFlash.h"
#include "IfxScuWdt.h"
#include "IfxCpu.h"
#include "IfxSmu.h"
#include "IfxScu_reg.h"

#define FLS_API_INIT               (0x00u)
#define FLS_API_ERASE              (0x01u)
#define FLS_API_WRITE              (0x02u)
#define FLS_API_READ               (0x03u)
#define FLS_API_CANCEL             (0x04u)
#define FLS_API_MAIN               (0x05u)
#define FLS_API_BLANK_CHECK        (0x06u)

#define FLS_E_UNINIT               (0x01u)
#define FLS_E_BUSY                 (0x02u)
#define FLS_E_PARAM_ADDRESS        (0x03u)
#define FLS_E_PARAM_LENGTH         (0x04u)
#define FLS_E_PARAM_POINTER        (0x05u)
#define FLS_E_VERIFY               (0x06u)
#define FLS_DMU_ERR_MASK           (0x7Fu)
#define FLS_DMU_WAIT_ERROR         (0x80000000u)
#define FLS_DMU_BUSY_POLL_LIMIT    (16000u)
#define FLS_DMU_D0_BUSY_MASK       (0x00000001u)
#define FLS_MAIN_STEP_BUDGET       (32u)
#define FLS_DFLASH_SMU_CLEAR_RETRIES (8u)

typedef enum
{
    FLS_JOB_NONE = 0,
    FLS_JOB_ERASE,
    FLS_JOB_WRITE,
    FLS_JOB_READ
} Fls_JobType;

typedef enum
{
    FLS_PHASE_IDLE = 0,
    FLS_PHASE_WRITE_ENTER_WAIT,
    FLS_PHASE_WRITE_WAIT,
    FLS_PHASE_ERASE_WAIT,
    FLS_PHASE_ERASE_VERIFY_WAIT
} Fls_JobPhaseType;

typedef enum
{
    FLS_DMU_POLL_BUSY = 0,
    FLS_DMU_POLL_READY,
    FLS_DMU_POLL_ERROR
} Fls_DmuPollResultType;

typedef struct
{
    MemIf_StatusType status;
    MemIf_JobResultType result;
    Fls_JobType job;
    Fls_JobPhaseType phase;
    Fls_AddressType start;
    Fls_LengthType length;
    Fls_LengthType progress;
    Fls_LengthType phaseProgress;
    const uint8 *src;
    uint8 *dst;
    MemIf_ModeType mode;
} Fls_StateType;

static Fls_StateType Fls_State =
{
    MEMIF_UNINIT,
    MEMIF_JOB_FAILED,
    FLS_JOB_NONE,
    FLS_PHASE_IDLE,
    0u,
    0u,
    0u,
    0u,
    NULL_PTR,
    NULL_PTR,
    MEMIF_MODE_SLOW
};

volatile uint32 Fls_LastDmuError = 0u;
volatile uint32 Fls_InitClearDmuStatusCount = 0u;
volatile uint32 Fls_InitDmuErrorAfterClear = 0u;
volatile uint32 Fls_DFlashSmuBusErrorClearCounter = 0u;
volatile uint32 Fls_DFlashSmuBusErrorPendingBeforeClear = 0u;
volatile uint32 Fls_DFlashSpbBusErrorPendingBeforeClear = 0u;
volatile uint32 Fls_DFlashTrapStatBeforeClear = 0u;
volatile uint32 Fls_DFlashTrapStatAfterClear = 0u;
volatile uint32 Fls_DFlashSmuBusErrorClearRetryCounter = 0u;
volatile uint32 Fls_DFlashSmuBusErrorStillPendingAfterClear = 0u;
/* Debug only: number of times the DFlash SMU-trap was suppressed (masked). */
volatile uint32 Fls_DFlashSmuTrapSuppressCounter = 0u;
/* Nesting depth of the SMU-trap suppression (mask once, unmask when back to 0). */
static volatile sint32 Fls_DFlashSmuTrapDepth = 0;
/* TRUE while the whole async write/erase job holds the trap masked. */
static volatile boolean Fls_DFlashJobTrapHeld = FALSE;
/* Debug only: last DFlash (logical) address successfully programmed by the async write path. */
volatile uint32 Fls_LastWrittenDFlashAddress = 0u;
/* Debug only: number of DFlash page writes successfully programmed */
volatile uint32 Fls_DFlashWriteCounter = 0u;
volatile uint32 Fls_DmuWaitCounter = 0u;
volatile uint32 Fls_DmuWaitTimeoutCounter = 0u;
volatile uint32 Fls_DmuWaitBusyCounter = 0u;
volatile uint32 Fls_DmuWaitEndinitOpenCounter = 0u;
volatile uint32 Fls_DmuWaitEndinitCloseCounter = 0u;
volatile uint32 Fls_DmuWaitLastLoops = 0u;
volatile uint32 Fls_DmuWaitLastStatus = 0u;
volatile uint32 Fls_DmuWaitConsecutiveBusyCounter = 0u;
volatile uint32 Fls_DmuRecoveryRequired = 0u;
volatile uint32 Fls_DmuBusyRejectCounter = 0u;
volatile uint32 Fls_DmuTimeoutJob = 0u;
volatile uint32 Fls_DmuTimeoutPhase = 0u;
volatile uint32 Fls_DmuTimeoutAddress = 0u;
volatile uint32 Fls_DmuTimeoutProgress = 0u;
volatile uint32 Fls_DmuRejectAddress = 0u;

static Std_ReturnType Fls_CopyReadableRange(Fls_AddressType address, uint8 *target, Fls_LengthType length);

static boolean Fls_IsRangeValid(Fls_AddressType address, Fls_LengthType length)
{
    if (length == 0u)
    {
        return FALSE;
    }
    if (address >= FLS_DFLASH0_TOTAL_SIZE)
    {
        return FALSE;
    }
    if (length > (FLS_DFLASH0_TOTAL_SIZE - address))
    {
        return FALSE;
    } // @suppress("Unused static function")
    return TRUE;
}

static void Fls_ClearDmuEndinit(uint16 *cpuPassword, uint16 *safetyPassword)
{
    *cpuPassword = IfxScuWdt_getCpuWatchdogPassword();
    *safetyPassword = IfxScuWdt_getSafetyWatchdogPassword();

    IfxScuWdt_clearSafetyEndinit(*safetyPassword);
    IfxScuWdt_clearCpuEndinit(*cpuPassword);
}

static void Fls_SetDmuEndinit(uint16 cpuPassword, uint16 safetyPassword)
{
    IfxScuWdt_setCpuEndinit(cpuPassword);
    IfxScuWdt_setSafetyEndinit(safetyPassword);
}

void Fls_ClearDmuStatus(void)
{
    uint16 cpuPassword;
    uint16 safetyPassword;
#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    boolean interruptState;
#endif

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    interruptState = IfxCpu_disableInterrupts();
#endif

    Fls_ClearDmuEndinit(&cpuPassword, &safetyPassword);
    IfxFlash_clearStatus(FLS_FLASH_MODULE);
    Fls_SetDmuEndinit(cpuPassword, safetyPassword);

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    IfxCpu_restoreInterrupts(interruptState);
#endif

    Fls_LastDmuError = 0u;
}

static uint32 Fls_ReadDmuErrorStatus(void)
{
    uint16 cpuPassword;
    uint16 safetyPassword;
    uint32 status;

    Fls_ClearDmuEndinit(&cpuPassword, &safetyPassword);
    status = DMU_HF_ERRSR.U;
    Fls_SetDmuEndinit(cpuPassword, safetyPassword);

    return status;
}

static uint32 Fls_ReadDmuStatus(void)
{
    uint16 cpuPassword;
    uint16 safetyPassword;
    uint32 status;

    Fls_ClearDmuEndinit(&cpuPassword, &safetyPassword);
    status = DMU_HF_STATUS.U;
    Fls_SetDmuEndinit(cpuPassword, safetyPassword);

    return status;
}

static void Fls_RecordDmuStuck(uint32 status)
{
    Fls_DmuRecoveryRequired = 1u;
    Fls_DmuWaitLastStatus = status;
    Fls_DmuTimeoutJob = (uint32)Fls_State.job;
    Fls_DmuTimeoutPhase = (uint32)Fls_State.phase;
    Fls_DmuTimeoutAddress = Fls_State.start + Fls_State.progress;
    Fls_DmuTimeoutProgress = Fls_State.progress;
    Fls_LastDmuError = FLS_DMU_WAIT_ERROR | (status & 0x7FFFFFFFu);
}

static Std_ReturnType Fls_CheckDmuReadyForCommand(Fls_AddressType address)
{
    uint32 status;

    status = Fls_ReadDmuStatus();
    Fls_DmuWaitLastStatus = status;

    if ((Fls_DmuRecoveryRequired != 0u) ||
            ((status & FLS_DMU_D0_BUSY_MASK) != 0u))
    {
        Fls_DmuBusyRejectCounter++;
        Fls_DmuRejectAddress = address;
        Fls_RecordDmuStuck(status);
        return E_NOT_OK;
    }

    return E_OK;
}

void Fls_ClearDFlashSmuBusError(void)
{
    uint16 password;
    boolean trapPending;

    Fls_DFlashSmuBusErrorPendingBeforeClear =
            (uint32)((MODULE_SMU.AG[7].U >> 17u) & 0x1u);
    Fls_DFlashSpbBusErrorPendingBeforeClear =
            (uint32)((MODULE_SMU.AG[7].U >> 20u) & 0x1u);
    Fls_DFlashTrapStatBeforeClear = SCU_TRAPSTAT.U;
    trapPending = (boolean)(SCU_TRAPSTAT.B.SMUT != 0u);

    if ((Fls_DFlashSmuBusErrorPendingBeforeClear != 0u) ||
            (Fls_DFlashSpbBusErrorPendingBeforeClear != 0u) ||
            (trapPending != FALSE))
    {
        if (Fls_DFlashSmuBusErrorPendingBeforeClear != 0u)
        {
            (void)IfxSmu_clearAlarmStatus(IfxSmu_Alarm_XBAR0_SRI_BusErrorEvent);
        }
        if (Fls_DFlashSpbBusErrorPendingBeforeClear != 0u)
        {
            (void)IfxSmu_clearAlarmStatus(IfxSmu_Alarm_SPB_BusErrorEvent);
        }

        password = IfxScuWdt_getCpuWatchdogPassword();
        IfxScuWdt_clearCpuEndinit(password);
        SCU_TRAPCLR.B.SMUT = 1u;
        IfxScuWdt_setCpuEndinit(password);

        Fls_DFlashSmuBusErrorClearCounter++;
    }

    Fls_DFlashTrapStatAfterClear = SCU_TRAPSTAT.U;
}

static boolean Fls_IsDFlashSmuBusErrorPending(void)
{
    return (boolean)((((MODULE_SMU.AG[7].U >> 17u) & 0x1u) != 0u) ||
            (((MODULE_SMU.AG[7].U >> 20u) & 0x1u) != 0u) ||
            (SCU_TRAPSTAT.B.SMUT != 0u));
}

static void Fls_DrainDFlashSmuBusError(void)
{
    uint8 retry;

    for (retry = 0u; retry < FLS_DFLASH_SMU_CLEAR_RETRIES; retry++)
    {
        __dsync();
        Fls_ClearDFlashSmuBusError();
        __dsync();

        if (Fls_IsDFlashSmuBusErrorPending() == FALSE)
        {
            break;
        }

        Fls_DFlashSmuBusErrorClearRetryCounter++;
    }

    Fls_DFlashSmuBusErrorStillPendingAfterClear =
            (uint32)Fls_IsDFlashSmuBusErrorPending();
}

/*
 * Scoped suppression of the DFlash SRI/SPB bus-error NMI.
 *
 * Reading blank / partially-programmed DFlash (Fee scan blank-checks, interrupted
 * writes) and some DMU command sequences make the DMU return an SRI error
 * response, which the SMU latches as ALM7[17] (XBAR0 SRI bus error) / ALM7[20]
 * (SPB bus error).  Those alarms are wired to an NMI (McuSm_TRAP7) and reset the
 * ECU *during* the access - the reactive "clear after the fact"
 * (Fls_ClearDFlashSmuBusError) can never win that race because the NMI preempts.
 *
 * So for the few microseconds of the access we mask delivery of the SMU trap to
 * the running core via SCU_TRAPDIS0, run the access, clear the (benign) DFlash
 * alarm + SMU trap latch while still masked, then re-arm.  Only trap *delivery*
 * to this core is masked, only for the access window; the SMU alarm stays
 * configured and every other alarm keeps its reaction, so the safety mechanism
 * is deviated around a known-benign DFlash error, not disabled.
 *
 * The mask MUST span the whole DFlash operation, not just the instant the
 * command is issued: a DFlash program/erase runs asynchronously in the DMU for
 * up to several ms AFTER the command call returns, and a corrupt cell raises
 * ALM7[1] (uncorrectable ECC) + ALM7[17] (SRI bus error) during that async
 * window.  So these calls are reference-counted (mask on 0->1, unmask on 1->0)
 * and the async state machine holds one count across the entire job.
 */
static uint32 Fls_GetSmuTrapDisableMask(void)
{
    /*
     * The SMU alarm trap is BROADCAST to all cores: ALM7[17] is raised by a
     * core0 DFlash access, but the resulting NMI is delivered to CPU0, CPU1 and
     * CPU2 alike (it was observed firing in the CPU1/CPU2 idle tasks once only
     * CPU0 was masked).  So we must mask the SMU trap on every core for the
     * duration of the access, not just the core issuing it.
     * SCU_TRAPDIS0: CPU0SMUT=bit3, CPU1SMUT=bit11, CPU2SMUT=bit19.
     */
    return (uint32)((1u << 3u) | (1u << 11u) | (1u << 19u));
}

static void Fls_SuppressDFlashSmuTrap(void)
{
    uint16 password = IfxScuWdt_getCpuWatchdogPassword();
    uint32 mask = Fls_GetSmuTrapDisableMask();

    if (Fls_DFlashSmuTrapDepth <= 0)
    {
        IfxScuWdt_clearCpuEndinit(password);
        SCU_TRAPDIS0.U |= mask;
        IfxScuWdt_setCpuEndinit(password);
        __dsync();
        Fls_DFlashSmuTrapDepth = 0;
    }
    Fls_DFlashSmuTrapDepth++;
    Fls_DFlashSmuTrapSuppressCounter++;
}

static void Fls_RestoreDFlashSmuTrap(void)
{
    uint16 password = IfxScuWdt_getCpuWatchdogPassword();
    uint32 mask = Fls_GetSmuTrapDisableMask();

    if (Fls_DFlashSmuTrapDepth > 0)
    {
        Fls_DFlashSmuTrapDepth--;
    }
    if (Fls_DFlashSmuTrapDepth > 0)
    {
        return;     /* still nested inside an outer suppression - stay masked */
    }
    Fls_DFlashSmuTrapDepth = 0;

    /* Let any posted SRI bus error land in the SMU before we clear it - it can
     * be latched slightly after the faulting access completes. */
    __dsync();

    /* Clear any delayed benign DFlash SRI/SPB alarm + SMU trap latch while the
     * trap is still masked, so re-arming cannot deliver a just-posted ALM7[20]. */
    Fls_DrainDFlashSmuBusError();

    IfxScuWdt_clearCpuEndinit(password);
    SCU_TRAPDIS0.U &= ~mask;
    IfxScuWdt_setCpuEndinit(password);
    __dsync();
}

/* Force the trap back to its armed/unmasked state regardless of nesting - used
 * at init to recover if a reset left the mask asserted mid-operation. */
static void Fls_ForceReleaseDFlashSmuTrap(void)
{
    uint16 password = IfxScuWdt_getCpuWatchdogPassword();
    uint32 mask = Fls_GetSmuTrapDisableMask();

    Fls_DFlashSmuTrapDepth = 0;
    Fls_DFlashJobTrapHeld = FALSE;

    IfxScuWdt_clearCpuEndinit(password);
    SCU_TRAPDIS0.U |= mask;
    IfxScuWdt_setCpuEndinit(password);
    __dsync();

    Fls_DrainDFlashSmuBusError();

    IfxScuWdt_clearCpuEndinit(password);
    SCU_TRAPDIS0.U &= ~mask;
    IfxScuWdt_setCpuEndinit(password);
    __dsync();
}

static Fls_DmuPollResultType Fls_PollDmuReady(void)
{
    uint16 cpuPassword;
    uint16 safetyPassword;
    uint32 status;
    Fls_DmuPollResultType result = FLS_DMU_POLL_READY;

    if (FLS_FLASH_MODULE != 0u)
    {
        Fls_LastDmuError = FLS_DMU_WAIT_ERROR;
        return FLS_DMU_POLL_ERROR;
    }

    if (Fls_DmuRecoveryRequired != 0u)
    {
        return FLS_DMU_POLL_ERROR;
    }

    Fls_DmuWaitCounter++;
    Fls_ClearDmuEndinit(&cpuPassword, &safetyPassword);
    Fls_DmuWaitEndinitOpenCounter++;

    status = DMU_HF_STATUS.U;
    Fls_DmuWaitLastStatus = status;
    Fls_DmuWaitLastLoops = 0u;
    if ((status & FLS_DMU_D0_BUSY_MASK) != 0u)
    {
        Fls_DmuWaitBusyCounter++;
        if (Fls_DmuWaitConsecutiveBusyCounter < 0xFFFFFFFFu)
        {
            Fls_DmuWaitConsecutiveBusyCounter++;
        }
        result = FLS_DMU_POLL_BUSY;

        if (Fls_DmuWaitConsecutiveBusyCounter >= FLS_DMU_BUSY_POLL_LIMIT)
        {
            Fls_DmuWaitTimeoutCounter++;
            Fls_RecordDmuStuck(status);
            result = FLS_DMU_POLL_ERROR;
        }
    }

    if (result == FLS_DMU_POLL_READY)
    {
        Fls_DmuWaitConsecutiveBusyCounter = 0u;
        __dsync();
        Fls_LastDmuError = (uint32)(DMU_HF_ERRSR.U & FLS_DMU_ERR_MASK);
        if (Fls_LastDmuError != 0u)
        {
            result = FLS_DMU_POLL_ERROR;
        }
    }

    if (result == FLS_DMU_POLL_ERROR)
    {
        Fls_DmuWaitConsecutiveBusyCounter = 0u;
    }

    Fls_SetDmuEndinit(cpuPassword, safetyPassword);
    Fls_DmuWaitEndinitCloseCounter++;

    return result;
}

static Std_ReturnType Fls_VerifyErasedPageByRead(Fls_AddressType address)
{
    uint8 page[FLS_DFLASH0_PAGE_SIZE];
    uint32 byteIndex;

    if (Fls_CopyReadableRange(address, page, FLS_DFLASH0_PAGE_SIZE) != E_OK)
    {
        return E_NOT_OK;
    }

    for (byteIndex = 0u; byteIndex < FLS_DFLASH0_PAGE_SIZE; byteIndex++)
    {
        if (page[byteIndex] != FLS_ERASED_VALUE)
        {
            return E_NOT_OK;
        }
    }

    return E_OK;
}

static Std_ReturnType Fls_VerifyErasedRange(Fls_AddressType address, Fls_LengthType length)
{
    Fls_AddressType current = address;
    Fls_LengthType remaining = length;

    while (remaining > 0u)
    {
        if (Fls_VerifyErasedPageByRead(current) != E_OK)
        {
            return E_NOT_OK;
        }
        current += FLS_DFLASH0_PAGE_SIZE;
        remaining -= FLS_DFLASH0_PAGE_SIZE;
    }

    return E_OK;
}

uint32 Fls_GetPhysicalAddress(Fls_AddressType Address)
{
    return FLS_DFLASH0_BASE_ADDRESS + Address;
}

static Std_ReturnType Fls_CopyReadableRange(Fls_AddressType address, uint8 *target, Fls_LengthType length)
{
    /*
     * Do not pre-blank-check pages here.  This helper is used for pages that
     * Fee metadata has already selected as readable; issuing VerifyErasedPage
     * on programmed record/data pages can itself raise DFlash bus-error alarms.
     *
     * Blank / interrupted-write pages still raise ALM7[17]/[20] on read, so mask
     * the SMU NMI for the copy and re-arm afterwards.
    */
    if (Fls_CheckDmuReadyForCommand(address) != E_OK)
    {
        return E_NOT_OK;
    }

    Fls_SuppressDFlashSmuTrap();
    Fls_DrainDFlashSmuBusError();
    Fls_ClearDmuStatus();
    memcpy(target, (const void *)Fls_GetPhysicalAddress(address), (size_t)length);
    Fls_RestoreDFlashSmuTrap();

    return E_OK;
}

static void Fls_FinishJob(MemIf_JobResultType result)
{
    /* Release the job-wide SMU-trap hold taken across an async write/erase. */
    if (Fls_DFlashJobTrapHeld != FALSE)
    {
        Fls_DFlashJobTrapHeld = FALSE;
        Fls_RestoreDFlashSmuTrap();
    }

    Fls_State.result = result;
    Fls_State.status = MEMIF_IDLE;
    Fls_State.job = FLS_JOB_NONE;
    Fls_State.phase = FLS_PHASE_IDLE;
    Fls_State.progress = 0u;
    Fls_State.phaseProgress = 0u;
    Fls_State.length = 0u;
    Fls_State.src = NULL_PTR;
    Fls_State.dst = NULL_PTR;
    Fls_DmuWaitConsecutiveBusyCounter = 0u;
}

static Std_ReturnType Fls_StartProgramEnterPage(Fls_AddressType address)
{
    uint32 physicalAddress = Fls_GetPhysicalAddress(address);
    uint16 cpuPassword;
    uint16 safetyPassword;
    uint8 enterResult;
#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    boolean interruptState;
#endif

    if (Fls_CheckDmuReadyForCommand(address) != E_OK)
    {
        return E_NOT_OK;
    }

    Fls_SuppressDFlashSmuTrap();
    Fls_ClearDFlashSmuBusError();
    Fls_ClearDmuStatus();

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    interruptState = IfxCpu_disableInterrupts();
#endif

    Fls_ClearDmuEndinit(&cpuPassword, &safetyPassword);
    enterResult = IfxFlash_enterPageMode(physicalAddress);
    Fls_SetDmuEndinit(cpuPassword, safetyPassword);

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    IfxCpu_restoreInterrupts(interruptState);
#endif

    Fls_RestoreDFlashSmuTrap();

    return (enterResult == 0u) ? E_OK : E_NOT_OK;
}

static void Fls_StartProgramWritePage(Fls_AddressType address, const uint8 *data)
{
    uint32 physicalAddress = Fls_GetPhysicalAddress(address);
    uint32 word0;
    uint32 word1;
    uint16 cpuPassword;
    uint16 safetyPassword;
#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    boolean interruptState;
#endif

    /* Suppress before the source memcpy too: during Fee GC the source pointer
     * points directly into DFlash, so even reading it could raise ALM7. */
    Fls_SuppressDFlashSmuTrap();

    memcpy(&word0, &data[0], sizeof(word0));
    memcpy(&word1, &data[4], sizeof(word1));

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    interruptState = IfxCpu_disableInterrupts();
#endif

    Fls_ClearDmuEndinit(&cpuPassword, &safetyPassword);
    IfxFlash_loadPage2X32(physicalAddress, word0, word1);
    Fls_SetDmuEndinit(cpuPassword, safetyPassword);

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    IfxCpu_restoreInterrupts(interruptState);
#endif

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    interruptState = IfxCpu_disableInterrupts();
#endif

    Fls_ClearDmuEndinit(&cpuPassword, &safetyPassword);
    IfxFlash_writePage(physicalAddress);
    Fls_SetDmuEndinit(cpuPassword, safetyPassword);

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    IfxCpu_restoreInterrupts(interruptState);
#endif

    Fls_RestoreDFlashSmuTrap();
}

static Std_ReturnType Fls_StartEraseOneSector(Fls_AddressType address)
{
    uint32 physicalAddress = Fls_GetPhysicalAddress(address);
    uint16 cpuPassword;
    uint16 safetyPassword;
#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    boolean interruptState;
#endif

    if (Fls_CheckDmuReadyForCommand(address) != E_OK)
    {
        return E_NOT_OK;
    }

    Fls_SuppressDFlashSmuTrap();
    Fls_ClearDFlashSmuBusError();
    Fls_ClearDmuStatus();

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    interruptState = IfxCpu_disableInterrupts();
#endif

    Fls_ClearDmuEndinit(&cpuPassword, &safetyPassword);
    IfxFlash_eraseSector(physicalAddress);
    Fls_SetDmuEndinit(cpuPassword, safetyPassword);

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    IfxCpu_restoreInterrupts(interruptState);
#endif

    Fls_RestoreDFlashSmuTrap();
    return E_OK;
}

void Fls_Init(const Fls_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;
    /* Recover the SMU trap to armed/unmasked in case a reset interrupted an
     * async op while the job hold was active and left the mask asserted. */
    Fls_ForceReleaseDFlashSmuTrap();
    Fls_ClearDFlashSmuBusError();
    Fls_ClearDmuStatus();
    Fls_LastDmuError = (uint32)(Fls_ReadDmuErrorStatus() & FLS_DMU_ERR_MASK);
    Fls_InitDmuErrorAfterClear = Fls_LastDmuError;
    Fls_InitClearDmuStatusCount++;

    Fls_State.status = MEMIF_IDLE;
    Fls_State.result = MEMIF_JOB_OK;
    Fls_State.job = FLS_JOB_NONE;
    Fls_State.phase = FLS_PHASE_IDLE;
    Fls_State.start = 0u;
    Fls_State.length = 0u;
    Fls_State.progress = 0u;
    Fls_State.phaseProgress = 0u;
    Fls_State.src = NULL_PTR;
    Fls_State.dst = NULL_PTR;
    Fls_State.mode = MEMIF_MODE_SLOW;
    Fls_DmuWaitConsecutiveBusyCounter = 0u;
    Fls_DmuRecoveryRequired = 0u;
}

Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length)
{
    if (Fls_State.status == MEMIF_UNINIT)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_ERASE, FLS_E_UNINIT, 0u);
        return E_NOT_OK;
    }
    if (Fls_State.status != MEMIF_IDLE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_ERASE, FLS_E_BUSY, 0u);
        return E_NOT_OK;
    }
    if (Fls_IsRangeValid(TargetAddress, Length) == FALSE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_ERASE, FLS_E_PARAM_ADDRESS, TargetAddress);
        return E_NOT_OK;
    }
    if (((TargetAddress % FLS_DFLASH0_SECTOR_SIZE) != 0u) || ((Length % FLS_DFLASH0_SECTOR_SIZE) != 0u))
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_ERASE, FLS_E_PARAM_LENGTH, Length);
        return E_NOT_OK;
    }
    if (Fls_CheckDmuReadyForCommand(TargetAddress) != E_OK)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_ERASE, FLS_E_BUSY, TargetAddress);
        return E_NOT_OK;
    }

    Fls_State.status = MEMIF_BUSY;
    Fls_State.result = MEMIF_JOB_PENDING;
    Fls_State.job = FLS_JOB_ERASE;
    Fls_State.phase = FLS_PHASE_IDLE;
    Fls_State.start = TargetAddress;
    Fls_State.length = Length;
    Fls_State.progress = 0u;
    Fls_State.phaseProgress = 0u;
    Fls_State.src = NULL_PTR;
    Fls_State.dst = NULL_PTR;
    return E_OK;
}

Std_ReturnType Fls_Write(Fls_AddressType TargetAddress, const uint8 *SourceAddressPtr, Fls_LengthType Length)
{
    if (Fls_State.status == MEMIF_UNINIT)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_WRITE, FLS_E_UNINIT, 0u);
        return E_NOT_OK;
    }
    if (Fls_State.status != MEMIF_IDLE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_WRITE, FLS_E_BUSY, 0u);
        return E_NOT_OK;
    }
    if (SourceAddressPtr == NULL_PTR)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_WRITE, FLS_E_PARAM_POINTER, 0u);
        return E_NOT_OK;
    }
    if (Fls_IsRangeValid(TargetAddress, Length) == FALSE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_WRITE, FLS_E_PARAM_ADDRESS, TargetAddress);
        return E_NOT_OK;
    }
    if (((TargetAddress % FLS_DFLASH0_PAGE_SIZE) != 0u) || ((Length % FLS_DFLASH0_PAGE_SIZE) != 0u))
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_WRITE, FLS_E_PARAM_LENGTH, Length);
        return E_NOT_OK;
    }
    if (Fls_CheckDmuReadyForCommand(TargetAddress) != E_OK)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_WRITE, FLS_E_BUSY, TargetAddress);
        return E_NOT_OK;
    }

    Fls_State.status = MEMIF_BUSY;
    Fls_State.result = MEMIF_JOB_PENDING;
    Fls_State.job = FLS_JOB_WRITE;
    Fls_State.phase = FLS_PHASE_IDLE;
    Fls_State.start = TargetAddress;
    Fls_State.length = Length;
    Fls_State.progress = 0u;
    Fls_State.phaseProgress = 0u;
    Fls_State.src = SourceAddressPtr;
    Fls_State.dst = NULL_PTR;
    return E_OK;
}

Std_ReturnType Fls_Read(Fls_AddressType SourceAddress, uint8 *TargetAddressPtr, Fls_LengthType Length)
{
    if (Fls_State.status == MEMIF_UNINIT)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_READ, FLS_E_UNINIT, 0u);
        return E_NOT_OK;
    }
    if (Fls_State.status != MEMIF_IDLE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_READ, FLS_E_BUSY, 0u);
        return E_NOT_OK;
    }
    if (TargetAddressPtr == NULL_PTR)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_READ, FLS_E_PARAM_POINTER, 0u);
        return E_NOT_OK;
    }
    if (Fls_IsRangeValid(SourceAddress, Length) == FALSE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_READ, FLS_E_PARAM_ADDRESS, SourceAddress);
        return E_NOT_OK;
    }
    if (Fls_CheckDmuReadyForCommand(SourceAddress) != E_OK)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_READ, FLS_E_BUSY, SourceAddress);
        return E_NOT_OK;
    }

    Fls_State.status = MEMIF_BUSY;
    Fls_State.result = MEMIF_JOB_PENDING;
    Fls_State.job = FLS_JOB_READ;
    Fls_State.phase = FLS_PHASE_IDLE;
    Fls_State.start = SourceAddress;
    Fls_State.length = Length;
    Fls_State.progress = 0u;
    Fls_State.phaseProgress = 0u;
    Fls_State.src = NULL_PTR;
    Fls_State.dst = TargetAddressPtr;
    return E_OK;
}

Std_ReturnType Fls_ReadImmediate(Fls_AddressType SourceAddress, uint8 *TargetAddressPtr, Fls_LengthType Length)
{
    if (Fls_State.status == MEMIF_UNINIT)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_READ, FLS_E_UNINIT, 0u);
        return E_NOT_OK;
    }
    if (Fls_State.status != MEMIF_IDLE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_READ, FLS_E_BUSY, 0u);
        return E_NOT_OK;
    }
    if (TargetAddressPtr == NULL_PTR)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_READ, FLS_E_PARAM_POINTER, 0u);
        return E_NOT_OK;
    }
    if (Fls_IsRangeValid(SourceAddress, Length) == FALSE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_READ, FLS_E_PARAM_ADDRESS, SourceAddress);
        return E_NOT_OK;
    }
    if (Fls_CheckDmuReadyForCommand(SourceAddress) != E_OK)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_READ, FLS_E_BUSY, SourceAddress);
        return E_NOT_OK;
    }

    return Fls_CopyReadableRange(SourceAddress, TargetAddressPtr, Length);
}

Std_ReturnType Fls_BlankCheck(Fls_AddressType TargetAddress, Fls_LengthType Length)
{
    if (Fls_State.status == MEMIF_UNINIT)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_BLANK_CHECK, FLS_E_UNINIT, 0u);
        return E_NOT_OK;
    }
    if (Fls_State.status != MEMIF_IDLE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_BLANK_CHECK, FLS_E_BUSY, 0u);
        return E_NOT_OK;
    }
    if (Fls_IsRangeValid(TargetAddress, Length) == FALSE)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_BLANK_CHECK, FLS_E_PARAM_ADDRESS, TargetAddress);
        return E_NOT_OK;
    }
    if (((TargetAddress % FLS_DFLASH0_PAGE_SIZE) != 0u) || ((Length % FLS_DFLASH0_PAGE_SIZE) != 0u))
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_BLANK_CHECK, FLS_E_PARAM_LENGTH, Length);
        return E_NOT_OK;
    }
    if (Fls_CheckDmuReadyForCommand(TargetAddress) != E_OK)
    {
        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_BLANK_CHECK, FLS_E_BUSY, TargetAddress);
        return E_NOT_OK;
    }

    return Fls_VerifyErasedRange(TargetAddress, Length);
}

void Fls_Cancel(void)
{
    if (Fls_State.status == MEMIF_BUSY)
    {
        Fls_FinishJob(MEMIF_JOB_CANCELED);
    }
}

MemIf_StatusType Fls_GetStatus(void)
{
    return Fls_State.status;
}

MemIf_JobResultType Fls_GetJobResult(void)
{
    return Fls_State.result;
}

void Fls_SetMode(MemIf_ModeType Mode)
{
    Fls_State.mode = Mode;
}

long long Fls_MainFunction_Counter = 0;

static void Fls_MainFunctionStep(void)
{
    Fls_AddressType current;
    const uint8 *src;
    uint8 *dst;
    Std_ReturnType result;
    Fls_DmuPollResultType pollResult;

    if (Fls_State.status != MEMIF_BUSY)
    {
        return;
    }

    /*
     * Hold the SMU trap masked for the WHOLE async write/erase job, not just the
     * command-issue instant: the DMU keeps operating for up to several ms after
     * the command returns, and a corrupt cell raises ALM7[17] during that window
     * (outside any per-command guard).  Read jobs are single-step and stay
     * covered by Fls_CopyReadableRange's own guard.
     */
    if (((Fls_State.job == FLS_JOB_WRITE) || (Fls_State.job == FLS_JOB_ERASE)) &&
            (Fls_DFlashJobTrapHeld == FALSE))
    {
        Fls_SuppressDFlashSmuTrap();
        Fls_DFlashJobTrapHeld = TRUE;
    }

    current = Fls_State.start + Fls_State.progress;

    switch (Fls_State.job)
    {
        case FLS_JOB_READ:
            dst = &Fls_State.dst[Fls_State.progress];
            if (Fls_CopyReadableRange(current, dst, Fls_State.length - Fls_State.progress) != E_OK)
            {
                MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_MAIN, FLS_E_VERIFY, current);
                Fls_FinishJob(MEMIF_JOB_FAILED);
                break;
            }
            Fls_FinishJob(MEMIF_JOB_OK);
            break;

        case FLS_JOB_WRITE:
            switch (Fls_State.phase)
            {
                case FLS_PHASE_IDLE:
                    result = Fls_StartProgramEnterPage(current);
                    if (result != E_OK)
                    {
                        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_MAIN, FLS_E_VERIFY, current);
                        Fls_FinishJob(MEMIF_JOB_FAILED);
                    }
                    else
                    {
                        Fls_State.phase = FLS_PHASE_WRITE_ENTER_WAIT;
                    }
                    break;

                case FLS_PHASE_WRITE_ENTER_WAIT:
                    pollResult = Fls_PollDmuReady();
                    if (pollResult == FLS_DMU_POLL_BUSY)
                    {
                        break;
                    }
                    if (pollResult == FLS_DMU_POLL_ERROR)
                    {
                        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_MAIN, FLS_E_VERIFY, current);
                        Fls_FinishJob(MEMIF_JOB_FAILED);
                        break;
                    }

                    src = &Fls_State.src[Fls_State.progress];
                    Fls_StartProgramWritePage(current, src);
                    Fls_State.phase = FLS_PHASE_WRITE_WAIT;
                    break;

                case FLS_PHASE_WRITE_WAIT:
                    pollResult = Fls_PollDmuReady();
                    if (pollResult == FLS_DMU_POLL_BUSY)
                    {
                        break;
                    }
                    if (pollResult == FLS_DMU_POLL_ERROR)
                    {
                        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_MAIN, FLS_E_VERIFY, current);
                        Fls_FinishJob(MEMIF_JOB_FAILED);
                        break;
                    }

#if (FLS_VERIFY_WRITE == STD_ON)
                    src = &Fls_State.src[Fls_State.progress];
                    if (memcmp((const void *)Fls_GetPhysicalAddress(current), src, FLS_DFLASH0_PAGE_SIZE) != 0)
                    {
                        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_MAIN, FLS_E_VERIFY, current);
                        Fls_FinishJob(MEMIF_JOB_FAILED);
                        break;
                    }
#endif
                    Fls_LastWrittenDFlashAddress = current;
                    Fls_DFlashWriteCounter++;
                    Fls_State.progress += FLS_DFLASH0_PAGE_SIZE;
                    Fls_State.phase = FLS_PHASE_IDLE;
                    Fls_State.phaseProgress = 0u;
                    if (Fls_State.progress >= Fls_State.length)
                    {
                        Fls_FinishJob(MEMIF_JOB_OK);
                    }
                    break;

                default:
                    MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_MAIN, FLS_E_VERIFY, current);
                    Fls_FinishJob(MEMIF_JOB_FAILED);
                    break;
            }
            break;

        case FLS_JOB_ERASE:
            switch (Fls_State.phase)
            {
                case FLS_PHASE_IDLE:
                    result = Fls_StartEraseOneSector(current);
                    if (result != E_OK)
                    {
                        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_MAIN, FLS_E_VERIFY, current);
                        Fls_FinishJob(MEMIF_JOB_FAILED);
                    }
                    else
                    {
                        Fls_State.phase = FLS_PHASE_ERASE_WAIT;
                    }
                    break;

                case FLS_PHASE_ERASE_WAIT:
                    pollResult = Fls_PollDmuReady();
                    if (pollResult == FLS_DMU_POLL_BUSY)
                    {
                        break;
                    }
                    if (pollResult == FLS_DMU_POLL_ERROR)
                    {
                        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_MAIN, FLS_E_VERIFY, current);
                        Fls_FinishJob(MEMIF_JOB_FAILED);
                        break;
                    }

#if (FLS_VERIFY_ERASE == STD_ON)
                    Fls_State.phase = FLS_PHASE_ERASE_VERIFY_WAIT;
                    Fls_State.phaseProgress = 0u;
#else
                    Fls_State.progress += FLS_DFLASH0_SECTOR_SIZE;
                    Fls_State.phase = FLS_PHASE_IDLE;
                    if (Fls_State.progress >= Fls_State.length)
                    {
                        Fls_FinishJob(MEMIF_JOB_OK);
                    }
#endif
                    break;

                case FLS_PHASE_ERASE_VERIFY_WAIT:
#if (FLS_VERIFY_ERASE == STD_ON)
                    if (Fls_VerifyErasedPageByRead(current + Fls_State.phaseProgress) != E_OK)
                    {
                        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_MAIN, FLS_E_VERIFY,
                                current + Fls_State.phaseProgress);
                        Fls_FinishJob(MEMIF_JOB_FAILED);
                        break;
                    }
                    Fls_State.phaseProgress += FLS_DFLASH0_PAGE_SIZE;
                    if (Fls_State.phaseProgress < FLS_DFLASH0_SECTOR_SIZE)
                    {
                        break;
                    }
#endif
                    Fls_State.progress += FLS_DFLASH0_SECTOR_SIZE;
                    Fls_State.phase = FLS_PHASE_IDLE;
                    Fls_State.phaseProgress = 0u;
                    if (Fls_State.progress >= Fls_State.length)
                    {
                        Fls_FinishJob(MEMIF_JOB_OK);
                    }
                    break;

                default:
                    MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_MAIN, FLS_E_VERIFY, current);
                    Fls_FinishJob(MEMIF_JOB_FAILED);
                    break;
            }
            break;

        default:
            Fls_FinishJob(MEMIF_JOB_FAILED);
            break;
    }

}

void Fls_MainFunction(void)
{
    uint8 stepBudget = FLS_MAIN_STEP_BUDGET;
    MemIf_StatusType statusBefore;
    Fls_JobType jobBefore;
    Fls_JobPhaseType phaseBefore;
    Fls_LengthType progressBefore;
    Fls_LengthType phaseProgressBefore;

    if (Fls_State.status != MEMIF_BUSY)
    {
        return;
    }

    do
    {
        statusBefore = Fls_State.status;
        jobBefore = Fls_State.job;
        phaseBefore = Fls_State.phase;
        progressBefore = Fls_State.progress;
        phaseProgressBefore = Fls_State.phaseProgress;

        Fls_MainFunctionStep();

        if (Fls_State.status != MEMIF_BUSY)
        {
            break;
        }

        if ((statusBefore == Fls_State.status) &&
                (jobBefore == Fls_State.job) &&
                (phaseBefore == Fls_State.phase) &&
                (progressBefore == Fls_State.progress) &&
                (phaseProgressBefore == Fls_State.phaseProgress))
        {
            break;
        }

        stepBudget--;
    } while (stepBudget > 0u);

    Fls_MainFunction_Counter++;
}
