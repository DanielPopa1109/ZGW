#include "BSW/Sys/CpuPerf/CpuPerf.h"

#if CPU_PERF_ENABLED

#include "IfxCpu.h"
#include "IfxCpu_reg.h"
#include "IfxStm_reg.h"
#include "FreeRTOSConfig_core0.h"
#include "FreeRTOSConfig_core1.h"
#include "FreeRTOSConfig_core2.h"

#define CPUPERF_ROUTINE_START                 (0x01u)
#define CPUPERF_ROUTINE_REQUEST_RESULTS       (0x03u)
#define CPUPERF_ROUTINE_RESET_STATS           (0x04u)

typedef struct
{
    CpuPerf_StatsType stats;
    uint8 coreId;
    uint8 valid;
} CpuPerf_RecordType;

static CpuPerf_RecordType CpuPerf_Records[CPUPERF_ID_COUNT];
static volatile uint8 CpuPerf_CoreInitialized[CPUPERF_CORE_COUNT];

static uint8 CpuPerf_GetCoreId(void);
static uint32 CpuPerf_ReadCounter(uint16 counterAddress);
static uint32 CpuPerf_Delta31(uint32 startValue, uint32 stopValue);
static boolean CpuPerf_HasOverflow(void);
static uint32 CpuPerf_CoreClockHz(uint8 coreId);
static uint32 CpuPerf_StmClockHz(uint8 coreId);
static uint32 CpuPerf_ReadTimeTicks(uint8 coreId);
static uint32 CpuPerf_ElapsedNs(uint8 coreId, uint32 startTicks, uint32 stopTicks);
static void CpuPerf_ResetOne(CpuPerf_RecordType *record);
static CpuPerf_StatsType CpuPerf_ReadStatsSnapshot(const CpuPerf_RecordType *record);
static void CpuPerf_UpdateRecord(CpuPerf_RecordType *record, uint32 cycles, uint32 instructions, uint32 elapsedNs, boolean overflow);
static void CpuPerf_PutU16(uint8 *data, uint16 value);
static void CpuPerf_PutU32(uint8 *data, uint32 value);
static void CpuPerf_PutU64(uint8 *data, uint64 value);
static uint8 CpuPerf_RecordCore(CpuPerf_MeasurementIdType measurementId);

static uint8 CpuPerf_GetCoreId(void)
{
    return (uint8)IfxCpu_getCoreIndex();
}

static uint32 CpuPerf_ReadCounter(uint16 counterAddress)
{
    return IfxCpu_getPerformanceCounter(counterAddress) & CPUPERF_COUNTER_VALUE_MASK;
}

static uint32 CpuPerf_Delta31(uint32 startValue, uint32 stopValue)
{
    return (stopValue - startValue) & CPUPERF_COUNTER_VALUE_MASK;
}

static boolean CpuPerf_HasOverflow(void)
{
    return ((IfxCpu_getPerformanceCounterStickyOverflow(CPU_CCNT) != FALSE) ||
            (IfxCpu_getPerformanceCounterStickyOverflow(CPU_ICNT) != FALSE)) ? TRUE : FALSE;
}

static uint32 CpuPerf_CoreClockHz(uint8 coreId)
{
    switch (coreId)
    {
        case 0u:
            return (uint32)configCPU_CLOCK_HZ_core0;
        case 1u:
            return (uint32)configCPU_CLOCK_HZ_core1;
        case 2u:
            return (uint32)configCPU_CLOCK_HZ_core2;
        default:
            return 0u;
    }
}

static uint32 CpuPerf_StmClockHz(uint8 coreId)
{
    switch (coreId)
    {
        case 0u:
            return (uint32)configSTM_CLOCK_HZ_core0;
        case 1u:
            return (uint32)configSTM_CLOCK_HZ_core1;
        case 2u:
            return (uint32)configSTM_CLOCK_HZ_core2;
        default:
            return 0u;
    }
}

static uint32 CpuPerf_ReadTimeTicks(uint8 coreId)
{
    switch (coreId)
    {
        case 0u:
            return STM0_TIM0.U;
        case 1u:
            return STM1_TIM0.U;
        case 2u:
            return STM2_TIM0.U;
        default:
            return 0u;
    }
}

static uint32 CpuPerf_ElapsedNs(uint8 coreId, uint32 startTicks, uint32 stopTicks)
{
    uint64 frequencyHz;
    uint64 deltaTicks;
    uint64 elapsedNs;

    frequencyHz = (uint64)CpuPerf_StmClockHz(coreId);
    if (frequencyHz == 0u)
    {
        return 0u;
    }

    deltaTicks = (uint64)((uint32)(stopTicks - startTicks));
    elapsedNs = ((deltaTicks * 1000000000ULL) + (frequencyHz / 2u)) / frequencyHz;
    return (elapsedNs > 0xFFFFFFFFULL) ? 0xFFFFFFFFu : (uint32)elapsedNs;
}

static void CpuPerf_ResetOne(CpuPerf_RecordType *record)
{
    record->stats.lastCycles = 0u;
    record->stats.minCycles = 0xFFFFFFFFu;
    record->stats.maxCycles = 0u;
    record->stats.totalCycles = 0u;
    record->stats.lastInstructions = 0u;
    record->stats.minInstructions = 0xFFFFFFFFu;
    record->stats.maxInstructions = 0u;
    record->stats.totalInstructions = 0u;
    record->stats.sampleCount = 0u;
    record->stats.overflowCount = 0u;
    record->stats.lastNs = 0u;
    record->stats.minNs = 0xFFFFFFFFu;
    record->stats.maxNs = 0u;
    record->stats.totalNs = 0u;
    record->stats.totalBytes = 0u;
}

static CpuPerf_StatsType CpuPerf_ReadStatsSnapshot(const CpuPerf_RecordType *record)
{
    CpuPerf_StatsType snapshot;
    uint32 sampleCount;

    do
    {
        sampleCount = record->stats.sampleCount;
        snapshot = record->stats;
    } while (sampleCount != record->stats.sampleCount);

    return snapshot;
}

static void CpuPerf_UpdateRecord(CpuPerf_RecordType *record, uint32 cycles, uint32 instructions, uint32 elapsedNs, boolean overflow)
{
    record->stats.lastCycles = cycles;
    record->stats.lastInstructions = instructions;
    record->stats.lastNs = elapsedNs;
    if (record->stats.sampleCount == 0u)
    {
        record->stats.minCycles = cycles;
        record->stats.maxCycles = cycles;
        record->stats.minInstructions = instructions;
        record->stats.maxInstructions = instructions;
        record->stats.minNs = elapsedNs;
        record->stats.maxNs = elapsedNs;
    }
    else
    {
        if (cycles < record->stats.minCycles) { record->stats.minCycles = cycles; }
        if (cycles > record->stats.maxCycles) { record->stats.maxCycles = cycles; }
        if (instructions < record->stats.minInstructions) { record->stats.minInstructions = instructions; }
        if (instructions > record->stats.maxInstructions) { record->stats.maxInstructions = instructions; }
        if (elapsedNs < record->stats.minNs) { record->stats.minNs = elapsedNs; }
        if (elapsedNs > record->stats.maxNs) { record->stats.maxNs = elapsedNs; }
    }

    if ((0xFFFFFFFFFFFFFFFFULL - record->stats.totalCycles) >= (uint64)cycles)
    {
        record->stats.totalCycles += (uint64)cycles;
    }
    if ((0xFFFFFFFFFFFFFFFFULL - record->stats.totalInstructions) >= (uint64)instructions)
    {
        record->stats.totalInstructions += (uint64)instructions;
    }
    if ((0xFFFFFFFFFFFFFFFFULL - record->stats.totalNs) >= (uint64)elapsedNs)
    {
        record->stats.totalNs += (uint64)elapsedNs;
    }
    if (record->stats.sampleCount < 0xFFFFFFFFu)
    {
        record->stats.sampleCount++;
    }
    if ((overflow != FALSE) && (record->stats.overflowCount < 0xFFFFFFFFu))
    {
        record->stats.overflowCount++;
    }
}

static void CpuPerf_PutU16(uint8 *data, uint16 value)
{
    data[0u] = (uint8)(value >> 8u);
    data[1u] = (uint8)value;
}

static void CpuPerf_PutU32(uint8 *data, uint32 value)
{
    data[0u] = (uint8)(value >> 24u);
    data[1u] = (uint8)(value >> 16u);
    data[2u] = (uint8)(value >> 8u);
    data[3u] = (uint8)value;
}

static void CpuPerf_PutU64(uint8 *data, uint64 value)
{
    CpuPerf_PutU32(&data[0u], (uint32)(value >> 32u));
    CpuPerf_PutU32(&data[4u], (uint32)value);
}

static uint8 CpuPerf_RecordCore(CpuPerf_MeasurementIdType measurementId)
{
    if (measurementId >= CPUPERF_ID_COUNT)
    {
        return CPUPERF_CORE_UNKNOWN;
    }
    return CpuPerf_Records[measurementId].coreId;
}

void CpuPerf_InitCore(void)
{
    uint8 coreId = CpuPerf_GetCoreId();
    Ifx_CPU_CCTRL cctrl;

    if (coreId >= CPUPERF_CORE_COUNT)
    {
        return;
    }

    if (CpuPerf_CoreInitialized[coreId] == 0u)
    {
        cctrl.U = __mfcr(CPU_CCTRL);
        cctrl.B.CM = IfxCpu_CounterMode_normal;
        cctrl.B.CE = 1u;
        __mtcr(CPU_CCTRL, cctrl.U);
        CpuPerf_CoreInitialized[coreId] = 1u;
    }
}

void CpuPerf_Start(CpuPerf_MeasurementIdType measurementId, CpuPerf_ContextType *context)
{
    uint8 coreId;

    (void)measurementId;
    if (context == NULL_PTR)
    {
        return;
    }

    coreId = CpuPerf_GetCoreId();
    context->startCycles = CpuPerf_ReadCounter(CPU_CCNT);
    context->startInstructions = CpuPerf_ReadCounter(CPU_ICNT);
    context->startTimeTicks = CpuPerf_ReadTimeTicks(coreId);
    context->startCoreId = coreId;
}

void CpuPerf_Stop(CpuPerf_MeasurementIdType measurementId, CpuPerf_ContextType *context)
{
    CpuPerf_RecordType *record;
    uint32 stopCycles;
    uint32 stopInstructions;
    uint32 stopTimeTicks;
    uint32 cycles;
    uint32 instructions;
    uint32 elapsedNs;
    boolean overflow;
    uint8 coreId;

    if ((measurementId >= CPUPERF_ID_COUNT) || (context == NULL_PTR))
    {
        return;
    }

    stopCycles = CpuPerf_ReadCounter(CPU_CCNT);
    stopInstructions = CpuPerf_ReadCounter(CPU_ICNT);
    coreId = CpuPerf_GetCoreId();
    if (context->startCoreId != coreId)
    {
        return;
    }

    stopTimeTicks = CpuPerf_ReadTimeTicks(coreId);
    overflow = CpuPerf_HasOverflow();
    cycles = CpuPerf_Delta31(context->startCycles, stopCycles);
    instructions = CpuPerf_Delta31(context->startInstructions, stopInstructions);
    elapsedNs = CpuPerf_ElapsedNs(coreId, (uint32)context->startTimeTicks, stopTimeTicks);

    record = &CpuPerf_Records[measurementId];
    record->coreId = coreId;
    record->valid = 1u;
    CpuPerf_UpdateRecord(record, cycles, instructions, elapsedNs, overflow);
}

void CpuPerf_AddBytes(CpuPerf_MeasurementIdType measurementId, uint32 byteCount)
{
    CpuPerf_RecordType *record;

    if (measurementId >= CPUPERF_ID_COUNT)
    {
        return;
    }

    record = &CpuPerf_Records[measurementId];
    if ((0xFFFFFFFFFFFFFFFFULL - record->stats.totalBytes) >= (uint64)byteCount)
    {
        record->stats.totalBytes += (uint64)byteCount;
    }
}

void CpuPerf_ResetStatistics(void)
{
    uint16 id;

    for (id = 0u; id < (uint16)CPUPERF_ID_COUNT; id++)
    {
        CpuPerf_ResetOne(&CpuPerf_Records[id]);
        CpuPerf_Records[id].valid = 0u;
    }
}

Std_ReturnType CpuPerf_GetStats(CpuPerf_MeasurementIdType measurementId, CpuPerf_StatsType *stats)
{
    if ((measurementId >= CPUPERF_ID_COUNT) || (stats == NULL_PTR) ||
            (CpuPerf_Records[measurementId].valid == 0u))
    {
        return E_NOT_OK;
    }

    *stats = CpuPerf_ReadStatsSnapshot(&CpuPerf_Records[measurementId]);
    return E_OK;
}

uint32 CpuPerf_GetAverageCycles(const CpuPerf_StatsType *stats)
{
    if ((stats == NULL_PTR) || (stats->sampleCount == 0u))
    {
        return 0u;
    }
    return (uint32)(stats->totalCycles / stats->sampleCount);
}

uint32 CpuPerf_GetAverageInstructions(const CpuPerf_StatsType *stats)
{
    if ((stats == NULL_PTR) || (stats->sampleCount == 0u))
    {
        return 0u;
    }
    return (uint32)(stats->totalInstructions / stats->sampleCount);
}

uint32 CpuPerf_GetCpiX1000(const CpuPerf_StatsType *stats)
{
    if ((stats == NULL_PTR) || (stats->totalInstructions == 0u))
    {
        return 0u;
    }
    return (uint32)((stats->totalCycles * 1000u) / stats->totalInstructions);
}

static uint32 CpuPerf_GetAverageNs(const CpuPerf_StatsType *stats)
{
    if ((stats == NULL_PTR) || (stats->sampleCount == 0u))
    {
        return 0u;
    }
    return (uint32)(stats->totalNs / stats->sampleCount);
}

uint32 CpuPerf_CyclesToNs(uint8 coreId, uint32 cycles)
{
    uint32 frequencyHz = CpuPerf_CoreClockHz(coreId);
    uint64 ns;

    if (frequencyHz == 0u)
    {
        return 0u;
    }

    ns = (((uint64)cycles * 1000000000ULL) + ((uint64)frequencyHz / 2u)) / (uint64)frequencyHz;
    return (ns > 0xFFFFFFFFULL) ? 0xFFFFFFFFu : (uint32)ns;
}

Std_ReturnType CpuPerf_BuildSnapshotPayload(uint8 startId, uint8 requestedCount, uint8* respData, uint16* respLen)
{
    uint8 count;
    uint8 index;
    uint16 offset;
    CpuPerf_RecordType *record;
    CpuPerf_StatsType stats;
    uint8 coreId;

    if ((respData == NULL_PTR) || (respLen == NULL_PTR) ||
            (startId >= (uint8)CPUPERF_ID_COUNT))
    {
        return E_NOT_OK;
    }

    count = (requestedCount > CPUPERF_RESPONSE_MAX_ENTRIES) ?
            CPUPERF_RESPONSE_MAX_ENTRIES : requestedCount;
    if (((uint16)startId + (uint16)count) > (uint16)CPUPERF_ID_COUNT)
    {
        count = (uint8)((uint16)CPUPERF_ID_COUNT - (uint16)startId);
    }

    CpuPerf_PutU32(&respData[0u], CPUPERF_MAGIC);
    CpuPerf_PutU16(&respData[4u], CPUPERF_VERSION);
    CpuPerf_PutU16(&respData[6u], (uint16)CPUPERF_ID_COUNT);
    respData[8u] = startId;
    respData[9u] = count;
    CpuPerf_PutU16(&respData[10u], (uint16)CPUPERF_RESPONSE_ENTRY_LEN);
    CpuPerf_PutU32(&respData[12u], CPUPERF_COUNTER_VALUE_MASK);
    offset = CPUPERF_RESPONSE_HEADER_LEN;

    for (index = 0u; index < count; index++)
    {
        record = &CpuPerf_Records[(uint16)startId + (uint16)index];
        stats = CpuPerf_ReadStatsSnapshot(record);
        coreId = (record->valid != 0u) ?
                CpuPerf_RecordCore((CpuPerf_MeasurementIdType)((uint16)startId + (uint16)index)) :
                CPUPERF_CORE_UNKNOWN;
        respData[offset + 0u] = (uint8)((uint16)startId + (uint16)index);
        respData[offset + 1u] = record->valid;
        respData[offset + 2u] = coreId;
        respData[offset + 3u] = 0u;
        CpuPerf_PutU32(&respData[offset + 4u], stats.lastCycles);
        CpuPerf_PutU32(&respData[offset + 8u], stats.minCycles);
        CpuPerf_PutU32(&respData[offset + 12u], stats.maxCycles);
        CpuPerf_PutU32(&respData[offset + 16u], CpuPerf_GetAverageCycles(&stats));
        CpuPerf_PutU32(&respData[offset + 20u], stats.lastInstructions);
        CpuPerf_PutU32(&respData[offset + 24u], CpuPerf_GetAverageInstructions(&stats));
        CpuPerf_PutU32(&respData[offset + 28u], CpuPerf_GetCpiX1000(&stats));
        CpuPerf_PutU32(&respData[offset + 32u], stats.sampleCount);
        CpuPerf_PutU32(&respData[offset + 36u], stats.overflowCount);
        CpuPerf_PutU32(&respData[offset + 40u], stats.lastNs);
        CpuPerf_PutU32(&respData[offset + 44u], CpuPerf_GetAverageNs(&stats));
        CpuPerf_PutU64(&respData[offset + 48u], stats.totalBytes);
        offset = (uint16)(offset + CPUPERF_RESPONSE_ENTRY_LEN);
    }

    *respLen = offset;
    return E_OK;
}

boolean CpuPerf_IsRoutineId(uint16 routineId)
{
    return (routineId == CPUPERF_ROUTINE_ID) ? TRUE : FALSE;
}

Dcm_ReturnType CpuPerf_HandleRoutineControl(
        Dcm_OpStatusType opStatus,
        uint8 routineControlType,
        uint16 routineId,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen)
{
    uint8 startId;
    uint8 requestedCount;
    uint16 payloadLen;

    (void)opStatus;

    if (routineId != CPUPERF_ROUTINE_ID)
    {
        return DCM_NRC_REQUEST_OUT_OF_RANGE;
    }

    if (routineControlType == CPUPERF_ROUTINE_RESET_STATS)
    {
        if (reqLen != 0u)
        {
            return DCM_NRC_INCORRECT_LENGTH;
        }
        CpuPerf_ResetStatistics();
        *respLen = 0u;
        return DCM_E_OK;
    }

    if ((routineControlType != CPUPERF_ROUTINE_START) &&
            (routineControlType != CPUPERF_ROUTINE_REQUEST_RESULTS))
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }

    if ((reqLen != 0u) && (reqLen != 1u) && (reqLen != 2u))
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    startId = (reqLen >= 1u) ? reqData[0u] : 0u;
    requestedCount = (reqLen >= 2u) ? reqData[1u] : CPUPERF_RESPONSE_MAX_ENTRIES;
    if (CpuPerf_BuildSnapshotPayload(startId, requestedCount, respData, &payloadLen) != E_OK)
    {
        return DCM_NRC_REQUEST_OUT_OF_RANGE;
    }

    *respLen = payloadLen;
    return DCM_E_OK;
}

#endif /* CPU_PERF_ENABLED */
