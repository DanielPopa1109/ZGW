#include "TimeBase.h"
#include "TimeSync_Cfg.h"

#include "IfxCpu.h"
#include "IfxStm.h"

#if (TIMESYNC_SCR_RTC_ENABLE == STD_ON)
#include "IfxPms_reg.h"
#include "../../SCR/scr_time_shared.h"
#endif

#if (TIMESYNC_NVM_ENABLE == STD_ON)
#include "NvM.h"
#include "NvM_Cfg.h"
#endif

#define TIMEBASE_SECONDS_PER_MINUTE            60ull
#define TIMEBASE_SECONDS_PER_HOUR              3600ull
#define TIMEBASE_SECONDS_PER_DAY               86400ull
#define TIMEBASE_STM_FREQUENCY_FALLBACK_HZ     100000000u
#define TIMEBASE_CRITICAL_CORE_COUNT           6u
#define TIMEBASE_CRITICAL_SPIN_TIMEOUT         10000u

#define TIMEBASE_NVM_MAGIC                     0x54494D45u
#define TIMEBASE_NVM_VERSION                   1u
#define TIMEBASE_NVM_VALID                     1u
#define TIMEBASE_NVM_OFFSET_MAGIC              0u
#define TIMEBASE_NVM_OFFSET_VERSION            4u
#define TIMEBASE_NVM_OFFSET_VALID              5u
#define TIMEBASE_NVM_OFFSET_SOURCE             6u
#define TIMEBASE_NVM_OFFSET_UTC_NS             8u
#define TIMEBASE_NVM_OFFSET_VEHICLE_NS         16u
#define TIMEBASE_NVM_OFFSET_SYNC_STATUS        24u
#define TIMEBASE_NVM_OFFSET_STORE_COUNTER      28u
#define TIMEBASE_NVM_UPDATE_PERIOD_NS          ((uint64)TIMESYNC_NVM_RAM_UPDATE_PERIOD_MS * TIMEBASE_NS_PER_MS)

typedef struct
{
    uint64 vehicle_time_at_utc_set_ns;
    uint64 utc_time_at_set_ns;
    boolean utc_valid;
    TimeBase_TimeSourceType time_source;
    uint32 sync_status;
} TimeBase_UtcMappingType;

static uint32 TimeBase_StmFrequencyHz = TIMEBASE_STM_FREQUENCY_FALLBACK_HZ;
static uint64 TimeBase_LastCounterTicks = 0ull;
static uint64 TimeBase_VehicleTimeNs = 0ull;
static TimeBase_UtcMappingType TimeBase_UtcMapping;
static boolean TimeBase_Initialized = FALSE;
static uint64 TimeBase_LastNvMImageVehicleNs = 0ull;
static uint32 TimeBase_NvMStoreCounter = 0u;

#if (TIMESYNC_SCR_RTC_ENABLE == STD_ON)
static uint8 TimeBase_ScrRtcBootImage[SCR_TIME_RECORD_LENGTH];
static boolean TimeBase_ScrRtcBootImageValid = FALSE;
#endif

static IfxCpu_spinLock TimeBase_CriticalSpinLock;
static boolean TimeBase_CriticalIrqState[TIMEBASE_CRITICAL_CORE_COUNT];
static volatile uint8 TimeBase_CriticalOwnedByCore[TIMEBASE_CRITICAL_CORE_COUNT];

volatile uint32 TimeBase_CriticalTimeoutCounter = 0u;

static void TimeBase_EnterCritical(void);
static void TimeBase_ExitCritical(void);
static void TimeBase_InitFrequency(void);
static uint64 TimeBase_PlatformGetCounterTicks(void);
static uint64 TimeBase_CounterTicksToNs(uint64 ticks);
static void TimeBase_UpdateVehicleTimeLocked(void);
static boolean TimeBase_IsLeapYear(uint16 year);
static uint8 TimeBase_GetDaysInMonth(uint16 year, uint8 month);
static Std_ReturnType TimeBase_GetYearStartUtcNs(uint16 year, uint64 *utcTimeNs);
static uint32 TimeBase_GetSyncStatusForSource(TimeBase_TimeSourceType source);
static uint64 TimeBase_GetMappedUtcLocked(uint64 vehicleTimeNs);
static void TimeBase_StoreU32(uint8 *buffer, uint16 offset, uint32 value);
static void TimeBase_StoreU64(uint8 *buffer, uint16 offset, uint64 value);
static uint32 TimeBase_LoadU32(const uint8 *buffer, uint16 offset);
static uint64 TimeBase_LoadU64(const uint8 *buffer, uint16 offset);
static boolean TimeBase_IsPersistentSource(TimeBase_TimeSourceType source);
static Std_ReturnType TimeBase_UpdateNvMImage(uint64 utcTimeNs, uint64 vehicleTimeNs, TimeBase_TimeSourceType source, uint32 syncStatus);
#if (TIMESYNC_SCR_RTC_ENABLE == STD_ON)
static volatile uint8 *TimeBase_GetScrRtcXram(void);
static void TimeBase_ScrStoreU8(uint16 offset, uint8 value);
static uint8 TimeBase_ScrLoadU8(uint16 offset);
static void TimeBase_ScrStoreU32(uint16 offset, uint32 value);
static uint32 TimeBase_ScrLoadU32(uint16 offset);
static void TimeBase_ScrStoreU64(uint16 offset, uint64 value);
static uint32 TimeBase_ScrLoadU32FromImage(const uint8 *image, uint16 offset);
static uint64 TimeBase_ScrRtcTicksToNs(uint32 elapsedTicks);
static uint64 TimeBase_ApplyScrRtcCalibration(uint64 elapsedNs);
static Std_ReturnType TimeBase_TryGetStandbyRtcElapsedNs(TimeBase_TimeSourceType source, uint64 *elapsedNs, uint32 *syncStatusMask);
#endif

static void TimeBase_EnterCritical(void)
{
    uint32 coreIndex;

    coreIndex = (uint32)IfxCpu_getCoreIndex();
    if (coreIndex >= TIMEBASE_CRITICAL_CORE_COUNT)
    {
        coreIndex = 0u;
    }

    TimeBase_CriticalIrqState[coreIndex] = IfxCpu_disableInterrupts();

    if (IfxCpu_setSpinLock(&TimeBase_CriticalSpinLock, TIMEBASE_CRITICAL_SPIN_TIMEOUT) != FALSE)
    {
        TimeBase_CriticalOwnedByCore[coreIndex] = TRUE;
        __dsync();
    }
    else
    {
        TimeBase_CriticalTimeoutCounter++;
    }
}

static void TimeBase_ExitCritical(void)
{
    uint32 coreIndex;

    coreIndex = (uint32)IfxCpu_getCoreIndex();
    if (coreIndex >= TIMEBASE_CRITICAL_CORE_COUNT)
    {
        coreIndex = 0u;
    }

    __dsync();

    if (TimeBase_CriticalOwnedByCore[coreIndex] != FALSE)
    {
        TimeBase_CriticalOwnedByCore[coreIndex] = FALSE;
        IfxCpu_resetSpinLock(&TimeBase_CriticalSpinLock);
    }

    IfxCpu_restoreInterrupts(TimeBase_CriticalIrqState[coreIndex]);
}

static void TimeBase_InitFrequency(void)
{
    uint32 frequencyHz;

    frequencyHz = (uint32)IfxStm_getFrequency(&MODULE_STM0);
    if (frequencyHz == 0u)
    {
        frequencyHz = TIMEBASE_STM_FREQUENCY_FALLBACK_HZ;
    }

    TimeBase_StmFrequencyHz = frequencyHz;
}

static uint64 TimeBase_PlatformGetCounterTicks(void)
{
    return IfxStm_get(&MODULE_STM0);
}

static uint64 TimeBase_CounterTicksToNs(uint64 ticks)
{
    uint64 seconds;
    uint64 remainder;

    seconds = ticks / (uint64)TimeBase_StmFrequencyHz;
    remainder = ticks % (uint64)TimeBase_StmFrequencyHz;

    return (seconds * TIMEBASE_NS_PER_S) +
           ((remainder * TIMEBASE_NS_PER_S) / (uint64)TimeBase_StmFrequencyHz);
}

uint64 TimeBase_PlatformGetCounterNs(void)
{
    TimeBase_InitFrequency();
    return TimeBase_CounterTicksToNs(TimeBase_PlatformGetCounterTicks());
}

static void TimeBase_UpdateVehicleTimeLocked(void)
{
    uint64 counterNow;
    uint64 counterDelta;

    counterNow = TimeBase_PlatformGetCounterTicks();
    counterDelta = counterNow - TimeBase_LastCounterTicks;
    TimeBase_LastCounterTicks = counterNow;

    if (counterDelta != 0ull)
    {
        TimeBase_VehicleTimeNs += TimeBase_CounterTicksToNs(counterDelta);
    }
}

static boolean TimeBase_IsLeapYear(uint16 year)
{
    boolean leap;

    leap = FALSE;
    if ((year % 4u) == 0u)
    {
        leap = TRUE;
        if (((year % 100u) == 0u) && ((year % 400u) != 0u))
        {
            leap = FALSE;
        }
    }

    return leap;
}

static uint8 TimeBase_GetDaysInMonth(uint16 year, uint8 month)
{
    static const uint8 daysInMonth[12] =
    {
        31u, 28u, 31u, 30u, 31u, 30u,
        31u, 31u, 30u, 31u, 30u, 31u
    };

    if ((month == 0u) || (month > 12u))
    {
        return 0u;
    }

    if ((month == 2u) && (TimeBase_IsLeapYear(year) != FALSE))
    {
        return 29u;
    }

    return daysInMonth[(uint32)month - 1u];
}

Std_ReturnType TimeBase_ConvertDateTimeToUtcNs(
        uint16 year,
        uint8 month,
        uint8 day,
        uint8 hour,
        uint8 minute,
        uint8 second,
        uint16 millisecond,
        sint16 timezone_offset_min,
        uint64 *utcTimeNs)
{
    uint16 yearIt;
    uint8 monthIt;
    uint64 days;
    uint64 localSeconds;
    sint64 utcSeconds;

    if (utcTimeNs == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if ((year < 1970u) ||
        (month < 1u) ||
        (month > 12u) ||
        (day < 1u) ||
        (day > TimeBase_GetDaysInMonth(year, month)) ||
        (hour > 23u) ||
        (minute > 59u) ||
        (second > 59u) ||
        (millisecond > 999u))
    {
        return E_NOT_OK;
    }

    days = 0ull;
    for (yearIt = 1970u; yearIt < year; yearIt++)
    {
        days += (TimeBase_IsLeapYear(yearIt) != FALSE) ? 366ull : 365ull;
    }

    for (monthIt = 1u; monthIt < month; monthIt++)
    {
        days += (uint64)TimeBase_GetDaysInMonth(year, monthIt);
    }

    days += (uint64)day - 1ull;

    localSeconds = (days * TIMEBASE_SECONDS_PER_DAY) +
                   ((uint64)hour * TIMEBASE_SECONDS_PER_HOUR) +
                   ((uint64)minute * TIMEBASE_SECONDS_PER_MINUTE) +
                   (uint64)second;

    utcSeconds = (sint64)localSeconds - ((sint64)timezone_offset_min * (sint64)TIMEBASE_SECONDS_PER_MINUTE);
    if (utcSeconds < 0)
    {
        return E_NOT_OK;
    }

    *utcTimeNs = ((uint64)utcSeconds * TIMEBASE_NS_PER_S) +
                 ((uint64)millisecond * TIMEBASE_NS_PER_MS);

    return E_OK;
}

Std_ReturnType TimeBase_ConvertUtcNsToDateTime(uint64 utcTimeNs, TimeBase_DateTimeType *dateTime)
{
    uint64 totalSeconds;
    uint64 remainingDays;
    uint32 secondsOfDay;
    uint16 year;
    uint8 month;
    uint16 daysInYear;
    uint8 daysInMonth;

    if (dateTime == NULL_PTR)
    {
        return E_NOT_OK;
    }

    totalSeconds = utcTimeNs / TIMEBASE_NS_PER_S;
    remainingDays = totalSeconds / TIMEBASE_SECONDS_PER_DAY;
    secondsOfDay = (uint32)(totalSeconds % TIMEBASE_SECONDS_PER_DAY);
    year = 1970u;

    while (year <= (uint16)TIMESYNC_UTC_PLAUSIBLE_MAX_YEAR)
    {
        daysInYear = (TimeBase_IsLeapYear(year) != FALSE) ? 366u : 365u;
        if (remainingDays < (uint64)daysInYear)
        {
            break;
        }

        remainingDays -= (uint64)daysInYear;
        year++;
    }

    if (year > (uint16)TIMESYNC_UTC_PLAUSIBLE_MAX_YEAR)
    {
        return E_NOT_OK;
    }

    month = 1u;
    while (month <= 12u)
    {
        daysInMonth = TimeBase_GetDaysInMonth(year, month);
        if (remainingDays < (uint64)daysInMonth)
        {
            break;
        }

        remainingDays -= (uint64)daysInMonth;
        month++;
    }

    if (month > 12u)
    {
        return E_NOT_OK;
    }

    dateTime->year = year;
    dateTime->month = month;
    dateTime->day = (uint8)(remainingDays + 1ull);
    dateTime->hour = (uint8)(secondsOfDay / (uint32)TIMEBASE_SECONDS_PER_HOUR);
    secondsOfDay %= (uint32)TIMEBASE_SECONDS_PER_HOUR;
    dateTime->minute = (uint8)(secondsOfDay / (uint32)TIMEBASE_SECONDS_PER_MINUTE);
    dateTime->second = (uint8)(secondsOfDay % (uint32)TIMEBASE_SECONDS_PER_MINUTE);
    dateTime->millisecond = (uint16)((utcTimeNs % TIMEBASE_NS_PER_S) / TIMEBASE_NS_PER_MS);

    return E_OK;
}

static Std_ReturnType TimeBase_GetYearStartUtcNs(uint16 year, uint64 *utcTimeNs)
{
    return TimeBase_ConvertDateTimeToUtcNs(
            year,
            1u,
            1u,
            0u,
            0u,
            0u,
            0u,
            0,
            utcTimeNs);
}

Std_ReturnType TimeBase_ValidateUtcTimeNs(uint64 utcTimeNs)
{
    uint64 minUtcNs;
    uint64 maxUtcNs;

    if (TimeBase_GetYearStartUtcNs((uint16)TIMESYNC_UTC_PLAUSIBLE_MIN_YEAR, &minUtcNs) != E_OK)
    {
        return E_NOT_OK;
    }

    if (TimeBase_GetYearStartUtcNs((uint16)(TIMESYNC_UTC_PLAUSIBLE_MAX_YEAR + 1u), &maxUtcNs) != E_OK)
    {
        return E_NOT_OK;
    }

    if ((utcTimeNs < minUtcNs) || (utcTimeNs >= maxUtcNs))
    {
        return E_NOT_OK;
    }

    return E_OK;
}

static uint32 TimeBase_GetSyncStatusForSource(TimeBase_TimeSourceType source)
{
    switch (source)
    {
        case TIMEBASE_SOURCE_DEFAULT_COMPILETIME:
            return TIMEBASE_SYNC_STATUS_DEFAULT_UTC;

        case TIMEBASE_SOURCE_UDS_ROUTINE:
            return TIMEBASE_SYNC_STATUS_UDS_SET;

        case TIMEBASE_SOURCE_GPTP_MASTER:
            return TIMEBASE_SYNC_STATUS_GPTP_ACTIVE | TIMEBASE_SYNC_STATUS_GPTP_MASTER;

        default:
            return 0u;
    }
}

static uint64 TimeBase_GetMappedUtcLocked(uint64 vehicleTimeNs)
{
    if (TimeBase_UtcMapping.utc_valid == FALSE)
    {
        return 0ull;
    }

    return TimeBase_UtcMapping.utc_time_at_set_ns +
           (vehicleTimeNs - TimeBase_UtcMapping.vehicle_time_at_utc_set_ns);
}

static void TimeBase_StoreU32(uint8 *buffer, uint16 offset, uint32 value)
{
    if (buffer == NULL_PTR)
    {
        return;
    }

    buffer[offset] = (uint8)((value >> 24u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)((value >> 16u) & 0xFFu);
    buffer[(uint16)(offset + 2u)] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 3u)] = (uint8)(value & 0xFFu);
}

static void TimeBase_StoreU64(uint8 *buffer, uint16 offset, uint64 value)
{
    TimeBase_StoreU32(buffer, offset, (uint32)((value >> 32u) & 0xFFFFFFFFull));
    TimeBase_StoreU32(buffer, (uint16)(offset + 4u), (uint32)(value & 0xFFFFFFFFull));
}

static uint32 TimeBase_LoadU32(const uint8 *buffer, uint16 offset)
{
    if (buffer == NULL_PTR)
    {
        return 0u;
    }

    return ((uint32)buffer[offset] << 24u) |
           ((uint32)buffer[(uint16)(offset + 1u)] << 16u) |
           ((uint32)buffer[(uint16)(offset + 2u)] << 8u) |
           (uint32)buffer[(uint16)(offset + 3u)];
}

static uint64 TimeBase_LoadU64(const uint8 *buffer, uint16 offset)
{
    return ((uint64)TimeBase_LoadU32(buffer, offset) << 32u) |
           (uint64)TimeBase_LoadU32(buffer, (uint16)(offset + 4u));
}

#if (TIMESYNC_SCR_RTC_ENABLE == STD_ON)
static volatile uint8 *TimeBase_GetScrRtcXram(void)
{
    return &((volatile uint8 *)PMS_XRAM)[SCR_TIME_XRAM_BASE];
}

static void TimeBase_ScrStoreU8(uint16 offset, uint8 value)
{
    volatile uint8 *record = TimeBase_GetScrRtcXram();

    record[offset] = value;
}

static uint8 TimeBase_ScrLoadU8(uint16 offset)
{
    volatile uint8 *record = TimeBase_GetScrRtcXram();

    return record[offset];
}

static void TimeBase_ScrStoreU32(uint16 offset, uint32 value)
{
    TimeBase_ScrStoreU8(offset, (uint8)((value >> 24u) & 0xFFu));
    TimeBase_ScrStoreU8((uint16)(offset + 1u), (uint8)((value >> 16u) & 0xFFu));
    TimeBase_ScrStoreU8((uint16)(offset + 2u), (uint8)((value >> 8u) & 0xFFu));
    TimeBase_ScrStoreU8((uint16)(offset + 3u), (uint8)(value & 0xFFu));
}

static uint32 TimeBase_ScrLoadU32(uint16 offset)
{
    return ((uint32)TimeBase_ScrLoadU8(offset) << 24u) |
           ((uint32)TimeBase_ScrLoadU8((uint16)(offset + 1u)) << 16u) |
           ((uint32)TimeBase_ScrLoadU8((uint16)(offset + 2u)) << 8u) |
           (uint32)TimeBase_ScrLoadU8((uint16)(offset + 3u));
}

static void TimeBase_ScrStoreU64(uint16 offset, uint64 value)
{
    TimeBase_ScrStoreU32(offset, (uint32)((value >> 32u) & 0xFFFFFFFFull));
    TimeBase_ScrStoreU32((uint16)(offset + 4u), (uint32)(value & 0xFFFFFFFFull));
}

static uint32 TimeBase_ScrLoadU32FromImage(const uint8 *image, uint16 offset)
{
    return ((uint32)image[offset] << 24u) |
           ((uint32)image[(uint16)(offset + 1u)] << 16u) |
           ((uint32)image[(uint16)(offset + 2u)] << 8u) |
           (uint32)image[(uint16)(offset + 3u)];
}

static uint64 TimeBase_ApplyScrRtcCalibration(uint64 elapsedNs)
{
#if (TIMESYNC_SCR_RTC_CALIBRATION_PPM != 0)
    sint32 ppm = (sint32)TIMESYNC_SCR_RTC_CALIBRATION_PPM;
    uint32 absPpm;
    uint64 correctionNs;

    if (ppm < 0)
    {
        absPpm = (uint32)(-ppm);
        correctionNs = (elapsedNs / 1000000ull) * (uint64)absPpm;
        if (correctionNs < elapsedNs)
        {
            elapsedNs -= correctionNs;
        }
        else
        {
            elapsedNs = 0ull;
        }
    }
    else
    {
        absPpm = (uint32)ppm;
        correctionNs = (elapsedNs / 1000000ull) * (uint64)absPpm;
        elapsedNs += correctionNs;
    }
#endif

    return elapsedNs;
}

static uint64 TimeBase_ScrRtcTicksToNs(uint32 elapsedTicks)
{
    uint64 scaledTicks;
    uint64 seconds;
    uint64 remainder;
    uint64 elapsedNs;

#if (TIMESYNC_SCR_RTC_TICK_HZ_NUM == 0u)
    (void)elapsedTicks;
    return 0ull;
#else
    scaledTicks = (uint64)elapsedTicks * (uint64)TIMESYNC_SCR_RTC_TICK_HZ_DEN;
    seconds = scaledTicks / (uint64)TIMESYNC_SCR_RTC_TICK_HZ_NUM;
    remainder = scaledTicks % (uint64)TIMESYNC_SCR_RTC_TICK_HZ_NUM;
    elapsedNs = (seconds * TIMEBASE_NS_PER_S) +
            ((remainder * TIMEBASE_NS_PER_S) / (uint64)TIMESYNC_SCR_RTC_TICK_HZ_NUM);

    return TimeBase_ApplyScrRtcCalibration(elapsedNs);
#endif
}

static Std_ReturnType TimeBase_TryGetStandbyRtcElapsedNs(
        TimeBase_TimeSourceType source,
        uint64 *elapsedNs,
        uint32 *syncStatusMask)
{
    const uint8 *image = TimeBase_ScrRtcBootImage;
    uint8 flags;
    uint32 elapsedTicks;

    if ((elapsedNs == NULL_PTR) || (syncStatusMask == NULL_PTR))
    {
        return E_NOT_OK;
    }

    *elapsedNs = 0ull;
    *syncStatusMask = 0u;

    if (TimeBase_ScrRtcBootImageValid == FALSE)
    {
        return E_NOT_OK;
    }

    if ((TimeBase_ScrLoadU32FromImage(image, SCR_TIME_OFFSET_MAGIC) != SCR_TIME_MAGIC) ||
        (image[SCR_TIME_OFFSET_VERSION] != SCR_TIME_VERSION) ||
        (image[SCR_TIME_OFFSET_VALID] != SCR_TIME_VALID) ||
        (image[SCR_TIME_OFFSET_SOURCE] != (uint8)source))
    {
        return E_NOT_OK;
    }

    flags = image[SCR_TIME_OFFSET_FLAGS];
    if (((flags & SCR_TIME_FLAG_ARMED) == 0u) ||
        ((flags & SCR_TIME_FLAG_CONSUMED) != 0u))
    {
        return E_NOT_OK;
    }

    elapsedTicks = TimeBase_ScrLoadU32FromImage(image, SCR_TIME_OFFSET_ELAPSED_TICKS);
    if (elapsedTicks == 0u)
    {
        elapsedTicks = TimeBase_ScrLoadU32FromImage(image, SCR_TIME_OFFSET_RTC_LAST_TICKS) -
                TimeBase_ScrLoadU32FromImage(image, SCR_TIME_OFFSET_RTC_START_TICKS);
    }

    *elapsedNs = TimeBase_ScrRtcTicksToNs(elapsedTicks);
    *syncStatusMask = TIMEBASE_SYNC_STATUS_SCR_RTC_ELAPSED;

#if (TIMESYNC_SCR_RTC_CALIBRATION_PPM != 0)
    *syncStatusMask |= TIMEBASE_SYNC_STATUS_SCR_RTC_CALIBRATED;
#endif

    TimeBase_ScrRtcBootImage[SCR_TIME_OFFSET_FLAGS] =
            (uint8)((flags & (uint8)(~SCR_TIME_FLAG_ARMED)) | SCR_TIME_FLAG_CONSUMED);

    return E_OK;
}
#endif

static boolean TimeBase_IsPersistentSource(TimeBase_TimeSourceType source)
{
    if ((source == TIMEBASE_SOURCE_UDS_ROUTINE) ||
        (source == TIMEBASE_SOURCE_GPTP_MASTER))
    {
        return TRUE;
    }

    return FALSE;
}

static Std_ReturnType TimeBase_UpdateNvMImage(
        uint64 utcTimeNs,
        uint64 vehicleTimeNs,
        TimeBase_TimeSourceType source,
        uint32 syncStatus)
{
#if (TIMESYNC_NVM_ENABLE == STD_ON)
    if (TimeBase_IsPersistentSource(source) == FALSE)
    {
        return E_OK;
    }

    if (NvM_GetStatus() == NVM_UNINIT)
    {
        return E_NOT_OK;
    }

    TimeBase_NvMStoreCounter++;
    TimeBase_StoreU32(NvM_TimeBase_Ram, TIMEBASE_NVM_OFFSET_MAGIC, TIMEBASE_NVM_MAGIC);
    NvM_TimeBase_Ram[TIMEBASE_NVM_OFFSET_VERSION] = TIMEBASE_NVM_VERSION;
    NvM_TimeBase_Ram[TIMEBASE_NVM_OFFSET_VALID] = TIMEBASE_NVM_VALID;
    NvM_TimeBase_Ram[TIMEBASE_NVM_OFFSET_SOURCE] = (uint8)source;
    NvM_TimeBase_Ram[7u] = 0u;
    TimeBase_StoreU64(NvM_TimeBase_Ram, TIMEBASE_NVM_OFFSET_UTC_NS, utcTimeNs);
    TimeBase_StoreU64(NvM_TimeBase_Ram, TIMEBASE_NVM_OFFSET_VEHICLE_NS, vehicleTimeNs);
    TimeBase_StoreU32(NvM_TimeBase_Ram, TIMEBASE_NVM_OFFSET_SYNC_STATUS, syncStatus);
    TimeBase_StoreU32(NvM_TimeBase_Ram, TIMEBASE_NVM_OFFSET_STORE_COUNTER, TimeBase_NvMStoreCounter);

    return NvM_SetRamBlockStatus(NVM_BLOCK_ID_TIMEBASE, TRUE);
#else
    (void)utcTimeNs;
    (void)vehicleTimeNs;
    (void)source;
    (void)syncStatus;
    return E_OK;
#endif
}

void TimeBase_CaptureStandbyRtcBeforeScrReset(void)
{
#if (TIMESYNC_SCR_RTC_ENABLE == STD_ON)
    uint16 index;

    for (index = 0u; index < SCR_TIME_RECORD_LENGTH; index++)
    {
        TimeBase_ScrRtcBootImage[index] = TimeBase_ScrLoadU8(index);
    }

    TimeBase_ScrRtcBootImageValid = TRUE;
#endif
}

Std_ReturnType TimeBase_PrepareStandbyRtc(void)
{
    uint64 vehicleTimeNs;
    uint64 utcTimeNs;
    TimeBase_TimeSourceType source;
    uint32 syncStatus;
    boolean utcValid;
    Std_ReturnType result;

    if (TimeBase_Initialized == FALSE)
    {
        return E_NOT_OK;
    }

    TimeBase_EnterCritical();
    TimeBase_UpdateVehicleTimeLocked();
    vehicleTimeNs = TimeBase_VehicleTimeNs;
    utcTimeNs = TimeBase_GetMappedUtcLocked(vehicleTimeNs);
    utcValid = TimeBase_UtcMapping.utc_valid;
    source = TimeBase_UtcMapping.time_source;
    syncStatus = TimeBase_UtcMapping.sync_status;
    TimeBase_LastNvMImageVehicleNs = vehicleTimeNs;
    TimeBase_ExitCritical();

    if ((utcValid == FALSE) || (TimeBase_IsPersistentSource(source) == FALSE))
    {
        return E_NOT_OK;
    }

    result = TimeBase_UpdateNvMImage(utcTimeNs, vehicleTimeNs, source, syncStatus);
    if (result != E_OK)
    {
        return result;
    }

#if (TIMESYNC_SCR_RTC_ENABLE == STD_ON)
    {
        uint8 flags = SCR_TIME_FLAG_ARMED;
        uint32 currentRtcTicks = TimeBase_ScrLoadU32(SCR_TIME_OFFSET_RTC_LAST_TICKS);

#if (TIMESYNC_SCR_RTC_CALIBRATION_PPM != 0)
        flags |= SCR_TIME_FLAG_CALIBRATION_APPLIED;
#endif

        TimeBase_ScrStoreU32(SCR_TIME_OFFSET_MAGIC, SCR_TIME_MAGIC);
        TimeBase_ScrStoreU8(SCR_TIME_OFFSET_VERSION, SCR_TIME_VERSION);
        TimeBase_ScrStoreU8(SCR_TIME_OFFSET_VALID, SCR_TIME_VALID);
        TimeBase_ScrStoreU8(SCR_TIME_OFFSET_SOURCE, (uint8)source);
        TimeBase_ScrStoreU8(SCR_TIME_OFFSET_FLAGS, flags);
        TimeBase_ScrStoreU64(SCR_TIME_OFFSET_UTC_NS, utcTimeNs);
        TimeBase_ScrStoreU32(SCR_TIME_OFFSET_RTC_START_TICKS, currentRtcTicks);
        TimeBase_ScrStoreU32(SCR_TIME_OFFSET_RTC_LAST_TICKS, currentRtcTicks);
        TimeBase_ScrStoreU32(SCR_TIME_OFFSET_ELAPSED_TICKS, 0u);
        TimeBase_ScrStoreU32(SCR_TIME_OFFSET_SYNC_STATUS, syncStatus);
        TimeBase_ScrStoreU8(SCR_TIME_OFFSET_WAKE_REASON, 0u);
        TimeBase_ScrStoreU8(SCR_TIME_OFFSET_SCR_FAULT_STATUS, 0u);
        TimeBase_ScrStoreU8(SCR_TIME_OFFSET_SCR_NMI_STATUS, 0u);
        TimeBase_ScrStoreU8(SCR_TIME_OFFSET_SCR_RST_STATUS, 0u);
    }
#endif

    return result;
}

void TimeBase_Init(void)
{
    uint64 defaultUtcNs = 0ull;

    TimeBase_InitFrequency();
    (void)TimeBase_ConvertDateTimeToUtcNs(
            (uint16)TIMESYNC_DEFAULT_UTC_YEAR,
            (uint8)TIMESYNC_DEFAULT_UTC_MONTH,
            (uint8)TIMESYNC_DEFAULT_UTC_DAY,
            (uint8)TIMESYNC_DEFAULT_UTC_HOUR,
            (uint8)TIMESYNC_DEFAULT_UTC_MINUTE,
            (uint8)TIMESYNC_DEFAULT_UTC_SECOND,
            0u,
            0,
            &defaultUtcNs);

    TimeBase_EnterCritical();
    TimeBase_LastCounterTicks = TimeBase_PlatformGetCounterTicks();
    TimeBase_VehicleTimeNs = 0ull;
    TimeBase_UtcMapping.vehicle_time_at_utc_set_ns = 0ull;
    TimeBase_UtcMapping.utc_time_at_set_ns = defaultUtcNs;
    TimeBase_UtcMapping.utc_valid = TRUE;
    TimeBase_UtcMapping.time_source = TIMEBASE_SOURCE_DEFAULT_COMPILETIME;
    TimeBase_UtcMapping.sync_status = TimeBase_GetSyncStatusForSource(TIMEBASE_SOURCE_DEFAULT_COMPILETIME);
    TimeBase_Initialized = TRUE;
    TimeBase_LastNvMImageVehicleNs = 0ull;
    TimeBase_ExitCritical();
}

void TimeBase_MainFunction(void)
{
    uint64 vehicleTimeNs = 0ull;
    uint64 utcTimeNs = 0ull;
    TimeBase_TimeSourceType source = TIMEBASE_SOURCE_INVALID;
    uint32 syncStatus = 0u;
    boolean updateNvM = FALSE;

    if (TimeBase_Initialized == FALSE)
    {
        return;
    }

    TimeBase_EnterCritical();
    TimeBase_UpdateVehicleTimeLocked();
    if ((TimeBase_UtcMapping.utc_valid != FALSE) &&
        (TimeBase_IsPersistentSource(TimeBase_UtcMapping.time_source) != FALSE) &&
        ((TimeBase_VehicleTimeNs - TimeBase_LastNvMImageVehicleNs) >= TIMEBASE_NVM_UPDATE_PERIOD_NS))
    {
        vehicleTimeNs = TimeBase_VehicleTimeNs;
        utcTimeNs = TimeBase_GetMappedUtcLocked(vehicleTimeNs);
        source = TimeBase_UtcMapping.time_source;
        syncStatus = TimeBase_UtcMapping.sync_status;
        TimeBase_LastNvMImageVehicleNs = vehicleTimeNs;
        updateNvM = TRUE;
    }
    TimeBase_ExitCritical();

    if (updateNvM != FALSE)
    {
        (void)TimeBase_UpdateNvMImage(utcTimeNs, vehicleTimeNs, source, syncStatus);
    }
}

uint64 TimeBase_GetVehicleTimeNs(void)
{
    uint64 vehicleTimeNs;

    TimeBase_EnterCritical();
    if (TimeBase_Initialized != FALSE)
    {
        TimeBase_UpdateVehicleTimeLocked();
    }
    vehicleTimeNs = TimeBase_VehicleTimeNs;
    TimeBase_ExitCritical();

    return vehicleTimeNs;
}

uint64 TimeBase_GetVehicleTimeUs(void)
{
    return TimeBase_GetVehicleTimeNs() / TIMEBASE_NS_PER_US;
}

uint32 TimeBase_GetVehicleTimeS(void)
{
    return (uint32)(TimeBase_GetVehicleTimeNs() / TIMEBASE_NS_PER_S);
}

boolean TimeBase_IsUtcValid(void)
{
    boolean valid;

    TimeBase_EnterCritical();
    valid = TimeBase_UtcMapping.utc_valid;
    TimeBase_ExitCritical();

    return valid;
}

uint64 TimeBase_GetUtcTimeNs(boolean *valid)
{
    uint64 vehicleTimeNs;
    uint64 utcTimeNs;
    boolean utcValid;

    TimeBase_EnterCritical();
    if (TimeBase_Initialized != FALSE)
    {
        TimeBase_UpdateVehicleTimeLocked();
    }
    vehicleTimeNs = TimeBase_VehicleTimeNs;
    utcValid = TimeBase_UtcMapping.utc_valid;
    utcTimeNs = TimeBase_GetMappedUtcLocked(vehicleTimeNs);
    TimeBase_ExitCritical();

    if (valid != NULL_PTR)
    {
        *valid = utcValid;
    }

    return utcTimeNs;
}

void TimeBase_SetUtcTimeNs(uint64 utcNowNs)
{
    (void)TimeBase_SetUtcTimeNsFromSource(utcNowNs, TIMEBASE_SOURCE_UDS_ROUTINE);
}

Std_ReturnType TimeBase_SetUtcTimeNsFromSource(uint64 utcNowNs, TimeBase_TimeSourceType source)
{
    uint64 vehicleTimeNs;
    uint32 syncStatus;

    if ((source != TIMEBASE_SOURCE_DEFAULT_COMPILETIME) &&
        (source != TIMEBASE_SOURCE_UDS_ROUTINE) &&
        (source != TIMEBASE_SOURCE_GPTP_MASTER))
    {
        return E_NOT_OK;
    }

    TimeBase_EnterCritical();
    if (TimeBase_Initialized != FALSE)
    {
        TimeBase_UpdateVehicleTimeLocked();
    }
    TimeBase_UtcMapping.vehicle_time_at_utc_set_ns = TimeBase_VehicleTimeNs;
    TimeBase_UtcMapping.utc_time_at_set_ns = utcNowNs;
    TimeBase_UtcMapping.utc_valid = TRUE;
    TimeBase_UtcMapping.time_source = source;
    TimeBase_UtcMapping.sync_status = TimeBase_GetSyncStatusForSource(source);
    vehicleTimeNs = TimeBase_VehicleTimeNs;
    syncStatus = TimeBase_UtcMapping.sync_status;
    TimeBase_LastNvMImageVehicleNs = vehicleTimeNs;
    TimeBase_ExitCritical();

#if (TIMESYNC_NVM_STORE_ON_SET == STD_ON)
    (void)TimeBase_UpdateNvMImage(utcNowNs, vehicleTimeNs, source, syncStatus);
#endif

    return E_OK;
}

void TimeBase_SetUtcTimeFromDateTime(
        uint16 year,
        uint8 month,
        uint8 day,
        uint8 hour,
        uint8 minute,
        uint8 second,
        uint16 millisecond,
        sint16 timezone_offset_min)
{
    uint64 utcTimeNs;

    if (TimeBase_ConvertDateTimeToUtcNs(
            year,
            month,
            day,
            hour,
            minute,
            second,
            millisecond,
            timezone_offset_min,
            &utcTimeNs) == E_OK)
    {
        TimeBase_SetUtcTimeNs(utcTimeNs);
    }
}

Std_ReturnType TimeBase_LoadUtcFromNvM(void)
{
#if ((TIMESYNC_NVM_ENABLE == STD_ON) && (TIMESYNC_NVM_RESTORE_ENABLE == STD_ON))
    NvM_RequestResultType nvmResult;
    uint64 utcTimeNs;
    uint64 vehicleTimeNs;
    TimeBase_TimeSourceType source;
    uint32 syncStatus;
#if (TIMESYNC_SCR_RTC_ENABLE == STD_ON)
    uint64 standbyElapsedNs = 0ull;
    uint32 standbySyncStatus = 0u;
    uint64 adjustedUtcTimeNs;
#endif

    if (NvM_GetErrorStatus(NVM_BLOCK_ID_TIMEBASE, &nvmResult) != E_OK)
    {
        return E_NOT_OK;
    }

    if (nvmResult != NVM_REQ_OK)
    {
        return E_NOT_OK;
    }

    if ((TimeBase_LoadU32(NvM_TimeBase_Ram, TIMEBASE_NVM_OFFSET_MAGIC) != TIMEBASE_NVM_MAGIC) ||
        (NvM_TimeBase_Ram[TIMEBASE_NVM_OFFSET_VERSION] != TIMEBASE_NVM_VERSION) ||
        (NvM_TimeBase_Ram[TIMEBASE_NVM_OFFSET_VALID] != TIMEBASE_NVM_VALID))
    {
        return E_NOT_OK;
    }

    source = (TimeBase_TimeSourceType)NvM_TimeBase_Ram[TIMEBASE_NVM_OFFSET_SOURCE];
    if (TimeBase_IsPersistentSource(source) == FALSE)
    {
        return E_NOT_OK;
    }

    utcTimeNs = TimeBase_LoadU64(NvM_TimeBase_Ram, TIMEBASE_NVM_OFFSET_UTC_NS);
    if (TimeBase_ValidateUtcTimeNs(utcTimeNs) != E_OK)
    {
        return E_NOT_OK;
    }

#if (TIMESYNC_SCR_RTC_ENABLE == STD_ON)
    if (TimeBase_TryGetStandbyRtcElapsedNs(source, &standbyElapsedNs, &standbySyncStatus) == E_OK)
    {
        adjustedUtcTimeNs = utcTimeNs + standbyElapsedNs;
        if ((adjustedUtcTimeNs >= utcTimeNs) &&
            (TimeBase_ValidateUtcTimeNs(adjustedUtcTimeNs) == E_OK))
        {
            utcTimeNs = adjustedUtcTimeNs;
        }
        else
        {
            standbySyncStatus = 0u;
        }
    }
#endif

    syncStatus = TimeBase_GetSyncStatusForSource(source) | TIMEBASE_SYNC_STATUS_NVM_RESTORED;
#if (TIMESYNC_SCR_RTC_ENABLE == STD_ON)
    syncStatus |= standbySyncStatus;
#endif

    TimeBase_EnterCritical();
    if (TimeBase_Initialized != FALSE)
    {
        TimeBase_UpdateVehicleTimeLocked();
    }
    vehicleTimeNs = TimeBase_VehicleTimeNs;
    TimeBase_UtcMapping.vehicle_time_at_utc_set_ns = vehicleTimeNs;
    TimeBase_UtcMapping.utc_time_at_set_ns = utcTimeNs;
    TimeBase_UtcMapping.utc_valid = TRUE;
    TimeBase_UtcMapping.time_source = source;
    TimeBase_UtcMapping.sync_status = syncStatus;
    TimeBase_LastNvMImageVehicleNs = vehicleTimeNs;
    TimeBase_NvMStoreCounter = TimeBase_LoadU32(NvM_TimeBase_Ram, TIMEBASE_NVM_OFFSET_STORE_COUNTER);
    TimeBase_ExitCritical();

    return E_OK;
#else
    return E_NOT_OK;
#endif
}

void TimeBase_GetTimestampSnapshot(TimeBase_TimestampSnapshotType *snapshot)
{
    uint64 vehicleTimeNs;

    if (snapshot == NULL_PTR)
    {
        return;
    }

    TimeBase_EnterCritical();
    if (TimeBase_Initialized != FALSE)
    {
        TimeBase_UpdateVehicleTimeLocked();
    }
    vehicleTimeNs = TimeBase_VehicleTimeNs;
    snapshot->vehicle_time_ns = vehicleTimeNs;
    snapshot->utc_time_ns = TimeBase_GetMappedUtcLocked(vehicleTimeNs);
    snapshot->utc_valid = TimeBase_UtcMapping.utc_valid;
    snapshot->time_source = (uint8)TimeBase_UtcMapping.time_source;
    snapshot->sync_status = TimeBase_UtcMapping.sync_status;
    TimeBase_ExitCritical();
}

void TimeBase_GetDtcTimestamp(TimeBase_DtcTimestampType *timestamp)
{
    TimeBase_TimestampSnapshotType snapshot;

    if (timestamp == NULL_PTR)
    {
        return;
    }

    TimeBase_GetTimestampSnapshot(&snapshot);

    /*
     * DTC/log snapshots should always persist vehicle_time_ns. Persist utc_time_ns
     * only when utc_valid is TRUE, and store time_source so the PC can label
     * default compile-time, diagnostic routine, or gPTP-originated time. Use
     * TimeBase_GetTimestampSnapshot() when sync_status is also stored.
     */
    timestamp->vehicle_time_ns = snapshot.vehicle_time_ns;
    timestamp->utc_time_ns = snapshot.utc_valid != FALSE ? snapshot.utc_time_ns : 0ull;
    timestamp->utc_valid = snapshot.utc_valid;
    timestamp->time_source = snapshot.time_source;
}
