#ifndef LWIP_GETH_PRIVATE_PHY_DP83825I_H
#define LWIP_GETH_PRIVATE_PHY_DP83825I_H

#include "lwip_geth_conf.h"

#if (PHY_DEVICE_NAME == PHY_DP83825I)

#include "IfxGeth_Eth.h"

typedef enum
{
    LWIP_GETH_PHY_DP83825I_STATE_UNINIT = 0,
    LWIP_GETH_PHY_DP83825I_STATE_RESET,
    LWIP_GETH_PHY_DP83825I_STATE_WAIT_RESET_DONE,
    LWIP_GETH_PHY_DP83825I_STATE_CONFIGURE,
    LWIP_GETH_PHY_DP83825I_STATE_WAIT_AUTONEG,
    LWIP_GETH_PHY_DP83825I_STATE_LINK_DOWN,
    LWIP_GETH_PHY_DP83825I_STATE_LINK_UP,
    LWIP_GETH_PHY_DP83825I_STATE_ERROR
} lwip_geth_PhyDp83825i_StateType;

typedef struct
{
    lwip_geth_PhyDp83825i_StateType state;
    uint32 initDone;
    uint32 linkUp;
    uint32 autonegDone;
    uint32 speed100;
    uint32 fullDuplex;
    uint32 resetTimeoutCnt;
    uint32 autonegTimeoutCnt;
    uint32 mdioErrorCnt;
    uint32 linkDownCnt;
    uint32 linkUpCnt;
} lwip_geth_PhyDp83825i_StatusType;

IFX_EXTERN void lwip_geth_private_Phy_Dp83825i_reset(void);
IFX_EXTERN uint32 lwip_geth_private_Phy_Dp83825i_init(void);
IFX_EXTERN void lwip_geth_private_Phy_Dp83825i_mainFunction_100ms(void);
IFX_EXTERN uint32 lwip_geth_private_Phy_Dp83825i_link_status(void);
IFX_EXTERN uint32 lwip_geth_private_Phy_Dp83825i_is_link_up(void);
IFX_EXTERN const lwip_geth_PhyDp83825i_StatusType *lwip_geth_private_Phy_Dp83825i_getStatus(void);

IFX_EXTERN uint32 lwip_geth_private_Phy_Dp83825i_read_mdio_reg(uint32 layeraddr, uint32 regaddr, uint32 *pdata);
IFX_EXTERN uint32 lwip_geth_private_Phy_Dp83825i_write_mdio_reg(uint32 layeraddr, uint32 regaddr, uint32 data);

IFX_EXTERN volatile uint32 Dp83825i_DebugPhyScanAddr;
IFX_EXTERN volatile uint32 Dp83825i_DebugPhyAddrFound;
IFX_EXTERN volatile uint32 Dp83825i_DebugPhyId1;
IFX_EXTERN volatile uint32 Dp83825i_DebugPhyId2;
IFX_EXTERN volatile uint32 Dp83825i_DebugBmcr;
IFX_EXTERN volatile uint32 Dp83825i_DebugBmsr;
IFX_EXTERN volatile uint32 Dp83825i_DebugPhysts;
IFX_EXTERN volatile uint32 Dp83825i_DebugReadOkCount;
IFX_EXTERN volatile uint32 Dp83825i_DebugReadFailCount;
IFX_EXTERN volatile uint32 Dp83825i_DebugInitFailCount;
IFX_EXTERN volatile uint32 Dp83825i_DebugResetWaitFailCount;
IFX_EXTERN volatile uint32 Dp83825i_DebugConfigureFailCount;

#endif

#endif
