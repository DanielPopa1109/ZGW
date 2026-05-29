#include "Nm.h"
#include "Nm_Cfg.h"

#include "GatewaySwc.h"
#include "../CanNm/CanNm.h"
#include "../Com.h"
#include "../ComM/ComM.h"
#include "../UdpNm/UdpNm.h"

#include <string.h>

typedef struct
{
    uint8 localRequested;
    Nm_StateType state;
    Nm_ModeType mode;
    uint16 readySleepTimer;
    uint16 prepareSleepTimer;
    uint8 userData[NM_MAX_USER_DATA_LEN];
    uint8 userDataLen;
} Nm_LinStateType;

static Nm_LinStateType Nm_LinState;
static uint8 Nm_Initialized;

static void Nm_LinWriteNm3Signal(uint8 value)
{
    (void)GatewaySwc_RequestComSendSignal(COM_SIG_TX_LIN_ZGW_NM3_ZGW_NM3_PN1, &value);
}

static uint8 Nm_LinGetNm3SignalValue(void)
{
    return (Nm_LinState.userDataLen > 0u) ?
           Nm_LinState.userData[0u] :
           0xFFu;
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
    Nm_LinWriteNm3Signal(Nm_LinGetNm3SignalValue());
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
    Nm_LinState.userDataLen = 0u;
    (void)memset(Nm_LinState.userData, 0, sizeof(Nm_LinState.userData));

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

Std_ReturnType Nm_PassiveStartUp(uint8 channel)
{
    if (Nm_Initialized == FALSE)
    {
        return E_NOT_OK;
    }

    switch (channel)
    {
        case COMM_CH_CAN:
        case COMM_CH_CANFD:
            return CanNm_PassiveStartUp(channel);

        case COMM_CH_LIN:
            ComM_Nm_NetworkStartIndication(COMM_CH_LIN);
            return Nm_LinNetworkRequest();

        case COMM_CH_ETH:
            return UdpNm_PassiveStartUp(channel);

        default:
            return E_NOT_OK;
    }
}

Std_ReturnType Nm_SetUserData(uint8 channel, const uint8* data, uint8 len)
{
    if ((Nm_Initialized == FALSE) ||
        ((data == NULL_PTR) && (len != 0u)) ||
        (len > NM_MAX_USER_DATA_LEN))
    {
        return E_NOT_OK;
    }

    switch (channel)
    {
        case COMM_CH_CAN:
        case COMM_CH_CANFD:
            return CanNm_SetUserData(channel, data, len);

        case COMM_CH_LIN:
            if (len > 1u)
            {
                return E_NOT_OK;
            }
            if (len != 0u)
            {
                (void)memcpy(Nm_LinState.userData, data, len);
            }
            Nm_LinState.userDataLen = len;
            if (Nm_LinState.localRequested != FALSE)
            {
                Nm_LinWriteNm3Signal(Nm_LinGetNm3SignalValue());
            }
            return E_OK;

        case COMM_CH_ETH:
            return UdpNm_SetUserData(channel, data, len);

        default:
            return E_NOT_OK;
    }
}

Std_ReturnType Nm_GetUserData(uint8 channel, uint8* data, uint8* len)
{
    uint8 copyLen;

    if ((Nm_Initialized == FALSE) || (data == NULL_PTR) || (len == NULL_PTR))
    {
        return E_NOT_OK;
    }

    switch (channel)
    {
        case COMM_CH_CAN:
        case COMM_CH_CANFD:
            return CanNm_GetUserData(channel, data, len);

        case COMM_CH_LIN:
            if (*len < Nm_LinState.userDataLen)
            {
                return E_NOT_OK;
            }
            copyLen = Nm_LinState.userDataLen;
            if (copyLen != 0u)
            {
                (void)memcpy(data, Nm_LinState.userData, copyLen);
            }
            *len = copyLen;
            return E_OK;

        case COMM_CH_ETH:
            return UdpNm_GetUserData(channel, data, len);

        default:
            return E_NOT_OK;
    }
}
