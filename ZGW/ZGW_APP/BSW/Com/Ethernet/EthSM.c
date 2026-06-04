#include "EthSM.h"

#include "EthStack.h"
#include "SoAd.h"
#include "TcpIpH.h"
#include "../ComM/ComM.h"

#define ETHSM_MAX_CHANNELS 1u
#define ETHSM_FIRST_SOCON  0u
#define ETHSM_LAST_SOCON   4u
#define ETHSM_DOIP_UDP_SOCON 0u
#define ETHSM_DOIP_TCP_SOCON 1u

typedef struct
{
    EthSM_ComModeType requestedMode;
    EthSM_ComModeType currentMode;
    uint8 ethStackInitialized;
    uint8 initialized;
} EthSM_ChannelType;

static EthSM_ChannelType EthSM_Channel[ETHSM_MAX_CHANNELS];

static void EthSM_NotifyComM(EthSM_ComModeType mode)
{
    ComM_ModeType commMode;

    if (mode == ETHSM_FULL_COMMUNICATION)
    {
        commMode = COMM_FULL_COMMUNICATION;
    }
    else if (mode == ETHSM_SILENT_COMMUNICATION)
    {
        commMode = COMM_SILENT_COMMUNICATION;
    }
    else
    {
        commMode = COMM_NO_COMMUNICATION;
    }

    ComM_BusSM_ModeIndication(COMM_CH_ETH, commMode);
}

static void EthSM_OpenConfiguredSockets(void)
{
    SoAd_SoConIdType id;

    for (id = ETHSM_FIRST_SOCON; id <= ETHSM_LAST_SOCON; id++)
    {
        if (SoAd_DebugState[id] == SOAD_SOCON_CLOSED)
        {
            (void)SoAd_OpenSoCon(id);
        }
    }
}

static void EthSM_CloseConfiguredSockets(void)
{
    SoAd_SoConIdType id;

    for (id = ETHSM_FIRST_SOCON; id <= ETHSM_LAST_SOCON; id++)
    {
        if ((id == ETHSM_DOIP_UDP_SOCON) || (id == ETHSM_DOIP_TCP_SOCON))
        {
            continue;
        }

        (void)SoAd_CloseSoCon(id);
    }
}

static void EthSM_SetCurrentMode(uint8 channel, EthSM_ComModeType mode)
{
    if (EthSM_Channel[channel].currentMode != mode)
    {
        EthSM_Channel[channel].currentMode = mode;
        EthSM_NotifyComM(mode);
    }
}

static void EthSM_EnsureStackInitialized(uint8 channel)
{
    if (EthSM_Channel[channel].ethStackInitialized == FALSE)
    {
        EthStack_Init();
        EthSM_Channel[channel].ethStackInitialized = TRUE;
    }
}

void EthSM_Init(void)
{
    uint8 channel;

    for (channel = 0u; channel < ETHSM_MAX_CHANNELS; channel++)
    {
        if (EthSM_Channel[channel].initialized == FALSE)
        {
            EthSM_Channel[channel].requestedMode = ETHSM_NO_COMMUNICATION;
            EthSM_Channel[channel].currentMode = ETHSM_NO_COMMUNICATION;
            EthSM_Channel[channel].ethStackInitialized = FALSE;
            EthSM_Channel[channel].initialized = TRUE;
        }
    }
}

Std_ReturnType EthSM_RequestComMode(uint8 channel, EthSM_ComModeType mode)
{
    if ((channel >= ETHSM_MAX_CHANNELS) ||
        (EthSM_Channel[channel].initialized == FALSE))
    {
        return E_NOT_OK;
    }

    if ((mode != ETHSM_NO_COMMUNICATION) &&
        (mode != ETHSM_FULL_COMMUNICATION) &&
        (mode != ETHSM_SILENT_COMMUNICATION))
    {
        return E_NOT_OK;
    }

    EthSM_Channel[channel].requestedMode = mode;
    /* Do not touch lwIP sockets from the caller context. In this target ComM
     * starts on core0, while lwIP creates its core2 mutexes later. The cyclic
     * EthSM_MainFunction performs the actual socket work after core2 bring-up.
     */
    return E_OK;
}

Std_ReturnType EthSM_GetCurrentComMode(uint8 channel, EthSM_ComModeType* mode)
{
    if ((channel >= ETHSM_MAX_CHANNELS) ||
        (mode == NULL_PTR) ||
        (EthSM_Channel[channel].initialized == FALSE))
    {
        return E_NOT_OK;
    }

    *mode = EthSM_Channel[channel].currentMode;
    return E_OK;
}

void EthSM_MainFunction(void)
{
    uint8 channel;

    for (channel = 0u; channel < ETHSM_MAX_CHANNELS; channel++)
    {
        if (EthSM_Channel[channel].initialized == FALSE)
        {
            continue;
        }

        if (EthSM_Channel[channel].requestedMode == ETHSM_NO_COMMUNICATION)
        {
            if (EthSM_Channel[channel].ethStackInitialized != FALSE)
            {
                EthSM_CloseConfiguredSockets();
            }
            EthSM_SetCurrentMode(channel, ETHSM_NO_COMMUNICATION);
            continue;
        }

        EthSM_EnsureStackInitialized(channel);
        EthSM_OpenConfiguredSockets();

        if (TcpIp_IsLinkAvailable() == FALSE)
        {
            EthSM_SetCurrentMode(channel, ETHSM_WAIT_LINK);
            continue;
        }

        if (EthSM_Channel[channel].requestedMode == ETHSM_FULL_COMMUNICATION)
        {
            EthSM_SetCurrentMode(channel, ETHSM_FULL_COMMUNICATION);
        }
        else
        {
            EthSM_SetCurrentMode(channel, ETHSM_SILENT_COMMUNICATION);
        }
    }
}
