/* Dcm_EthTpBridge.c */
#include "Dcm_EthTpBridge.h"
#include "DoIP.h"
#include <string.h>

#define DCM_ETHTP_RX_LEN 4095u

static uint8_t DcmEth_RxBuf[DCM_ETHTP_RX_LEN];
static uint16_t DcmEth_RxLen;
static uint16_t DcmEth_SourceAddress;
static uint16_t DcmEth_TargetAddress;
static volatile uint8_t DcmEth_RxPending;

void Dcm_EthTp_Init(void)
{
    DcmEth_RxLen = 0u;
    DcmEth_SourceAddress = 0u;
    DcmEth_TargetAddress = 0u;
    DcmEth_RxPending = 0u;
}

void Dcm_EthTp_RxIndication(uint16_t sourceAddress,
                            uint16_t targetAddress,
                            const uint8_t *data,
                            uint16_t len)
{
    if ((data == 0) || (len > DCM_ETHTP_RX_LEN))
    {
        return;
    }

    memcpy(DcmEth_RxBuf, data, len);

    DcmEth_RxLen = len;
    DcmEth_SourceAddress = sourceAddress;
    DcmEth_TargetAddress = targetAddress;
    DcmEth_RxPending = 1u;
}

uint8_t Dcm_EthTp_GetRx(uint16_t *sourceAddress,
                        uint16_t *targetAddress,
                        uint8_t *data,
                        uint16_t *len,
                        uint16_t maxLen)
{
    if ((sourceAddress == 0) || (targetAddress == 0) || (data == 0) || (len == 0))
    {
        return 0u;
    }

    if (DcmEth_RxPending == 0u)
    {
        return 0u;
    }

    if (DcmEth_RxLen > maxLen)
    {
        return 0u;
    }

    memcpy(data, DcmEth_RxBuf, DcmEth_RxLen);

    *sourceAddress = DcmEth_SourceAddress;
    *targetAddress = DcmEth_TargetAddress;
    *len = DcmEth_RxLen;

    DcmEth_RxPending = 0u;

    return 1u;
}

void Dcm_EthTp_SendResponse(const uint8_t *data, uint16_t len)
{
    (void)DoIP_SendDiagnosticResponse(DcmEth_TargetAddress,
                                      DcmEth_SourceAddress,
                                      data,
                                      len);
}
