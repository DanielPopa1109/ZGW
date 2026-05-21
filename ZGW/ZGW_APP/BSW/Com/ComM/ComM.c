#include "ComM.h"
#include "ComM_Cfg.h"

#include "../Com.h"
#include "../CanSM/CanSM.h"
#include "../Ethernet/EthSM.h"
#include "../Lin/LinSM.h"
#include "../Nm/Nm.h"
#include "../../Sys/SysMgr/SysMgr.h"

#define COMM_INVALID_REQUESTED_MODE 0xFFu

static ComM_ModeType ComM_UserRequestedMode[COMM_NUM_USERS];
static ComM_ModeType ComM_ChannelCurrentMode[COMM_NUM_CHANNELS];
static uint8 ComM_ChannelRequestedMode[COMM_NUM_CHANNELS];
static uint8 ComM_ChannelLimitedNoCom[COMM_NUM_CHANNELS];
static uint8 ComM_NmRemoteRequest[COMM_NUM_CHANNELS];
static uint8 ComM_Initialized;

static uint8 ComM_IsValidChannel(ComM_ChannelType channel)
{
    return (channel < COMM_NUM_CHANNELS) ? TRUE : FALSE;
}

static uint8 ComM_IsValidUser(ComM_UserHandleType user)
{
    return (user < COMM_NUM_USERS) ? TRUE : FALSE;
}

static void ComM_SetChannelCurrentMode(ComM_ChannelType channel,
                                       ComM_ModeType mode)
{
    ComM_ModeType previousMode;

    previousMode = ComM_ChannelCurrentMode[channel];
    ComM_ChannelCurrentMode[channel] = mode;

    if ((previousMode == COMM_NO_COMMUNICATION) &&
        (mode == COMM_FULL_COMMUNICATION))
    {
        Com_TriggerFullComRestartBurst(channel);
    }
}

static ComM_ModeType ComM_CanSMToComM(CanSM_ComModeType mode)
{
    switch (mode)
    {
        case CANSM_COMM_FULL_COMMUNICATION:
            return COMM_FULL_COMMUNICATION;
        case CANSM_COMM_SILENT_COMMUNICATION:
            return COMM_SILENT_COMMUNICATION;
        default:
            return COMM_NO_COMMUNICATION;
    }
}

static ComM_ModeType ComM_LinSMToComM(LinSM_StateType state)
{
    if ((state == LINSM_FULL_COMMUNICATION) || (state == LINSM_WAKEUP))
    {
        return COMM_FULL_COMMUNICATION;
    }

    return COMM_NO_COMMUNICATION;
}

static ComM_ModeType ComM_EthSMToComM(EthSM_ComModeType mode)
{
    switch (mode)
    {
        case ETHSM_FULL_COMMUNICATION:
            return COMM_FULL_COMMUNICATION;
        case ETHSM_SILENT_COMMUNICATION:
            return COMM_SILENT_COMMUNICATION;
        default:
            return COMM_NO_COMMUNICATION;
    }
}

static ComM_ModeType ComM_GetLocalUserAggregateForChannel(ComM_ChannelType channel)
{
    ComM_UserHandleType user;
    ComM_ModeType highest = COMM_NO_COMMUNICATION;
    uint8 channelMask = (uint8)(1u << channel);

    for (user = 0u; user < COMM_NUM_USERS; user++)
    {
        if ((ComM_UserChannelMap[user] & channelMask) != 0u)
        {
            if (ComM_UserRequestedMode[user] > highest)
            {
                highest = ComM_UserRequestedMode[user];
            }
        }
    }

    if (ComM_ChannelLimitedNoCom[channel] != FALSE)
    {
        highest = COMM_NO_COMMUNICATION;
    }

    return highest;
}

static ComM_ModeType ComM_GetUserAggregateForChannel(ComM_ChannelType channel)
{
    ComM_ModeType highest = ComM_GetLocalUserAggregateForChannel(channel);

    if (ComM_NmRemoteRequest[channel] != FALSE)
    {
        highest = COMM_FULL_COMMUNICATION;
    }

    if (ComM_ChannelLimitedNoCom[channel] != FALSE)
    {
        highest = COMM_NO_COMMUNICATION;
    }

    return highest;
}

static void ComM_ServiceNmRequest(ComM_ChannelType channel, ComM_ModeType mode)
{
    if (mode == COMM_FULL_COMMUNICATION)
    {
        if (ComM_GetLocalUserAggregateForChannel(channel) ==
            COMM_FULL_COMMUNICATION)
        {
            (void)Nm_NetworkRequest(channel);
        }
    }
    else
    {
        (void)Nm_NetworkRelease(channel);
    }
}

static Std_ReturnType ComM_ApplyChannelMode(ComM_ChannelType channel,
                                            ComM_ModeType mode)
{
    Std_ReturnType ret = E_NOT_OK;

    switch (channel)
    {
        case COMM_CH_CAN:
            if (mode == COMM_FULL_COMMUNICATION)
            {
                ret = CanSM_RequestComMode(CAN_CONTROLLER_CLASSIC,
                                           CANSM_COMM_FULL_COMMUNICATION);
            }
            else if (mode == COMM_SILENT_COMMUNICATION)
            {
                ret = CanSM_RequestComMode(CAN_CONTROLLER_CLASSIC,
                                           CANSM_COMM_SILENT_COMMUNICATION);
            }
            else
            {
                ret = CanSM_RequestComMode(CAN_CONTROLLER_CLASSIC,
                                           CANSM_COMM_NO_COMMUNICATION);
            }
            break;

        case COMM_CH_CANFD:
            if (mode == COMM_FULL_COMMUNICATION)
            {
                ret = CanSM_RequestComMode(CAN_CONTROLLER_FD,
                                           CANSM_COMM_FULL_COMMUNICATION);
            }
            else if (mode == COMM_SILENT_COMMUNICATION)
            {
                ret = CanSM_RequestComMode(CAN_CONTROLLER_FD,
                                           CANSM_COMM_SILENT_COMMUNICATION);
            }
            else
            {
                ret = CanSM_RequestComMode(CAN_CONTROLLER_FD,
                                           CANSM_COMM_NO_COMMUNICATION);
            }
            break;

        case COMM_CH_LIN:
            if (mode == COMM_FULL_COMMUNICATION)
            {
                ret = LinSM_RequestComMode(0u, LINSM_FULL_COMMUNICATION);
            }
            else if (mode == COMM_SILENT_COMMUNICATION)
            {
                /* LIN has no silent communication state in this lab stack.
                 * Keep the scheduler awake for RX and let ComM_IsTxAllowed()
                 * block upper transmissions.
                 */
                ret = LinSM_RequestComMode(0u, LINSM_FULL_COMMUNICATION);
            }
            else
            {
                ret = LinSM_RequestComMode(0u, LINSM_SLEEP);
                if (ret != E_OK)
                {
                    ret = LinSM_RequestComMode(0u, LINSM_NO_COMMUNICATION);
                }
            }

            if (ret == E_OK)
            {
                ComM_SetChannelCurrentMode(channel, mode);
            }
            break;

        case COMM_CH_ETH:
            if (mode == COMM_FULL_COMMUNICATION)
            {
                ret = EthSM_RequestComMode(0u, ETHSM_FULL_COMMUNICATION);
            }
            else if (mode == COMM_SILENT_COMMUNICATION)
            {
                ret = EthSM_RequestComMode(0u, ETHSM_SILENT_COMMUNICATION);
            }
            else
            {
                ret = EthSM_RequestComMode(0u, ETHSM_NO_COMMUNICATION);
            }
            break;

        default:
            break;
    }

    if (ret == E_OK)
    {
        ComM_ServiceNmRequest(channel, mode);
    }

    return ret;
}

static void ComM_EvaluateChannel(ComM_ChannelType channel)
{
    ComM_ModeType target;

    if (ComM_IsValidChannel(channel) == FALSE)
    {
        return;
    }

    target = ComM_GetUserAggregateForChannel(channel);

    if ((ComM_ChannelRequestedMode[channel] != (uint8)target) ||
        (ComM_ChannelCurrentMode[channel] != target))
    {
        if (ComM_ApplyChannelMode(channel, target) == E_OK)
        {
            ComM_ChannelRequestedMode[channel] = (uint8)target;
        }
    }
    else
    {
        ComM_ServiceNmRequest(channel, target);
    }
}

static void ComM_EvaluateMappedChannels(uint8 channelMask)
{
    ComM_ChannelType channel;

    for (channel = 0u; channel < COMM_NUM_CHANNELS; channel++)
    {
        if ((channelMask & (uint8)(1u << channel)) != 0u)
        {
            ComM_EvaluateChannel(channel);
        }
    }
}

static void ComM_UpdateGatewayRequestFromBusActivity(void)
{
    ComM_ModeType requestedMode;

    requestedMode = (SysMgr_BusActivityCounter > 0u) ?
            COMM_FULL_COMMUNICATION :
            COMM_NO_COMMUNICATION;

    ComM_UserRequestedMode[COMM_USER_GATEWAY] = requestedMode;
}

void ComM_Init(void)
{
    ComM_UserHandleType user;
    ComM_ChannelType channel;
    CanSM_ComModeType canSmMode;
    EthSM_ComModeType ethSmMode;

    for (user = 0u; user < COMM_NUM_USERS; user++)
    {
        ComM_UserRequestedMode[user] = COMM_NO_COMMUNICATION;
    }

    ComM_UserRequestedMode[COMM_USER_GATEWAY] = COMM_FULL_COMMUNICATION;

    for (channel = 0u; channel < COMM_NUM_CHANNELS; channel++)
    {
        ComM_ChannelCurrentMode[channel] = COMM_NO_COMMUNICATION;
        ComM_ChannelRequestedMode[channel] = COMM_INVALID_REQUESTED_MODE;
        ComM_ChannelLimitedNoCom[channel] = FALSE;
        ComM_NmRemoteRequest[channel] = FALSE;
    }

    if (CanSM_GetCurrentComMode(CAN_CONTROLLER_CLASSIC, &canSmMode) == E_OK)
    {
        ComM_ChannelCurrentMode[COMM_CH_CAN] = ComM_CanSMToComM(canSmMode);
    }

    if (CanSM_GetCurrentComMode(CAN_CONTROLLER_FD, &canSmMode) == E_OK)
    {
        ComM_ChannelCurrentMode[COMM_CH_CANFD] = ComM_CanSMToComM(canSmMode);
    }

    ComM_ChannelCurrentMode[COMM_CH_LIN] = ComM_LinSMToComM(LinSM_GetState(0u));

    EthSM_Init();
    if (EthSM_GetCurrentComMode(0u, &ethSmMode) == E_OK)
    {
        ComM_ChannelCurrentMode[COMM_CH_ETH] = ComM_EthSMToComM(ethSmMode);
    }

    ComM_Initialized = TRUE;
    ComM_EvaluateMappedChannels(ComM_UserChannelMap[COMM_USER_GATEWAY]);
}

void ComM_MainFunction(void)
{
    ComM_ChannelType channel;

    if (ComM_Initialized == FALSE)
    {
        return;
    }

    ComM_UpdateGatewayRequestFromBusActivity();

    for (channel = 0u; channel < COMM_NUM_CHANNELS; channel++)
    {
        ComM_EvaluateChannel(channel);
    }
}

Std_ReturnType ComM_RequestComMode(ComM_UserHandleType user, ComM_ModeType mode)
{
    if ((ComM_Initialized == FALSE) ||
        (ComM_IsValidUser(user) == FALSE) ||
        (mode > COMM_FULL_COMMUNICATION))
    {
        return E_NOT_OK;
    }

    ComM_UserRequestedMode[user] = mode;
    ComM_EvaluateMappedChannels(ComM_UserChannelMap[user]);
    return E_OK;
}

Std_ReturnType ComM_GetRequestedComMode(ComM_UserHandleType user,
                                        ComM_ModeType* mode)
{
    if ((ComM_Initialized == FALSE) ||
        (ComM_IsValidUser(user) == FALSE) ||
        (mode == NULL_PTR))
    {
        return E_NOT_OK;
    }

    *mode = ComM_UserRequestedMode[user];
    return E_OK;
}

Std_ReturnType ComM_GetCurrentComMode(ComM_ChannelType channel,
                                      ComM_ModeType* mode)
{
    if ((ComM_Initialized == FALSE) ||
        (ComM_IsValidChannel(channel) == FALSE) ||
        (mode == NULL_PTR))
    {
        return E_NOT_OK;
    }

    *mode = ComM_ChannelCurrentMode[channel];
    return E_OK;
}

Std_ReturnType ComM_GetMaxComMode(ComM_UserHandleType user, ComM_ModeType* mode)
{
    ComM_ChannelType channel;
    uint8 channelMask;
    uint8 mapped = FALSE;
    uint8 limitedCount = 0u;
    uint8 mappedCount = 0u;

    if ((ComM_Initialized == FALSE) ||
        (ComM_IsValidUser(user) == FALSE) ||
        (mode == NULL_PTR))
    {
        return E_NOT_OK;
    }

    channelMask = ComM_UserChannelMap[user];

    for (channel = 0u; channel < COMM_NUM_CHANNELS; channel++)
    {
        if ((channelMask & (uint8)(1u << channel)) != 0u)
        {
            mapped = TRUE;
            mappedCount++;
            if (ComM_ChannelLimitedNoCom[channel] != FALSE)
            {
                limitedCount++;
            }
        }
    }

    *mode = ((mapped == FALSE) || (limitedCount == mappedCount)) ?
            COMM_NO_COMMUNICATION :
            COMM_FULL_COMMUNICATION;
    return E_OK;
}

Std_ReturnType ComM_LimitChannelToNoComMode(ComM_ChannelType channel,
                                            uint8 status)
{
    if ((ComM_Initialized == FALSE) ||
        (ComM_IsValidChannel(channel) == FALSE))
    {
        return E_NOT_OK;
    }

    ComM_ChannelLimitedNoCom[channel] = (status != FALSE) ? TRUE : FALSE;
    ComM_EvaluateChannel(channel);
    return E_OK;
}

uint8 ComM_IsTxAllowed(ComM_ChannelType channel)
{
    if (ComM_Initialized == FALSE)
    {
        return TRUE;
    }

    if (ComM_IsValidChannel(channel) == FALSE)
    {
        return FALSE;
    }

    return (ComM_ChannelCurrentMode[channel] == COMM_FULL_COMMUNICATION) ?
           TRUE :
           FALSE;
}

uint8 ComM_IsRxAllowed(ComM_ChannelType channel)
{
    if (ComM_Initialized == FALSE)
    {
        return TRUE;
    }

    if (ComM_IsValidChannel(channel) == FALSE)
    {
        return FALSE;
    }

    return (ComM_ChannelCurrentMode[channel] != COMM_NO_COMMUNICATION) ?
           TRUE :
           FALSE;
}

void ComM_BusSM_ModeIndication(ComM_ChannelType channel, ComM_ModeType mode)
{
    if ((ComM_Initialized == FALSE) ||
        (ComM_IsValidChannel(channel) == FALSE) ||
        (mode > COMM_FULL_COMMUNICATION))
    {
        return;
    }

    ComM_SetChannelCurrentMode(channel, mode);
}

void ComM_Nm_NetworkStartIndication(ComM_ChannelType channel)
{
    if ((ComM_Initialized == FALSE) ||
        (ComM_IsValidChannel(channel) == FALSE))
    {
        return;
    }

    ComM_NmRemoteRequest[channel] = TRUE;
    ComM_EvaluateChannel(channel);
}

void ComM_Nm_NetworkMode(ComM_ChannelType channel)
{
    if ((ComM_Initialized == FALSE) ||
        (ComM_IsValidChannel(channel) == FALSE))
    {
        return;
    }

    ComM_SetChannelCurrentMode(channel, COMM_FULL_COMMUNICATION);
}

void ComM_Nm_PrepareBusSleepMode(ComM_ChannelType channel)
{
    if ((ComM_Initialized == FALSE) ||
        (ComM_IsValidChannel(channel) == FALSE))
    {
        return;
    }

    if (ComM_GetUserAggregateForChannel(channel) != COMM_FULL_COMMUNICATION)
    {
        ComM_ChannelCurrentMode[channel] = COMM_SILENT_COMMUNICATION;
    }
}

void ComM_Nm_BusSleepMode(ComM_ChannelType channel)
{
    if ((ComM_Initialized == FALSE) ||
        (ComM_IsValidChannel(channel) == FALSE))
    {
        return;
    }

    ComM_NmRemoteRequest[channel] = FALSE;
    ComM_ChannelCurrentMode[channel] = COMM_NO_COMMUNICATION;
    ComM_EvaluateChannel(channel);
}

void CanSM_BusOffBeginNotification(uint8 ControllerId)
{
    if (ControllerId == CAN_CONTROLLER_CLASSIC)
    {
        ComM_BusSM_ModeIndication(COMM_CH_CAN, COMM_NO_COMMUNICATION);
    }
    else if (ControllerId == CAN_CONTROLLER_FD)
    {
        ComM_BusSM_ModeIndication(COMM_CH_CANFD, COMM_NO_COMMUNICATION);
    }
    else
    {
        /* Ignore unknown lab controllers. */
    }
}

void CanSM_ModeChangeNotification(uint8 ControllerId, CanSM_ComModeType Mode)
{
    if (ControllerId == CAN_CONTROLLER_CLASSIC)
    {
        ComM_BusSM_ModeIndication(COMM_CH_CAN, ComM_CanSMToComM(Mode));
    }
    else if (ControllerId == CAN_CONTROLLER_FD)
    {
        ComM_BusSM_ModeIndication(COMM_CH_CANFD, ComM_CanSMToComM(Mode));
    }
    else
    {
        /* Ignore unknown lab controllers. */
    }
}
