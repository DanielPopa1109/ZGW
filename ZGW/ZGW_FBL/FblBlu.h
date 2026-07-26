#ifndef FBLBLU_H
#define FBLBLU_H

#include "Ifx_Types.h"

/* Persistent BLU progress record in SCR XRAM. This is diagnostic state only;
 * the direct FBL update intentionally has no rollback image. */
#define FBL_SCR_XRAM_ADDR                0xF0240000u
#define FBL_SCR_BLU_BASE                 0x17F0u
#define FBL_SCR_BLU_MAGIC                0x424C5531u
#define FBL_SCR_BLU_VERSION              1u
#define FBL_SCR_BLU_VALID                1u
#define FBL_SCR_BLU_OFF_MAGIC            0u
#define FBL_SCR_BLU_OFF_VERSION          4u
#define FBL_SCR_BLU_OFF_VALID            5u
#define FBL_SCR_BLU_OFF_STATE            6u
#define FBL_SCR_BLU_OFF_FAILURE          7u
#define FBL_SCR_BLU_OFF_IMAGE_KIND       8u
#define FBL_SCR_BLU_OFF_RESERVED         9u
#define FBL_SCR_BLU_OFF_IMAGE_LEN        12u
#define FBL_SCR_BLU_OFF_IMAGE_CRC        16u
#define FBL_SCR_BLU_OFF_FLAGS            20u
#define FBL_SCR_BLU_OFF_CHECKSUM         24u

#define FBL_BLU_STATE_IDLE               0u
#define FBL_BLU_STATE_ENTER_REQUESTED    1u
#define FBL_BLU_STATE_RAM_READY          2u
#define FBL_BLU_STATE_ERASING            3u
#define FBL_BLU_STATE_ERASE_COMPLETE     4u
#define FBL_BLU_STATE_DOWNLOAD_ACTIVE    5u
#define FBL_BLU_STATE_PROGRAMMING        6u
#define FBL_BLU_STATE_VERIFYING          7u
#define FBL_BLU_STATE_TRANSFER_COMPLETE  8u
#define FBL_BLU_STATE_FINAL_VERIFY       9u
#define FBL_BLU_STATE_COMMITTING         10u
#define FBL_BLU_STATE_RESPONSE_DRAIN     11u
#define FBL_BLU_STATE_COMPLETE           12u
#define FBL_BLU_STATE_FAILED             13u
#define FBL_BLU_STATE_RECOVERY_WAIT      14u
#define FBL_BLU_STATE_FBL_VERIFIED       15u
#define FBL_BLU_STATE_REBOOT_PENDING     16u
#define FBL_BLU_STATE_FBL_STARTED        17u

#define FBL_BLU_STATE_PREPARE            FBL_BLU_STATE_ENTER_REQUESTED
#define FBL_BLU_STATE_ERASE              FBL_BLU_STATE_ERASING
#define FBL_BLU_STATE_RECEIVE_BLOCK      FBL_BLU_STATE_DOWNLOAD_ACTIVE
#define FBL_BLU_STATE_PROGRAM_BLOCK      FBL_BLU_STATE_PROGRAMMING
#define FBL_BLU_STATE_VERIFY_BLOCK       FBL_BLU_STATE_VERIFYING
#define FBL_BLU_STATE_WAIT_NEXT_BLOCK    FBL_BLU_STATE_DOWNLOAD_ACTIVE
#define FBL_BLU_STATE_DOWNLOAD           FBL_BLU_STATE_DOWNLOAD_ACTIVE
#define FBL_BLU_STATE_VERIFY             FBL_BLU_STATE_FINAL_VERIFY
#define FBL_BLU_STATE_VERIFIED           FBL_BLU_STATE_FBL_VERIFIED
#define FBL_BLU_STATE_ACTIVATE           FBL_BLU_STATE_FBL_STARTED

#define FBL_BLU_IMAGE_KIND_NONE          0u
#define FBL_BLU_IMAGE_KIND_APPLICATION   1u
#define FBL_BLU_IMAGE_KIND_BOOTLOADER    2u

#define FBL_BLU_FAILURE_NONE             0u
#define FBL_BLU_FAILURE_DOWNLOAD         1u
#define FBL_BLU_FAILURE_VERIFY           2u
#define FBL_BLU_FAILURE_ACTIVATE         3u
#define FBL_BLU_FAILURE_RESET            4u
#define FBL_BLU_FAILURE_SEQUENCE         5u
#define FBL_BLU_FAILURE_RAM_CLOSURE      6u
#define FBL_BLU_FAILURE_FLASH            7u

#define FBL_BLU_FLAG_TARGET_LOCKED       0x00000001u
#define FBL_BLU_FLAG_DIRECT_UPDATE       0x00000002u

#define FBL_RAM_BLU_CODE                 __attribute__((section(".ram_blu_code"))) __attribute__((noinline))
#define FBL_RAM_HELPER_CODE              __attribute__((section(".ram_blu_helpers"))) __attribute__((noinline))
#define FBL_RAM_FLASH_CODE               __attribute__((section(".ram_flash_code"))) __attribute__((noinline))
#define FBL_RAM_FLASH_HELPER_CODE        __attribute__((section(".ram_flash_helpers"))) __attribute__((noinline))
#define FBL_RAM_ETH_CODE                 __attribute__((section(".ram_eth_code"))) __attribute__((noinline))
#define FBL_RAM_ISR_CODE                 __attribute__((section(".ram_isr_code"))) __attribute__((noinline))
#define FBL_RAM_TRAP_CODE                __attribute__((section(".ram_trap_code"))) __attribute__((used)) __attribute__((noinline))
#define FBL_RAM_CONST                    __attribute__((section(".ram_blu_rodata")))
#define FBL_RAM_DATA                     __attribute__((section(".ram_blu_data")))
#define FBL_RAM_DSPR_DATA                __attribute__((section(".ram_data_dspr")))

typedef struct
{
    uint8 state;
    uint8 failure;
    uint8 imageKind;
    uint32 imageLen;
    uint32 imageCrc;
    uint32 flags;
} FblBlu_RecordType;

/* RAM runtime support. All implementations are linked into CPU0 PSPR. */
uint8 FblRamRuntime_EnterCritical(void);
uint8 FblRamRuntime_IsActive(void);
uint8 FblRamRuntime_IsExecutableAddress(uint32 address);
void FblRamRuntime_RequestReset(void);
void FblRamRuntime_SetDestructivePhase(uint8 active);
void Fbl_BluEnterRecoveryWaitFromTrap(void);

typedef uint32 FblRam_InterruptState;

FblRam_InterruptState FblRam_DisableInterrupts(void);
void FblRam_RestoreInterrupts(FblRam_InterruptState state);
void FblRam_InvalidateProgramCache(void);
uint16 FblRam_GetCpuWatchdogPassword(void);
void FblRam_ClearCpuEndinit(uint16 password);
void FblRam_SetCpuEndinit(uint16 password);
uint16 FblRam_GetSafetyWatchdogPassword(void);
void FblRam_ClearSafetyEndinit(uint16 password);
void FblRam_SetSafetyEndinit(uint16 password);
void FblRam_RequestSystemReset(void);
void FblRam_CopyBytes(void *dst, const void *src, uint32 length);
void FblRam_SetBytes(void *dst, uint8 value, uint32 length);
void FblRam_MoveBytes(void *dst, const void *src, uint32 length);
sint32 FblRam_CompareBytes(const void *a, const void *b, uint32 length);
void *FblRam_Memcpy(void *destination, const void *source, uint32 length);
void *FblRam_Memset(void *destination, uint8 value, uint32 length);
void *FblRam_Memmove(void *destination, const void *source, uint32 length);
sint32 FblRam_Memcmp(const void *a, const void *b, uint32 length);
uint32 FblRam_Strlen(const char *text);

/* Direct PFLASH primitives. Return 0 on success, non-zero on failure. */
uint32 FblRamFlash_EraseNextChunk(uint32 address, uint32 remaining, uint32 *erasedLength);
uint32 FblRamFlash_EraseRange(uint32 address, uint32 length);
uint32 FblRamFlash_ProgramPage(uint32 address, const uint8 *data);
void FblRamFlash_ClearStatus(void);

extern volatile uint32 g_FblRamRuntimeActive;
extern volatile uint32 g_FblRamRuntimeTrapClass;
extern volatile uint32 g_FblRamRuntimeTrapTin;
extern volatile uint32 g_FblRamRuntimeLastAddress;
extern volatile uint32 g_FblRamRuntimeLastDmuError;
extern volatile uint32 g_FblRamRuntimeLastDmuStatus;
extern volatile uint32 g_FblRamRuntimeLastFlashType;
extern volatile uint32 g_FblRamRuntimeLastWaitMask;
extern volatile uint32 g_FblRamRuntimeLastWaitGuard;
extern volatile uint32 g_FblTransferAdvertisedBlockLength;
extern volatile uint32 g_FblTransferLastUdsLength;
extern volatile uint32 g_FblTransferLastDataLength;
extern volatile uint32 g_FblTransferLastDoipPayloadLength;
extern volatile uint32 g_FblTransferLastFrameLength;
extern volatile uint32 g_FblTransferTcpStreamHighWatermark;
extern volatile uint32 g_FblTransferTcpCallbackCount;
extern volatile uint32 g_FblTransferOverflowCount;
extern volatile uint32 g_FblTransferProgrammedPageCount;
extern volatile uint32 g_FblEraseCommandCount;
extern volatile uint32 g_FblEraseLogicalCallCount;
extern volatile uint32 g_FblEraseLogicalLastStart;
extern volatile uint32 g_FblEraseLogicalLastLength;
extern volatile uint32 g_FblEraseLogicalLastResult;
extern volatile uint32 g_FblEraseLogicalStartTick;
extern volatile uint32 g_FblEraseLogicalEndTick;
extern volatile uint32 g_FblEraseLogicalTicks;
extern volatile uint32 g_FblEraseDoIpDrainStartTick;
extern volatile uint32 g_FblEraseDoIpDrainEndTick;
extern volatile uint32 g_FblEraseDoIpDrainTicks;
extern volatile uint32 g_FblEraseDmuCommandCount;
extern volatile uint32 g_FblEraseLastStart;
extern volatile uint32 g_FblEraseLastLength;
extern volatile uint32 g_FblEraseLastChunkLength;
extern volatile uint32 g_FblEraseLastSectorCount;
extern volatile uint32 g_FblEraseLastPhysicalBoundary;
extern volatile uint32 g_FblEraseLastFlashType;
extern volatile uint32 g_FblEraseLastError;
extern volatile uint32 g_FblEraseErrorBeforeCommand;
extern volatile uint32 g_FblEraseErrorAfterCommand;
extern volatile uint32 g_FblEraseFailedCommandIndex;
extern volatile uint32 g_FblEraseDmuStatusBeforeClear;
extern volatile uint32 g_FblEraseDmuErrorBeforeClear;
extern volatile uint32 g_FblEraseDmuStatusAfterCommand;
extern volatile uint32 g_FblEraseDmuErrorAfterCommand;
extern volatile uint32 g_FblEraseDmuStatusAfterWait;
extern volatile uint32 g_FblEraseDmuErrorAfterWait;
extern volatile uint32 g_FblEraseBank0CommandCount;
extern volatile uint32 g_FblEraseBank1CommandCount;
extern volatile uint32 g_FblEraseDmuStartTick;
extern volatile uint32 g_FblEraseDmuEndTick;
extern volatile uint32 g_FblEraseDmuTicks;
extern volatile uint32 g_FblEraseWaitStartTick;
extern volatile uint32 g_FblEraseWaitEndTick;
extern volatile uint32 g_FblEraseWaitTicks;
extern volatile uint32 g_FblRamRuntimeDestructivePhase;
extern volatile uint32 g_FblRuntimeClosureFailStep;
extern volatile uint32 g_FblEthRuntimeClosureFailStep;
extern volatile uint32 g_LwipGethRuntimeClosureFailStep;

#endif
