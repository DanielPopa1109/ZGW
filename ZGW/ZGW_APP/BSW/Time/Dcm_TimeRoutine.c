#include "Dcm_TimeRoutine.h"
#include "Gptp_Lab.h"
#include "TimeBase.h"
#include "TimeSync_Cfg.h"

#define DCM_TIMEROUTINE_START_ROUTINE          0x01u
#define DCM_TIMEROUTINE_REQUEST_RESULTS        0x03u
#define DCM_TIMEROUTINE_UTC_NS_LENGTH          8u
#define DCM_TIMEROUTINE_DATETIME_LENGTH        12u
#define DCM_TIMEROUTINE_STATUS_LENGTH          40u

static uint16 Dcm_TimeRoutine_GetU16(const uint8 *data);
static uint64 Dcm_TimeRoutine_GetU64(const uint8 *data);
static void Dcm_TimeRoutine_PutU16(uint8 *data, uint16 value);
static void Dcm_TimeRoutine_PutU32(uint8 *data, uint32 value);
static void Dcm_TimeRoutine_PutU64(uint8 *data, uint64 value);
static Dcm_ReturnType Dcm_TimeRoutine_BuildStatusResponse(uint8 *respData, Dcm_PduLengthType *respLen);
static Dcm_ReturnType Dcm_TimeRoutine_SetUtc(const uint8 *reqData, Dcm_PduLengthType reqLen, uint8 *respData, Dcm_PduLengthType *respLen);

static uint16 Dcm_TimeRoutine_GetU16(const uint8 *data)
{
    return (uint16)(((uint16)data[0u] << 8u) | (uint16)data[1u]);
}

static uint64 Dcm_TimeRoutine_GetU64(const uint8 *data)
{
    return (((uint64)data[0u]) << 56u) |
           (((uint64)data[1u]) << 48u) |
           (((uint64)data[2u]) << 40u) |
           (((uint64)data[3u]) << 32u) |
           (((uint64)data[4u]) << 24u) |
           (((uint64)data[5u]) << 16u) |
           (((uint64)data[6u]) << 8u) |
           ((uint64)data[7u]);
}

static void Dcm_TimeRoutine_PutU16(uint8 *data, uint16 value)
{
    data[0u] = (uint8)(value >> 8u);
    data[1u] = (uint8)value;
}

static void Dcm_TimeRoutine_PutU32(uint8 *data, uint32 value)
{
    data[0u] = (uint8)(value >> 24u);
    data[1u] = (uint8)(value >> 16u);
    data[2u] = (uint8)(value >> 8u);
    data[3u] = (uint8)value;
}

static void Dcm_TimeRoutine_PutU64(uint8 *data, uint64 value)
{
    data[0u] = (uint8)(value >> 56u);
    data[1u] = (uint8)(value >> 48u);
    data[2u] = (uint8)(value >> 40u);
    data[3u] = (uint8)(value >> 32u);
    data[4u] = (uint8)(value >> 24u);
    data[5u] = (uint8)(value >> 16u);
    data[6u] = (uint8)(value >> 8u);
    data[7u] = (uint8)value;
}

boolean Dcm_TimeRoutine_IsRoutineId(uint16 routineId)
{
#if (TIMESYNC_DCM_ROUTINE_ENABLE == STD_ON)
    if ((routineId == TIMESYNC_ROUTINE_SET_UTC_ID) ||
        (routineId == TIMESYNC_ROUTINE_GET_STATUS_ID))
    {
        return TRUE;
    }
#else
    (void)routineId;
#endif

    return FALSE;
}

static Dcm_ReturnType Dcm_TimeRoutine_BuildStatusResponse(uint8 *respData, Dcm_PduLengthType *respLen)
{
    TimeBase_TimestampSnapshotType snapshot;
    Gptp_Lab_StatusType gptpStatus;

    if ((respData == NULL_PTR) || (respLen == NULL_PTR))
    {
        return DCM_NRC_GENERAL_REJECT;
    }

    TimeBase_GetTimestampSnapshot(&snapshot);
    Gptp_Lab_GetStatus(&gptpStatus);

    Dcm_TimeRoutine_PutU64(&respData[0u], snapshot.vehicle_time_ns);
    Dcm_TimeRoutine_PutU64(&respData[8u], snapshot.utc_time_ns);
    respData[16u] = snapshot.utc_valid;
    respData[17u] = snapshot.time_source;
    Dcm_TimeRoutine_PutU32(&respData[18u], snapshot.sync_status);
    respData[22u] = gptpStatus.state;
    respData[23u] = gptpStatus.enabled;
    respData[24u] = gptpStatus.forceMaster;
    respData[25u] = 0u;
    Dcm_TimeRoutine_PutU16(&respData[26u], gptpStatus.txSequenceId);
    Dcm_TimeRoutine_PutU32(&respData[28u], gptpStatus.syncTxCounter);
    Dcm_TimeRoutine_PutU32(&respData[32u], gptpStatus.announceTxCounter);
    Dcm_TimeRoutine_PutU32(&respData[36u], gptpStatus.rxAnnounceCounter);
    *respLen = DCM_TIMEROUTINE_STATUS_LENGTH;

    return DCM_E_OK;
}

static Dcm_ReturnType Dcm_TimeRoutine_SetUtc(const uint8 *reqData, Dcm_PduLengthType reqLen, uint8 *respData, Dcm_PduLengthType *respLen)
{
    uint64 utcTimeNs;
    uint16 year;
    uint8 month;
    uint8 day;
    uint8 hour;
    uint8 minute;
    uint8 second;
    uint16 millisecond;
    sint16 timezoneOffsetMin;

    if ((reqData == NULL_PTR) || (respData == NULL_PTR) || (respLen == NULL_PTR))
    {
        return DCM_NRC_GENERAL_REJECT;
    }

    if (reqLen == DCM_TIMEROUTINE_UTC_NS_LENGTH)
    {
        utcTimeNs = Dcm_TimeRoutine_GetU64(reqData);
    }
    else if (reqLen == DCM_TIMEROUTINE_DATETIME_LENGTH)
    {
        year = Dcm_TimeRoutine_GetU16(&reqData[0u]);
        month = reqData[2u];
        day = reqData[3u];
        hour = reqData[4u];
        minute = reqData[5u];
        second = reqData[6u];
        millisecond = Dcm_TimeRoutine_GetU16(&reqData[7u]);
        timezoneOffsetMin = (sint16)Dcm_TimeRoutine_GetU16(&reqData[9u]);
        (void)reqData[11u];

        if (TimeBase_ConvertDateTimeToUtcNs(
                year,
                month,
                day,
                hour,
                minute,
                second,
                millisecond,
                timezoneOffsetMin,
                &utcTimeNs) != E_OK)
        {
            return DCM_NRC_REQUEST_OUT_OF_RANGE;
        }
    }
    else
    {
        return DCM_NRC_INCORRECT_LENGTH;
    }

    if (TimeBase_ValidateUtcTimeNs(utcTimeNs) != E_OK)
    {
        return DCM_NRC_REQUEST_OUT_OF_RANGE;
    }

    TimeBase_SetUtcTimeNs(utcTimeNs);

    return Dcm_TimeRoutine_BuildStatusResponse(respData, respLen);
}

Dcm_ReturnType Dcm_TimeRoutine_HandleRoutineControl(
        Dcm_OpStatusType opStatus,
        uint8 routineControlType,
        uint16 routineId,
        const uint8 *reqData,
        Dcm_PduLengthType reqLen,
        uint8 *respData,
        Dcm_PduLengthType *respLen)
{
#if (TIMESYNC_DCM_ROUTINE_ENABLE == STD_ON)
    if ((respData == NULL_PTR) || (respLen == NULL_PTR))
    {
        return DCM_NRC_GENERAL_REJECT;
    }

    if (opStatus == DCM_CANCEL)
    {
        *respLen = 0u;
        return DCM_E_OK;
    }

    if (routineId == TIMESYNC_ROUTINE_SET_UTC_ID)
    {
        if (routineControlType != DCM_TIMEROUTINE_START_ROUTINE)
        {
            return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
        }

        return Dcm_TimeRoutine_SetUtc(reqData, reqLen, respData, respLen);
    }

    if (routineId == TIMESYNC_ROUTINE_GET_STATUS_ID)
    {
        if ((routineControlType != DCM_TIMEROUTINE_START_ROUTINE) &&
            (routineControlType != DCM_TIMEROUTINE_REQUEST_RESULTS))
        {
            return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
        }

        if (reqLen != 0u)
        {
            return DCM_NRC_INCORRECT_LENGTH;
        }

        return Dcm_TimeRoutine_BuildStatusResponse(respData, respLen);
    }
#else
    (void)opStatus;
    (void)routineControlType;
    (void)routineId;
    (void)reqData;
    (void)reqLen;
    (void)respData;
    (void)respLen;
#endif

    return DCM_NRC_REQUEST_OUT_OF_RANGE;
}
