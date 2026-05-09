#ifndef LINIF_H
#define LINIF_H

#include "Lin.h"
#include "ComStack_Types.h"

#define LINIF_SCHED_NORMAL       0u
#define LINIF_SCHED_DIAG_REQ     1u
#define LINIF_SCHED_DIAG_RESP    2u
#define LINIF_DIAG_TIMEOUT_TICKS 100u


/* LDF ZGW_LIN_4 generated LinIf PDU IDs. */
#define LINIF_TX_PDU_ZGW_NM3             10u
#define LINIF_TX_PDU_ZGW_REQUEST_ALT     11u
#define LINIF_TX_PDU_ZGW_REQUEST_HVDCDC  12u
#define LINIF_TX_PDU_ZGW_REQUEST_PCU48   13u
#define LINIF_RX_PDU_ALT_STATUS          10u
#define LINIF_RX_PDU_HVDCDC_STATUS       11u
#define LINIF_RX_PDU_PCU48_STATUS        12u

#define LINIF_NAD_ALT                    2u
#define LINIF_NAD_PCU48                  3u
#define LINIF_NAD_HVDCDC                 4u

#define LINIF_MAX_APP_TX_PDUS    4u
#define LINIF_MAX_APP_RX_PDUS    3u

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
Std_ReturnType LinIf_Transmit(PduIdType LinIfTxPduId,
                              const uint8* data,
                              PduLengthType len);

Std_ReturnType LinIf_SetDiagRequest(const uint8 data[8]);
Std_ReturnType LinIf_GetDiagResponse(uint8 data[8]);

LinIf_DiagStateType LinIf_GetDiagState(void);
void LinIf_DiagFrameDone(uint8 success);

#endif
