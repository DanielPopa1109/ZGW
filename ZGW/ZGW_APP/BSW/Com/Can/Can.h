#ifndef CAN_H
#define CAN_H

#include "ComStack_Types.h"
#include "IfxCan_Can.h"
#include "IfxCan.h"
#include "IfxPort.h"

#define CAN_MAINFUNCTION_PERIOD_MS 5u

#define CAN_CONTROLLER_CLASSIC 0u
#define CAN_CONTROLLER_FD      1u
#define CAN_NUM_CONTROLLERS    2u

#define CAN_HTH_CLASSIC        0u
#define CAN_HTH_FD             1u

#define CAN_MAX_DLC            64u
#define CAN_TX_QUEUE_SIZE      64u
#define CAN_RX_QUEUE_SIZE      100u

#define CAN_ID_STANDARD        0u
#define CAN_ID_EXTENDED        1u

#define CAN_FRAME_CLASSIC      0u
#define CAN_FRAME_FD           1u

#define CAN_CLASSIC_MAX_DLC    8u
#define CANFD_MAX_DLC          64u

#define CAN_STD_ID_MAX         0x7FFu
#define CAN_EXT_ID_MAX         0x1FFFFFFFu

#define CAN_TX_BUDGET_PER_MAIN 64u

#define CAN_BUSOFF_RECOVERY_TIME_MS     100u
#define CAN_BUSOFF_RECOVERY_TICKS       (CAN_BUSOFF_RECOVERY_TIME_MS / CAN_MAINFUNCTION_PERIOD_MS)

#define CAN_ERROR_WARNING_LIMIT         96u
#define CAN_ERROR_PASSIVE_LIMIT         128u

typedef enum
{
    CAN_ERROR_ACTIVE = 0u,
    CAN_ERROR_WARNING,
    CAN_ERROR_PASSIVE,
    CAN_ERROR_BUS_OFF
} Can_ErrorStateType;

typedef enum
{
    CAN_UNINIT = 0u,
    CAN_READY,
    CAN_SLEEP,
    CAN_BUS_OFF
} Can_ControllerStateType;

typedef struct
{
    uint32 id;
    uint8 idType;
    uint8 controllerId;
    uint8 frameType;
    uint8 dlc;
    uint8 data[CAN_MAX_DLC];
} Can_FrameType;

typedef struct
{
    PduIdType swPduHandle;
    uint32 id;
    uint8 idType;
    uint8 controllerId;
    uint8 frameType;
    uint8 dlc;
    const uint8* sdu;
} Can_PduType;

void Can_Init(void);
void Can_MainFunction(void);

Std_ReturnType Can_Write(PduIdType Hth, const Can_PduType* PduInfo);
Std_ReturnType Can_DrainTxPdu(PduIdType SwPduHandle, uint8 ControllerId, uint32 PollLimit);

void Can_IsrRxClassic(void);
void Can_IsrRxFd(void);
void Can_IsrBusOffClassic(void);
void Can_IsrBusOffFd(void);

Can_ControllerStateType Can_GetControllerState(uint8 controllerId);
Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_ControllerStateType Transition);
Can_ControllerStateType Can_GetControllerMode(uint8 Controller);

Std_ReturnType Can_GetControllerRxErrorCounter(uint8 Controller, uint8* RxErrorCounterPtr);
Std_ReturnType Can_GetControllerTxErrorCounter(uint8 Controller, uint8* TxErrorCounterPtr);

void Can_EnableControllerInterrupts(uint8 Controller);
void Can_DisableControllerInterrupts(uint8 Controller);

Can_ErrorStateType Can_GetControllerErrorState(uint8 Controller);
void Can_MainFunction_BusOff(void);
void Can_MainFunction_Mode(void);
void Can_MainFunction_Read(void);
void Can_MainFunction_Write(void);

#endif
