#include "CanNm.h"

#include "../Com.h"
#include "../ComM/ComM.h"

#include <string.h>

#define CANNM_NUM_CHANNELS              2u
#define CANNM_NM3_ACTIVE_VALUE          0xFFu
#define CANNM_TX_PERIOD_TICKS           20u
#define CANNM_TX_RELOAD_TICKS           (CANNM_TX_PERIOD_TICKS - 1u)
#define CANNM_TIMEOUT_TICKS             200u
#define CANNM_REPEAT_MESSAGE_TICKS      40u
#define CANNM_READY_SLEEP_TICKS         100u
#define CANNM_PREPARE_BUS_SLEEP_TICKS   40u

typedef struct
{
    uint8 commChannel;
    Com_SignalIdType nm3SignalId;
} CanNm_ChannelConfigType;

typedef struct
{
    uint8 localRequested;
    uint8 remoteActive;
    uint8 txConfirmed;
    Nm_StateType state;
    Nm_ModeType mode;
    uint16 txTimer;
    uint16 timeoutTimer;
    uint16 repeatTimer;
    uint16 readySleepTimer;
    uint16 prepareSleepTimer;
    uint8 userData[CANNM_USER_DATA_LEN];
    uint8 userDataLen;
} CanNm_ChannelStateType;

static const CanNm_ChannelConfigType CanNm_ChannelConfig[CANNM_NUM_CHANNELS] =
{
    {
        COMM_CH_CAN,
        COM_SIG_TX_NM3_NM3_PN1
    },
    {
        COMM_CH_CANFD,
        COM_SIG_TX_CANFD_NM3_NM3_PN1
    }
};

static CanNm_ChannelStateType CanNm_ChannelState[CANNM_NUM_CHANNELS];
static uint8 CanNm_Initialized;

static uint8 CanNm_FindChannelIndex(uint8 channel, uint8* index)
{
    uint8 i;

    for (i = 0u; i < CANNM_NUM_CHANNELS; i++)
    {
        if (CanNm_ChannelConfig[i].commChannel == channel)
        {
            if (index != NULL_PTR)
            {
                *index = i;
            }
            return TRUE;
        }
    }

    return FALSE;
}

static void CanNm_SetState(uint8 index, Nm_StateType state, Nm_ModeType mode)
{
    CanNm_ChannelStateType* rt = &CanNm_ChannelState[index];
    uint8 channel = CanNm_ChannelConfig[index].commChannel;

    if ((rt->state == state) && (rt->mode == mode))
    {
        return;
    }

    rt->state = state;
    rt->mode = mode;

    if (mode == NM_MODE_NETWORK)
    {
        ComM_Nm_NetworkMode(channel);
    }
    else if (mode == NM_MODE_PREPARE_BUS_SLEEP)
    {
        ComM_Nm_PrepareBusSleepMode(channel);
    }
    else if (mode == NM_MODE_BUS_SLEEP)
    {
        ComM_Nm_BusSleepMode(channel);
    }
    else
    {
        /* Synchronize mode is not used by this lab CanNm subset. */
    }
}

static void CanNm_EnterNetwork(uint8 index, uint8 repeatMessage)
{
    CanNm_ChannelStateType* rt = &CanNm_ChannelState[index];

    rt->readySleepTimer = 0u;
    rt->prepareSleepTimer = 0u;

    if (repeatMessage != FALSE)
    {
        rt->repeatTimer = CANNM_REPEAT_MESSAGE_TICKS;
        CanNm_SetState(index, NM_STATE_REPEAT_MESSAGE, NM_MODE_NETWORK);
    }
    else
    {
        rt->repeatTimer = 0u;
        CanNm_SetState(index, NM_STATE_NORMAL_OPERATION, NM_MODE_NETWORK);
    }
}

static void CanNm_WriteNm3Signal(uint8 index, uint8 nm3Pn1)
{
    const CanNm_ChannelConfigType* cfg = &CanNm_ChannelConfig[index];

    (void)Com_SendSignal(cfg->nm3SignalId, &nm3Pn1);
}

static void CanNm_TransmitNmPdu(uint8 index)
{
    const CanNm_ChannelStateType* rt = &CanNm_ChannelState[index];
    uint8 nm3Pn1;

    /* The generated NM3 COM frames are the NM carriers in this lab stack.
     * They expose PN1 as a single byte, so CanNm user data is bounded to that
     * byte and the normal COM/PduR route sends the CAN frame.
     */
    nm3Pn1 = (rt->userDataLen > 0u) ?
             rt->userData[0u] :
             CANNM_NM3_ACTIVE_VALUE;

    CanNm_WriteNm3Signal(index, nm3Pn1);
}

static void CanNm_HandleReleasedNetwork(uint8 index)
{
    CanNm_ChannelStateType* rt = &CanNm_ChannelState[index];

    if (rt->remoteActive != FALSE)
    {
        CanNm_SetState(index, NM_STATE_READY_SLEEP, NM_MODE_NETWORK);
        return;
    }

    if ((rt->state == NM_STATE_NORMAL_OPERATION) ||
        (rt->state == NM_STATE_REPEAT_MESSAGE))
    {
        rt->readySleepTimer = CANNM_READY_SLEEP_TICKS;
        CanNm_SetState(index, NM_STATE_READY_SLEEP, NM_MODE_NETWORK);
    }
    else if (rt->state == NM_STATE_READY_SLEEP)
    {
        if (rt->readySleepTimer > 0u)
        {
            rt->readySleepTimer--;
        }
        else
        {
            rt->prepareSleepTimer = CANNM_PREPARE_BUS_SLEEP_TICKS;
            CanNm_SetState(index, NM_STATE_PREPARE_BUS_SLEEP,
                           NM_MODE_PREPARE_BUS_SLEEP);
        }
    }
    else if (rt->state == NM_STATE_PREPARE_BUS_SLEEP)
    {
        if (rt->prepareSleepTimer > 0u)
        {
            rt->prepareSleepTimer--;
        }
        else
        {
            CanNm_SetState(index, NM_STATE_BUS_SLEEP, NM_MODE_BUS_SLEEP);
        }
    }
    else
    {
        /* Already in bus sleep. */
    }
}

void CanNm_Init(void)
{
    uint8 i;

    for (i = 0u; i < CANNM_NUM_CHANNELS; i++)
    {
        CanNm_ChannelState[i].localRequested = FALSE;
        CanNm_ChannelState[i].remoteActive = FALSE;
        CanNm_ChannelState[i].txConfirmed = FALSE;
        CanNm_ChannelState[i].state = NM_STATE_BUS_SLEEP;
        CanNm_ChannelState[i].mode = NM_MODE_BUS_SLEEP;
        CanNm_ChannelState[i].txTimer = 0u;
        CanNm_ChannelState[i].timeoutTimer = 0u;
        CanNm_ChannelState[i].repeatTimer = 0u;
        CanNm_ChannelState[i].readySleepTimer = 0u;
        CanNm_ChannelState[i].prepareSleepTimer = 0u;
        CanNm_ChannelState[i].userDataLen = 0u;
        (void)memset(CanNm_ChannelState[i].userData,
                     0,
                     sizeof(CanNm_ChannelState[i].userData));
    }

    CanNm_Initialized = TRUE;
}

void CanNm_MainFunction(void)
{
    uint8 i;

    if (CanNm_Initialized == FALSE)
    {
        return;
    }

    for (i = 0u; i < CANNM_NUM_CHANNELS; i++)
    {
        CanNm_ChannelStateType* rt = &CanNm_ChannelState[i];

        if (rt->timeoutTimer > 0u)
        {
            rt->timeoutTimer--;
            if (rt->timeoutTimer == 0u)
            {
                rt->remoteActive = FALSE;
            }
        }

        if (rt->localRequested != FALSE)
        {
            if (rt->state == NM_STATE_BUS_SLEEP)
            {
                CanNm_EnterNetwork(i, TRUE);
            }
            else if (rt->state == NM_STATE_REPEAT_MESSAGE)
            {
                if (rt->repeatTimer > 0u)
                {
                    rt->repeatTimer--;
                }
                else
                {
                    CanNm_SetState(i, NM_STATE_NORMAL_OPERATION,
                                   NM_MODE_NETWORK);
                }
            }
            else if (rt->mode != NM_MODE_NETWORK)
            {
                CanNm_EnterNetwork(i, TRUE);
            }
            else
            {
                /* Stay in network mode. */
            }

            if (rt->txTimer > 0u)
            {
                rt->txTimer--;
            }
            else
            {
                CanNm_TransmitNmPdu(i);
                rt->txTimer = CANNM_TX_RELOAD_TICKS;
            }
        }
        else
        {
            CanNm_HandleReleasedNetwork(i);
        }
    }
}

Std_ReturnType CanNm_NetworkRequest(uint8 channel)
{
    uint8 index;

    if ((CanNm_Initialized == FALSE) ||
        (CanNm_FindChannelIndex(channel, &index) == FALSE))
    {
        return E_NOT_OK;
    }

    CanNm_ChannelState[index].localRequested = TRUE;
    CanNm_ChannelState[index].txTimer = 0u;
    CanNm_EnterNetwork(index, TRUE);
    CanNm_TransmitNmPdu(index);
    return E_OK;
}

Std_ReturnType CanNm_NetworkRelease(uint8 channel)
{
    uint8 index;

    if ((CanNm_Initialized == FALSE) ||
        (CanNm_FindChannelIndex(channel, &index) == FALSE))
    {
        return E_NOT_OK;
    }

    CanNm_ChannelState[index].localRequested = FALSE;
    CanNm_WriteNm3Signal(index, 0u);

    if (CanNm_ChannelState[index].mode == NM_MODE_NETWORK)
    {
        CanNm_ChannelState[index].readySleepTimer = CANNM_READY_SLEEP_TICKS;
        CanNm_SetState(index, NM_STATE_READY_SLEEP, NM_MODE_NETWORK);
    }

    return E_OK;
}

Std_ReturnType CanNm_GetState(uint8 channel,
                              Nm_StateType* state,
                              Nm_ModeType* mode)
{
    uint8 index;

    if ((CanNm_Initialized == FALSE) ||
        (state == NULL_PTR) ||
        (mode == NULL_PTR) ||
        (CanNm_FindChannelIndex(channel, &index) == FALSE))
    {
        return E_NOT_OK;
    }

    *state = CanNm_ChannelState[index].state;
    *mode = CanNm_ChannelState[index].mode;
    return E_OK;
}

Std_ReturnType CanNm_PassiveStartUp(uint8 channel)
{
    uint8 index;

    if ((CanNm_Initialized == FALSE) ||
        (CanNm_FindChannelIndex(channel, &index) == FALSE))
    {
        return E_NOT_OK;
    }

    ComM_Nm_NetworkStartIndication(channel);
    CanNm_EnterNetwork(index, TRUE);
    return E_OK;
}

Std_ReturnType CanNm_SetUserData(uint8 channel, const uint8* data, uint8 len)
{
    uint8 index;

    if ((CanNm_Initialized == FALSE) ||
        (CanNm_FindChannelIndex(channel, &index) == FALSE) ||
        ((data == NULL_PTR) && (len != 0u)) ||
        (len > CANNM_USER_DATA_LEN))
    {
        return E_NOT_OK;
    }

    if (len != 0u)
    {
        (void)memcpy(CanNm_ChannelState[index].userData, data, len);
    }

    CanNm_ChannelState[index].userDataLen = len;
    if (CanNm_ChannelState[index].localRequested != FALSE)
    {
        CanNm_TransmitNmPdu(index);
    }
    return E_OK;
}

Std_ReturnType CanNm_GetUserData(uint8 channel, uint8* data, uint8* len)
{
    uint8 index;
    uint8 copyLen;

    if ((CanNm_Initialized == FALSE) ||
        (data == NULL_PTR) ||
        (len == NULL_PTR) ||
        (CanNm_FindChannelIndex(channel, &index) == FALSE))
    {
        return E_NOT_OK;
    }

    copyLen = CanNm_ChannelState[index].userDataLen;

    if (*len < copyLen)
    {
        return E_NOT_OK;
    }

    if (copyLen != 0u)
    {
        (void)memcpy(data, CanNm_ChannelState[index].userData, copyLen);
    }

    *len = copyLen;
    return E_OK;
}

void CanNm_RxIndication(PduIdType rxPduId, const uint8* data, PduLengthType len)
{
    (void)rxPduId;
    (void)data;
    (void)len;
    /* CAN NM3 frames are generated COM Tx frames for ZGW in this lab stack.
     * No separate 0x500/0x501 CanNm Rx path is used.
     */
}

void CanNm_TxConfirmation(PduIdType txPduId)
{
    (void)txPduId;
    /* Tx confirmation remains owned by COM/PduR for NM3. */
}
