#ifndef NM_H
#define NM_H

#include "ComStack_Types.h"

#define NM_MAX_USER_DATA_LEN 8u

typedef enum
{
    NM_MODE_BUS_SLEEP = 0u,
    NM_MODE_PREPARE_BUS_SLEEP,
    NM_MODE_SYNCHRONIZE,
    NM_MODE_NETWORK
} Nm_ModeType;

typedef enum
{
    NM_STATE_BUS_SLEEP = 0u,
    NM_STATE_PREPARE_BUS_SLEEP,
    NM_STATE_READY_SLEEP,
    NM_STATE_NORMAL_OPERATION,
    NM_STATE_REPEAT_MESSAGE
} Nm_StateType;

void Nm_Init(void);
void Nm_MainFunction(void);

Std_ReturnType Nm_NetworkRequest(uint8 channel);
Std_ReturnType Nm_NetworkRelease(uint8 channel);
Std_ReturnType Nm_GetState(uint8 channel, Nm_StateType* state, Nm_ModeType* mode);
Std_ReturnType Nm_PassiveStartUp(uint8 channel);
Std_ReturnType Nm_SetUserData(uint8 channel, const uint8* data, uint8 len);
Std_ReturnType Nm_GetUserData(uint8 channel, uint8* data, uint8* len);

#endif
