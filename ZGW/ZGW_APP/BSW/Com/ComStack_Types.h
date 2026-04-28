#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H
#include "Std_Types.h"
typedef uint16 PduIdType;
typedef uint16 PduLengthType;
typedef struct { uint8* SduDataPtr; PduLengthType SduLength; } PduInfoType;
typedef enum { BUFREQ_OK=0u, BUFREQ_E_NOT_OK=1u, BUFREQ_E_BUSY=2u, BUFREQ_E_OVFL=3u } BufReq_ReturnType;
typedef enum { NTFRSLT_OK=0u, NTFRSLT_E_NOT_OK=1u, NTFRSLT_E_TIMEOUT_A=2u, NTFRSLT_E_TIMEOUT_BS=3u, NTFRSLT_E_TIMEOUT_CR=4u, NTFRSLT_E_WRONG_SN=5u, NTFRSLT_E_NO_BUFFER=6u, NTFRSLT_E_CANCELATION_OK=7u } NotifResultType;
#endif
