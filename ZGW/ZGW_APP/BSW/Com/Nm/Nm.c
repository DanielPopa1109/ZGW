#include "Nm.h"
#include "Nm_Cfg.h"

#include "../CanNm/CanNm.h"
#include "../Com.h"
#include "../ComM/ComM.h"
#include "../UdpNm/UdpNm.h"

typedef struct
{
    uint8 localRequested;
    Nm_StateType state;
    Nm_ModeType mode;
    uint16 readySleepTimer;
    uint16 prepareSleepTimer;
} Nm_LinStateType;

static Nm_LinStateType Nm_LinState;
static uint8 Nm_Initialized;

static void Nm_LinWriteNm3Signal(uint8 value)
{
    (void)value;
}

static void Nm_LinSetState(Nm_StateType state, Nm_ModeType mode)
{
    if ((Nm_LinState.state == state) && (Nm_LinState.mode == mode))
    {
        return;
    }

    Nm_LinState.state = state;
    Nm_LinState.mode = mode;

    if (mode == NM_MODE_NETWORK)
    {
        ComM_Nm_NetworkMode(COMM_CH_LIN);
    }
    else if (mode == NM_MODE_PREPARE_BUS_SLEEP)
    {
        ComM_Nm_PrepareBusSleepMode(COMM_CH_LIN);
    }
    else if (mode == NM_MODE_BUS_SLEEP)
    {
        ComM_Nm_BusSleepMode(COMM_CH_LIN);
    }
    else
    {
        /* Synchronize mode is not used for the lab LIN passive model. */
    }
}

static Std_ReturnType Nm_LinNetworkRequest(void)
{
    Nm_LinState.localRequested = TRUE;
    Nm_LinState.readySleepTimer = 0u;
    Nm_LinState.prepareSleepTimer = 0u;
    Nm_LinSetState(NM_STATE_NORMAL_OPERATION, NM_MODE_NETWORK);
    Nm_LinWriteNm3Signal(0xFFu);
    return E_OK;
}

static Std_ReturnType Nm_LinNetworkRelease(void)
{
    Nm_LinState.localRequested = FALSE;
    Nm_LinWriteNm3Signal(0u);

    if (Nm_LinState.mode == NM_MODE_NETWORK)
    {
        Nm_LinState.readySleepTimer = NM_LIN_READY_SLEEP_TICKS;
        Nm_LinSetState(NM_STATE_READY_SLEEP, NM_MODE_NETWORK);
    }

    return E_OK;
}

static void Nm_LinMainFunction(void)
{
    if (Nm_LinState.localRequested != FALSE)
    {
        Nm_LinSetState(NM_STATE_NORMAL_OPERATION, NM_MODE_NETWORK);
        return;
    }

    if (Nm_LinState.state == NM_STATE_READY_SLEEP)
    {
        if (Nm_LinState.readySleepTimer > 0u)
        {
            Nm_LinState.readySleepTimer--;
        }
        else
        {
            Nm_LinState.prepareSleepTimer = NM_LIN_PREPARE_SLEEP_TICKS;
            Nm_LinSetState(NM_STATE_PREPARE_BUS_SLEEP,
                           NM_MODE_PREPARE_BUS_SLEEP);
        }
    }
    else if (Nm_LinState.state == NM_STATE_PREPARE_BUS_SLEEP)
    {
        if (Nm_LinState.prepareSleepTimer > 0u)
        {
            Nm_LinState.prepareSleepTimer--;
        }
        else
        {
            Nm_LinSetState(NM_STATE_BUS_SLEEP, NM_MODE_BUS_SLEEP);
        }
    }
    else
    {
        /* LIN has no AUTOSAR LinNm frame handling in this lab stack. */
    }
}

void Nm_Init(void)
{
    Nm_LinState.localRequested = FALSE;
    Nm_LinState.state = NM_STATE_BUS_SLEEP;
    Nm_LinState.mode = NM_MODE_BUS_SLEEP;
    Nm_LinState.readySleepTimer = 0u;
    Nm_LinState.prepareSleepTimer = 0u;

    CanNm_Init();
    UdpNm_Init();

    Nm_Initialized = TRUE;
}

void Nm_MainFunction(void)
{
    if (Nm_Initialized == FALSE)
    {
        return;
    }

    Nm_LinMainFunction();
}

Std_ReturnType Nm_NetworkRequest(uint8 channel)
{
    if (Nm_Initialized == FALSE)
    {
        return E_NOT_OK;
    }

    switch (channel)
    {
        case COMM_CH_CAN:
        case COMM_CH_CANFD:
            return CanNm_NetworkRequest(channel);

        case COMM_CH_LIN:
            return Nm_LinNetworkRequest();

        case COMM_CH_ETH:
            return UdpNm_NetworkRequest(channel);

        default:
            return E_NOT_OK;
    }
}

Std_ReturnType Nm_NetworkRelease(uint8 channel)
{
    if (Nm_Initialized == FALSE)
    {
        return E_NOT_OK;
    }

    switch (channel)
    {
        case COMM_CH_CAN:
        case COMM_CH_CANFD:
            return CanNm_NetworkRelease(channel);

        case COMM_CH_LIN:
            return Nm_LinNetworkRelease();

        case COMM_CH_ETH:
            return UdpNm_NetworkRelease(channel);

        default:
            return E_NOT_OK;
    }
}

Std_ReturnType Nm_GetState(uint8 channel, Nm_StateType* state, Nm_ModeType* mode)
{
    if ((Nm_Initialized == FALSE) || (state == NULL_PTR) || (mode == NULL_PTR))
    {
        return E_NOT_OK;
    }

    switch (channel)
    {
        case COMM_CH_CAN:
        case COMM_CH_CANFD:
            return CanNm_GetState(channel, state, mode);

        case COMM_CH_LIN:
            *state = Nm_LinState.state;
            *mode = Nm_LinState.mode;
            return E_OK;

        case COMM_CH_ETH:
            return UdpNm_GetState(channel, state, mode);

        default:
            return E_NOT_OK;
    }
}
