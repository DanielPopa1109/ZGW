#ifndef DCM_CFG_H_
#define DCM_CFG_H_

/* Number of entries in Dcm_DefaultServices[] (Dcm.c). Keep in sync. */
#define DCM_DEFAULT_SERVICE_COUNT    13u

#define DCM_RX_CAN_PHYS          0u
#define DCM_RX_CANFD_PHYS        2u
#define DCM_RX_LIN_PHYS          4u
#define DCM_RX_ETH_PHYS          5u
#define DCM_RX_CAN_EXT_PHYS      6u
#define DCM_RX_CAN_EXT_PHYS_2    7u
#define DCM_RX_CANFD_EXT_PHYS    8u
#define DCM_RX_CANFD_EXT_PHYS_2  9u
#define DCM_RX_CAN_EXT_PHYS_3    10u
#define DCM_RX_CAN_EXT_PHYS_4    11u
#define DCM_RX_CANFD_EXT_PHYS_3  12u
#define DCM_RX_CANFD_EXT_PHYS_4  13u

#define DCM_TX_CAN_PHYS          0u
#define DCM_TX_CANFD_PHYS        2u
#define DCM_TX_LIN_PHYS          4u
#define DCM_TX_ETH_PHYS          5u
#define DCM_TX_CAN_EXT_PHYS      6u
#define DCM_TX_CAN_EXT_PHYS_2    7u
#define DCM_TX_CANFD_EXT_PHYS    8u
#define DCM_TX_CANFD_EXT_PHYS_2  9u
#define DCM_TX_CAN_EXT_PHYS_3    10u
#define DCM_TX_CAN_EXT_PHYS_4    11u
#define DCM_TX_CANFD_EXT_PHYS_3  12u
#define DCM_TX_CANFD_EXT_PHYS_4  13u

#define DCM_EXT_ADDR_ZGW         0x41u
#define DCM_EXT_ADDR_TESTER      0x41u

#endif
