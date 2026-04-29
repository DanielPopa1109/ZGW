/* Dcm_EthTpBridge.h */
#ifndef DCM_ETHTPBRIDGE_H_
#define DCM_ETHTPBRIDGE_H_

#include <stdint.h>

typedef enum
{
    DCM_ETH_TRANSPORT_NONE = 0,
    DCM_ETH_TRANSPORT_DOIP
} Dcm_EthTransportType;

void Dcm_EthTp_Init(void);

void Dcm_EthTp_RxIndication(uint16_t sourceAddress,
                            uint16_t targetAddress,
                            const uint8_t *data,
                            uint16_t len);

void Dcm_EthTp_SendResponse(const uint8_t *data, uint16_t len);

uint8_t Dcm_EthTp_GetRx(uint16_t *sourceAddress,
                        uint16_t *targetAddress,
                        uint8_t *data,
                        uint16_t *len,
                        uint16_t maxLen);

#endif
