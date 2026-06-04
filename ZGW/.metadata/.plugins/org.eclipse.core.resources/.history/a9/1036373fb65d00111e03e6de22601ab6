#include "McuSm.h"
#include "IfxScuLbist.h"
#include "IfxCpu_IntrinsicsTasking.h"
#include "IfxCpu_reg.h"
#include "IfxPms_reg.h"
#include "BSW/Time/TimeBase.h"

uint32 McuSm_AGs[12u];
uint32 McuSm_LastResetReason;
uint32 McuSm_LastResetInformation;
uint32 McuSm_IndexResetHistory;
McuSm_ResetHistory_t McuSm_ResetHistory[20u];
uint32 DiagMaster_AliveTime;
uint8 DiagMaster_ActiveSessionState;
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
uint8 McuSm_Trap4ScrRtcRecord[SCR_TIME_RECORD_LENGTH];
volatile uint8 McuSm_Trap4ScrRtcRecordValid;
volatile uint32 McuSm_Trap4ScrRtcRecordCounter;
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
#define MCUSM_TRAP4_REACTION_NCR  2u
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

extern uint8 _lc_gb_NCR[];
extern uint8 _lc_ge_NCR[];

static boolean McuSm_IsAddressInRange(uint32 address, uint32 rangeStart, uint32 rangeEnd);
static boolean McuSm_IsScrXramAddress(uint32 address);
static void McuSm_ZeroFillRange(uint32 rangeStart, uint32 rangeEnd);
static uint32 McuSm_LoadU32FromBuffer(const uint8 *buffer, uint16 offset);
static void McuSm_CaptureScrRtcRecord(void);
static uint32 McuSm_GetTrap4ErrorAddress(IfxCpu_Trap trapInfo);
static uint32 McuSm_Trap4ZeroFillIfRecoverable(uint32 errorAddress);

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

void McuSm_PerformResetHook(uint32 resetReason, uint32 resetInformation)
{
    if(0xEFEFU != resetReason && 0xDFDFU != resetReason)
    {
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
    }
    else
    {
        McuSm_LastResetReason = 0u;
        McuSm_LastResetInformation = 0u;
    }

    if(McuSm_IndexResetHistory >= 20u)
    {
        McuSm_IndexResetHistory = 0u;
    }
    else
    {
        /* Do nothing. */
    }
    
    IfxScuRcu_performReset(IfxScuRcu_ResetType_application, 0u);

}

static boolean McuSm_IsAddressInRange(uint32 address, uint32 rangeStart, uint32 rangeEnd)
{
    return ((address >= rangeStart) && (address < rangeEnd)) ? TRUE : FALSE;
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

void McuSm_CaptureTimeImageFromScr(void)
{
    McuSm_CaptureScrRtcRecord();
}

static void McuSm_CaptureScrRtcRecord(void)
{
    volatile uint8 *scrTime = &((volatile uint8 *)PMS_XRAM)[SCR_TIME_XRAM_BASE];
    uint16 index;

    if (PMS_PMSWCR2.B.SCRECC != 0u)
    {
        McuSm_Trap4ScrRtcRecordValid = FALSE;
        return;
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
    uint32 const ncrStart = (uint32)&_lc_gb_NCR[0u];
    uint32 const ncrEnd = (uint32)&_lc_ge_NCR[0u];

    if (McuSm_IsAddressInRange(errorAddress, xramStart, xramEnd) != FALSE)
    {
        McuSm_ZeroFillRange(xramStart, xramEnd);
        reaction = MCUSM_TRAP4_REACTION_XRAM;
    }
    else if (McuSm_IsAddressInRange(errorAddress, ncrStart, ncrEnd) != FALSE)
    {
        McuSm_ZeroFillRange(ncrStart, ncrEnd);
        reaction = MCUSM_TRAP4_REACTION_NCR;
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
    if (McuSm_IsScrXramAddress(McuSm_Trap4ErrorAddress) == FALSE)
    {
        TimeBase_CaptureStandbyRtcBeforeScrReset();
    }

    if ((McuSm_IsScrXramAddress(McuSm_Trap4ErrorAddress) == FALSE) &&
        (McuSm_IsAddressInRange(
                McuSm_Trap4ErrorAddress,
                (uint32)&_lc_gb_NCR[0u],
                (uint32)&_lc_ge_NCR[0u]) == FALSE))
    {
        McuSm_CaptureScrRtcRecord();
    }

    if (McuSm_Trap4ZeroFillIfRecoverable(McuSm_Trap4ErrorAddress) == MCUSM_TRAP4_REACTION_NCR)
    {
        McuSm_CaptureScrRtcRecord();
    }
    else if (McuSm_IsScrXramAddress(McuSm_Trap4ErrorAddress) != FALSE)
    {
        McuSm_Trap4ScrRtcRecordValid = FALSE;
    }
    McuSm_PerformResetHook(374u, trapInfo.tId);
}

void McuSm_TRAP7(IfxCpu_Trap trapInfo)
{
    uint32 const volatile* ag;
    uint32 agRstRsn = 0u;
    uint32 agRstInfo = 0u;

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
        if(0u != (ag[i] & McuSm_AGs[i]))
        {
            agRstRsn = i;
            agRstInfo = (sint8)(31u - (uint8)__clz(ag[i] & McuSm_AGs[i]));
            break;
        }
        else
        {
            /* Do nothing. */
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
