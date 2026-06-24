#ifndef WDG_CFG_H_
#define WDG_CFG_H_

#include "IfxScuWdt.h"

#define WDG_TIMEOUT_MS                 (1000u)
#define WDG_INPUT_CLOCK_HZ             (100000000u)
#define WDG_INPUT_CLOCK_KHZ            (WDG_INPUT_CLOCK_HZ / 1000u)
#define WDG_INPUT_DIVIDER              (16384u)
#define WDG_INPUT_FREQUENCY            (IfxScu_WDTCON1_IR_divBy16384)
#define WDG_TIMEOUT_TICKS              (((WDG_INPUT_CLOCK_KHZ * WDG_TIMEOUT_MS) + \
                                          (WDG_INPUT_DIVIDER - 1u)) / WDG_INPUT_DIVIDER)
#define WDG_RELOAD_VALUE               ((uint16)(0x10000u - WDG_TIMEOUT_TICKS))

#endif /* WDG_CFG_H_ */
