#include "TcpIpH.h"
#include "SoAd.h"
#include "DoIP.h"
#include "SomeIp.h"
#include "SomeIpSd.h"
#include "EthernetDiag.h"
#include "Dcm_EthTpBridge.h"
#include "PduR.h"
#include "BSW/Com/Ethernet/EthStartupTiming.h"

#include "FreeRTOS_core2.h"
#include "task_core2.h"

extern const SoAd_ConfigType SoAd_Config;
extern const DoIP_ConfigType DoIP_Config;
extern const SomeIp_ConfigType SomeIp_Config;
extern const SomeIpSd_ConfigType SomeIpSd_Config;

static uint8 EthStack_Initialized = 0u;

void EthStack_Init(void)
{
    if (EthStack_Initialized != 0u)
    {
        /* A caller attempting to initialize an already-running stack marks a
         * runtime reinitialization boundary for the boot timing recorder. */
        EthStartupTiming_NotifyReinitialization();
        return;
    }

    EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_ETH_STARTUP_ENTER);

    TcpIp_Init();
    EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_TCPIP_READY);
    EthernetDiag_Init();

    SoAd_Init(&SoAd_Config);
    EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_SOAD_INIT_COMPLETE);

    Dcm_EthTp_Init();

    DoIP_Init(&DoIP_Config);
    DoIP_SetDcmRxIndication(PduR_DoIPRxIndication);
    DoIP_SetSessionResetIndication(PduR_DoIPResetSession);
    EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_DOIP_INIT_COMPLETE);

    SomeIp_Init(&SomeIp_Config);
    SomeIpSd_Init(&SomeIpSd_Config);

    EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_SOCKET_OPEN_REQUEST);
    (void)SoAd_OpenSoCon(0u);
    (void)SoAd_OpenSoCon(1u);
    (void)SoAd_OpenSoCon(2u);
    (void)SoAd_OpenSoCon(3u);
    (void)SoAd_OpenSoCon(4u);
    EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_SOCKET_READY);
    EthStartupTiming_Capture(ETHSTARTUPTIMING_EVENT_ETH_STARTUP_COMPLETE);

    EthStack_Initialized = 1u;
}
