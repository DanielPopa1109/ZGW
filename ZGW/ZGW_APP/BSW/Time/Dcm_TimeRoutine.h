#ifndef DCM_TIMEROUTINE_H
#define DCM_TIMEROUTINE_H

#include "BSW/Diag/Dcm/Dcm.h"

boolean Dcm_TimeRoutine_IsRoutineId(uint16 routineId);

Dcm_ReturnType Dcm_TimeRoutine_HandleRoutineControl(
        Dcm_OpStatusType opStatus,
        uint8 routineControlType,
        uint16 routineId,
        const uint8 *reqData,
        Dcm_PduLengthType reqLen,
        uint8 *respData,
        Dcm_PduLengthType *respLen);

#endif /* DCM_TIMEROUTINE_H */
