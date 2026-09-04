#ifndef GPTP_LAB_H
#define GPTP_LAB_H

#include "Std_Types.h"

#define GPTP_LAB_CLOCK_IDENTITY_LENGTH         8u
#define GPTP_LAB_FRAME_BUFFER_LENGTH           128u

#define GPTP_LAB_MESSAGE_SYNC                  0x0u
#define GPTP_LAB_MESSAGE_FOLLOW_UP             0x8u
#define GPTP_LAB_MESSAGE_ANNOUNCE              0xBu

#define GPTP_LAB_STATE_DISABLED                0u
#define GPTP_LAB_STATE_MASTER                  1u

typedef struct
{
    uint8 enabled;
    uint8 forceMaster;
    uint8 state;
    uint16 txSequenceId;
    uint32 syncTxCounter;
    uint32 followUpTxCounter;
    uint32 announceTxCounter;
    uint32 rxAnnounceCounter;
    uint32 platformTxDropCounter;
} Gptp_Lab_StatusType;

void Gptp_Lab_Init(void);
void Gptp_Lab_MainFunction(uint32 elapsedMs);
void Gptp_Lab_GetStatus(Gptp_Lab_StatusType *status);

Std_ReturnType Gptp_Lab_BuildSync(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 originTimestampNs);
Std_ReturnType Gptp_Lab_BuildFollowUp(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 originTimestampNs);
Std_ReturnType Gptp_Lab_BuildAnnounce(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 originTimestampNs);

Std_ReturnType Gptp_Lab_PlatformTransmitFrame(const uint8 *frame, uint16 frameLen);
Std_ReturnType Gptp_Lab_PlatformGetTxTimestamp(uint64 *timestampNs);

#endif /* GPTP_LAB_H */
