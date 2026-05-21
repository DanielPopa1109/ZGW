#ifndef ETHSM_H
#define ETHSM_H

#include "Std_Types.h"

typedef enum
{
    ETHSM_NO_COMMUNICATION = 0u,
    ETHSM_WAIT_LINK,
    ETHSM_FULL_COMMUNICATION,
    ETHSM_SILENT_COMMUNICATION
} EthSM_ComModeType;

void EthSM_Init(void);
void EthSM_MainFunction(void);
Std_ReturnType EthSM_RequestComMode(uint8 channel, EthSM_ComModeType mode);
Std_ReturnType EthSM_GetCurrentComMode(uint8 channel, EthSM_ComModeType* mode);

#endif
