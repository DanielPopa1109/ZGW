#include "Dcm_EthTpBridge.h"
#include "DoIP.h"
#include <string.h>

typedef struct
{
    uint8_t rxBuf[DCM_ETHTP_MAX_PAYLOAD_LEN];
    uint16_t rxLen;
    uint16_t sourceAddress;
    uint16_t targetAddress;
    volatile uint8_t rxPending;
    volatile uint8_t rxOverflow;
} Dcm_EthTpRuntimeType;

static Dcm_EthTpRuntimeType DcmEth;

void Dcm_EthTp_Init(void)
{
    memset(&DcmEth, 0, sizeof(DcmEth));
}

void Dcm_EthTp_RxIndication(uint16_t sourceAddress,
                            uint16_t targetAddress,
                            const uint8_t *data,
                            uint16_t len)
{
    if ((data == 0) || (len == 0u) || (len > DCM_ETHTP_MAX_PAYLOAD_LEN))
    {
        return;
    }

    if (DcmEth.rxPending != 0u)
    {
        DcmEth.rxOverflow = 1u;
        return;
    }

    memcpy(DcmEth.rxBuf, data, len);

    DcmEth.rxLen = len;
    DcmEth.sourceAddress = sourceAddress;
    DcmEth.targetAddress = targetAddress;
    DcmEth.rxPending = 1u;
}

uint8_t Dcm_EthTp_GetRx(uint16_t *sourceAddress,
                        uint16_t *targetAddress,
                        uint8_t *data,
                        uint16_t *len,
                        uint16_t maxLen)
{
    if ((sourceAddress == 0) ||
        (targetAddress == 0) ||
        (data == 0) ||
        (len == 0))
    {
        return 0u;
    }

    if (DcmEth.rxPending == 0u)
    {
        return 0u;
    }

    if (DcmEth.rxLen > maxLen)
    {
        return 0u;
    }

    memcpy(data, DcmEth.rxBuf, DcmEth.rxLen);

    *sourceAddress = DcmEth.sourceAddress;
    *targetAddress = DcmEth.targetAddress;
    *len = DcmEth.rxLen;

    DcmEth.rxPending = 0u;

    return 1u;
}

void Dcm_EthTp_SendResponse(const uint8_t *data, uint16_t len)
{
    if ((data == 0) || (len == 0u))
    {
        return;
    }

    (void)DoIP_SendDiagnosticResponse(DcmEth.targetAddress,
                                      DcmEth.sourceAddress,
                                      data,
                                      len);
}

uint8_t Dcm_EthTp_HasPendingRx(void)
{
    return DcmEth.rxPending;
}
