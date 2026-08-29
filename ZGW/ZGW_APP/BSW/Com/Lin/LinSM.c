#include "LinSM.h"
#include "LinIf.h"
#include "Lin.h"
#include "LinDiag.h"

#define LINSM_MAX_CHANNELS 1u
#define LINSM_WAKEUP_TICKS 30u

static LinSM_StateType LinSM_State[LINSM_MAX_CHANNELS];
static uint16 LinSM_Timer[LINSM_MAX_CHANNELS];
long long LinSM_MainFunction_Counter = 0;

void LinSM_Init(void)
{
    uint8 ch;

    for (ch = 0u; ch < LINSM_MAX_CHANNELS; ch++)
    {
        LinSM_State[ch] = LINSM_NO_COMMUNICATION;
        LinSM_Timer[ch] = 0u;
    }

    LinDiag_Init();
}

Std_ReturnType LinSM_RequestComMode(uint8 Channel, LinSM_StateType RequestedState)
{
    if (Channel >= LINSM_MAX_CHANNELS)
    {
        return E_NOT_OK;
    }

    switch (RequestedState)
    {
        case LINSM_FULL_COMMUNICATION:
            if (LinSM_State[Channel] == LINSM_SLEEP)
            {
                if (Lin_Wakeup(Channel) != E_OK)
                {
                    LinDiag_ReportWakeupFailure(Channel);
                    return E_NOT_OK;
                }
                LinSM_State[Channel] = LINSM_WAKEUP;
                LinSM_Timer[Channel] = LINSM_WAKEUP_TICKS;
            }
            else
            {
                LinIf_Init();
                LinSM_State[Channel] = LINSM_FULL_COMMUNICATION;
                (void)LinIf_SwitchSchedule(LINIF_SCHED_NORMAL);
            }
            return E_OK;

        case LINSM_SLEEP:
            if (Lin_GoToSleep(Channel) == E_OK)
            {
                LinSM_State[Channel] = LINSM_GOTO_SLEEP;
                LinSM_Timer[Channel] = LINSM_WAKEUP_TICKS;
                return E_OK;
            }
            LinDiag_ReportSleepFailure(Channel);
            return E_NOT_OK;

        case LINSM_NO_COMMUNICATION:
            LinIf_ResetDiagnostic();
            LinSM_State[Channel] = LINSM_NO_COMMUNICATION;
            return E_OK;

        default:
            return E_NOT_OK;
    }
}

LinSM_StateType LinSM_GetState(uint8 Channel)
{
    if (Channel >= LINSM_MAX_CHANNELS)
    {
        return LINSM_UNINIT;
    }

    return LinSM_State[Channel];
}

void LinSM_MainFunction(void)
{
    uint8 ch;

    for (ch = 0u; ch < LINSM_MAX_CHANNELS; ch++)
    {
        if (LinSM_State[ch] == LINSM_GOTO_SLEEP)
        {
            if (Lin_GetState(ch) == LIN_SLEEP)
            {
                LinSM_State[ch] = LINSM_SLEEP;
                LinDiag_ReportFrameResult(ch, LINIF_SCHED_DIAG_REQ, 0x3Cu,
                        LIN_PID_MASTER_REQUEST, 0u, FALSE, LIN_RES_OK);
            }
            else if (LinSM_Timer[ch] > 0u)
            {
                LinSM_Timer[ch]--;
            }
            else
            {
                LinDiag_ReportSleepFailure(ch);
                LinSM_State[ch] = LINSM_NO_COMMUNICATION;
            }
        }
        else if (LinSM_State[ch] == LINSM_WAKEUP)
        {
            if (LinSM_Timer[ch] > 0u)
            {
                LinSM_Timer[ch]--;
            }
            else
            {
                LinSM_State[ch] = LINSM_FULL_COMMUNICATION;
                (void)LinIf_SwitchSchedule(LINIF_SCHED_NORMAL);
            }
        }
        else if (LinSM_State[ch] == LINSM_FULL_COMMUNICATION)
        {
            /* Keep the active schedule running in full communication. */
        }
        else
        {
            if (LinSM_Timer[ch] > 0u)
            {
                LinSM_Timer[ch]--;
            }
            else
            {
                LinSM_State[ch] = LINSM_SLEEP;
            }
        }
    }

    LinSM_MainFunction_Counter++;
}
