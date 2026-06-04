#ifndef SCR_TIME_SHARED_H
#define SCR_TIME_SHARED_H

/*
 * Retained SCR XRAM handoff record shared by TriCore TimeBase and SCR.
 * Existing debug bytes use 0x1760..0x1769; keep this record directly after
 * them and away from the SCR image copied at PMS_XRAM base.
 */
#define SCR_TIME_XRAM_BASE                  0x1790u
#define SCR_TIME_MAGIC                      0x54534352u /* "TSCR" */
#define SCR_TIME_VERSION                    3u

#define SCR_TIME_OFFSET_MAGIC               0u
#define SCR_TIME_OFFSET_VERSION             4u
#define SCR_TIME_OFFSET_VALID               5u
#define SCR_TIME_OFFSET_SOURCE              6u
#define SCR_TIME_OFFSET_FLAGS               7u
#define SCR_TIME_OFFSET_UTC_NS              8u
#define SCR_TIME_OFFSET_RTC_START_TICKS     16u
#define SCR_TIME_OFFSET_RTC_LAST_TICKS      20u
#define SCR_TIME_OFFSET_ELAPSED_TICKS       24u
#define SCR_TIME_OFFSET_SYNC_STATUS         28u
#define SCR_TIME_OFFSET_WAKE_REASON         32u
#define SCR_TIME_OFFSET_SCR_FAULT_STATUS    33u
#define SCR_TIME_OFFSET_SCR_NMI_STATUS      34u
#define SCR_TIME_OFFSET_SCR_RST_STATUS      35u
#define SCR_TIME_OFFSET_VEHICLE_NS          36u
#define SCR_TIME_OFFSET_DEBUG_RTC_CON       44u
#define SCR_TIME_OFFSET_DEBUG_PMCON1        45u
#define SCR_TIME_OFFSET_DEBUG_RTC_CNT0      46u
#define SCR_TIME_OFFSET_DEBUG_RTC_CNT1      47u
#define SCR_TIME_OFFSET_DEBUG_RTC_CNT2      48u
#define SCR_TIME_OFFSET_DEBUG_RTC_CNT3      49u
#define SCR_TIME_OFFSET_DEBUG_TIME_ACTIVE   50u
#define SCR_TIME_OFFSET_DEBUG_FLAGS_READ    51u
#define SCR_TIME_OFFSET_DEBUG_MAGIC0_READ   52u
#define SCR_TIME_OFFSET_DEBUG_MAGIC1_READ   53u
#define SCR_TIME_OFFSET_DEBUG_MAGIC2_READ   54u
#define SCR_TIME_OFFSET_DEBUG_MAGIC3_READ   55u
#define SCR_TIME_OFFSET_DEBUG_VERSION_READ  56u
#define SCR_TIME_OFFSET_DEBUG_VALID_READ    57u
#define SCR_TIME_OFFSET_DEBUG_INIT_STATUS   58u
#define SCR_TIME_OFFSET_ELAPSED_TICKS_HIGH  59u
#define SCR_TIME_OFFSET_RTC_LAST_TICKS_HIGH 63u
#define SCR_TIME_RECORD_LENGTH              67u

#define BOOT_STAGE_RTC_INIT_ENTER      (0x60u)
#define BOOT_STAGE_RTC_PMCON_DONE      (0x61u)
#define BOOT_STAGE_RTC_STOP_DONE       (0x62u)
#define BOOT_STAGE_RTC_CON_DONE        (0x63u)
#define BOOT_STAGE_RTC_CNT_DONE        (0x64u)
#define BOOT_STAGE_RTC_START_DONE      (0x65u)
#define BOOT_STAGE_RTC_INIT_EXIT       (0x66u)

#define SCR_TIME_VALID                      1u
#define SCR_TIME_FLAG_ARMED                 0x01u
#define SCR_TIME_FLAG_CONSUMED              0x02u
#define SCR_TIME_FLAG_CALIBRATION_APPLIED   0x04u

#define SCR_TIME_WAKE_REASON_WCAN_WUF       0x01u
#define SCR_TIME_WAKE_REASON_WCAN_SYNC      0x02u
#define SCR_TIME_WAKE_REASON_WCAN_FDF       0x10u
#define SCR_TIME_WAKE_REASON_ECC_DBE        0x20u
#define SCR_TIME_WAKE_REASON_WDT            0x40u

#define SCR_TIME_FAULT_STATUS_ECC_DBE       0x01u
#define SCR_TIME_FAULT_STATUS_WDT           0x02u


#define SCR_TIME_RTC_USE_PCLK_20MHZ         1u
#define SCR_TIME_RTC_TICK_HZ_NUM            20000000u
#define SCR_TIME_RTC_TICK_HZ_DEN            1u
#define SCR_TIME_RTC_TICK_NS                50u
#define SCR_TIME_USE_RTC_INTERRUPT          1u
#define SCR_TIME_RTC_COMPARE_TICKS          2000000u
#define SCR_TIME_RTC_COMPARE_NS             (SCR_TIME_RTC_COMPARE_TICKS * SCR_TIME_RTC_TICK_NS)
#define SCR_TIME_USE_WDT_FALLBACK           0u
#define SCR_TIME_WDT_TICK_NS                6400u

#endif /* SCR_TIME_SHARED_H */
