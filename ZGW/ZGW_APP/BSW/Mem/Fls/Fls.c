#include <string.h>
#include "Fls.h"
#include "MemStack_Error.h"
#include "IfxFlash.h"
#include "IfxScuWdt.h"
#include "IfxCpu.h"

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
/* Page programs are spun to completion; sector erases need a larger bounded window under fast NVM draining. */
#define FLS_DMU_WRITE_BUSY_POLL_LIMIT    (16000u)
#define FLS_DMU_ERASE_BUSY_POLL_LIMIT    (1000000u)
#define FLS_DMU_D0_BUSY_MASK       (0x00000001u)
#define FLS_MAIN_STEP_BUDGET       (32u)

/*
 * Compile-time guards binding the hand-written DFlash geometry to the
 * authoritative iLLD device configuration (IfxFlash_cfg_TC37x.h for
 * DEVICE_TC37X).  These ensure the Fls config cannot silently drift from the
 * real TC375 DF0 layout, so the stack always spans the full data flash from its
 * true start address to its true end address.  For the TC375: DF0 =
 * 64 logical sectors x 4 KiB = 256 KiB at 0xAF000000..0xAF03FFFF, page = 8 B.
 */
#define FLS_ASSERT_CONCAT_(a, b) a##b
#define FLS_ASSERT_CONCAT(a, b)  FLS_ASSERT_CONCAT_(a, b)
#define FLS_STATIC_ASSERT(cond) \
    typedef char FLS_ASSERT_CONCAT(Fls_StaticAssert_, __LINE__)[(cond) ? 1 : -1]

FLS_STATIC_ASSERT(FLS_DFLASH0_BASE_ADDRESS == IFXFLASH_DFLASH_START);
FLS_STATIC_ASSERT(FLS_DFLASH0_TOTAL_SIZE == IFXFLASH_DFLASH_SIZE);
FLS_STATIC_ASSERT(FLS_DFLASH0_PAGE_SIZE == IFXFLASH_DFLASH_PAGE_LENGTH);
FLS_STATIC_ASSERT(FLS_DFLASH0_SECTOR_SIZE ==
        (IFXFLASH_DFLASH_SIZE / IFXFLASH_DFLASH_NUM_LOG_SECTORS));
FLS_STATIC_ASSERT((FLS_DFLASH0_BASE_ADDRESS + FLS_DFLASH0_TOTAL_SIZE - 1u) ==
        IFXFLASH_DFLASH_END);

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
volatile uint32 Fls_DmuBusyRejectCounter = 0u;
volatile uint32 Fls_DmuTimeoutJob = 0u;
volatile uint32 Fls_DmuTimeoutPhase = 0u;
volatile uint32 Fls_DmuTimeoutAddress = 0u;
volatile uint32 Fls_DmuTimeoutProgress = 0u;
volatile uint32 Fls_DmuRejectAddress = 0u;

static Std_ReturnType Fls_CopyReadableRange(Fls_AddressType address, uint8 *target, Fls_LengthType length);

static uint32 Fls_GetDmuBusyPollLimit(void)
{
    if (Fls_State.job == FLS_JOB_ERASE)
    {
        return FLS_DMU_ERASE_BUSY_POLL_LIMIT;
    }

    return FLS_DMU_WRITE_BUSY_POLL_LIMIT;
}

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

/*
 * DMU_HF_ERRSR / DMU_HF_STATUS are read-only flash status registers.  EndInit
 * (CPU + safety) protects WRITES to safety-critical registers; it has no bearing
 * on reads.  The old code cleared and re-set both EndInits around every status
 * read - including inside the page-program busy spin, thousands of times per
 * block - which was pure overhead (and repeatedly opened the safety-EndInit
 * window).  Reading the registers directly is correct and removes that cost.
 */
static uint32 Fls_ReadDmuErrorStatus(void)
{
    return DMU_HF_ERRSR.U;
}

static uint32 Fls_ReadDmuStatus(void)
{
    return DMU_HF_STATUS.U;
}

static void Fls_RecordDmuStuck(uint32 status)
{
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

    /* Reject only if the DMU is busy RIGHT NOW (a command cannot be issued mid-
     * operation - a true hardware constraint). This is transient: the command
     * fails, the caller retries, and the next attempt succeeds once the DMU
     * drains. There is deliberately no persistent "recovery required" latch - a
     * single transient busy must not block all future DFlash access until the
     * next reset (that was a self-inflicted blocked-in-busy deadlock). */
    if ((status & FLS_DMU_D0_BUSY_MASK) != 0u)
    {
        Fls_DmuBusyRejectCounter++;
        Fls_DmuRejectAddress = address;
        return E_NOT_OK;
    }

    return E_OK;
}

static Fls_DmuPollResultType Fls_PollDmuReady(void)
{
    uint32 status;
    Fls_DmuPollResultType result = FLS_DMU_POLL_READY;

    if (FLS_FLASH_MODULE != 0u)
    {
        Fls_LastDmuError = FLS_DMU_WAIT_ERROR;
        return FLS_DMU_POLL_ERROR;
    }

    Fls_DmuWaitCounter++;

    /* Poll the read-only DMU status directly - no EndInit window (see
     * Fls_ReadDmuStatus). This is the hot path of the page-program busy spin. */
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

        if (Fls_DmuWaitConsecutiveBusyCounter >= Fls_GetDmuBusyPollLimit())
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
    if (Fls_CheckDmuReadyForCommand(address) != E_OK)
    {
        return E_NOT_OK;
    }

    Fls_ClearDmuStatus();
    memcpy(target, (const void *)Fls_GetPhysicalAddress(address), (size_t)length);

    return E_OK;
}

static void Fls_FinishJob(MemIf_JobResultType result)
{
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

/*
 * Issue a complete DFlash page program (enter page mode + load assembly buffer
 * + write command) in a SINGLE protected critical section.
 *
 * The previous implementation split this across two async phases with a DMU
 * poll in between (FLS_PHASE_WRITE_ENTER_WAIT).  enterPageMode for DFlash only
 * writes the 0x5D page-mode command to the command buffer - it does NOT start
 * an asynchronous DMU program (the long DMU-busy window only opens after
 * writePage), so that intermediate poll was a wasted round-trip per page.
 * Collapsing enter+load+write removes one main-function pass and one
 * endinit/interrupt toggle per 8-byte page, so a block is programmed in the
 * fewest possible steps (req: write DFlash ASAP, no unnecessary steps).
 *
 * enterPageMode -> loadPage2X32 -> writePage are sequenced by the __dsync()
 * inside each iLLD primitive, so no DMU wait is required between them; only the
 * post-writePage completion is asynchronous and is polled by the caller.
 */
static Std_ReturnType Fls_StartProgramPage(Fls_AddressType address, const uint8 *data)
{
    uint32 physicalAddress = Fls_GetPhysicalAddress(address);
    uint32 word0;
    uint32 word1;
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

    Fls_ClearDmuStatus();

    memcpy(&word0, &data[0], sizeof(word0));
    memcpy(&word1, &data[4], sizeof(word1));

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    interruptState = IfxCpu_disableInterrupts();
#endif

    Fls_ClearDmuEndinit(&cpuPassword, &safetyPassword);
    enterResult = IfxFlash_enterPageMode(physicalAddress);
    if (enterResult == 0u)
    {
        IfxFlash_loadPage2X32(physicalAddress, word0, word1);
        IfxFlash_writePage(physicalAddress);
    }
    Fls_SetDmuEndinit(cpuPassword, safetyPassword);

#if (FLS_DISABLE_INTERRUPTS_FOR_COMMAND == STD_ON)
    IfxCpu_restoreInterrupts(interruptState);
#endif

    return (enterResult == 0u) ? E_OK : E_NOT_OK;
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

    return E_OK;
}

void Fls_Init(const Fls_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;
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
                    src = &Fls_State.src[Fls_State.progress];
                    result = Fls_StartProgramPage(current, src);
                    if (result != E_OK)
                    {
                        MemStack_ReportError(MEMSTACK_MODULE_ID_FLS, FLS_API_MAIN, FLS_E_VERIFY, current);
                        Fls_FinishJob(MEMIF_JOB_FAILED);
                    }
                    else
                    {
                        Fls_State.phase = FLS_PHASE_WRITE_WAIT;
                    }
                    break;

                case FLS_PHASE_WRITE_WAIT:
                    /* A page program is short (tens of microseconds), so poll it
                     * to completion in this call instead of yielding once per
                     * poll: the whole multi-page block is committed in the fewest
                     * main-function passes (req: 100% CPU is acceptable to finish
                     * ASAP).  The consecutive-busy guard inside Fls_PollDmuReady
                     * still bounds the spin and turns a stuck DMU into
                     * FLS_DMU_POLL_ERROR.  Sector erase keeps its single-poll /
                     * yield model below, where the much longer DMU-busy window
                     * must accumulate across calls. */
                    do
                    {
                        pollResult = Fls_PollDmuReady();
                    } while (pollResult == FLS_DMU_POLL_BUSY);

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
