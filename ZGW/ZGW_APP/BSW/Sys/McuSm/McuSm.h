#ifndef MCUSM_H
#define MCUSM_H

#include "Ifx_Types.h"
#include "IfxCpu_reg.h"
#include "IfxMtu.h"
#include "IfxScuRcu.h"
#include "Ifx_Cfg_Trap.h"
#include "IfxDts_Dts.h"
#include "../../../SCR/scr_time_shared.h"

#define MCUSM_FBL_PROGRAMMING_REQUEST_NONE     0u
#define MCUSM_FBL_PROGRAMMING_REQUEST_ACTIVE   1u

#define MCUSM_FBL_COMM_ETHERNET                1u
#define MCUSM_FBL_COMM_CANFD                   2u
#define MCUSM_FBL_COMM_CAN_CLASSIC             3u

#define MCUSM_RESET_REASON_SAFETYKIT_TEST      385u

#define MCUSM_SAFETYKIT_FAIL_LBIST             (1u << 0u)
#define MCUSM_SAFETYKIT_FAIL_MONBIST           (1u << 1u)
#define MCUSM_SAFETYKIT_FAIL_MCU_FW_CHECK      (1u << 2u)
#define MCUSM_SAFETYKIT_FAIL_MCU_STARTUP       (1u << 3u)
#define MCUSM_SAFETYKIT_FAIL_ALIVE_ALARM       (1u << 4u)
#define MCUSM_SAFETYKIT_FAIL_REG_MONITOR       (1u << 5u)
#define MCUSM_SAFETYKIT_FAIL_MBIST             (1u << 6u)
#define MCUSM_SAFETYKIT_FAIL_SMU_KEYS          (1u << 7u)
#define MCUSM_SAFETYKIT_FAIL_SMU_KEYS_CLEAR    (1u << 8u)
#define MCUSM_SAFETYKIT_FAIL_SMU_INIT          (1u << 9u)

#define MCUSM_FW_CHECK_RESULT_SMU_FAILED               (1u << 0u)
#define MCUSM_FW_CHECK_RESULT_STMEM_FAILED             (1u << 1u)
#define MCUSM_FW_CHECK_RESULT_LCLCON_FAILED            (1u << 2u)
#define MCUSM_FW_CHECK_RESULT_SSH_FAILED               (1u << 3u)
#define MCUSM_FW_CHECK_RESULT_LBIST_SSH_ZERO_ACCEPTED  (1u << 4u)
#define MCUSM_FW_CHECK_RESULT_LBIST_SSH_TABLE_ACCEPTED (1u << 5u)
#define MCUSM_FW_CHECK_RESULT_LBIST_SSH_NOINIT_ACCEPTED (1u << 6u)
#define MCUSM_FW_CHECK_RESULT_LBIST_SSH_INIT_ACCEPTED  (1u << 7u)

typedef struct
{
        uint32 reason;
        uint32 information;
}McuSm_ResetHistory_t;

typedef struct
{
        uint8  lbistAppSwReq;
        uint8  lbistRuns;
        uint8  mcuFwcheckRuns;
        uint8  lbistStatus;
        uint8  monbistStatus;
        uint8  mcuFwcheckStatus;
        uint8  mcuStartupStatus;
        uint8  aliveAlarmTestStatus;
        uint8  regMonitorTestStatus;
        uint8  mbistStatus;
        uint8  smuCoreKeysTestSts;
        uint8  smuCoreKeysTestClearSts;
        uint8  smuCoreInitSts;
        uint8  wakeupFromStandby;
        uint16 resetReason;
        uint32 rstStat;
        uint32 resetType;
        uint32 resetTrigger;
}McuSm_SswStatusData_t;

extern uint32 McuSm_LastResetReason;
extern uint32 McuSm_LastResetInformation;
extern uint32 McuSm_IndexResetHistory;
extern McuSm_ResetHistory_t McuSm_ResetHistory[20u];
extern uint8 McuSm_FBL_ResetCounter;
extern uint8 McuSm_FBL_ProgrammingRequest;
extern uint8 McuSm_FBL_CommInterface;
extern uint32 McuSm_SswStartupCounter;
extern uint32 McuSm_SafetyKitFailureMask;
extern uint32 McuSm_SafetyKitResetReactionCounter;
extern uint8 McuSm_SafetyKitResetInhibit;
extern volatile uint32 McuSm_SafetyKitFwCheckLastSshFail;
extern volatile McuSm_SswStatusData_t McuSm_SswStatusData;
extern volatile uint32 McuSm_SafetyKitFwCheckResultMask;
extern volatile uint32 McuSm_SafetyKitFwCheckSshActualEccd;
extern volatile uint32 McuSm_SafetyKitFwCheckSshActualFaultsts;
extern volatile uint32 McuSm_SafetyKitFwCheckSshActualErrinfo;
extern volatile uint32 McuSm_SafetyKitFwCheckSshExpectedEccd;
extern volatile uint32 McuSm_SafetyKitFwCheckSshExpectedFaultsts;
extern volatile uint32 McuSm_SafetyKitFwCheckSshExpectedErrinfo;
extern volatile uint32 McuSm_LastTrapClass;
extern volatile uint32 McuSm_LastTrapId;
extern volatile uint32 McuSm_LastTrapCoreId;
extern volatile uint32 McuSm_LastTrapTAddr;
extern volatile uint32 McuSm_LastTrapPcxi;
extern volatile uint32 McuSm_LastTrapFcx;
extern volatile uint32 McuSm_LastTrapLcx;
extern volatile uint32 McuSm_LastTrapPsw;
extern volatile uint32 McuSm_TrapCounter;
extern volatile uint32 McuSm_EndinitWaitCounter;
extern volatile uint32 McuSm_EndinitTimeoutCounter;
extern volatile uint32 McuSm_Trap4Dstr;
extern volatile uint32 McuSm_Trap4Datr;
extern volatile uint32 McuSm_Trap4Deadd;
extern volatile uint32 McuSm_Trap4Diear;
extern volatile uint32 McuSm_Trap4Dietr;
extern volatile uint32 McuSm_Trap4Piear;
extern volatile uint32 McuSm_Trap4Pietr;
extern volatile uint32 McuSm_Trap4ErrorAddress;
extern volatile uint32 McuSm_Trap4ZeroFillReaction;
extern volatile uint32 McuSm_Trap4ZeroFillReactionCounter;
extern uint8 McuSm_Trap4ScrRtcRecord[SCR_TIME_RECORD_LENGTH];
extern volatile uint8 McuSm_Trap4ScrRtcRecordValid;
extern volatile uint32 McuSm_Trap4ScrRtcRecordCounter;
extern volatile uint32 McuSm_BusMpuConfigured;
extern volatile uint32 McuSm_BusMpuWriteAccessMaskA;
extern volatile uint32 McuSm_BusMpuWriteAccessMaskB;

extern void McuSm_InitializeBusMpu(void);
extern void McuSm_PerformResetHook(uint32 resetReason, uint32 resetInformation);
extern void McuSm_CaptureTimeImageFromScr(void);
extern void McuSm_TRAP1(IfxCpu_Trap trapInfo);
extern void McuSm_TRAP2(IfxCpu_Trap trapInfo);
extern void McuSm_TRAP3(IfxCpu_Trap trapInfo);
extern void McuSm_TRAP4(IfxCpu_Trap trapInfo);
extern void McuSm_TRAP7(IfxCpu_Trap trapInfo);

#endif /* MCUSM_H */
