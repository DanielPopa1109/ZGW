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
    uint8 schedulerTraceValid;
} CpuPerf_RecordType;

typedef struct
{
    const void *monitoredTask;
    const void *currentTask;
    uint32 switchedOutTick;
    uint64 totalDescheduledTicks;
    uint64 switchOutCount;
    uint8 switchedOut;
} CpuPerf_SchedulerTraceType;

static CpuPerf_RecordType CpuPerf_Records[CPUPERF_ID_COUNT];
static volatile uint8 CpuPerf_CoreInitialized[CPUPERF_CORE_COUNT];
static volatile uint8 CpuPerf_HardwareCountersAvailable[CPUPERF_CORE_COUNT];
static CpuPerf_SchedulerTraceType CpuPerf_SchedulerTrace[CPUPERF_CORE_COUNT];

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
static void CpuPerf_UpdateRecord(CpuPerf_RecordType *record, uint32 cycles, uint32 instructions,
        uint32 elapsedNs, uint32 descheduledNs, uint32 switchOutCount, boolean overflow);
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
    record->stats.over5msCount = 0u;
    record->stats.over10msCount = 0u;
    record->stats.over20msCount = 0u;
    record->stats.lastDescheduledNs = 0u;
    record->stats.maxDescheduledNs = 0u;
    record->stats.totalDescheduledNs = 0u;
    record->stats.lastSwitchOutCount = 0u;
    record->stats.totalSwitchOutCount = 0u;
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

static void CpuPerf_UpdateRecord(CpuPerf_RecordType *record, uint32 cycles, uint32 instructions,
        uint32 elapsedNs, uint32 descheduledNs, uint32 switchOutCount, boolean overflow)
{
    record->stats.lastCycles = cycles;
    record->stats.lastInstructions = instructions;
    record->stats.lastNs = elapsedNs;
    record->stats.lastDescheduledNs = descheduledNs;
    record->stats.lastSwitchOutCount = switchOutCount;
    if (record->stats.sampleCount == 0u)
    {
        record->stats.minCycles = cycles;
        record->stats.maxCycles = cycles;
        record->stats.minInstructions = instructions;
        record->stats.maxInstructions = instructions;
        record->stats.minNs = elapsedNs;
        record->stats.maxNs = elapsedNs;
        record->stats.maxDescheduledNs = descheduledNs;
    }
    else
    {
        if (cycles < record->stats.minCycles) { record->stats.minCycles = cycles; }
        if (cycles > record->stats.maxCycles) { record->stats.maxCycles = cycles; }
        if (instructions < record->stats.minInstructions) { record->stats.minInstructions = instructions; }
        if (instructions > record->stats.maxInstructions) { record->stats.maxInstructions = instructions; }
        if (elapsedNs < record->stats.minNs) { record->stats.minNs = elapsedNs; }
        if (elapsedNs > record->stats.maxNs) { record->stats.maxNs = elapsedNs; }
        if (descheduledNs > record->stats.maxDescheduledNs) { record->stats.maxDescheduledNs = descheduledNs; }
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
    if ((0xFFFFFFFFFFFFFFFFULL - record->stats.totalDescheduledNs) >= (uint64)descheduledNs)
    {
        record->stats.totalDescheduledNs += (uint64)descheduledNs;
    }
    if ((0xFFFFFFFFFFFFFFFFULL - record->stats.totalSwitchOutCount) >= (uint64)switchOutCount)
    {
        record->stats.totalSwitchOutCount += (uint64)switchOutCount;
    }
    if ((overflow != FALSE) && (record->stats.overflowCount < 0xFFFFFFFFu))
    {
        record->stats.overflowCount++;
    }
    if ((elapsedNs > 5000000u) && (record->stats.over5msCount < 0xFFFFFFFFu))
    {
        record->stats.over5msCount++;
    }
    if ((elapsedNs > 10000000u) && (record->stats.over10msCount < 0xFFFFFFFFu))
    {
        record->stats.over10msCount++;
    }
    if ((elapsedNs > 20000000u) && (record->stats.over20msCount < 0xFFFFFFFFu))
    {
        record->stats.over20msCount++;
    }
    /* Publish the sample last; snapshot readers use this as the sequence
     * boundary for every field above, including the overrun histogram. */
    if (record->stats.sampleCount < 0xFFFFFFFFu)
    {
        record->stats.sampleCount++;
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
    uint32 beforeCycles;
    uint32 afterCycles;
    uint32 beforeInstructions;
    uint32 afterInstructions;

    if (coreId >= CPUPERF_CORE_COUNT)
    {
        return;
    }

    if (CpuPerf_CoreInitialized[coreId] == 0u)
    {
        /* Clear stale/debugger-owned counter state before enabling normal mode. */
        IfxCpu_resetAndStartCounters(IfxCpu_CounterMode_normal);
        __isync();
        beforeCycles = CpuPerf_ReadCounter(CPU_CCNT);
        beforeInstructions = CpuPerf_ReadCounter(CPU_ICNT);
        __nop();
        __nop();
        __nop();
        __nop();
        __nop();
        __nop();
        afterCycles = CpuPerf_ReadCounter(CPU_CCNT);
        afterInstructions = CpuPerf_ReadCounter(CPU_ICNT);
        CpuPerf_HardwareCountersAvailable[coreId] =
                ((afterCycles != beforeCycles) &&
                 (afterInstructions != beforeInstructions)) ? 1u : 0u;
        CpuPerf_CoreInitialized[coreId] = 1u;
    }
}

void CpuPerf_RegisterMonitoredTask(uint8 coreId, const void *taskHandle)
{
    if (coreId < CPUPERF_CORE_COUNT)
    {
        CpuPerf_SchedulerTrace[coreId].monitoredTask = taskHandle;
        CpuPerf_SchedulerTrace[coreId].switchedOut = 0u;
    }
}

void CpuPerf_TraceTaskSwitchedOut(uint8 coreId, const void *taskHandle)
{
    CpuPerf_SchedulerTraceType *trace;
    if (coreId >= CPUPERF_CORE_COUNT) { return; }
    trace = &CpuPerf_SchedulerTrace[coreId];
    trace->currentTask = NULL_PTR;
    if ((taskHandle == trace->monitoredTask) && (taskHandle != NULL_PTR) && (trace->switchedOut == 0u))
    {
        trace->switchedOutTick = CpuPerf_ReadTimeTicks(coreId);
        trace->switchedOut = 1u;
        if (trace->switchOutCount < 0xFFFFFFFFFFFFFFFFULL) { trace->switchOutCount++; }
    }
}

void CpuPerf_TraceTaskSwitchedIn(uint8 coreId, const void *taskHandle)
{
    CpuPerf_SchedulerTraceType *trace;
    uint32 now;
    if (coreId >= CPUPERF_CORE_COUNT) { return; }
    trace = &CpuPerf_SchedulerTrace[coreId];
    if ((taskHandle == trace->monitoredTask) && (taskHandle != NULL_PTR) && (trace->switchedOut != 0u))
    {
        now = CpuPerf_ReadTimeTicks(coreId);
        trace->totalDescheduledTicks += (uint64)((uint32)(now - trace->switchedOutTick));
        trace->switchedOut = 0u;
    }
    trace->currentTask = taskHandle;
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
    if ((coreId < CPUPERF_CORE_COUNT) &&
            (CpuPerf_HardwareCountersAvailable[coreId] != 0u))
    {
        context->startCycles = CpuPerf_ReadCounter(CPU_CCNT);
        context->startInstructions = CpuPerf_ReadCounter(CPU_ICNT);
    }
    else
    {
        context->startCycles = 0u;
        context->startInstructions = 0u;
    }
    context->startTimeTicks = CpuPerf_ReadTimeTicks(coreId);
    context->startCoreId = coreId;
    if ((coreId < CPUPERF_CORE_COUNT) &&
            (CpuPerf_SchedulerTrace[coreId].currentTask == CpuPerf_SchedulerTrace[coreId].monitoredTask) &&
            (CpuPerf_SchedulerTrace[coreId].monitoredTask != NULL_PTR) &&
            (CpuPerf_SchedulerTrace[coreId].switchedOut == 0u))
    {
        context->startDescheduledTicks = CpuPerf_SchedulerTrace[coreId].totalDescheduledTicks;
        context->startSwitchOutCount = CpuPerf_SchedulerTrace[coreId].switchOutCount;
        context->schedulerTraceAvailable = 1u;
    }
    else
    {
        context->startDescheduledTicks = 0u;
        context->startSwitchOutCount = 0u;
        context->schedulerTraceAvailable = 0u;
    }
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
    uint32 descheduledNs = 0u;
    uint32 switchOutCount = 0u;
    boolean overflow;
    uint8 coreId;

    if ((measurementId >= CPUPERF_ID_COUNT) || (context == NULL_PTR))
    {
        return;
    }

    coreId = CpuPerf_GetCoreId();
    if (context->startCoreId != coreId)
    {
        return;
    }

    stopTimeTicks = CpuPerf_ReadTimeTicks(coreId);
    if ((coreId < CPUPERF_CORE_COUNT) &&
            (CpuPerf_HardwareCountersAvailable[coreId] != 0u))
    {
        stopCycles = CpuPerf_ReadCounter(CPU_CCNT);
        stopInstructions = CpuPerf_ReadCounter(CPU_ICNT);
        overflow = CpuPerf_HasOverflow();
        cycles = CpuPerf_Delta31(context->startCycles, stopCycles);
        instructions = CpuPerf_Delta31(context->startInstructions, stopInstructions);
    }
    else
    {
        overflow = FALSE;
        cycles = 0u;
        instructions = 0u;
    }
    elapsedNs = CpuPerf_ElapsedNs(coreId, (uint32)context->startTimeTicks, stopTimeTicks);
    if ((context->schedulerTraceAvailable != 0u) &&
            (coreId < CPUPERF_CORE_COUNT) &&
            (CpuPerf_SchedulerTrace[coreId].switchedOut == 0u))
    {
        uint64 descheduledTicks = CpuPerf_SchedulerTrace[coreId].totalDescheduledTicks - context->startDescheduledTicks;
        uint64 switches = CpuPerf_SchedulerTrace[coreId].switchOutCount - context->startSwitchOutCount;
        uint64 frequencyHz = (uint64)CpuPerf_StmClockHz(coreId);
        uint64 ns = (frequencyHz != 0u) ?
                (((descheduledTicks * 1000000000ULL) + (frequencyHz / 2u)) / frequencyHz) : 0u;
        descheduledNs = (ns > (uint64)elapsedNs) ? elapsedNs : (uint32)ns;
        switchOutCount = (switches > 0xFFFFFFFFULL) ? 0xFFFFFFFFu : (uint32)switches;
    }

    record = &CpuPerf_Records[measurementId];
    record->coreId = coreId;
    record->valid = 1u;
    record->schedulerTraceValid = context->schedulerTraceAvailable;
    CpuPerf_UpdateRecord(record, cycles, instructions, elapsedNs, descheduledNs, switchOutCount, overflow);
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
        CpuPerf_Records[id].schedulerTraceValid = 0u;
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
    uint8 availableCore;
    uint32 counterMask;

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
    counterMask = 0u;
    for (availableCore = 0u; availableCore < CPUPERF_CORE_COUNT; availableCore++)
    {
        if (CpuPerf_HardwareCountersAvailable[availableCore] != 0u)
        {
            counterMask = CPUPERF_COUNTER_VALUE_MASK;
            break;
        }
    }
    CpuPerf_PutU32(&respData[12u], counterMask);
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
        respData[offset + 3u] = ((coreId < CPUPERF_CORE_COUNT) &&
                (CpuPerf_HardwareCountersAvailable[coreId] != 0u)) ?
                CPUPERF_ENTRY_FLAG_HW_COUNTERS : 0u;
        if (record->schedulerTraceValid != 0u)
        {
            respData[offset + 3u] |= CPUPERF_ENTRY_FLAG_SCHED_TRACE;
        }
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
        CpuPerf_PutU32(&respData[offset + 56u], stats.minNs);
        CpuPerf_PutU32(&respData[offset + 60u], stats.maxNs);
        CpuPerf_PutU32(&respData[offset + 64u], stats.over5msCount);
        CpuPerf_PutU32(&respData[offset + 68u], stats.over10msCount);
        CpuPerf_PutU32(&respData[offset + 72u], stats.over20msCount);
        CpuPerf_PutU32(&respData[offset + 76u], stats.lastDescheduledNs);
        CpuPerf_PutU32(&respData[offset + 80u], (stats.sampleCount != 0u) ?
                (uint32)(stats.totalDescheduledNs / stats.sampleCount) : 0u);
        CpuPerf_PutU32(&respData[offset + 84u], stats.maxDescheduledNs);
        CpuPerf_PutU64(&respData[offset + 88u], stats.totalDescheduledNs);
        CpuPerf_PutU32(&respData[offset + 96u], stats.lastSwitchOutCount);
        CpuPerf_PutU32(&respData[offset + 100u], (stats.totalSwitchOutCount > 0xFFFFFFFFULL) ?
                0xFFFFFFFFu : (uint32)stats.totalSwitchOutCount);
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
