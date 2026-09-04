#ifndef LINTP_H
#define LINTP_H

#include "ComStack_Types.h"

#define LINTP_NAD_BROADCAST       0x7Fu
#define LINTP_NAD_FUNCTIONAL      0x7Eu
#define LINTP_MAX_PAYLOAD         256u

#define LINTP_PCI_SF              0x00u
#define LINTP_PCI_FF              0x10u
#define LINTP_PCI_CF              0x20u

#define LINTP_PDUR_MASTER_REQ_ID  4u
#define LINTP_PDUR_SLAVE_RESP_ID  0xFFFFu
#define LINTP_PDUR_ID             LINTP_PDUR_MASTER_REQ_ID

#define LINTP_NAD_ZGW_DEFAULT     1u

void LinTp_Init(uint8 configuredNad);
void LinTp_MainFunction(void);

Std_ReturnType LinTp_Transmit(PduIdType TxPduId, const uint8* data, PduLengthType len);
Std_ReturnType LinTp_TransmitToNad(PduIdType TxPduId, uint8 targetNad, const uint8* data, PduLengthType len);
uint8 LinTp_CanStartTransmitToNadNow(uint8 targetNad);
void LinTp_TxFrameConfirmation(uint8 success);

void LinTp_RxSlaveResponse(const uint8 frame[8]);

uint8 LinTp_IsConfiguredNad(uint8 nad);

#endif
