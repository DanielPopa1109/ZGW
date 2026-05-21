#ifndef CANTP_H
#define CANTP_H

#include "CanIf.h"

#define CANTP_MAX_CHANNELS       8u
#define CANTP_MAX_TX_BUFFERS     4u
#define CANTP_MAX_PAYLOAD_LEN    4095u
#define CANTP_MAIN_PERIOD_MS     5u

#define CANTP_NPCI_SF            0x00u
#define CANTP_NPCI_FF            0x10u
#define CANTP_NPCI_CF            0x20u
#define CANTP_NPCI_FC            0x30u

#define CANTP_FS_CTS             0x00u
#define CANTP_FS_WAIT            0x01u
#define CANTP_FS_OVFLW           0x02u

#define CANTP_ADDR_PHYSICAL      0u
#define CANTP_ADDR_FUNCTIONAL    1u

#define CANTP_WFT_MAX            8u

#define CANTP_MS_TO_TICKS(ms)    (((ms) + CANTP_MAIN_PERIOD_MS - 1u) / CANTP_MAIN_PERIOD_MS)

#define CANTP_N_AS_TICKS         CANTP_MS_TO_TICKS(1000u)
#define CANTP_N_AR_TICKS         CANTP_MS_TO_TICKS(1000u)
#define CANTP_N_BS_TICKS         CANTP_MS_TO_TICKS(1000u)
#define CANTP_N_BR_TICKS         CANTP_MS_TO_TICKS(1000u)
#define CANTP_N_CS_TICKS         CANTP_MS_TO_TICKS(1000u)
#define CANTP_N_CR_TICKS         CANTP_MS_TO_TICKS(1000u)

#define CANTP_FORMAT_NORMAL      0u
#define CANTP_FORMAT_EXTENDED    1u
#define CANTP_FORMAT_MIXED       2u

#define CANTP_PADDING_OFF        0u
#define CANTP_PADDING_ON         1u

typedef enum
{
    CANTP_IDLE = 0u,
    CANTP_RX_WAIT_FC_TX_CONFIRM,
    CANTP_RX_IN_PROGRESS,
    CANTP_TX_WAIT_CONFIRM,
    CANTP_TX_WAIT_FC,
    CANTP_TX_CF
} CanTp_StateType;

typedef enum
{
    CANTP_TIMER_NONE = 0u,
    CANTP_TIMER_N_AS,
    CANTP_TIMER_N_AR,
    CANTP_TIMER_N_BS,
    CANTP_TIMER_N_BR,
    CANTP_TIMER_N_CS,
    CANTP_TIMER_N_CR
} CanTp_TimerType;

typedef struct
{
    PduIdType rxPduId;
    PduIdType txPduId;
    PduIdType upperRxPduId;
    PduIdType upperTxPduId;
    uint8 addressingType;
    uint8 canDl;
    uint8 blockSize;
    uint8 stMinMs;
    uint8 padding;

    uint8 addressFormat;
    uint8 nTa;
    uint8 txNta;
    uint8 paddingActivation;

    uint16 nAsTicks;
    uint16 nArTicks;
    uint16 nBsTicks;
    uint16 nBrTicks;
    uint16 nCsTicks;
    uint16 nCrTicks;
} CanTp_ChannelConfigType;

typedef struct
{
    const CanTp_ChannelConfigType* channels;
    uint8 numChannels;
} CanTp_ConfigType;

void CanTp_Init(const CanTp_ConfigType* ConfigPtr);
void CanTp_MainFunction(void);

Std_ReturnType CanTp_Transmit(PduIdType CanTpTxSduId, const uint8* data, PduLengthType len);
void CanTp_RxIndication(PduIdType CanIfRxPduId, const uint8* data, PduLengthType len);
void CanTp_TxConfirmation(PduIdType CanIfTxPduId);

extern const CanTp_ConfigType CanTp_Config;

#endif
