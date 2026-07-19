#ifndef CANIF_H
#define CANIF_H

#include "Can.h"

#define CANIF_MAX_RX_PDUS 128u
#define CANIF_MAX_TX_PDUS 48u

#define CANIF_PDU_MODE_OFFLINE   0u
#define CANIF_PDU_MODE_RX_ONLINE 1u
#define CANIF_PDU_MODE_TX_ONLINE 2u
#define CANIF_PDU_MODE_ONLINE    3u

#define CANIF_PDU_CLASSIC_PHYS_RX 0u
#define CANIF_PDU_FD_PHYS_RX      2u
#define CANIF_PDU_CLASSIC_PHYS_TX 0u
#define CANIF_PDU_FD_PHYS_TX      1u
#define CANIF_PDU_CLASSIC_EXT_PHYS_TX0 2u
#define CANIF_PDU_CLASSIC_EXT_PHYS_TX1 3u
#define CANIF_PDU_FD_EXT_PHYS_TX0      4u
#define CANIF_PDU_CLASSIC_EXT_PHYS_TX2 6u
#define CANIF_PDU_CLASSIC_EXT_PHYS_TX3 7u
#define CANIF_DIAGREQUEST_TESTER_CAN_ID 0x710u
#define CANIF_DIAGRESPONSE_ZGW_CAN_ID   0x711u
#define CANIF_DIAGREQUEST_CLASSIC_CAN_ID CANIF_DIAGREQUEST_TESTER_CAN_ID
#define CANIF_DIAGREQUEST_CANFD_CAN_ID   CANIF_DIAGREQUEST_TESTER_CAN_ID
#define CANIF_RX_PDU_CENTRALLOCKDATA                          10u
#define CANIF_RX_PDU_LIGHTDATA1                               11u
#define CANIF_RX_PDU_STATUSACTUATOR                           12u
#define CANIF_RX_PDU_OUTSIDETEMPERATURESTATUS                 13u
#define CANIF_RX_PDU_CENTRALCOMMAND1                          14u
#define CANIF_RX_PDU_DMUSTATUS                                15u
#define CANIF_RX_PDU_BATTFULLSTAT                             17u
#define CANIF_RX_PDU_PDCSTAT                                  28u
#define CANIF_RX_PDU_MILEAGE                                  29u
#define CANIF_RX_PDU_DMU_ALIVE                                30u
#define CANIF_RX_PDU_XCPRESPONSE_7C7                          31u
#define CANIF_RX_PDU_XCPRESPONSE_7C5                          32u
#define CANIF_RX_PDU_XCPRESPONSE_7C3                          33u
#define CANIF_RX_PDU_XCPRESPONSE_7C1                          34u
#define CANIF_RX_PDU_VOLTAGECURRENT                           35u
#define CANIF_RX_PDU_TEMPMEAS                                 36u
#define CANIF_RX_PDU_LOADSTATUS                               37u
#define CANIF_RX_PDU_L1_I2T_COUNTER                           38u
#define CANIF_RX_PDU_ERROR_706                                39u
#define CANIF_RX_PDU_ERROR_704                                40u
#define CANIF_RX_PDU_ERROR_702                                41u
#define CANIF_RX_PDU_ERROR_700                                42u
#define CANIF_RX_PDU_DIAGREQUEST_710                          43u
#define CANIF_RX_PDU_DIAGRESPONSE_707                         44u
#define CANIF_RX_PDU_DIAGRESPONSE_705                         45u
#define CANIF_RX_PDU_DIAGRESPONSE_703                         46u
#define CANIF_RX_PDU_DIAGRESPONSE_701                         47u
#define CANIF_RX_PDU_BATTSOCSOH                               48u
#define CANIF_RX_PDU_BATTSOC                                  49u
#define CANIF_RX_PDU_BATTDIAGNOSIS                            50u
#define CANIF_RX_PDU_BATTCURRENT                              51u
#define CANIF_RX_PDU_BATTCAPDISCHARGE                         52u
#define CANIF_RX_PDU_BATTCAPRES                               53u
#define CANIF_TX_PDU_VEHICLESTATE                             10u
#define CANIF_TX_PDU_DISPLAYOUTTEMP                           11u
#define CANIF_TX_PDU_STATUSBODYDATA1                          12u
#define CANIF_TX_PDU_COMMANDDISPLAYSTATUS                     13u
#define CANIF_TX_PDU_XCPREQUEST_7C8                           14u
#define CANIF_TX_PDU_XCPREQUEST_7C6                           15u
#define CANIF_TX_PDU_XCPREQUEST_7C4                           16u
#define CANIF_TX_PDU_XCPREQUEST_7C2                           17u
#define CANIF_TX_PDU_XCPREQUEST_7C0                           18u
#define CANIF_TX_PDU_SDAT                                     19u
#define CANIF_TX_PDU_NM3                                      20u
#define CANIF_TX_PDU_LOADREQUEST                              21u
#define CANIF_TX_PDU_DIAGREQUEST_706                          22u
#define CANIF_TX_PDU_DIAGREQUEST_704                          23u
#define CANIF_TX_PDU_DIAGREQUEST_702                          24u
#define CANIF_TX_PDU_DIAGREQUEST_700                          25u


/* DBC generated CAN-FD COM I-PDU IDs. Existing CAN Classic IDs above are untouched. */
#define CANIF_RX_PDU_CANFD_PDM1_LOADSTATUS                                    103u
#define CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_1                             104u
#define CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_2                             105u
#define CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_3                             106u
#define CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_4                             107u
#define CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_5                             108u
#define CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_1                             109u
#define CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_2                             110u
#define CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_3                             111u
#define CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_4                             112u
#define CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_5                             113u
#define CANIF_RX_PDU_CANFD_PDM1_STUCKATONEVENT                                114u
#define CANIF_RX_PDU_CANFD_PDM1_STUCKATOFFEVENT                               115u
#define CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_1                         116u
#define CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_2                         117u
#define CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_3                         118u
#define CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_4                         119u
#define CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_5                         120u
#define CANIF_RX_PDU_CANFD_PDM1_DIAGRESPONSE                                  172u
#define CANIF_TX_PDU_CANFD_INFOTAINMENTDATA1                                  100u
#define CANIF_TX_PDU_CANFD_ENERGYMANAGEMENTDATA2                              101u
#define CANIF_TX_PDU_CANFD_ENERGYMANAGEMENTDATA1                              102u
#define CANIF_TX_PDU_CANFD_VEHICLESTATE                                       103u
#define CANIF_TX_PDU_CANFD_NM3                                                104u
#define CANIF_TX_PDU_CANFD_SDAT                                               105u
#define CANIF_TX_PDU_CANFD_LIGHTDATA1                                         106u
#define CANIF_TX_PDU_CANFD_BODYDATA1                                          109u
#define CANIF_TX_PDU_CANFD_PDM1_DIAGREQUEST                                   110u
#define CANIF_TX_PDU_CANFD_COMMANDLOAD_PDM1                                   114u
#define CANIF_TX_PDU_CANFD_ENERGYMANAGEMENTDATA3                              118u

typedef enum
{
    CANIF_RX_TARGET_CANTP = 0u,
    CANIF_RX_TARGET_COM   = 1u
} CanIf_RxTargetType;

typedef enum
{
    CANIF_TX_TARGET_CANTP = 0u,
    CANIF_TX_TARGET_COM   = 1u
} CanIf_TxTargetType;

typedef struct
{
    PduIdType rxPduId;
    uint8 controllerId;
    uint32 canId;
    uint8 idType;
    uint8 frameType;
    uint8 minDlc;
    uint8 maxDlc;
    CanIf_RxTargetType target;
} CanIf_RxPduConfigType;

typedef struct
{
    PduIdType txPduId;
    PduIdType hth;
    uint8 controllerId;
    uint32 canId;
    uint8 idType;
    uint8 frameType;
    uint8 fixedDlc;
    CanIf_TxTargetType target;
} CanIf_TxPduConfigType;

typedef struct
{
    const CanIf_RxPduConfigType* rxPdus;
    uint8 numRxPdus;
    const CanIf_TxPduConfigType* txPdus;
    uint8 numTxPdus;
} CanIf_ConfigType;

void CanIf_Init(const CanIf_ConfigType* ConfigPtr);
Std_ReturnType CanIf_SetPduMode(uint8 ControllerId, uint8 PduMode);
uint8 CanIf_GetPduMode(uint8 ControllerId);

Std_ReturnType CanIf_Transmit(PduIdType CanIfTxSduId, const uint8* data, PduLengthType len);

void CanIf_RxIndication(const Can_FrameType* frame);
void CanIf_TxConfirmation(PduIdType CanIfTxSduId);
void CanIf_ControllerBusOff(uint8 ControllerId);

void CanIf_AppBusOff(uint8 ControllerId);

void CanIf_ControllerRecovered(uint8 ControllerId);
void CanIf_ControllerErrorPassive(uint8 ControllerId);
void CanIf_ControllerErrorWarning(uint8 ControllerId);

void CanIf_AppControllerRecovered(uint8 ControllerId);
void CanIf_AppErrorPassive(uint8 ControllerId);
void CanIf_AppErrorWarning(uint8 ControllerId);

extern const CanIf_ConfigType CanIf_Config;

#endif
