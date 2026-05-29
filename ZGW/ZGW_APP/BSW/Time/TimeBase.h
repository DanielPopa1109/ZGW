#ifndef TIMEBASE_H
#define TIMEBASE_H

#include "Std_Types.h"

#define TIMEBASE_NS_PER_US                     1000ull
#define TIMEBASE_NS_PER_MS                     1000000ull
#define TIMEBASE_NS_PER_S                      1000000000ull

#define TIMEBASE_SYNC_STATUS_DEFAULT_UTC       (1u << 0u)
#define TIMEBASE_SYNC_STATUS_UDS_SET           (1u << 1u)
#define TIMEBASE_SYNC_STATUS_GPTP_ACTIVE       (1u << 2u)
#define TIMEBASE_SYNC_STATUS_GPTP_MASTER       (1u << 3u)
#define TIMEBASE_SYNC_STATUS_NVM_RESTORED      (1u << 4u)
#define TIMEBASE_SYNC_STATUS_SCR_RTC_ELAPSED   (1u << 5u)
#define TIMEBASE_SYNC_STATUS_SCR_RTC_CALIBRATED (1u << 6u)

typedef enum
{
    TIMEBASE_SOURCE_DEFAULT_COMPILETIME = 0u,
    TIMEBASE_SOURCE_UDS_ROUTINE         = 1u,
    TIMEBASE_SOURCE_GPTP_MASTER         = 2u,
    TIMEBASE_SOURCE_INVALID             = 255u
} TimeBase_TimeSourceType;

typedef struct
{
    uint64 vehicle_time_ns;
    uint64 utc_time_ns;
    boolean utc_valid;
    uint8 time_source;
    uint32 sync_status;
} TimeBase_TimestampSnapshotType;

typedef struct
{
    uint64 vehicle_time_ns;
    uint64 utc_time_ns;
    boolean utc_valid;
    uint8 time_source;
} TimeBase_DtcTimestampType;

typedef struct
{
    uint16 year;
    uint8 month;
    uint8 day;
    uint8 hour;
    uint8 minute;
    uint8 second;
    uint16 millisecond;
} TimeBase_DateTimeType;

void TimeBase_Init(void);
void TimeBase_MainFunction(void);

uint64 TimeBase_GetVehicleTimeNs(void);
uint64 TimeBase_GetVehicleTimeUs(void);
uint32 TimeBase_GetVehicleTimeS(void);

boolean TimeBase_IsUtcValid(void);
uint64 TimeBase_GetUtcTimeNs(boolean *valid);
void TimeBase_SetUtcTimeNs(uint64 utcNowNs);
Std_ReturnType TimeBase_SetUtcTimeNsFromSource(uint64 utcNowNs, TimeBase_TimeSourceType source);

void TimeBase_SetUtcTimeFromDateTime(
        uint16 year,
        uint8 month,
        uint8 day,
        uint8 hour,
        uint8 minute,
        uint8 second,
        uint16 millisecond,
        sint16 timezone_offset_min);

Std_ReturnType TimeBase_ConvertDateTimeToUtcNs(
        uint16 year,
        uint8 month,
        uint8 day,
        uint8 hour,
        uint8 minute,
        uint8 second,
        uint16 millisecond,
        sint16 timezone_offset_min,
        uint64 *utcTimeNs);

Std_ReturnType TimeBase_ConvertUtcNsToDateTime(uint64 utcTimeNs, TimeBase_DateTimeType *dateTime);
Std_ReturnType TimeBase_ValidateUtcTimeNs(uint64 utcTimeNs);
void TimeBase_CaptureStandbyRtcBeforeScrReset(void);
Std_ReturnType TimeBase_RestoreStandbyRtcCapture(const uint8 *image, uint16 length);
Std_ReturnType TimeBase_PrepareStandbyRtc(void);
Std_ReturnType TimeBase_RearmStandbyRtc(void);
Std_ReturnType TimeBase_LoadUtcFromNvM(void);

void TimeBase_GetTimestampSnapshot(TimeBase_TimestampSnapshotType *snapshot);
void TimeBase_GetDtcTimestamp(TimeBase_DtcTimestampType *timestamp);

uint64 TimeBase_PlatformGetCounterNs(void);

#endif /* TIMEBASE_H */
