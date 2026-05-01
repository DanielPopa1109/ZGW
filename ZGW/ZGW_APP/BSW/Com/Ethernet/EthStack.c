#include "TcpIpH.h"
#include "SoAd.h"
#include "DoIP.h"
#include "SomeIp.h"
#include "SomeIpSd.h"
#include "Dcm_EthTpBridge.h"

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
        return;
    }

    TcpIp_Init();

    SoAd_Init(&SoAd_Config);

    Dcm_EthTp_Init();

    DoIP_Init(&DoIP_Config);
    DoIP_SetDcmRxIndication(Dcm_EthTp_RxIndication);

    SomeIp_Init(&SomeIp_Config);
    SomeIpSd_Init(&SomeIpSd_Config);

    (void)SoAd_OpenSoCon(0u);
    (void)SoAd_OpenSoCon(1u);
    (void)SoAd_OpenSoCon(2u);
    (void)SoAd_OpenSoCon(3u);

    EthStack_Initialized = 1u;
}
