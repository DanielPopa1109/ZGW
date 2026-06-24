#include "Dcm_DoIP_Adapter.h"
#include "Dcm_Cfg.h"
#include "PduR.h"

void Dcm_DoIP_RxIndication(uint16_t sourceAddress,
                           uint16_t targetAddress,
                           const uint8_t *uds,
                           uint16_t udsLen)
{
    (void)PduR_DoIPRxIndication((uint16)sourceAddress,
                                (uint16)targetAddress,
                                uds,
                                (uint16)udsLen);
}

void Dcm_DoIP_TxConfirmation(const uint8_t *udsResponse,
                             uint16_t udsResponseLen)
{
    (void)PduR_DcmTransmit(DCM_TX_ETH_PHYS,
                           udsResponse,
                           (PduLengthType)udsResponseLen);
}
