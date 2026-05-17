#ifndef SYSMGR_H_
#define SYSMGR_H_

#include "Ifx_Types.h"

typedef enum
{
    SYSMGR_INIT = 0U,
    SYSMGR_STARTUP = 1U,
    SYSMGR_RUN = 2U,
    SYSMGR_POSTRUN = 3U,
    SYSMGR_SLEEP = 4U
}SysMgr_EcuState_t;

extern volatile SysMgr_EcuState_t SysMgr_EcuState;
extern volatile uint8 SysMgr_NoBusActivity;
extern volatile uint32 SysMgr_BusActivityCounter;
extern volatile uint32 SysMgr_GoSleepCounter;

extern void SysMgr_MainFunction(void);
extern void SysMgr_NotifyBusActivity(void);

#endif /* SYSMGR_H_ */
