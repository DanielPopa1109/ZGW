#include "Dcm.h"
#include "Dcm_Cfg.h"

const Dcm_ConnectionConfigType Dcm_DefaultConnections[] =
{
    {
        DCM_RX_CAN_PHYS,
        DCM_TX_CAN_PHYS,
        0x710u,
        0x711u,
        DCM_ADDR_PHYSICAL,
        DCM_BUS_CAN_CLASSIC
    },
    {
        DCM_RX_CANFD_PHYS,
        DCM_TX_CANFD_PHYS,
        0x710u,
        0x711u,
        DCM_ADDR_PHYSICAL,
        DCM_BUS_CAN_FD
    },
    {
        DCM_RX_CAN_EXT_PHYS,
        DCM_TX_CAN_EXT_PHYS,
        0x710u,
        0x711u,
        DCM_ADDR_PHYSICAL,
        DCM_BUS_CAN_CLASSIC
    },
    {
        DCM_RX_CANFD_EXT_PHYS,
        DCM_TX_CANFD_EXT_PHYS,
        0x710u,
        0x711u,
        DCM_ADDR_PHYSICAL,
        DCM_BUS_CAN_FD
    },
    {
        DCM_RX_LIN_PHYS,
        DCM_TX_LIN_PHYS,
        0x712u,
        0x71Au,
        DCM_ADDR_PHYSICAL,
        DCM_BUS_LIN
    },
    {
        DCM_RX_ETH_PHYS,
        DCM_TX_ETH_PHYS,
        0x710u,
        0x1001u,
        DCM_ADDR_PHYSICAL,
        DCM_BUS_ETHERNET
    }
};

const Dcm_ConfigType Dcm_Config =
{
    Dcm_DefaultConnections,
    (uint8)(sizeof(Dcm_DefaultConnections) / sizeof(Dcm_DefaultConnections[0])),
    Dcm_DefaultServices,
    (uint8)DCM_DEFAULT_SERVICE_COUNT
};
