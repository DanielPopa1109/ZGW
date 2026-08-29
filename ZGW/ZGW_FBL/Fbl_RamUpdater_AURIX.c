#include "Ifx_Types.h"
#include "IfxCpu_Intrinsics.h"
#include "IfxCpu_reg.h"
#include "IfxCpu_Trap.h"
#include "IfxDmu_reg.h"
#include "IfxScu_reg.h"
#include "IfxStm_reg.h"
#include "FblBlu.h"

#define RAM_CODE         FBL_RAM_BLU_CODE
#define RAM_HELPER_CODE  FBL_RAM_HELPER_CODE
#define RAM_FLASH_CODE   FBL_RAM_FLASH_CODE
#define RAM_FLASH_HELPER FBL_RAM_FLASH_HELPER_CODE
#define RAM_DATA         FBL_RAM_DSPR_DATA
#define RAM_TRAP_CODE    FBL_RAM_TRAP_CODE

#define CPU0_PSPR_GLOBAL_START           0x70100000u
#define CPU0_PSPR_GLOBAL_END             0x7010FFFFu
#define CPU0_PSPR_LOCAL_START            0xC0000000u
#define CPU0_PSPR_LOCAL_END              0xC000FFFFu
#define CPU0_DSPR_GLOBAL_START           0x70000000u
#define CPU0_DSPR_GLOBAL_END             0x7003BFFFu
#define CPU0_DSPR_LOCAL_START            0xD0000000u
#define CPU0_DSPR_LOCAL_END              0xD003BFFFu

#define PFLASH_START_NC                  0xA0000000u
#define PFLASH_END_NC                    0xA05FFFFFu
#define PFLASH_BANK_A_END                0xA02FFFFFu
#define PFLASH_BANK_B_END                PFLASH_END_NC
#define PFLASH_NONCACHED_BASE            0xA0000000u
#define PFLASH_ALIAS_MASK                0x1FFFFFFFu
#define PFLASH_PAGE_SIZE                 32u
#define PFLASH_SECTOR_SIZE               0x4000u
#define PFLASH_SECTOR_SHIFT              14u
#define PFLASH_LOGICAL_SECTOR_SIZE       PFLASH_SECTOR_SIZE
#define PFLASH_PHYSICAL_SECTOR_SIZE      0x00100000u
/* TC37x Erratum FLASH_TC.053: limit PFLASH logical erase commands to 256 KiB. */
#define PFLASH_ERASE_MAX_COMMAND_SIZE    0x00040000u
#define PFLASH_ERASE_MAX_SECTORS         (PFLASH_ERASE_MAX_COMMAND_SIZE / PFLASH_SECTOR_SIZE)

#define FBL_FLASH_OK                     0u
#define FBL_FLASH_ERROR_RANGE            1u
#define FBL_FLASH_ERROR_TIMEOUT          2u
#define FBL_FLASH_ERROR_DMU              3u

#define FLASH_CMD_BASE                   0xAF000000u
#define FLASH_TYPE_P0                    2u
#define FLASH_TYPE_P1                    3u
#define FLASH_DMU_ERROR_MASK             0x0000001Fu
#define FLASH_DMU_CLEAR_MASK             0x0000001Eu
#define FLASH_WAIT_TIMEOUT               0x02000000u

typedef char FblRam_AssertUint32Size[(sizeof(uint32) == 4u) ? 1 : -1];
typedef char FblRam_AssertPflashPageSize[(PFLASH_PAGE_SIZE == 32u) ? 1 : -1];

RAM_DATA volatile uint32 g_FblRamRuntimeActive;
RAM_DATA volatile uint32 g_FblRamRuntimeTrapClass;
RAM_DATA volatile uint32 g_FblRamRuntimeTrapTin;
RAM_DATA volatile uint32 g_FblRamRuntimeLastAddress;
RAM_DATA volatile uint32 g_FblRamRuntimeLastDmuError;
RAM_DATA volatile uint32 g_FblRamRuntimeLastDmuStatus;
RAM_DATA volatile uint32 g_FblRamRuntimeLastFlashType;
RAM_DATA volatile uint32 g_FblRamRuntimeLastWaitMask;
RAM_DATA volatile uint32 g_FblRamRuntimeLastWaitGuard;
RAM_DATA volatile uint32 g_FblEraseCommandCount;
RAM_DATA volatile uint32 g_FblEraseDmuCommandCount;
RAM_DATA volatile uint32 g_FblEraseLastStart;
RAM_DATA volatile uint32 g_FblEraseLastLength;
RAM_DATA volatile uint32 g_FblEraseLastChunkLength;
RAM_DATA volatile uint32 g_FblEraseLastSectorCount;
RAM_DATA volatile uint32 g_FblEraseLastPhysicalBoundary;
RAM_DATA volatile uint32 g_FblEraseLastFlashType;
RAM_DATA volatile uint32 g_FblEraseLastError;
RAM_DATA volatile uint32 g_FblEraseErrorBeforeCommand;
RAM_DATA volatile uint32 g_FblEraseErrorAfterCommand;
RAM_DATA volatile uint32 g_FblEraseFailedCommandIndex;
RAM_DATA volatile uint32 g_FblEraseDmuStatusBeforeClear;
RAM_DATA volatile uint32 g_FblEraseDmuErrorBeforeClear;
RAM_DATA volatile uint32 g_FblEraseDmuStatusAfterCommand;
RAM_DATA volatile uint32 g_FblEraseDmuErrorAfterCommand;
RAM_DATA volatile uint32 g_FblEraseDmuStatusAfterWait;
RAM_DATA volatile uint32 g_FblEraseDmuErrorAfterWait;
RAM_DATA volatile uint32 g_FblEraseBank0CommandCount;
RAM_DATA volatile uint32 g_FblEraseBank1CommandCount;
RAM_DATA volatile uint32 g_FblEraseDmuStartTick;
RAM_DATA volatile uint32 g_FblEraseDmuEndTick;
RAM_DATA volatile uint32 g_FblEraseDmuTicks;
RAM_DATA volatile uint32 g_FblEraseWaitStartTick;
RAM_DATA volatile uint32 g_FblEraseWaitEndTick;
RAM_DATA volatile uint32 g_FblEraseWaitTicks;
RAM_DATA volatile uint32 g_FblRamRuntimeDestructivePhase;
RAM_DATA volatile uint32 g_FblRuntimeClosureFailStep;
RAM_DATA volatile uint32 g_FblEthRuntimeClosureFailStep;
RAM_DATA volatile uint32 g_LwipGethRuntimeClosureFailStep;

static void FblRamRuntime_TrapHandler(uint32 trapClass, uint32 tin) RAM_CODE;
static void FblRamRuntime_TrapHandlerClass0(uint32 tin) RAM_CODE;
static void FblRamRuntime_TrapHandlerClass1(uint32 tin) RAM_CODE;
static void FblRamRuntime_TrapHandlerClass2(uint32 tin) RAM_CODE;
static void FblRamRuntime_TrapHandlerClass3(uint32 tin) RAM_CODE;
static void FblRamRuntime_TrapHandlerClass4(uint32 tin) RAM_CODE;
static void FblRamRuntime_TrapHandlerClass5(uint32 tin) RAM_CODE;
static void FblRamRuntime_TrapHandlerClass6(uint32 tin) RAM_CODE;
static void FblRamRuntime_TrapHandlerClass7(uint32 tin) RAM_CODE;
void FblRamRuntime_TrapVectorTable(void) RAM_TRAP_CODE;
static uint8 FblRamFlash_WaitUnbusy(uint32 flashType) RAM_FLASH_CODE;
static uint32 FblRamFlash_Bank(uint32 address) RAM_FLASH_CODE;
static uint32 FblRamFlash_BankEndExclusive(uint32 flashType) RAM_FLASH_CODE;
static uint32 FblRamFlash_ToNonCached(uint32 address) RAM_FLASH_CODE;
static uint32 FblRamFlash_GetEraseChunkLength(uint32 address, uint32 remaining) RAM_FLASH_CODE;
static uint32 FblRamFlash_ValidateEraseRange(uint32 address, uint32 length) RAM_FLASH_CODE;
static uint32 FblRamFlash_IssueEraseMultiple(uint32 address, uint32 sectorCount, uint32 flashType) RAM_FLASH_CODE;
uint8 FblRamFlash_HasError(void) RAM_FLASH_CODE;
static uint8 FblRamFlash_EnterPageMode(uint32 address) RAM_FLASH_HELPER;
static void FblRamFlash_Load2X32(uint32 wordLow, uint32 wordHigh) RAM_FLASH_HELPER;
static void FblRamFlash_WritePage(uint32 address) RAM_FLASH_HELPER;

RAM_HELPER_CODE FblRam_InterruptState FblRam_DisableInterrupts(void)
{
    Ifx_CPU_ICR icr;
    icr.U = __mfcr(CPU_ICR);
    __disable();
    __nop();
    return (icr.B.IE != 0u) ? 1u : 0u;
}

RAM_HELPER_CODE void FblRam_RestoreInterrupts(FblRam_InterruptState state)
{
    if(state != 0u)
    {
        __enable();
    }
}

RAM_HELPER_CODE uint16 FblRam_GetCpuWatchdogPassword(void)
{
    uint16 password = MODULE_SCU.WDTCPU[0].CON0.B.PW;
    password ^= 0x003Fu;
    return password;
}

RAM_HELPER_CODE void FblRam_ClearCpuEndinit(uint16 password)
{
    volatile Ifx_SCU_WDTCPU_CON0 *watchdog = &SCU_WDTCPU0_CON0;
    Ifx_SCU_WDTCPU_CON0 con0;

    if(watchdog->B.LCK != 0u)
    {
        con0.U = 0u;
        con0.B.ENDINIT = 1u;
        con0.B.LCK = 0u;
        con0.B.PW = password;
        con0.B.REL = watchdog->B.REL;
        watchdog->U = con0.U;
    }

    con0.U = 0u;
    con0.B.ENDINIT = 0u;
    con0.B.LCK = 1u;
    con0.B.PW = password;
    con0.B.REL = watchdog->B.REL;
    watchdog->U = con0.U;

    while(watchdog->B.ENDINIT != 0u)
    {}
    __dsync();
}

RAM_HELPER_CODE void FblRam_SetCpuEndinit(uint16 password)
{
    volatile Ifx_SCU_WDTCPU_CON0 *watchdog = &SCU_WDTCPU0_CON0;
    Ifx_SCU_WDTCPU_CON0 con0;

    if(watchdog->B.LCK != 0u)
    {
        con0.U = 0u;
        con0.B.ENDINIT = 1u;
        con0.B.LCK = 0u;
        con0.B.PW = password;
        con0.B.REL = watchdog->B.REL;
        watchdog->U = con0.U;
    }

    con0.U = 0u;
    con0.B.ENDINIT = 1u;
    con0.B.LCK = 1u;
    con0.B.PW = password;
    con0.B.REL = watchdog->B.REL;
    watchdog->U = con0.U;

    while(watchdog->B.ENDINIT == 0u)
    {}
    __dsync();
}

RAM_HELPER_CODE uint16 FblRam_GetSafetyWatchdogPassword(void)
{
    uint16 password = MODULE_SCU.WDTS.CON0.B.PW;
    password ^= 0x003Fu;
    return password;
}

RAM_HELPER_CODE void FblRam_ClearSafetyEndinit(uint16 password)
{
    Ifx_SCU_WDTS_CON0 con0;

    if(SCU_WDTS_CON0.B.LCK != 0u)
    {
        con0.U = 0u;
        con0.B.ENDINIT = 1u;
        con0.B.LCK = 0u;
        con0.B.PW = password;
        con0.B.REL = SCU_WDTS_CON0.B.REL;
        SCU_WDTS_CON0.U = con0.U;
    }

    con0.U = 0u;
    con0.B.ENDINIT = 0u;
    con0.B.LCK = 1u;
    con0.B.PW = password;
    con0.B.REL = SCU_WDTS_CON0.B.REL;
    SCU_WDTS_CON0.U = con0.U;

    while(SCU_WDTS_CON0.B.ENDINIT != 0u)
    {}
    __dsync();
}

RAM_HELPER_CODE void FblRam_SetSafetyEndinit(uint16 password)
{
    Ifx_SCU_WDTS_CON0 con0;

    if(SCU_WDTS_CON0.B.LCK != 0u)
    {
        con0.U = 0u;
        con0.B.ENDINIT = 1u;
        con0.B.LCK = 0u;
        con0.B.PW = password;
        con0.B.REL = SCU_WDTS_CON0.B.REL;
        SCU_WDTS_CON0.U = con0.U;
    }

    con0.U = 0u;
    con0.B.ENDINIT = 1u;
    con0.B.LCK = 1u;
    con0.B.PW = password;
    con0.B.REL = SCU_WDTS_CON0.B.REL;
    SCU_WDTS_CON0.U = con0.U;

    while(SCU_WDTS_CON0.B.ENDINIT == 0u)
    {}
    __dsync();
}

RAM_HELPER_CODE void FblRam_InvalidateProgramCache(void)
{
    uint16 password = FblRam_GetCpuWatchdogPassword();
    Ifx_CPU_PCON1 pcon1;

    FblRam_ClearCpuEndinit(password);
    pcon1.U = __mfcr(CPU_PCON1);
    pcon1.B.PCINV = 1u;
    __dsync();
    __mtcr(CPU_PCON1, pcon1.U);
    __isync();
    FblRam_SetCpuEndinit(password);
}

RAM_HELPER_CODE void FblRam_RequestSystemReset(void)
{
    uint16 safetyPassword;
    uint16 cpuPassword;

    (void)FblRam_DisableInterrupts();
    __dsync();

    safetyPassword = FblRam_GetSafetyWatchdogPassword();
    FblRam_ClearSafetyEndinit(safetyPassword);
    MODULE_SCU.RSTCON.B.SW = 2u;
    FblRam_SetSafetyEndinit(safetyPassword);

    cpuPassword = FblRam_GetCpuWatchdogPassword();
    FblRam_ClearCpuEndinit(cpuPassword);
    MODULE_SCU.RSTCON2.B.USRINFO = 0u;
    MODULE_SCU.SWRSTCON.B.SWRSTREQ = 1u;
    FblRam_SetCpuEndinit(cpuPassword);

    for(;;)
    {
        __nop();
    }
}

RAM_HELPER_CODE void FblRam_CopyBytes(void *dst, const void *src, uint32 length)
{
    uint8 *d = (uint8 *)dst;
    const uint8 *s = (const uint8 *)src;
    uint32 i;

    for(i = 0u; i < length; i++)
    {
        d[i] = s[i];
    }
}

RAM_HELPER_CODE void *FblRam_Memcpy(void *destination, const void *source, uint32 length)
{
    uint8 *d = (uint8 *)destination;
    const uint8 *s = (const uint8 *)source;
    uint32 i;

    for(i = 0u; i < length; i++)
    {
        d[i] = s[i];
    }

    return destination;
}

RAM_HELPER_CODE void *FblRam_Memset(void *destination, uint8 value, uint32 length)
{
    uint8 *d = (uint8 *)destination;
    uint32 i;

    for(i = 0u; i < length; i++)
    {
        d[i] = value;
    }

    return destination;
}

RAM_HELPER_CODE void *FblRam_Memmove(void *destination, const void *source, uint32 length)
{
    uint8 *d = (uint8 *)destination;
    const uint8 *s = (const uint8 *)source;
    uint32 i;

    if((d > s) && (d < &s[length]))
    {
        for(i = length; i > 0u; i--)
        {
            d[i - 1u] = s[i - 1u];
        }
    }
    else
    {
        for(i = 0u; i < length; i++)
        {
            d[i] = s[i];
        }
    }

    return destination;
}

RAM_HELPER_CODE sint32 FblRam_Memcmp(const void *left, const void *right, uint32 length)
{
    const uint8 *a = (const uint8 *)left;
    const uint8 *b = (const uint8 *)right;

    while(length > 0u)
    {
        if(*a != *b)
        {
            return (*a < *b) ? -1 : 1;
        }

        ++a;
        ++b;
        --length;
    }

    return 0;
}

RAM_HELPER_CODE uint32 FblRam_Strlen(const char *text)
{
    uint32 length = 0u;

    if(text == (const char *)0)
    {
        return 0u;
    }

    while(text[length] != '\0')
    {
        length++;
    }

    return length;
}

RAM_HELPER_CODE void FblRam_SetBytes(void *dst, uint8 value, uint32 length)
{
    uint8 *d = (uint8 *)dst;
    uint32 i;

    for(i = 0u; i < length; i++)
    {
        d[i] = value;
    }
}

RAM_HELPER_CODE void FblRam_MoveBytes(void *dst, const void *src, uint32 length)
{
    uint8 *d = (uint8 *)dst;
    const uint8 *s = (const uint8 *)src;
    uint32 i;

    if((d > s) && (d < &s[length]))
    {
        for(i = length; i > 0u; i--)
        {
            d[i - 1u] = s[i - 1u];
        }
    }
    else
    {
        FblRam_CopyBytes(dst, src, length);
    }
}

RAM_HELPER_CODE sint32 FblRam_CompareBytes(const void *a, const void *b, uint32 length)
{
    const uint8 *pa = (const uint8 *)a;
    const uint8 *pb = (const uint8 *)b;
    uint32 i;

    for(i = 0u; i < length; i++)
    {
        if(pa[i] != pb[i])
        {
            return (sint32)pa[i] - (sint32)pb[i];
        }
    }

    return 0;
}

RAM_CODE uint8 FblRamRuntime_IsExecutableAddress(uint32 address)
{
    if((address >= CPU0_PSPR_GLOBAL_START) && (address <= CPU0_PSPR_GLOBAL_END))
    {
        return 1u;
    }

    if((address >= CPU0_PSPR_LOCAL_START) && (address <= CPU0_PSPR_LOCAL_END))
    {
        return 1u;
    }

    if((address >= CPU0_DSPR_GLOBAL_START) && (address <= CPU0_DSPR_GLOBAL_END))
    {
        return 1u;
    }

    if((address >= CPU0_DSPR_LOCAL_START) && (address <= CPU0_DSPR_LOCAL_END))
    {
        return 1u;
    }

    return 0u;
}

RAM_CODE uint8 FblRamRuntime_EnterCritical(void)
{
    FblRam_InterruptState state = FblRam_DisableInterrupts();
    __isync();

    if((FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_EnterCritical) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_TrapVectorTable) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_TrapHandlerClass0) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_TrapHandlerClass1) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_TrapHandlerClass2) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_TrapHandlerClass3) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_TrapHandlerClass4) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_TrapHandlerClass5) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_TrapHandlerClass6) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_TrapHandlerClass7) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_SetDestructivePhase) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamFlash_EraseRange) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamFlash_ProgramPage) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamFlash_HasError) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRamRuntime_RequestReset) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRam_RequestSystemReset) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRam_InvalidateProgramCache) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRam_CopyBytes) == 0u) ||
       (FblRamRuntime_IsExecutableAddress((uint32)FblRam_SetBytes) == 0u))
    {
        FblRam_RestoreInterrupts(state);
        return 0u;
    }

    __mtcr(CPU_BTV, (uint32)FblRamRuntime_TrapVectorTable);
    __isync();

    g_FblRamRuntimeActive = 1u;
    __dsync();
    return 1u;
}

RAM_CODE uint8 FblRamRuntime_IsActive(void)
{
    return (g_FblRamRuntimeActive != 0u) ? 1u : 0u;
}

RAM_CODE void FblRamRuntime_SetDestructivePhase(uint8 active)
{
    g_FblRamRuntimeDestructivePhase = (active != 0u) ? 1u : 0u;
    __dsync();
}

RAM_CODE static void FblRamRuntime_TrapHandler(uint32 trapClass, uint32 tin)
{
    g_FblRamRuntimeTrapClass = trapClass;
    g_FblRamRuntimeTrapTin = tin;
    g_FblRamRuntimeLastAddress = (uint32)__mfcr(CPU_DEADD);
    __dsync();

    if(g_FblRamRuntimeDestructivePhase != 0u)
    {
        Fbl_BluEnterRecoveryWaitFromTrap();
        __disable();
        for(;;)
        {
            __nop();
        }
    }

    FblRamRuntime_RequestReset();
}

RAM_CODE static void FblRamRuntime_TrapHandlerClass0(uint32 tin) { FblRamRuntime_TrapHandler(0u, tin); }
RAM_CODE static void FblRamRuntime_TrapHandlerClass1(uint32 tin) { FblRamRuntime_TrapHandler(1u, tin); }
RAM_CODE static void FblRamRuntime_TrapHandlerClass2(uint32 tin) { FblRamRuntime_TrapHandler(2u, tin); }
RAM_CODE static void FblRamRuntime_TrapHandlerClass3(uint32 tin) { FblRamRuntime_TrapHandler(3u, tin); }
RAM_CODE static void FblRamRuntime_TrapHandlerClass4(uint32 tin) { FblRamRuntime_TrapHandler(4u, tin); }
RAM_CODE static void FblRamRuntime_TrapHandlerClass5(uint32 tin) { FblRamRuntime_TrapHandler(5u, tin); }
RAM_CODE static void FblRamRuntime_TrapHandlerClass6(uint32 tin) { FblRamRuntime_TrapHandler(6u, tin); }
RAM_CODE static void FblRamRuntime_TrapHandlerClass7(uint32 tin) { FblRamRuntime_TrapHandler(7u, tin); }

RAM_TRAP_CODE void FblRamRuntime_TrapVectorTable(void)
{
    IfxCpu_Tsr_CallTSR(FblRamRuntime_TrapHandlerClass0);
    IfxCpu_Tsr_CallTSR(FblRamRuntime_TrapHandlerClass1);
    IfxCpu_Tsr_CallTSR(FblRamRuntime_TrapHandlerClass2);
    IfxCpu_Tsr_CallCSATSR(FblRamRuntime_TrapHandlerClass3);
    IfxCpu_Tsr_CallTSR(FblRamRuntime_TrapHandlerClass4);
    IfxCpu_Tsr_CallTSR(FblRamRuntime_TrapHandlerClass5);
    IfxCpu_Tsr_CallTSR(FblRamRuntime_TrapHandlerClass6);
    IfxCpu_Tsr_CallTSR(FblRamRuntime_TrapHandlerClass7);
}

RAM_CODE void FblRamRuntime_RequestReset(void)
{
    FblRam_RequestSystemReset();
}

RAM_FLASH_CODE void FblRamFlash_ClearStatus(void)
{
    volatile uint32 *command = (volatile uint32 *)(FLASH_CMD_BASE | 0x5554u);

    DMU_HF_CLRE.U = FLASH_DMU_CLEAR_MASK;
    *command = 0xFAu;
    __dsync();
}

RAM_FLASH_CODE uint8 FblRamFlash_HasError(void)
{
    g_FblRamRuntimeLastDmuError = DMU_HF_ERRSR.U;
    return ((g_FblRamRuntimeLastDmuError & FLASH_DMU_ERROR_MASK) != 0u) ? 1u : 0u;
}

RAM_FLASH_CODE static uint8 FblRamFlash_WaitUnbusy(uint32 flashType)
{
    uint32 guard = FLASH_WAIT_TIMEOUT;
    uint32 mask = 1u << flashType;

#if (FBL_FLASH_UPDATE_DEBUG_STATE_EACH_PAGE != 0u)
    g_FblRamRuntimeLastFlashType = flashType;
    g_FblRamRuntimeLastWaitMask = mask;
#endif
    g_FblEraseWaitStartTick = STM0_TIM0.U;

    while((DMU_HF_STATUS.U & mask) != 0u)
    {
#if (FBL_FLASH_UPDATE_DEBUG_STATE_EACH_PAGE != 0u)
        g_FblRamRuntimeLastDmuStatus = DMU_HF_STATUS.U;
        g_FblRamRuntimeLastWaitGuard = guard;
#endif
        if(guard == 0u)
        {
            g_FblEraseWaitEndTick = STM0_TIM0.U;
            g_FblEraseWaitTicks = g_FblEraseWaitEndTick - g_FblEraseWaitStartTick;
            return 0u;
        }
        guard--;
    }

    __dsync();
#if (FBL_FLASH_UPDATE_DEBUG_STATE_EACH_PAGE != 0u)
    g_FblRamRuntimeLastDmuStatus = DMU_HF_STATUS.U;
    g_FblRamRuntimeLastWaitGuard = guard;
#endif
    g_FblEraseWaitEndTick = STM0_TIM0.U;
    g_FblEraseWaitTicks = g_FblEraseWaitEndTick - g_FblEraseWaitStartTick;
    return 1u;
}

RAM_FLASH_CODE static uint32 FblRamFlash_Bank(uint32 address)
{
    return (address > PFLASH_BANK_A_END) ? FLASH_TYPE_P1 : FLASH_TYPE_P0;
}

RAM_FLASH_CODE static uint32 FblRamFlash_BankEndExclusive(uint32 flashType)
{
    if(flashType == FLASH_TYPE_P0)
    {
        return PFLASH_BANK_A_END + 1u;
    }

    return PFLASH_BANK_B_END + 1u;
}

RAM_FLASH_CODE static uint32 FblRamFlash_ToNonCached(uint32 address)
{
    return (address & PFLASH_ALIAS_MASK) | PFLASH_NONCACHED_BASE;
}

RAM_FLASH_CODE static uint32 FblRamFlash_GetEraseChunkLength(uint32 address, uint32 remaining)
{
    uint32 physicalOffset;
    uint32 untilPhysicalBoundary;
    uint32 untilBankBoundary;
    uint32 chunk;

    physicalOffset = address & (PFLASH_PHYSICAL_SECTOR_SIZE - 1u);
    untilPhysicalBoundary = PFLASH_PHYSICAL_SECTOR_SIZE - physicalOffset;
    untilBankBoundary = FblRamFlash_BankEndExclusive(FblRamFlash_Bank(address)) - address;

    chunk = remaining;

    if(chunk > PFLASH_ERASE_MAX_COMMAND_SIZE)
    {
        chunk = PFLASH_ERASE_MAX_COMMAND_SIZE;
    }

    if(chunk > untilPhysicalBoundary)
    {
        chunk = untilPhysicalBoundary;
    }

    if(chunk > untilBankBoundary)
    {
        chunk = untilBankBoundary;
    }

    g_FblEraseLastPhysicalBoundary = address + untilPhysicalBoundary;
    return chunk;
}

RAM_FLASH_CODE static uint32 FblRamFlash_ValidateEraseRange(uint32 address, uint32 length)
{
    if((length == 0u) ||
       (address < PFLASH_START_NC) ||
       (address > PFLASH_END_NC) ||
       ((address & (PFLASH_LOGICAL_SECTOR_SIZE - 1u)) != 0u) ||
       ((length & (PFLASH_LOGICAL_SECTOR_SIZE - 1u)) != 0u) ||
       ((address + length) < address) ||
       ((address + length - 1u) > PFLASH_END_NC))
    {
        return FBL_FLASH_ERROR_RANGE;
    }

    return FBL_FLASH_OK;
}

RAM_FLASH_CODE static uint32 FblRamFlash_IssueEraseMultiple(uint32 address, uint32 sectorCount, uint32 flashType)
{
    uint16 password;

    if((sectorCount == 0u) || (sectorCount > PFLASH_ERASE_MAX_SECTORS))
    {
        g_FblEraseLastError = FBL_FLASH_ERROR_RANGE;
        return FBL_FLASH_ERROR_RANGE;
    }

    g_FblEraseCommandCount++;
    g_FblEraseDmuCommandCount++;
    g_FblEraseLastStart = address;
    g_FblEraseLastLength = sectorCount << PFLASH_SECTOR_SHIFT;
    g_FblEraseLastChunkLength = g_FblEraseLastLength;
    g_FblEraseLastSectorCount = sectorCount;
    g_FblEraseLastFlashType = flashType;
    g_FblRamRuntimeLastAddress = address;

    if(flashType == FLASH_TYPE_P0)
    {
        g_FblEraseBank0CommandCount++;
    }
    else
    {
        g_FblEraseBank1CommandCount++;
    }

    password = FblRam_GetSafetyWatchdogPassword();
    g_FblEraseDmuStartTick = STM0_TIM0.U;
    FblRam_ClearSafetyEndinit(password);
    g_FblEraseDmuStatusBeforeClear = DMU_HF_STATUS.U;
    g_FblEraseDmuErrorBeforeClear = DMU_HF_ERRSR.U;
    g_FblEraseErrorBeforeCommand = g_FblEraseDmuErrorBeforeClear;
    FblRamFlash_ClearStatus();
    *((volatile uint32 *)(FLASH_CMD_BASE | 0xAA50u)) = address;
    *((volatile uint32 *)(FLASH_CMD_BASE | 0xAA58u)) = sectorCount;
    *((volatile uint32 *)(FLASH_CMD_BASE | 0xAAA8u)) = 0x80u;
    *((volatile uint32 *)(FLASH_CMD_BASE | 0xAAA8u)) = 0x50u;
    __dsync();
    g_FblEraseDmuStatusAfterCommand = DMU_HF_STATUS.U;
    g_FblEraseDmuErrorAfterCommand = DMU_HF_ERRSR.U;
    g_FblEraseErrorAfterCommand = g_FblEraseDmuErrorAfterCommand;
    FblRam_SetSafetyEndinit(password);

    if(FblRamFlash_WaitUnbusy(flashType) == 0u)
    {
        g_FblEraseDmuEndTick = STM0_TIM0.U;
        g_FblEraseDmuTicks = g_FblEraseDmuEndTick - g_FblEraseDmuStartTick;
        g_FblEraseDmuStatusAfterWait = DMU_HF_STATUS.U;
        g_FblEraseDmuErrorAfterWait = DMU_HF_ERRSR.U;
        g_FblEraseLastError = FBL_FLASH_ERROR_TIMEOUT;
        g_FblEraseFailedCommandIndex = g_FblEraseCommandCount;
        return FBL_FLASH_ERROR_TIMEOUT;
    }

    g_FblEraseDmuEndTick = STM0_TIM0.U;
    g_FblEraseDmuTicks = g_FblEraseDmuEndTick - g_FblEraseDmuStartTick;
    g_FblEraseDmuStatusAfterWait = DMU_HF_STATUS.U;
    g_FblEraseDmuErrorAfterWait = DMU_HF_ERRSR.U;

    if(FblRamFlash_HasError() != 0u)
    {
        g_FblEraseLastError = g_FblRamRuntimeLastDmuError;
        g_FblEraseFailedCommandIndex = g_FblEraseCommandCount;
        FblRamFlash_ClearStatus();
        return FBL_FLASH_ERROR_DMU;
    }

    g_FblEraseLastError = FBL_FLASH_OK;
    return FBL_FLASH_OK;
}

RAM_FLASH_CODE uint32 FblRamFlash_EraseRange(uint32 address, uint32 length)
{
    uint32 current;
    uint32 remaining;
    uint32 erasedLength;

    address = FblRamFlash_ToNonCached(address);
    g_FblEraseLastStart = address;
    g_FblEraseLastLength = length;
    g_FblEraseLastError = FBL_FLASH_OK;

    if(FblRamFlash_ValidateEraseRange(address, length) != FBL_FLASH_OK)
    {
        g_FblEraseLastError = FBL_FLASH_ERROR_RANGE;
        return FBL_FLASH_ERROR_RANGE;
    }

    current = address;
    remaining = length;

    while(remaining != 0u)
    {
        if(FblRamFlash_EraseNextChunk(current, remaining, &erasedLength) != FBL_FLASH_OK)
        {
            return g_FblEraseLastError;
        }

        current += erasedLength;
        remaining -= erasedLength;
    }

    return FBL_FLASH_OK;
}

RAM_FLASH_CODE uint32 FblRamFlash_EraseNextChunk(uint32 address, uint32 remaining, uint32 *erasedLength)
{
    uint32 current;
    uint32 flashType;
    uint32 chunkLength;
    uint32 sectorCount;

    if(erasedLength == NULL_PTR)
    {
        g_FblEraseLastError = FBL_FLASH_ERROR_RANGE;
        return FBL_FLASH_ERROR_RANGE;
    }

    *erasedLength = 0u;
    current = FblRamFlash_ToNonCached(address);

    if(FblRamFlash_ValidateEraseRange(current, remaining) != FBL_FLASH_OK)
    {
        g_FblEraseLastError = FBL_FLASH_ERROR_RANGE;
        return FBL_FLASH_ERROR_RANGE;
    }

    chunkLength = FblRamFlash_GetEraseChunkLength(current, remaining);
    flashType = FblRamFlash_Bank(current);

    if((chunkLength == 0u) ||
       ((chunkLength & (PFLASH_LOGICAL_SECTOR_SIZE - 1u)) != 0u) ||
       (chunkLength > PFLASH_ERASE_MAX_COMMAND_SIZE) ||
       (((current + chunkLength - 1u) / PFLASH_PHYSICAL_SECTOR_SIZE) !=
        (current / PFLASH_PHYSICAL_SECTOR_SIZE)) ||
       (FblRamFlash_Bank(current + chunkLength - 1u) != flashType))
    {
        g_FblEraseLastError = FBL_FLASH_ERROR_RANGE;
        g_FblEraseFailedCommandIndex = g_FblEraseCommandCount + 1u;
        return FBL_FLASH_ERROR_RANGE;
    }

    sectorCount = chunkLength >> PFLASH_SECTOR_SHIFT;
    if((sectorCount == 0u) || (sectorCount > PFLASH_ERASE_MAX_SECTORS))
    {
        g_FblEraseLastError = FBL_FLASH_ERROR_RANGE;
        g_FblEraseFailedCommandIndex = g_FblEraseCommandCount + 1u;
        return FBL_FLASH_ERROR_RANGE;
    }

    if(FblRamFlash_IssueEraseMultiple(current, sectorCount, flashType) != FBL_FLASH_OK)
    {
        return g_FblEraseLastError;
    }

    *erasedLength = chunkLength;
    return FBL_FLASH_OK;
}

RAM_FLASH_CODE uint32 FblRamFlash_ProgramPage(uint32 address, const uint8 *data)
{
    uint32 words[8];
    uint32 index;
    uint32 bank;
    uint16 password;

#if (FBL_FLASH_CHECK_PAGE_ARGUMENTS != 0u)
    if((data == NULL_PTR) ||
       (address < PFLASH_START_NC) ||
       (address > (PFLASH_END_NC - (PFLASH_PAGE_SIZE - 1u))) ||
       ((address & (PFLASH_PAGE_SIZE - 1u)) != 0u))
    {
        return 1u;
    }
#endif

    if((((uint32)data) & 0x03u) == 0u)
    {
        const uint32 *srcWords = (const uint32 *)data;
        for(index = 0u; index < 8u; index++)
        {
            words[index] = srcWords[index];
        }
    }
    else
    {
        for(index = 0u; index < 8u; index++)
        {
            const uint8 *p = &data[index * 4u];
            words[index] = ((uint32)p[0u]) |
                           ((uint32)p[1u] << 8u) |
                           ((uint32)p[2u] << 16u) |
                           ((uint32)p[3u] << 24u);
        }
    }

    bank = FblRamFlash_Bank(address);
#if (FBL_FLASH_UPDATE_DEBUG_STATE_EACH_PAGE != 0u)
    g_FblRamRuntimeLastAddress = address;
#endif

    password = FblRam_GetSafetyWatchdogPassword();
    FblRam_ClearSafetyEndinit(password);
#if (FBL_FLASH_CLEAR_STATUS_EACH_PAGE != 0u)
    FblRamFlash_ClearStatus();
#endif

    if((FblRamFlash_EnterPageMode(address) == 0u) ||
       (FblRamFlash_WaitUnbusy(bank) == 0u))
    {
        FblRam_SetSafetyEndinit(password);
        return 1u;
    }

    FblRamFlash_Load2X32(words[0u], words[1u]);
    FblRamFlash_Load2X32(words[2u], words[3u]);
    FblRamFlash_Load2X32(words[4u], words[5u]);
    FblRamFlash_Load2X32(words[6u], words[7u]);
    __dsync();
    FblRamFlash_WritePage(address);

    if(FblRamFlash_WaitUnbusy(bank) == 0u)
    {
        FblRam_SetSafetyEndinit(password);
        return 1u;
    }

    FblRam_SetSafetyEndinit(password);

#if (FBL_FLASH_CHECK_DMU_ERROR_EACH_PAGE != 0u)
    if(FblRamFlash_HasError() != 0u)
    {
        FblRamFlash_ClearStatus();
        return 1u;
    }
#endif

    return 0u;
}

static uint8 FblRamFlash_EnterPageMode(uint32 address) RAM_FLASH_HELPER
{
    if((address & 0xFF000000u) != PFLASH_START_NC)
    {
        return 0u;
    }

    *((volatile uint32 *)(FLASH_CMD_BASE | 0x5554u)) = 0x50u;
    __dsync();
    return 1u;
}

static void FblRamFlash_Load2X32(uint32 wordLow, uint32 wordHigh) RAM_FLASH_HELPER
{
    volatile uint32 *command = (volatile uint32 *)(FLASH_CMD_BASE | 0x55F0u);

#if (FBL_FLASH_EXTRA_LOAD_DSYNC != 0u)
    __dsync();
#endif
    command[0] = wordLow;
#if (FBL_FLASH_EXTRA_LOAD_DSYNC != 0u)
    __dsync();
#endif
    command[1] = wordHigh;
#if (FBL_FLASH_EXTRA_LOAD_DSYNC != 0u)
    __dsync();
#endif
}

static void FblRamFlash_WritePage(uint32 address) RAM_FLASH_HELPER
{
    *((volatile uint32 *)(FLASH_CMD_BASE | 0xAA50u)) = address;
    *((volatile uint32 *)(FLASH_CMD_BASE | 0xAA58u)) = 0u;
    *((volatile uint32 *)(FLASH_CMD_BASE | 0xAAA8u)) = 0xA0u;
    *((volatile uint32 *)(FLASH_CMD_BASE | 0xAAA8u)) = 0xAAu;
    __dsync();
}
