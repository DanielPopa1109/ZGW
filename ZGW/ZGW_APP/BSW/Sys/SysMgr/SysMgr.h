#ifndef SYSMGR_H_
#define SYSMGR_H_

#include "Ifx_Types.h"
#include "Dem_Types.h"

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
extern void SysMgr_CaptureScrFaultBeforeScrReset(void);
extern Std_ReturnType SysMgr_CaptureMcuSmFreezeFrame(Dem_EventIdType eventId, uint8 *buffer, uint16 *length);
extern Std_ReturnType SysMgr_CaptureMcuSmExtendedData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length);

#endif /* SYSMGR_H_ */
