#ifndef CANNM_H
#define CANNM_H

#include "../Nm/Nm.h"

#define CANNM_USER_DATA_LEN 1u

void CanNm_Init(void);
void CanNm_MainFunction(void);

Std_ReturnType CanNm_NetworkRequest(uint8 channel);
Std_ReturnType CanNm_NetworkRelease(uint8 channel);
Std_ReturnType CanNm_GetState(uint8 channel, Nm_StateType* state, Nm_ModeType* mode);
Std_ReturnType CanNm_PassiveStartUp(uint8 channel);
Std_ReturnType CanNm_SetUserData(uint8 channel, const uint8* data, uint8 len);
Std_ReturnType CanNm_GetUserData(uint8 channel, uint8* data, uint8* len);

void CanNm_RxIndication(PduIdType rxPduId, const uint8* data, PduLengthType len);
void CanNm_TxConfirmation(PduIdType txPduId);

#endif
