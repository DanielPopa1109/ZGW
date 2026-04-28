#ifndef LIN_PROTOCOL_H
#define LIN_PROTOCOL_H

#include "Std_Types.h"

#define LIN_PID_MASTER_REQUEST 0x3Cu
#define LIN_PID_SLAVE_RESPONSE 0x3Du

typedef enum
{
    LIN_CS_CLASSIC = 0u,
    LIN_CS_ENHANCED = 1u
} Lin_ChecksumType;

typedef enum
{
    LIN_FRM_UNCONDITIONAL = 0u,
    LIN_FRM_EVENT_TRIGGERED,
    LIN_FRM_SPORADIC,
    LIN_FRM_DIAGNOSTIC_MRF,
    LIN_FRM_DIAGNOSTIC_SRF,
    LIN_FRM_SLEEP
} Lin_FrameClassType;

typedef enum
{
    LIN_RES_OK = 0u,
    LIN_RES_NOT_OK,
    LIN_RES_NO_RESPONSE,
    LIN_RES_CHECKSUM_ERROR,
    LIN_RES_PID_ERROR,
    LIN_RES_FRAMING_ERROR,
    LIN_RES_SYNC_ERROR,
    LIN_RES_TIMEOUT
} Lin_ResultType;

typedef struct
{
    uint8 id;
    uint8 pid;
    uint8 dlc;
    Lin_ChecksumType checksumType;
    Lin_FrameClassType frameClass;
    uint16 responseTimeoutTicks;
} Lin_FrameConfigType;

uint8 Lin_MakePid(uint8 id);
Std_ReturnType Lin_CheckPid(uint8 pid, uint8* idOut);
uint8 Lin_CalcChecksum(uint8 pid, const uint8* data, uint8 len, Lin_ChecksumType type);

#endif
