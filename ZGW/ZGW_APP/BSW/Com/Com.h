#ifndef COM_H
#define COM_H

#include "ComStack_Types.h"

#define COM_MAX_IPDU_LEN      64u
#define COM_MAIN_PERIOD_MS    5u

#define COM_SIGNAL_U8         1u
#define COM_SIGNAL_U16        2u
#define COM_SIGNAL_U32        4u

#define COM_TX_MODE_NONE      0u
#define COM_TX_MODE_DIRECT    1u
#define COM_TX_MODE_PERIODIC  2u
#define COM_TX_MODE_MIXED     3u

#define COM_IPDU_GROUP_0      0u
#define COM_IPDU_GROUP_1      1u

typedef uint16 Com_SignalIdType;
typedef uint16 Com_IpduGroupIdType;

void Com_Init(void);
void Com_MainFunctionTx(void);
void Com_MainFunctionRx(void);

Std_ReturnType Com_IpduGroupStart(Com_IpduGroupIdType groupId);
Std_ReturnType Com_IpduGroupStop(Com_IpduGroupIdType groupId);

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr);
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr);
Std_ReturnType Com_InvalidateSignal(Com_SignalIdType SignalId);

void Com_RxIndication(PduIdType RxPduId, const uint8* data, PduLengthType len);
void Com_TxConfirmation(PduIdType TxPduId);

#endif
