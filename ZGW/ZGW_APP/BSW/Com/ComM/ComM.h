#ifndef COMM_H
#define COMM_H

#include "ComStack_Types.h"

#define COMM_CH_CAN      0u
#define COMM_CH_CANFD    1u
#define COMM_CH_LIN      2u
#define COMM_CH_ETH      3u
#define COMM_NUM_CHANNELS 4u

#define COMM_USER_GATEWAY  0u
#define COMM_USER_DCM      1u
#define COMM_USER_DOIP     2u
#define COMM_USER_SOMEIP   3u
#define COMM_USER_APP      4u
#define COMM_USER_LIN_DIAG 5u
#define COMM_USER_ETH_DIAG 6u
#define COMM_NUM_USERS     7u

typedef uint8 ComM_UserHandleType;
typedef uint8 ComM_ChannelType;

typedef enum
{
    COMM_NO_COMMUNICATION = 0u,
    COMM_SILENT_COMMUNICATION,
    COMM_FULL_COMMUNICATION
} ComM_ModeType;

void ComM_Init(void);
void ComM_MainFunction(void);

Std_ReturnType ComM_RequestComMode(ComM_UserHandleType user, ComM_ModeType mode);
Std_ReturnType ComM_GetRequestedComMode(ComM_UserHandleType user, ComM_ModeType* mode);
Std_ReturnType ComM_GetCurrentComMode(ComM_ChannelType channel, ComM_ModeType* mode);
Std_ReturnType ComM_GetMaxComMode(ComM_UserHandleType user, ComM_ModeType* mode);
Std_ReturnType ComM_LimitChannelToNoComMode(ComM_ChannelType channel, uint8 status);

uint8 ComM_IsTxAllowed(ComM_ChannelType channel);
uint8 ComM_IsRxAllowed(ComM_ChannelType channel);

void ComM_BusSM_ModeIndication(ComM_ChannelType channel, ComM_ModeType mode);
void ComM_Nm_NetworkStartIndication(ComM_ChannelType channel);
void ComM_Nm_NetworkMode(ComM_ChannelType channel);
void ComM_Nm_PrepareBusSleepMode(ComM_ChannelType channel);
void ComM_Nm_BusSleepMode(ComM_ChannelType channel);

/* Cyclic scheduling contract for the lab stack:
 *   ComM_MainFunction()
 *   Nm_MainFunction()
 *   CanNm_MainFunction()
 *   UdpNm_MainFunction()
 *   EthSM_MainFunction()
 * plus the existing CanSM, LinSM, TcpIp, SoAd, DoIP, SomeIp and SomeIpSd
 * main functions.
 */

#endif
