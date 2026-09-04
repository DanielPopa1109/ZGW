#ifndef NVMTIMING_H
#define NVMTIMING_H

#include "Std_Types.h"
#include "BSW/Diag/Dcm/Dcm.h"

#define NVMTIMING_ROUTINE_ID                    (0xF193u)
#define NVMTIMING_MAGIC                         (0x4E564D54u)
#define NVMTIMING_VERSION                       (1u)
#define NVMTIMING_STARTUP_EVENT_COUNT           (16u)
#define NVMTIMING_HISTORY_SIZE                  (8u)

typedef enum
{
    NVMTIMING_OP_NONE = 0u,
    NVMTIMING_OP_READ_BLOCK = 1u,
    NVMTIMING_OP_WRITE_BLOCK = 2u,
    NVMTIMING_OP_READ_ALL = 3u,
    NVMTIMING_OP_WRITE_ALL = 4u,
    NVMTIMING_OP_RESTORE_DEFAULTS = 5u,
    NVMTIMING_OP_INVALIDATE_BLOCK = 6u,
    NVMTIMING_OP_DEFERRED_DEFAULT = 7u,
    NVMTIMING_OP_SET_RAM_STATUS = 8u
} NvMTiming_OperationType;

typedef enum
{
    NVMTIMING_STARTUP_BOOT_REFERENCE = 0u,
    NVMTIMING_STARTUP_UNINIT_STATE,
    NVMTIMING_STARTUP_INIT_ENTER,
    NVMTIMING_STARTUP_INIT_EXIT,
    NVMTIMING_STARTUP_READALL_REQUEST,
    NVMTIMING_STARTUP_READALL_FIRST_MAIN,
    NVMTIMING_STARTUP_READALL_FIRST_MEMIF,
    NVMTIMING_STARTUP_READALL_FIRST_FEE,
    NVMTIMING_STARTUP_READALL_FIRST_FLS,
    NVMTIMING_STARTUP_READALL_LAST_FLS_COMPLETE,
    NVMTIMING_STARTUP_READALL_LAST_FEE_COMPLETE,
    NVMTIMING_STARTUP_READALL_LAST_MEMIF_COMPLETE,
    NVMTIMING_STARTUP_READALL_COMPLETE,
    NVMTIMING_STARTUP_NVM_READY,
    NVMTIMING_STARTUP_WRITEALL_REQUEST,
    NVMTIMING_STARTUP_ERROR_COMPLETE
} NvMTiming_StartupEventType;

typedef struct
{
    uint16 eventId;
    uint16 valid;
    uint64 ticks;
    uint32 elapsedUs;
} NvMTiming_EventType;

typedef struct
{
    uint8 operation;
    uint8 result;
    uint8 memIfResult;
    uint8 reserved;
    uint16 blockId;
    uint16 sequence;
    uint64 requestTicks;
    uint64 acceptedTicks;
    uint64 firstMainTicks;
    uint64 processingTicks;
    uint64 memIfRequestTicks;
    uint64 feeRequestTicks;
    uint64 flsRequestTicks;
    uint64 flsCompleteTicks;
    uint64 completionTicks;
    uint32 nvmMainCycles;
    uint32 feeMainCycles;
    uint32 flsMainCycles;
    uint32 flags;
} NvMTiming_SingleBlockType;

typedef struct
{
    uint8 operation;
    uint8 result;
    uint8 memIfResult;
    uint8 reserved;
    uint16 sequence;
    uint16 activeBlockId;
    uint64 requestTicks;
    uint64 firstMainTicks;
    uint64 firstBlockTicks;
    uint64 firstMemIfTicks;
    uint64 firstFeeTicks;
    uint64 firstFlsTicks;
    uint64 lastFlsCompleteTicks;
    uint64 lastFeeCompleteTicks;
    uint64 completionTicks;
    uint32 nvmMainCycles;
    uint32 feeMainCycles;
    uint32 flsMainCycles;
    uint16 blocksPlanned;
    uint16 blocksStarted;
    uint16 blocksDone;
    uint16 blocksFailed;
    uint16 blocksSkipped;
    uint16 reserved16;
    uint32 flags;
} NvMTiming_MultiBlockType;

typedef struct
{
    uint32 count;
    uint64 totalTicks;
    uint64 minTicks;
    uint64 maxTicks;
} NvMTiming_StatsType;

typedef struct
{
    uint32 magic;
    uint16 version;
    uint16 historySize;
    uint64 stmFrequencyHz;
    uint64 bootReferenceTicks;
    NvMTiming_EventType startup[NVMTIMING_STARTUP_EVENT_COUNT];
    NvMTiming_MultiBlockType lastReadAll;
    NvMTiming_MultiBlockType lastWriteAll;
    NvMTiming_SingleBlockType history[NVMTIMING_HISTORY_SIZE];
    NvMTiming_StatsType readBlockStats;
    NvMTiming_StatsType writeBlockStats;
    uint32 historyWriteIndex;
    uint32 sequence;
    uint32 flags;
    uint32 dirtyCounter;
} NvMTiming_NvImageType;

#define NVMTIMING_NVM_IMAGE_SIZE                ((uint32)sizeof(NvMTiming_NvImageType))

void NvMTiming_BootReference(void);
void NvMTiming_StartupEvent(uint16 eventId);
void NvMTiming_InitEnter(void);
void NvMTiming_InitExit(void);
void NvMTiming_SetNvMReady(void);
void NvMTiming_ReadAllRequest(void);
void NvMTiming_WriteAllRequest(void);
void NvMTiming_BeginMulti(uint8 operation);
void NvMTiming_MultiProcessing(uint8 operation, uint16 blockId);
void NvMTiming_MultiMemIfRequest(uint8 operation, uint16 blockId);
void NvMTiming_MultiCounters(
        uint8 operation,
        uint16 planned,
        uint16 started,
        uint16 done,
        uint16 failed,
        uint16 skipped);
void NvMTiming_MultiComplete(uint8 operation, uint8 result, uint8 memIfResult);
void NvMTiming_StartSingle(uint8 operation, uint16 blockId, Std_ReturnType accepted, uint8 result);
void NvMTiming_SingleProcessing(uint16 blockId);
void NvMTiming_SingleMemIfRequest(uint16 blockId);
void NvMTiming_SingleComplete(uint16 blockId, uint8 result, uint8 memIfResult);
void NvMTiming_SetRamStatus(uint16 blockId, boolean changed);
void NvMTiming_FeeRequest(uint8 operation, uint16 blockId);
void NvMTiming_FeeComplete(uint8 memIfResult);
void NvMTiming_FlsRequest(uint8 operation, uint32 address, uint32 length);
void NvMTiming_FlsComplete(uint8 memIfResult);
void NvMTiming_NvMMainCycle(void);
void NvMTiming_FeeMainCycle(void);
void NvMTiming_FlsMainCycle(void);
boolean NvMTiming_IsDirty(void);
void NvMTiming_ClearDirty(void);
boolean NvMTiming_IsRoutineId(uint16 routineId);
Dcm_ReturnType NvMTiming_HandleRoutineControl(
        Dcm_OpStatusType opStatus,
        uint8 routineControlType,
        uint16 routineId,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen);

extern uint8 NvM_NvMTiming_Ram[];
extern const uint8 NvM_NvMTiming_Rom[];

#endif /* NVMTIMING_H */
