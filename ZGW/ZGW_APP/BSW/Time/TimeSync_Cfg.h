#ifndef TIMESYNC_CFG_H
#define TIMESYNC_CFG_H

#include "Std_Types.h"

/* Fallback only. The primary TimeBase boot default is the NvM_TimeBase_Rom image in PFLASH. */
#define TIMESYNC_DEFAULT_UTC_YEAR              2026u
#define TIMESYNC_DEFAULT_UTC_MONTH             5u
#define TIMESYNC_DEFAULT_UTC_DAY               21u
#define TIMESYNC_DEFAULT_UTC_HOUR              6u
#define TIMESYNC_DEFAULT_UTC_MINUTE            10u
#define TIMESYNC_DEFAULT_UTC_SECOND            42u

#define TIMESYNC_UTC_PLAUSIBLE_MIN_YEAR        2020u
#define TIMESYNC_UTC_PLAUSIBLE_MAX_YEAR        2100u

#define TIMESYNC_UDP_ENABLE                    STD_ON
#define TIMESYNC_UDP_PORT                      35000u
#define TIMESYNC_UDP_PERIOD_MS                 100u
#define TIMESYNC_UDP_BROADCAST_ADDR            0xFFFFFFFFu

#define TIMESYNC_DCM_ROUTINE_ENABLE            STD_ON
#define TIMESYNC_ROUTINE_SET_UTC_ID            0xF190u
#define TIMESYNC_ROUTINE_GET_STATUS_ID         0xF191u

#define TIMESYNC_NVM_ENABLE                    STD_ON
#define TIMESYNC_NVM_RESTORE_ENABLE            STD_ON
#define TIMESYNC_NVM_STORE_ON_SET              STD_ON
#define TIMESYNC_NVM_RAM_UPDATE_PERIOD_MS      1000u

#define TIMESYNC_SCR_RTC_ENABLE                STD_ON
#define TIMESYNC_SCR_RTC_TICK_HZ_NUM           20000000u
#define TIMESYNC_SCR_RTC_TICK_HZ_DEN           1u
#define TIMESYNC_SCR_RTC_CALIBRATION_PPM       0
#define TIMESYNC_SCR_RTC_USE_CCU6_CALIBRATION  STD_OFF
#define TIMESYNC_SCR_RTC_LOW_POWER_70KHZ       STD_OFF

/* First-stage lab gPTP. Disabled by default because raw Ethernet TX/RX is not exposed. */
#define TIMESYNC_GPTP_ENABLE                   STD_OFF
#define TIMESYNC_GPTP_FORCE_MASTER             STD_ON
#define TIMESYNC_GPTP_SYNC_PERIOD_MS           125u
#define TIMESYNC_GPTP_ANNOUNCE_PERIOD_MS       1000u
#define TIMESYNC_USE_GETH_HW_TIMESTAMP         STD_OFF

#endif /* TIMESYNC_CFG_H */
