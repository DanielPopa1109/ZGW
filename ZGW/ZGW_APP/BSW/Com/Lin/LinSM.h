#ifndef LINSM_H
#define LINSM_H

#include "Std_Types.h"

typedef enum
{
    LINSM_UNINIT = 0u,
    LINSM_NO_COMMUNICATION,
    LINSM_FULL_COMMUNICATION,
    LINSM_GOTO_SLEEP,
    LINSM_SLEEP,
    LINSM_WAKEUP
} LinSM_StateType;

void LinSM_Init(void);
void LinSM_MainFunction(void);

Std_ReturnType LinSM_RequestComMode(uint8 Channel, LinSM_StateType RequestedState);
LinSM_StateType LinSM_GetState(uint8 Channel);

#endif
