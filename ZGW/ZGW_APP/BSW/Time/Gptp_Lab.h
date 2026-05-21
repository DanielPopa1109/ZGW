#ifndef GPTP_LAB_H
#define GPTP_LAB_H

#include "Std_Types.h"

#define GPTP_LAB_CLOCK_IDENTITY_LENGTH         8u
#define GPTP_LAB_FRAME_BUFFER_LENGTH           128u

#define GPTP_LAB_MESSAGE_SYNC                  0x0u
#define GPTP_LAB_MESSAGE_DELAY_REQ             0x1u
#define GPTP_LAB_MESSAGE_FOLLOW_UP             0x8u
#define GPTP_LAB_MESSAGE_DELAY_RESP            0x9u
#define GPTP_LAB_MESSAGE_ANNOUNCE              0xBu

typedef enum
{
    GPTP_LAB_STATE_DISABLED = 0u,
    GPTP_LAB_STATE_MASTER   = 1u,
    GPTP_LAB_STATE_SLAVE    = 2u,
    GPTP_LAB_STATE_PASSIVE  = 3u
} Gptp_Lab_StateType;

typedef struct
{
    uint8 priority1;
    uint8 clockClass;
    uint8 clockAccuracy;
    uint16 offsetScaledLogVariance;
    uint8 priority2;
    uint8 clockIdentity[GPTP_LAB_CLOCK_IDENTITY_LENGTH];
} Gptp_Lab_DatasetType;

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
void Gptp_Lab_RxIndication(const uint8 *frame, uint16 frameLen, uint64 rxTimestampNs);
void Gptp_Lab_GetStatus(Gptp_Lab_StatusType *status);

sint32 Gptp_Lab_CompareDatasets(const Gptp_Lab_DatasetType *left, const Gptp_Lab_DatasetType *right);

Std_ReturnType Gptp_Lab_BuildSync(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 originTimestampNs);
Std_ReturnType Gptp_Lab_BuildFollowUp(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 originTimestampNs);
Std_ReturnType Gptp_Lab_BuildAnnounce(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 originTimestampNs);
Std_ReturnType Gptp_Lab_BuildDelayReq(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 originTimestampNs);
Std_ReturnType Gptp_Lab_BuildDelayResp(uint8 *frame, uint16 frameLen, uint16 *outLen, uint16 sequenceId, uint64 receiveTimestampNs);
Std_ReturnType Gptp_Lab_ParseAnnounce(const uint8 *frame, uint16 frameLen, Gptp_Lab_DatasetType *dataset);

Std_ReturnType Gptp_Lab_PlatformTransmitFrame(const uint8 *frame, uint16 frameLen);
Std_ReturnType Gptp_Lab_PlatformGetTxTimestamp(uint64 *timestampNs);
Std_ReturnType Gptp_Lab_PlatformGetRxTimestamp(uint64 *timestampNs);

#endif /* GPTP_LAB_H */
