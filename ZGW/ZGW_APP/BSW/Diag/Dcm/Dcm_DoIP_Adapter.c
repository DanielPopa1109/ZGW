/* Dcm_DoIP_Adapter.c */
#include "DoIP.h"
#include "Dcm.h"

static uint16_t DcmDoIP_SourceAddress;
static uint16_t DcmDoIP_TargetAddress;

void Dcm_DoIP_RxIndication(uint16_t sourceAddress,
                           uint16_t targetAddress,
                           const uint8_t *uds,
                           uint16_t udsLen)
{
    DcmDoIP_SourceAddress = sourceAddress;
    DcmDoIP_TargetAddress = targetAddress;

    //Dcm_ProvideRxBufferFromDoIP(uds, udsLen); todo
}

void Dcm_DoIP_TxConfirmation(const uint8_t *udsResponse, uint16_t udsResponseLen)
{
    (void)DoIP_SendDiagnosticResponse(DcmDoIP_TargetAddress,
                                      DcmDoIP_SourceAddress,
                                      udsResponse,
                                      udsResponseLen);
}
