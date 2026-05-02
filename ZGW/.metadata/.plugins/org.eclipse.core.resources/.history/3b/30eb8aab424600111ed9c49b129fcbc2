#ifndef LINIF_H
#define LINIF_H

#include "Lin.h"

#define LINIF_SCHED_NORMAL    0u
#define LINIF_SCHED_DIAG_REQ  1u
#define LINIF_SCHED_DIAG_RESP 2u
#define LINIF_DIAG_TIMEOUT_TICKS 100u

typedef enum
{
    LINIF_CHANNEL_IDLE = 0u,
    LINIF_CHANNEL_BUSY,
    LINIF_CHANNEL_ERROR
} LinIf_ChannelStateType;

LinIf_ChannelStateType LinIf_GetChannelState(void);
void LinIf_ResetDiagnostic(void);

typedef enum
{
    LINIF_DIAG_IDLE = 0u,
    LINIF_DIAG_MRF_PENDING,
    LINIF_DIAG_MRF_ACTIVE,
    LINIF_DIAG_SRF_PENDING,
    LINIF_DIAG_SRF_ACTIVE,
    LINIF_DIAG_DONE,
    LINIF_DIAG_ERROR
} LinIf_DiagStateType;

typedef struct
{
    const Lin_FrameConfigType* frame;
    uint16 delayTicks;
    Lin_DirectionType direction;
} LinIf_ScheduleEntryType;

typedef struct
{
    const LinIf_ScheduleEntryType* entries;
    uint8 numEntries;
    uint8 runOnce;
} LinIf_ScheduleTableType;

void LinIf_Init(void);
void LinIf_MainFunction(void);

Std_ReturnType LinIf_SwitchSchedule(uint8 scheduleId);
Std_ReturnType LinIf_SetDiagRequest(const uint8 data[8]);
Std_ReturnType LinIf_GetDiagResponse(uint8 data[8]);

LinIf_DiagStateType LinIf_GetDiagState(void);
void LinIf_DiagFrameDone(uint8 success);

#endif
