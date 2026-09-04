#ifndef CANNM_H
#define CANNM_H

#include "../Nm/Nm.h"

void CanNm_Init(void);
void CanNm_MainFunction(void);

Std_ReturnType CanNm_NetworkRequest(uint8 channel);
Std_ReturnType CanNm_NetworkRelease(uint8 channel);
Std_ReturnType CanNm_GetState(uint8 channel, Nm_StateType* state, Nm_ModeType* mode);

void CanNm_RxIndication(PduIdType rxPduId, const uint8* data, PduLengthType len);
void CanNm_TxConfirmation(PduIdType txPduId);

#endif
