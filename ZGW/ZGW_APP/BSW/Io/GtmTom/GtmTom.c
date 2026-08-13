#include "GtmTom.h"

#include "Gtm/Std/IfxGtm.h"
#include "Gtm/Std/IfxGtm_Cmu.h"
#include "_PinMap/IfxGtm_PinMap.h"

#define GTMTOM_ERROR_NONE                  0u
#define GTMTOM_ERROR_NOT_INITIALIZED       1u
#define GTMTOM_ERROR_BAD_FREQUENCY         3u
#define GTMTOM_TASK_PERIOD_MS              5u

typedef struct
{
    uint16 periodTicks;
    uint16 dutyTicks;
    uint32 frequencyHz;
} GtmTom_PwmValuesType;

static Ifx_GTM *GtmTom_Gtm;
static Ifx_GTM_TOM *GtmTom_Tom;
static Ifx_GTM_TOM_TGC *GtmTom_Tgc;
static uint8 GtmTom_LastEnable;
static uint8 GtmTom_LastInvertOutput;
static uint8 GtmTom_LastIdleLevelHigh;
static uint8 GtmTom_LastControlMode;
static uint32 GtmTom_LastPeriodTicks;
static uint32 GtmTom_LastDutyTicks;
static uint32 GtmTom_LastFrequencyHz;
static uint32 GtmTom_LastDutyPermille;
static uint32 GtmTom_LastDutyPpm;

volatile uint8 GtmTom_Enable = 0u;
volatile uint8 GtmTom_ControlMode = GTMTOM_CONTROL_MODE_RAW_TICKS;
volatile uint32 GtmTom_PeriodTicks = 2u;
volatile uint32 GtmTom_DutyTicks = 1u;
volatile uint32 GtmTom_FrequencyHz = 100000000u;
volatile uint32 GtmTom_DutyPermille = 500u;
volatile uint32 GtmTom_DutyPpm = 500000u;
volatile uint8 GtmTom_InvertOutput = 0u;
volatile uint8 GtmTom_IdleLevelHigh = 0u;
volatile uint8 GtmTom_ApplyNow = 0u;
volatile uint8 GtmTom_SynchronousUpdate = 1u;

volatile uint8 GtmTom_SweepEnable = 0u;
volatile uint32 GtmTom_SweepFrequencyHzMin = 1000000u;
volatile uint32 GtmTom_SweepFrequencyHzMax = 100000000u;
volatile uint32 GtmTom_SweepFrequencyHzStep = 1000000u;
volatile uint8 GtmTom_SweepDirectionUp = 1u;

volatile uint8 GtmTom_DutySweepEnable = 0u;
volatile uint32 GtmTom_DutySweepPermilleMin = 100u;
volatile uint32 GtmTom_DutySweepPermilleMax = 900u;
volatile uint32 GtmTom_DutySweepPermilleStep = 100u;
volatile uint8 GtmTom_DutySweepDirectionUp = 1u;

volatile uint8 GtmTom_BurstArm = 0u;
volatile uint32 GtmTom_BurstCycles = 0u;
volatile uint8 GtmTom_SingleShotArm = 0u;
volatile uint32 GtmTom_SingleShotPulseTicks = 1u;

volatile uint32 GtmTom_AppliedPeriodTicks = 0u;
volatile uint32 GtmTom_AppliedDutyTicks = 0u;
volatile uint32 GtmTom_AppliedFrequencyHz = 0u;
volatile uint32 GtmTom_MainFunctionCounter = 0u;
volatile uint32 GtmTom_ApplyCounter = 0u;
volatile uint32 GtmTom_ErrorCounter = 0u;
volatile uint32 GtmTom_LastError = GTMTOM_ERROR_NONE;
volatile uint8 GtmTom_Initialized = 0u;
volatile uint8 GtmTom_OutputActive = 0u;
volatile uint8 GtmTom_BurstState = GTMTOM_BURST_IDLE;
volatile uint32 GtmTom_BurstRemainingTaskTicks = 0u;

static uint16 GtmTom_ClampU16(uint32 Value, uint32 Min, uint32 Max)
{
    if (Value < Min)
    {
        Value = Min;
    }
    else if (Value > Max)
    {
        Value = Max;
    }
    else
    {
        /* Value is already in range. */
    }

    return (uint16)Value;
}

static void GtmTom_SetError(uint32 Error)
{
    GtmTom_LastError = Error;
    if (Error != GTMTOM_ERROR_NONE)
    {
        GtmTom_ErrorCounter++;
    }
}

static uint32 GtmTom_PeriodToFrequency(uint32 PeriodTicks)
{
    if (PeriodTicks == 0u)
    {
        return 0u;
    }

    return (GTMTOM_CLOCK_HZ_DEFAULT + (PeriodTicks / 2u)) / PeriodTicks;
}

static uint16 GtmTom_FrequencyToPeriod(uint32 FrequencyHz)
{
    uint32 periodTicks;

    if (FrequencyHz == 0u)
    {
        GtmTom_SetError(GTMTOM_ERROR_BAD_FREQUENCY);
        return GTMTOM_PERIOD_TICKS_MAX;
    }

    periodTicks = (GTMTOM_CLOCK_HZ_DEFAULT + (FrequencyHz / 2u)) / FrequencyHz;
    return GtmTom_ClampU16(periodTicks, GTMTOM_PERIOD_TICKS_MIN, GTMTOM_PERIOD_TICKS_MAX);
}

static uint16 GtmTom_ScaleDuty(uint16 PeriodTicks, uint32 Duty, uint32 DutyMax)
{
    uint64 scaled;

    if (Duty > DutyMax)
    {
        Duty = DutyMax;
    }

    scaled = ((uint64)PeriodTicks * Duty) + (DutyMax / 2u);
    return (uint16)(scaled / DutyMax);
}

static void GtmTom_GetRequestedValues(GtmTom_PwmValuesType *Values)
{
    uint16 periodTicks;
    uint16 dutyTicks;
    uint8 controlMode;

    controlMode = GtmTom_ControlMode;

    if (controlMode == GTMTOM_CONTROL_MODE_FREQ_DUTY_PERMILLE)
    {
        periodTicks = GtmTom_FrequencyToPeriod(GtmTom_FrequencyHz);
        dutyTicks = GtmTom_ScaleDuty(periodTicks, GtmTom_DutyPermille, GTMTOM_DUTY_PERMILLE_MAX);
    }
    else if (controlMode == GTMTOM_CONTROL_MODE_FREQ_DUTY_PPM)
    {
        periodTicks = GtmTom_FrequencyToPeriod(GtmTom_FrequencyHz);
        dutyTicks = GtmTom_ScaleDuty(periodTicks, GtmTom_DutyPpm, GTMTOM_DUTY_PPM_MAX);
    }
    else
    {
        periodTicks = GtmTom_ClampU16(GtmTom_PeriodTicks, GTMTOM_PERIOD_TICKS_MIN, GTMTOM_PERIOD_TICKS_MAX);
        dutyTicks = GtmTom_ClampU16(GtmTom_DutyTicks, 0u, periodTicks);
    }

    if (dutyTicks > periodTicks)
    {
        dutyTicks = periodTicks;
    }

    Values->periodTicks = periodTicks;
    Values->dutyTicks = dutyTicks;
    Values->frequencyHz = GtmTom_PeriodToFrequency(periodTicks);
}

static void GtmTom_ApplyValues(const GtmTom_PwmValuesType *Values)
{
    Ifx_GTM_TOM *tom;
    IfxGtm_Tom_Ch channel;
    uint8 synchronousUpdate;

    if (GtmTom_Initialized == 0u)
    {
        GtmTom_SetError(GTMTOM_ERROR_NOT_INITIALIZED);
        return;
    }

    tom = GtmTom_Tom;
    channel = GTMTOM_PWM_CHANNEL;
    synchronousUpdate = GtmTom_SynchronousUpdate;

    IfxGtm_Tom_Tgc_enableChannelUpdate(GtmTom_Tgc, channel, FALSE);
    IfxGtm_Tom_Ch_setCompareZeroShadow(tom, channel, Values->periodTicks);
    IfxGtm_Tom_Ch_setCompareOneShadow(tom, channel, Values->dutyTicks);
    IfxGtm_Tom_Tgc_enableChannelUpdate(GtmTom_Tgc, channel, TRUE);

    if (synchronousUpdate == 0u)
    {
        IfxGtm_Tom_Tgc_trigger(GtmTom_Tgc);
    }

    GtmTom_AppliedPeriodTicks = Values->periodTicks;
    GtmTom_AppliedDutyTicks = Values->dutyTicks;
    GtmTom_AppliedFrequencyHz = Values->frequencyHz;
    GtmTom_ApplyCounter++;
    GtmTom_SetError(GTMTOM_ERROR_NONE);
}

static void GtmTom_SetOutputEnabled(uint8 Enable)
{
    if (GtmTom_Initialized == 0u)
    {
        GtmTom_SetError(GTMTOM_ERROR_NOT_INITIALIZED);
        return;
    }

    if (Enable != 0u)
    {
        IfxGtm_Tom_Ch_setSignalLevel(
                GtmTom_Tom,
                GTMTOM_PWM_CHANNEL,
                (GtmTom_InvertOutput != 0u) ? Ifx_ActiveState_low : Ifx_ActiveState_high);
        IfxGtm_Tom_Tgc_enableChannel(GtmTom_Tgc, GTMTOM_PWM_CHANNEL, TRUE, TRUE);
        IfxGtm_Tom_Tgc_enableChannelOutput(GtmTom_Tgc, GTMTOM_PWM_CHANNEL, TRUE, TRUE);
        IfxGtm_Tom_Tgc_trigger(GtmTom_Tgc);
        GtmTom_OutputActive = 1u;
    }
    else
    {
        IfxGtm_Tom_Tgc_enableChannelOutput(GtmTom_Tgc, GTMTOM_PWM_CHANNEL, FALSE, TRUE);
        IfxGtm_Tom_Tgc_enableChannel(GtmTom_Tgc, GTMTOM_PWM_CHANNEL, FALSE, TRUE);
        IfxGtm_Tom_Tgc_trigger(GtmTom_Tgc);
        IfxGtm_Tom_Ch_setSignalLevel(
                GtmTom_Tom,
                GTMTOM_PWM_CHANNEL,
                (GtmTom_IdleLevelHigh != 0u) ? Ifx_ActiveState_high : Ifx_ActiveState_low);
        GtmTom_OutputActive = 0u;
    }
}

static void GtmTom_UpdatePolarity(void)
{
    Ifx_ActiveState activeState;

    if (GtmTom_Initialized == 0u)
    {
        return;
    }

    activeState = (GtmTom_InvertOutput != 0u) ? Ifx_ActiveState_low : Ifx_ActiveState_high;
    IfxGtm_Tom_Ch_setSignalLevel(GtmTom_Tom, GTMTOM_PWM_CHANNEL, activeState);

    if (GtmTom_OutputActive == 0u)
    {
        IfxGtm_Tom_Ch_setSignalLevel(
                GtmTom_Tom,
                GTMTOM_PWM_CHANNEL,
                (GtmTom_IdleLevelHigh != 0u) ? Ifx_ActiveState_high : Ifx_ActiveState_low);
    }
}

static void GtmTom_UpdateSweep(void)
{
    uint32 step;

    if (GtmTom_SweepEnable == 0u)
    {
        return;
    }

    step = GtmTom_SweepFrequencyHzStep;
    if (step == 0u)
    {
        step = 1u;
    }

    if (GtmTom_SweepDirectionUp != 0u)
    {
        if ((GtmTom_FrequencyHz + step) >= GtmTom_SweepFrequencyHzMax)
        {
            GtmTom_FrequencyHz = GtmTom_SweepFrequencyHzMax;
            GtmTom_SweepDirectionUp = 0u;
        }
        else
        {
            GtmTom_FrequencyHz += step;
        }
    }
    else
    {
        if ((GtmTom_FrequencyHz <= (GtmTom_SweepFrequencyHzMin + step)) || (GtmTom_FrequencyHz <= step))
        {
            GtmTom_FrequencyHz = GtmTom_SweepFrequencyHzMin;
            GtmTom_SweepDirectionUp = 1u;
        }
        else
        {
            GtmTom_FrequencyHz -= step;
        }
    }

    GtmTom_ControlMode = GTMTOM_CONTROL_MODE_FREQ_DUTY_PERMILLE;
}

static void GtmTom_UpdateDutySweep(void)
{
    uint32 step;

    if (GtmTom_DutySweepEnable == 0u)
    {
        return;
    }

    step = GtmTom_DutySweepPermilleStep;
    if (step == 0u)
    {
        step = 1u;
    }

    if (GtmTom_DutySweepDirectionUp != 0u)
    {
        if ((GtmTom_DutyPermille + step) >= GtmTom_DutySweepPermilleMax)
        {
            GtmTom_DutyPermille = GtmTom_DutySweepPermilleMax;
            GtmTom_DutySweepDirectionUp = 0u;
        }
        else
        {
            GtmTom_DutyPermille += step;
        }
    }
    else
    {
        if ((GtmTom_DutyPermille <= (GtmTom_DutySweepPermilleMin + step)) || (GtmTom_DutyPermille <= step))
        {
            GtmTom_DutyPermille = GtmTom_DutySweepPermilleMin;
            GtmTom_DutySweepDirectionUp = 1u;
        }
        else
        {
            GtmTom_DutyPermille -= step;
        }
    }

    GtmTom_ControlMode = GTMTOM_CONTROL_MODE_FREQ_DUTY_PERMILLE;
}

static uint8 GtmTom_RequestChanged(void)
{
    if (GtmTom_ApplyNow != 0u)
    {
        return 1u;
    }

    return ((GtmTom_LastControlMode != GtmTom_ControlMode) ||
            (GtmTom_LastPeriodTicks != GtmTom_PeriodTicks) ||
            (GtmTom_LastDutyTicks != GtmTom_DutyTicks) ||
            (GtmTom_LastFrequencyHz != GtmTom_FrequencyHz) ||
            (GtmTom_LastDutyPermille != GtmTom_DutyPermille) ||
            (GtmTom_LastDutyPpm != GtmTom_DutyPpm)) ? 1u : 0u;
}

static void GtmTom_StoreLastRequest(void)
{
    GtmTom_LastControlMode = GtmTom_ControlMode;
    GtmTom_LastPeriodTicks = GtmTom_PeriodTicks;
    GtmTom_LastDutyTicks = GtmTom_DutyTicks;
    GtmTom_LastFrequencyHz = GtmTom_FrequencyHz;
    GtmTom_LastDutyPermille = GtmTom_DutyPermille;
    GtmTom_LastDutyPpm = GtmTom_DutyPpm;
}

static void GtmTom_UpdateBurst(void)
{
    uint64 activeMs;
    uint64 numerator;

    if (GtmTom_SingleShotArm != 0u)
    {
        GtmTom_SingleShotArm = 0u;
        GtmTom_PeriodTicks = GtmTom_ClampU16(GtmTom_PeriodTicks, GTMTOM_PERIOD_TICKS_MIN, GTMTOM_PERIOD_TICKS_MAX);
        GtmTom_DutyTicks = GtmTom_ClampU16(GtmTom_SingleShotPulseTicks, 1u, GtmTom_PeriodTicks);
        GtmTom_BurstCycles = 1u;
        GtmTom_BurstArm = 1u;
        GtmTom_ControlMode = GTMTOM_CONTROL_MODE_RAW_TICKS;
    }

    if (GtmTom_BurstArm != 0u)
    {
        GtmTom_BurstArm = 0u;
        if (GtmTom_BurstCycles != 0u)
        {
            numerator = (uint64)GtmTom_BurstCycles * (uint64)GtmTom_AppliedPeriodTicks * 1000u;
            activeMs = (numerator + (GTMTOM_CLOCK_HZ_DEFAULT - 1u)) / GTMTOM_CLOCK_HZ_DEFAULT;
            GtmTom_BurstRemainingTaskTicks = (uint32)((activeMs + (GTMTOM_TASK_PERIOD_MS - 1u)) / GTMTOM_TASK_PERIOD_MS);
            if (GtmTom_BurstRemainingTaskTicks == 0u)
            {
                GtmTom_BurstRemainingTaskTicks = 1u;
            }

            GtmTom_Enable = 1u;
            GtmTom_BurstState = GTMTOM_BURST_ACTIVE;
        }
    }

    if (GtmTom_BurstState == GTMTOM_BURST_ACTIVE)
    {
        if (GtmTom_BurstRemainingTaskTicks > 0u)
        {
            GtmTom_BurstRemainingTaskTicks--;
        }

        if (GtmTom_BurstRemainingTaskTicks == 0u)
        {
            GtmTom_Enable = 0u;
            GtmTom_BurstState = GTMTOM_BURST_DONE;
        }
    }
}

void GtmTom_Init(void)
{
    float32 moduleFrequency;
    Ifx_GTM *gtm;

    gtm = &MODULE_GTM;
    IfxGtm_enable(gtm);
    moduleFrequency = IfxGtm_Cmu_getModuleFrequency(gtm);
    IfxGtm_Cmu_setGclkFrequency(gtm, moduleFrequency);
    IfxGtm_Cmu_setClkFrequency(gtm, IfxGtm_Cmu_Clk_0, moduleFrequency);
    IfxGtm_Cmu_enableClocks(gtm, IFXGTM_CMU_CLKEN_FXCLK | IFXGTM_CMU_CLKEN_CLK0);

    GtmTom_Gtm = gtm;
    GtmTom_Tom = &GtmTom_Gtm->TOM[GTMTOM_PWM_TOM];
    GtmTom_Tgc = IfxGtm_Tom_Ch_getTgcPointer(GtmTom_Tom, ((uint8)GTMTOM_PWM_CHANNEL <= 7u) ? 0u : 1u);

    IfxGtm_Tom_Ch_setClockSource(GtmTom_Tom, GTMTOM_PWM_CHANNEL, GTMTOM_PWM_CLOCK_SOURCE);
    IfxGtm_Tom_Ch_setSignalLevel(GtmTom_Tom, GTMTOM_PWM_CHANNEL, Ifx_ActiveState_high);
    IfxGtm_Tom_Tgc_setChannelForceUpdate(GtmTom_Tgc, GTMTOM_PWM_CHANNEL, TRUE, TRUE);
    IfxGtm_Tom_Tgc_enableChannel(GtmTom_Tgc, GTMTOM_PWM_CHANNEL, TRUE, FALSE);
    IfxGtm_Tom_Tgc_enableChannelOutput(GtmTom_Tgc, GTMTOM_PWM_CHANNEL, TRUE, FALSE);
    IfxGtm_Tom_Tgc_enableChannelUpdate(GtmTom_Tgc, GTMTOM_PWM_CHANNEL, FALSE);
    IfxGtm_Tom_Ch_setCompareZeroShadow(GtmTom_Tom, GTMTOM_PWM_CHANNEL, (uint16)GtmTom_PeriodTicks);
    IfxGtm_Tom_Ch_setCompareOneShadow(GtmTom_Tom, GTMTOM_PWM_CHANNEL, (uint16)GtmTom_DutyTicks);
    IfxGtm_Tom_Tgc_enableChannelUpdate(GtmTom_Tgc, GTMTOM_PWM_CHANNEL, TRUE);
    IfxGtm_PinMap_setTomTout(&GTMTOM_PWM_OUTPUT_PIN, IfxPort_OutputMode_pushPull, IfxPort_PadDriver_cmosAutomotiveSpeed4);
    IfxGtm_Tom_Tgc_trigger(GtmTom_Tgc);
    IfxGtm_Tom_Tgc_enableChannelOutput(GtmTom_Tgc, GTMTOM_PWM_CHANNEL, FALSE, TRUE);
    IfxGtm_Tom_Tgc_enableChannel(GtmTom_Tgc, GTMTOM_PWM_CHANNEL, FALSE, TRUE);
    IfxGtm_Tom_Tgc_trigger(GtmTom_Tgc);

    GtmTom_Initialized = 1u;
    GtmTom_AppliedPeriodTicks = GtmTom_PeriodTicks;
    GtmTom_AppliedDutyTicks = GtmTom_DutyTicks;
    GtmTom_AppliedFrequencyHz = GtmTom_PeriodToFrequency(GtmTom_PeriodTicks);
    GtmTom_StoreLastRequest();
    GtmTom_LastEnable = GtmTom_Enable;
    GtmTom_LastInvertOutput = GtmTom_InvertOutput;
    GtmTom_LastIdleLevelHigh = GtmTom_IdleLevelHigh;
    GtmTom_UpdatePolarity();
}

void GtmTom_MainFunction(void)
{
    GtmTom_PwmValuesType values;

    GtmTom_MainFunctionCounter++;

    if (GtmTom_Initialized == 0u)
    {
        GtmTom_SetError(GTMTOM_ERROR_NOT_INITIALIZED);
        return;
    }

    GtmTom_UpdateSweep();
    GtmTom_UpdateDutySweep();

    if ((GtmTom_LastInvertOutput != GtmTom_InvertOutput) ||
        (GtmTom_LastIdleLevelHigh != GtmTom_IdleLevelHigh))
    {
        GtmTom_UpdatePolarity();
        GtmTom_LastInvertOutput = GtmTom_InvertOutput;
        GtmTom_LastIdleLevelHigh = GtmTom_IdleLevelHigh;
    }

    if (GtmTom_RequestChanged() != 0u)
    {
        GtmTom_GetRequestedValues(&values);
        GtmTom_ApplyValues(&values);
        GtmTom_StoreLastRequest();
        GtmTom_ApplyNow = 0u;
    }

    GtmTom_UpdateBurst();

    if (GtmTom_LastEnable != GtmTom_Enable)
    {
        GtmTom_SetOutputEnabled(GtmTom_Enable);
        GtmTom_LastEnable = GtmTom_Enable;
    }
}
