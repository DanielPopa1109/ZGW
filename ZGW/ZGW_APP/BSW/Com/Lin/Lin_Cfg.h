#ifndef LIN_CFG_H_
#define LIN_CFG_H_

#include "IfxAsclin_Lin.h"
#include "IfxAsclin_PinMap.h"
#include "IfxPort.h"

#define LIN_ASCLIN_MODULE       MODULE_ASCLIN1
#define LIN_DEFAULT_BAUDRATE    19200.0f

#define LIN_PINS                Lin_Pins

extern const IfxAsclin_Lin_Pins Lin_Pins;

#define LIN_TX_PORT             (&MODULE_P15)
#define LIN_TX_PIN_INDEX        4u
#define LIN_WAKEUP_DELAY_TICKS  4000u
#define LIN_LDF_TIME_BASE_MS     10u
#define LIN_LDF_JITTER_MS        5u

#endif
