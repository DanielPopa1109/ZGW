#ifndef DCM_DOIP_ADAPTER_H_
#define DCM_DOIP_ADAPTER_H_

#include <stdint.h>

void Dcm_DoIP_RxIndication(uint16_t sourceAddress,
                           uint16_t targetAddress,
                           const uint8_t *uds,
                           uint16_t udsLen);

void Dcm_DoIP_TxConfirmation(const uint8_t *udsResponse,
                             uint16_t udsResponseLen);

#endif
