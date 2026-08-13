#ifndef GTMTOM_H
#define GTMTOM_H

#include "Ifx_Types.h"
#include "Gtm/Std/IfxGtm_Tom.h"
#include "_PinMap/IfxGtm_PinMap.h"

#define GTMTOM_CLOCK_HZ_DEFAULT             200000000u
#define GTMTOM_PERIOD_TICKS_MIN            2u
#define GTMTOM_PERIOD_TICKS_MAX            65535u
#define GTMTOM_DUTY_PERMILLE_MAX           1000u
#define GTMTOM_DUTY_PPM_MAX                1000000u

#define GTMTOM_PWM_TOM                     IfxGtm_Tom_0
#define GTMTOM_PWM_CHANNEL                 IfxGtm_Tom_Ch_4
#define GTMTOM_PWM_CLOCK_SOURCE            IfxGtm_Tom_Ch_ClkSrc_cmuFxclk0
#define GTMTOM_PWM_OUTPUT_PIN              IfxGtm_TOM0_4_TOUT9_P00_0_OUT

typedef enum
{
    GTMTOM_CONTROL_MODE_RAW_TICKS = 0u,
    GTMTOM_CONTROL_MODE_FREQ_DUTY_PERMILLE = 1u,
    GTMTOM_CONTROL_MODE_FREQ_DUTY_PPM = 2u
} GtmTom_ControlModeType;

typedef enum
{
    GTMTOM_BURST_IDLE = 0u,
    GTMTOM_BURST_ARMED = 1u,
    GTMTOM_BURST_ACTIVE = 2u,
    GTMTOM_BURST_DONE = 3u
} GtmTom_BurstStateType;

void GtmTom_Init(void);
void GtmTom_MainFunction(void);

extern volatile uint8 GtmTom_Enable;
extern volatile uint8 GtmTom_ControlMode;
extern volatile uint32 GtmTom_PeriodTicks;
extern volatile uint32 GtmTom_DutyTicks;
extern volatile uint32 GtmTom_FrequencyHz;
extern volatile uint32 GtmTom_DutyPermille;
extern volatile uint32 GtmTom_DutyPpm;
extern volatile uint8 GtmTom_InvertOutput;
extern volatile uint8 GtmTom_IdleLevelHigh;
extern volatile uint8 GtmTom_ApplyNow;
extern volatile uint8 GtmTom_SynchronousUpdate;

extern volatile uint8 GtmTom_SweepEnable;
extern volatile uint32 GtmTom_SweepFrequencyHzMin;
extern volatile uint32 GtmTom_SweepFrequencyHzMax;
extern volatile uint32 GtmTom_SweepFrequencyHzStep;
extern volatile uint8 GtmTom_SweepDirectionUp;

extern volatile uint8 GtmTom_DutySweepEnable;
extern volatile uint32 GtmTom_DutySweepPermilleMin;
extern volatile uint32 GtmTom_DutySweepPermilleMax;
extern volatile uint32 GtmTom_DutySweepPermilleStep;
extern volatile uint8 GtmTom_DutySweepDirectionUp;

extern volatile uint8 GtmTom_BurstArm;
extern volatile uint32 GtmTom_BurstCycles;
extern volatile uint8 GtmTom_SingleShotArm;
extern volatile uint32 GtmTom_SingleShotPulseTicks;

extern volatile uint32 GtmTom_AppliedPeriodTicks;
extern volatile uint32 GtmTom_AppliedDutyTicks;
extern volatile uint32 GtmTom_AppliedFrequencyHz;
extern volatile uint32 GtmTom_MainFunctionCounter;
extern volatile uint32 GtmTom_ApplyCounter;
extern volatile uint32 GtmTom_ErrorCounter;
extern volatile uint32 GtmTom_LastError;
extern volatile uint8 GtmTom_Initialized;
extern volatile uint8 GtmTom_OutputActive;
extern volatile uint8 GtmTom_BurstState;
extern volatile uint32 GtmTom_BurstRemainingTaskTicks;

#endif /* GTMTOM_H */
