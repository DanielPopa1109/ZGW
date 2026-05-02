#ifndef PDUR_H
#define PDUR_H

#include "ComStack_Types.h"

#define PDUR_MAX_TP_BUFFER      4095u
#define PDUR_MAX_CANTP_ROUTES        4u
#define PDUR_MAX_LINTP_ROUTES        1u

void PduR_Init(void);

Std_ReturnType PduR_ComTransmit(PduIdType TxPduId, const uint8* data, PduLengthType len);
Std_ReturnType PduR_DcmTransmit(PduIdType DcmTxPduId, const uint8* data, PduLengthType len);

Std_ReturnType PduR_CanTpStartOfReception(PduIdType CanTpRxPduId, PduLengthType len);
Std_ReturnType PduR_CanTpCopyRxData(PduIdType CanTpRxPduId, const uint8* data, PduLengthType len);
void PduR_CanTpRxIndication(PduIdType CanTpRxPduId, Std_ReturnType result);
void PduR_CanTpTxConfirmation(PduIdType CanTpTxPduId, Std_ReturnType result);

Std_ReturnType PduR_LinTpStartOfReception(PduIdType LinTpRxPduId, PduLengthType len);
Std_ReturnType PduR_LinTpCopyRxData(PduIdType LinTpRxPduId, const uint8* data, PduLengthType len);
void PduR_LinTpRxIndication(PduIdType LinTpRxPduId, Std_ReturnType result);
void PduR_LinTpTxConfirmation(PduIdType LinTpTxPduId, Std_ReturnType result);

#endif
