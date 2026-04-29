#include "LinIf.h"
#include "LinTp.h"
#include <string.h>

static const Lin_FrameConfigType LinIf_Frame_MRF =
{
    0x3Cu, LIN_PID_MASTER_REQUEST, 8u, LIN_CS_CLASSIC, LIN_FRM_DIAGNOSTIC_MRF, 20u
};

static const Lin_FrameConfigType LinIf_Frame_SRF =
{
    0x3Du, LIN_PID_SLAVE_RESPONSE, 8u, LIN_CS_CLASSIC, LIN_FRM_DIAGNOSTIC_SRF, 20u
};

static const Lin_FrameConfigType LinIf_Frame_App10 =
{
    0x10u, 0u, 8u, LIN_CS_ENHANCED, LIN_FRM_UNCONDITIONAL, 20u
};

static const LinIf_ScheduleEntryType LinIf_NormalEntries[] =
{
    { &LinIf_Frame_App10, 10u, LIN_SLAVE_RESPONSE }
};

static const LinIf_ScheduleEntryType LinIf_DiagReqEntries[] =
{
    { &LinIf_Frame_MRF, 10u, LIN_MASTER_RESPONSE }
};

static const LinIf_ScheduleEntryType LinIf_DiagRespEntries[] =
{
    { &LinIf_Frame_SRF, 10u, LIN_SLAVE_RESPONSE }
};

static const LinIf_ScheduleTableType LinIf_Schedules[] =
{
    { LinIf_NormalEntries,   1u, FALSE },
    { LinIf_DiagReqEntries,  1u, TRUE  },
    { LinIf_DiagRespEntries, 1u, TRUE  }
};

typedef struct
{
    uint8 activeSchedule;
    uint8 index;
    uint16 timer;
    uint8 busy;

    uint8 diagReq[8];
    uint8 diagResp[8];
    uint8 diagRespValid;
    LinIf_DiagStateType diagState;

    uint16 diagTimer;
    LinIf_ChannelStateType channelState;
    uint32 scheduleErrorCounter;
    uint32 diagTimeoutCounter;
} LinIf_StateType;

static LinIf_StateType LinIf_State;

void LinIf_Init(void)
{
    memset(&LinIf_State, 0, sizeof(LinIf_State));

    LinIf_State.activeSchedule = LINIF_SCHED_NORMAL;
    LinIf_State.diagState = LINIF_DIAG_IDLE;
    LinIf_State.channelState = LINIF_CHANNEL_IDLE;

    Lin_Init();
}

Std_ReturnType LinIf_SwitchSchedule(uint8 scheduleId)
{
    if (scheduleId >= (uint8)(sizeof(LinIf_Schedules) / sizeof(LinIf_Schedules[0])))
    {
        return E_NOT_OK;
    }

    LinIf_State.activeSchedule = scheduleId;
    LinIf_State.index = 0u;
    LinIf_State.timer = 0u;
    LinIf_State.busy = FALSE;

    return E_OK;
}

Std_ReturnType LinIf_SetDiagRequest(const uint8 data[8])
{
    if (data == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if ((LinIf_State.diagState != LINIF_DIAG_IDLE) &&
        (LinIf_State.diagState != LINIF_DIAG_DONE) &&
        (LinIf_State.diagState != LINIF_DIAG_ERROR))
    {
        return E_NOT_OK;
    }

    memcpy(LinIf_State.diagReq, data, 8u);
    LinIf_State.diagRespValid = FALSE;
    LinIf_State.diagState = LINIF_DIAG_MRF_PENDING;
    LinIf_State.diagTimer = LINIF_DIAG_TIMEOUT_TICKS;

    return LinIf_SwitchSchedule(LINIF_SCHED_DIAG_REQ);
}

Std_ReturnType LinIf_GetDiagResponse(uint8 data[8])
{
    if ((data == NULL_PTR) || (LinIf_State.diagRespValid == FALSE))
    {
        return E_NOT_OK;
    }

    memcpy(data, LinIf_State.diagResp, 8u);
    LinIf_State.diagRespValid = FALSE;

    return E_OK;
}

LinIf_DiagStateType LinIf_GetDiagState(void)
{
    return LinIf_State.diagState;
}

void LinIf_DiagFrameDone(uint8 success)
{
    if (success == FALSE)
    {
        LinIf_State.diagState = LINIF_DIAG_ERROR;
        LinIf_State.channelState = LINIF_CHANNEL_ERROR;
        LinIf_State.scheduleErrorCounter++;
        (void)LinIf_SwitchSchedule(LINIF_SCHED_NORMAL);
        return;
    }

    if (LinIf_State.diagState == LINIF_DIAG_MRF_ACTIVE)
    {
        LinIf_State.diagState = LINIF_DIAG_SRF_PENDING;
        (void)LinIf_SwitchSchedule(LINIF_SCHED_DIAG_RESP);
    }
    else if (LinIf_State.diagState == LINIF_DIAG_SRF_ACTIVE)
    {
        LinIf_State.diagState = LINIF_DIAG_DONE;
        (void)LinIf_SwitchSchedule(LINIF_SCHED_NORMAL);
    }
}

static Std_ReturnType LinIf_TransmitFrame(const Lin_FrameConfigType* frame)
{
    Lin_PduType pdu;

    if (frame == NULL_PTR)
    {
        return E_NOT_OK;
    }

    memset(&pdu, 0, sizeof(pdu));

    pdu.pid = frame->pid;
    pdu.dlc = frame->dlc;
    pdu.checksumType = frame->checksumType;

    if ((pdu.pid == 0u) && (frame->id <= 0x3Fu))
    {
        pdu.pid = Lin_MakePid(frame->id);
    }

    if (frame->frameClass == LIN_FRM_DIAGNOSTIC_MRF)
    {
        pdu.direction = LIN_MASTER_RESPONSE;
        memcpy(pdu.data, LinIf_State.diagReq, 8u);
        LinIf_State.diagState = LINIF_DIAG_MRF_ACTIVE;
    }
    else if (frame->frameClass == LIN_FRM_DIAGNOSTIC_SRF)
    {
        pdu.direction = LIN_SLAVE_RESPONSE;
        LinIf_State.diagState = LINIF_DIAG_SRF_ACTIVE;
    }
    else
    {
        pdu.direction = LIN_SLAVE_RESPONSE;
    }

    return Lin_SendFrame(LIN_CHANNEL_0, &pdu);
}

void LinIf_MainFunction(void)
{
    const LinIf_ScheduleTableType* sched;
    const LinIf_ScheduleEntryType* entry;
    uint8 rx[8];
    uint8 len;
    Lin_ResultType res;

    Lin_MainFunction();

    if ((LinIf_State.diagState != LINIF_DIAG_IDLE) &&
        (LinIf_State.diagState != LINIF_DIAG_DONE) &&
        (LinIf_State.diagState != LINIF_DIAG_ERROR))
    {
        if (LinIf_State.diagTimer > 0u)
        {
            LinIf_State.diagTimer--;
        }
        else
        {
            LinIf_State.diagTimeoutCounter++;
            LinIf_State.diagState = LINIF_DIAG_ERROR;
            LinIf_State.busy = FALSE;
            LinIf_State.channelState = LINIF_CHANNEL_ERROR;
            (void)LinIf_SwitchSchedule(LINIF_SCHED_NORMAL);
            return;
        }
    }

    if (LinIf_State.busy == TRUE)
    {
        if (Lin_GetState(LIN_CHANNEL_0) == LIN_IDLE)
        {
            res = Lin_GetStatus(LIN_CHANNEL_0, rx, &len);

            if (res == LIN_RES_OK)
            {
                entry = &LinIf_Schedules[LinIf_State.activeSchedule].entries[LinIf_State.index];

                if (entry->frame->frameClass == LIN_FRM_DIAGNOSTIC_MRF)
                {
                    LinTp_TxFrameConfirmation(TRUE);
                    LinIf_DiagFrameDone(TRUE);
                }
                else if (entry->frame->frameClass == LIN_FRM_DIAGNOSTIC_SRF)
                {
                    memcpy(LinIf_State.diagResp, rx, 8u);
                    LinIf_State.diagRespValid = TRUE;

                    LinTp_RxSlaveResponse(rx);
                    LinIf_DiagFrameDone(TRUE);
                }
            }
            else
            {
                entry = &LinIf_Schedules[LinIf_State.activeSchedule].entries[LinIf_State.index];

                if (entry->frame->frameClass == LIN_FRM_DIAGNOSTIC_MRF)
                {
                    LinTp_TxFrameConfirmation(FALSE);
                }

                LinIf_DiagFrameDone(FALSE);
            }

            LinIf_State.busy = FALSE;
            LinIf_State.channelState = LINIF_CHANNEL_IDLE;
            LinIf_State.index++;

            sched = &LinIf_Schedules[LinIf_State.activeSchedule];

            if (LinIf_State.index >= sched->numEntries)
            {
                if (sched->runOnce != FALSE)
                {
                    if ((LinIf_State.diagState != LINIF_DIAG_SRF_PENDING) &&
                        (LinIf_State.diagState != LINIF_DIAG_MRF_ACTIVE))
                    {
                        (void)LinIf_SwitchSchedule(LINIF_SCHED_NORMAL);
                    }
                }
                else
                {
                    LinIf_State.index = 0u;
                }
            }
        }

        return;
    }

    if (LinIf_State.timer > 0u)
    {
        LinIf_State.timer--;
        return;
    }

    sched = &LinIf_Schedules[LinIf_State.activeSchedule];
    entry = &sched->entries[LinIf_State.index];

    if (LinIf_TransmitFrame(entry->frame) == E_OK)
    {
        LinIf_State.timer = entry->delayTicks;
        LinIf_State.busy = TRUE;
        LinIf_State.channelState = LINIF_CHANNEL_BUSY;
        Lin_SetResponseTimeout(entry->frame->responseTimeoutTicks);
    }
}

LinIf_ChannelStateType LinIf_GetChannelState(void)
{
    return LinIf_State.channelState;
}

void LinIf_ResetDiagnostic(void)
{
    LinIf_State.diagRespValid = FALSE;
    LinIf_State.diagState = LINIF_DIAG_IDLE;
    LinIf_State.diagTimer = 0u;
}
