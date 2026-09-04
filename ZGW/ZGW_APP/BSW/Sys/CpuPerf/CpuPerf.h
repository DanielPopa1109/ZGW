#ifndef CPUPERF_H
#define CPUPERF_H

#include "Std_Types.h"
#include "BSW/Diag/Dcm/Dcm.h"

#ifndef CPU_PERF_ENABLED
#define CPU_PERF_ENABLED                 1u
#endif

#ifndef CPU_PERF_TASKS_ENABLED
#define CPU_PERF_TASKS_ENABLED           CPU_PERF_ENABLED
#endif

#ifndef CPU_PERF_ISR_ENABLED
#define CPU_PERF_ISR_ENABLED             CPU_PERF_ENABLED
#endif

#ifndef CPU_PERF_NVM_ENABLED
#define CPU_PERF_NVM_ENABLED             CPU_PERF_ENABLED
#endif

#ifndef CPU_PERF_ETH_ENABLED
#define CPU_PERF_ETH_ENABLED             CPU_PERF_ENABLED
#endif

#ifndef CPU_PERF_DIAG_ENABLED
#define CPU_PERF_DIAG_ENABLED            CPU_PERF_ENABLED
#endif

#ifndef CPU_PERF_AI_ENABLED
#define CPU_PERF_AI_ENABLED              CPU_PERF_ENABLED
#endif

#ifndef CPU_PERF_CAN_ENABLED
#define CPU_PERF_CAN_ENABLED             CPU_PERF_ENABLED
#endif

#ifndef CPU_PERF_LIN_ENABLED
#define CPU_PERF_LIN_ENABLED             CPU_PERF_ENABLED
#endif

#ifndef CPU_PERF_CRC_ENABLED
#define CPU_PERF_CRC_ENABLED             CPU_PERF_ENABLED
#endif

#define CPUPERF_ROUTINE_ID               (0xF194u)
#define CPUPERF_MAGIC                    (0x43504631u)
#define CPUPERF_VERSION                  (1u)
#define CPUPERF_CORE_COUNT               (3u)
#define CPUPERF_CORE_UNKNOWN             (0xFFu)
#define CPUPERF_COUNTER_VALUE_MASK       (0x7FFFFFFFUL)
#define CPUPERF_RESPONSE_HEADER_LEN      (16u)
#define CPUPERF_RESPONSE_ENTRY_LEN       (56u)
#define CPUPERF_RESPONSE_MAX_ENTRIES     (4u)

typedef enum
{
    CPUPERF_ID_OS_ASIL_BSW_TASK_C0 = 0u,
    CPUPERF_ID_OS_ASIL_NVM_TASK_C0,
    CPUPERF_ID_OS_NVM_STARTUP_MAIN_C0,
    CPUPERF_ID_OS_NVM_FLS_MAIN_C0,
    CPUPERF_ID_OS_NVM_FEE_MAIN_C0,
    CPUPERF_ID_OS_NVM_NVM_MAIN_C0,
    CPUPERF_ID_NVM_MAIN_STEP_C0,
    CPUPERF_ID_FEE_MAIN_STEP_C0,
    CPUPERF_ID_FLS_MAIN_STEP_C0,
    CPUPERF_ID_CAN_IRQ_RX_CLASSIC_C0,
    CPUPERF_ID_CAN_IRQ_RX_FD_C0,
    CPUPERF_ID_ETH_IRQ_TX_C2,
    CPUPERF_ID_ETH_IRQ_RX_C2,
    CPUPERF_ID_OS_QM_BSW_TASK_C2,
    CPUPERF_ID_DOIP_MAIN_C2,
    CPUPERF_ID_PDUR_DOIP_CORE0_MAIN_C0,
    CPUPERF_ID_PDUR_DOIP_CORE2_MAIN_C2,
    CPUPERF_ID_DCM_MAIN_C0,
    CPUPERF_ID_AI_MODEL_MAIN_C1,
    CPUPERF_ID_CAN_MAIN_C0,
    CPUPERF_ID_CANIF_RX_INDICATION_C0,
    CPUPERF_ID_CANTP_RX_INDICATION_C0,
    CPUPERF_ID_CANTP_MAIN_C0,
    CPUPERF_ID_LINIF_MAIN_C0,
    CPUPERF_ID_LINTP_MAIN_C0,
    CPUPERF_ID_SOAD_MAIN_C2,
    CPUPERF_ID_TCPIP_MAIN_C2,
    CPUPERF_ID_GATEWAY_MAIN_C0,
    CPUPERF_ID_GATEWAY_ETH_MAIN_C2,
    CPUPERF_ID_CRC32,
    CPUPERF_ID_COUNT
} CpuPerf_MeasurementIdType;

typedef struct
{
    uint32 startCycles;
    uint32 startInstructions;
    uint32 startTimeTicks;
    uint8 startCoreId;
} CpuPerf_ContextType;

typedef struct
{
    uint32 lastCycles;
    uint32 minCycles;
    uint32 maxCycles;
    uint64 totalCycles;
    uint32 lastInstructions;
    uint32 minInstructions;
    uint32 maxInstructions;
    uint64 totalInstructions;
    uint32 sampleCount;
    uint32 overflowCount;
    uint32 lastNs;
    uint32 minNs;
    uint32 maxNs;
    uint64 totalNs;
    uint64 totalBytes;
} CpuPerf_StatsType;

#if CPU_PERF_ENABLED
void CpuPerf_InitCore(void);
void CpuPerf_Start(CpuPerf_MeasurementIdType measurementId, CpuPerf_ContextType *context);
void CpuPerf_Stop(CpuPerf_MeasurementIdType measurementId, CpuPerf_ContextType *context);
void CpuPerf_AddBytes(CpuPerf_MeasurementIdType measurementId, uint32 byteCount);
void CpuPerf_ResetStatistics(void);
Std_ReturnType CpuPerf_GetStats(CpuPerf_MeasurementIdType measurementId, CpuPerf_StatsType *stats);
uint32 CpuPerf_GetAverageCycles(const CpuPerf_StatsType *stats);
uint32 CpuPerf_GetAverageInstructions(const CpuPerf_StatsType *stats);
uint32 CpuPerf_GetCpiX1000(const CpuPerf_StatsType *stats);
uint32 CpuPerf_CyclesToNs(uint8 coreId, uint32 cycles);
Std_ReturnType CpuPerf_BuildSnapshotPayload(uint8 startId, uint8 requestedCount, uint8* respData, uint16* respLen);
boolean CpuPerf_IsRoutineId(uint16 routineId);
Dcm_ReturnType CpuPerf_HandleRoutineControl(
        Dcm_OpStatusType opStatus,
        uint8 routineControlType,
        uint16 routineId,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen);
#else
static inline void CpuPerf_InitCore(void) {}
static inline void CpuPerf_Start(CpuPerf_MeasurementIdType measurementId, CpuPerf_ContextType *context)
{
    (void)measurementId;
    (void)context;
}
static inline void CpuPerf_Stop(CpuPerf_MeasurementIdType measurementId, CpuPerf_ContextType *context)
{
    (void)measurementId;
    (void)context;
}
static inline void CpuPerf_AddBytes(CpuPerf_MeasurementIdType measurementId, uint32 byteCount)
{
    (void)measurementId;
    (void)byteCount;
}
static inline void CpuPerf_ResetStatistics(void) {}
static inline Std_ReturnType CpuPerf_GetStats(CpuPerf_MeasurementIdType measurementId, CpuPerf_StatsType *stats)
{
    (void)measurementId;
    (void)stats;
    return E_NOT_OK;
}
static inline uint32 CpuPerf_GetAverageCycles(const CpuPerf_StatsType *stats)
{
    (void)stats;
    return 0u;
}
static inline uint32 CpuPerf_GetAverageInstructions(const CpuPerf_StatsType *stats)
{
    (void)stats;
    return 0u;
}
static inline uint32 CpuPerf_GetCpiX1000(const CpuPerf_StatsType *stats)
{
    (void)stats;
    return 0u;
}
static inline uint32 CpuPerf_CyclesToNs(uint8 coreId, uint32 cycles)
{
    (void)coreId;
    (void)cycles;
    return 0u;
}
static inline Std_ReturnType CpuPerf_BuildSnapshotPayload(uint8 startId, uint8 requestedCount, uint8* respData, uint16* respLen)
{
    (void)startId;
    (void)requestedCount;
    (void)respData;
    (void)respLen;
    return E_NOT_OK;
}
static inline boolean CpuPerf_IsRoutineId(uint16 routineId)
{
    (void)routineId;
    return FALSE;
}
static inline Dcm_ReturnType CpuPerf_HandleRoutineControl(
        Dcm_OpStatusType opStatus,
        uint8 routineControlType,
        uint16 routineId,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    (void)opStatus;
    (void)routineControlType;
    (void)routineId;
    (void)reqData;
    (void)reqLen;
    (void)respData;
    (void)respLen;
    return DCM_NRC_REQUEST_OUT_OF_RANGE;
}
#endif

#endif /* CPUPERF_H */
