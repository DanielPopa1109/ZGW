#ifndef COM_H
#define COM_H

#include "ComStack_Types.h"

#define COM_MAX_IPDU_LEN      64u
#define COM_MAIN_PERIOD_MS    5u

#ifndef COM_DEBUG_INSTRUMENTATION
#define COM_DEBUG_INSTRUMENTATION 0
#endif

#define COM_SIGNAL_U8         1u
#define COM_SIGNAL_U16        2u
#define COM_SIGNAL_U32        4u

#define COM_TX_MODE_NONE      0u
#define COM_TX_MODE_DIRECT    1u
#define COM_TX_MODE_PERIODIC  2u
#define COM_TX_MODE_MIXED     3u

#define COM_IPDU_GROUP_0      0u
#define COM_IPDU_GROUP_1      1u

#define COM_RX_DIAG_STATUS_OK       0x00u
#define COM_RX_DIAG_STATUS_TIMEOUT  0x01u
#define COM_RX_DIAG_STATUS_INVALID  0x02u

typedef uint16 Com_SignalIdType;
typedef uint16 Com_IpduGroupIdType;

void Com_Init(void);
void Com_MainFunctionTx(void);
void Com_MainFunctionRx(void);

Std_ReturnType Com_IpduGroupStart(Com_IpduGroupIdType groupId);
Std_ReturnType Com_IpduGroupStop(Com_IpduGroupIdType groupId);
void Com_TriggerFullComRestartBurst(uint8 channel);

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr);
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr);
Std_ReturnType Com_InvalidateSignal(Com_SignalIdType SignalId);
Std_ReturnType Com_GetRxPduDiagStatus(PduIdType RxPduId, uint8* StatusPtr);
Std_ReturnType Com_GetRxPduDiagConfig(PduIdType RxPduId,
        uint16* CycleTicksPtr,
        uint16* TimeoutTicksPtr);
Std_ReturnType Com_GetRxSignalDiagStatus(Com_SignalIdType SignalId,
        uint32* ValuePtr,
        uint32* InvalidValuePtr,
        uint8* StatusPtr);

void Com_RxIndication(PduIdType RxPduId, const uint8* data, PduLengthType len);
void Com_TxConfirmation(PduIdType TxPduId);

#if COM_DEBUG_INSTRUMENTATION
extern volatile uint16 Com_DebugFindSignalLastId;
extern volatile uint16 Com_DebugFindSignalIndex;
extern volatile uint16 Com_DebugFindSignalCount;
extern volatile uint16 Com_DebugFindSignalEntryId;
extern volatile uint8 Com_DebugFindSignalHit;
extern volatile uint32 Com_DebugFindSignalTableBase;
extern volatile uint32 Com_DebugFindSignalTableEnd;
extern volatile uint32 Com_DebugFindSignalEntryAddress;
extern volatile uint32 Com_DebugFindSignalMatchAddress;
extern volatile uint16 Com_DebugFindSignalEntryPduId;
extern volatile uint8 Com_DebugFindSignalEntryIsTx;
extern volatile uint16 Com_DebugSendSignalLastId;
extern volatile uint16 Com_DebugSendSignalLastPduId;
extern volatile uint8 Com_DebugSendSignalLastTxIdx;
extern volatile uint8 Com_DebugSendSignalLastRet;
extern volatile uint32 Com_DebugSendSignalLastValue;
extern volatile uint32 Com_DebugSendSignalLastSigAddress;
extern volatile uint16 Com_DebugReceiveSignalLastId;
extern volatile uint16 Com_DebugReceiveSignalLastPduId;
extern volatile uint8 Com_DebugReceiveSignalLastRxIdx;
extern volatile uint8 Com_DebugReceiveSignalLastRet;
extern volatile uint32 Com_DebugReceiveSignalLastValue;
#endif


/* DBC generated CAN Classic COM I-PDU IDs and Signal IDs. */
#define COM_TX_PDU_VEHICLESTATE                                 10u
#define COM_TX_PDU_DISPLAYOUTTEMP                               11u
#define COM_TX_PDU_STATUSBODYDATA1                              12u
#define COM_TX_PDU_COMMANDDISPLAYSTATUS                         13u
#define COM_TX_PDU_XCPREQUEST_7C8                               14u
#define COM_TX_PDU_XCPREQUEST_7C6                               15u
#define COM_TX_PDU_XCPREQUEST_7C4                               16u
#define COM_TX_PDU_XCPREQUEST_7C2                               17u
#define COM_TX_PDU_XCPREQUEST_7C0                               18u
#define COM_TX_PDU_SDAT                                         19u
#define COM_TX_PDU_NM3                                          20u
#define COM_TX_PDU_LOADREQUEST                                  21u
#define COM_TX_PDU_DIAGREQUEST_706                              22u
#define COM_TX_PDU_DIAGREQUEST_704                              23u
#define COM_TX_PDU_DIAGREQUEST_702                              24u
#define COM_TX_PDU_DIAGREQUEST_700                              25u
#define COM_RX_PDU_CENTRALLOCKDATA                              10u
#define COM_RX_PDU_LIGHTDATA1                                   11u
#define COM_RX_PDU_STATUSACTUATOR                               12u
#define COM_RX_PDU_OUTSIDETEMPERATURESTATUS                     13u
#define COM_RX_PDU_CENTRALCOMMAND1                              14u
#define COM_RX_PDU_DMUSTATUS                                    15u
#define COM_RX_PDU_BATTFULLSTAT                                 17u
#define COM_RX_PDU_PDCSTAT                                      28u
#define COM_RX_PDU_MILEAGE                                      29u
#define COM_RX_PDU_DMU_ALIVE                                    30u
#define COM_RX_PDU_XCPRESPONSE_7C7                              31u
#define COM_RX_PDU_XCPRESPONSE_7C5                              32u
#define COM_RX_PDU_XCPRESPONSE_7C3                              33u
#define COM_RX_PDU_XCPRESPONSE_7C1                              34u
#define COM_RX_PDU_VOLTAGECURRENT                               35u
#define COM_RX_PDU_TEMPMEAS                                     36u
#define COM_RX_PDU_LOADSTATUS                                   37u
#define COM_RX_PDU_L1_I2T_COUNTER                               38u
#define COM_RX_PDU_ERROR_706                                    39u
#define COM_RX_PDU_ERROR_704                                    40u
#define COM_RX_PDU_ERROR_702                                    41u
#define COM_RX_PDU_ERROR_700                                    42u
#define COM_RX_PDU_DIAGREQUEST_710                              43u
#define COM_RX_PDU_DIAGRESPONSE_707                             44u
#define COM_RX_PDU_DIAGRESPONSE_705                             45u
#define COM_RX_PDU_DIAGRESPONSE_703                             46u
#define COM_RX_PDU_DIAGRESPONSE_701                             47u
#define COM_RX_PDU_BATTSOCSOH                                   48u
#define COM_RX_PDU_BATTSOC                                      49u
#define COM_RX_PDU_BATTDIAGNOSIS                                50u
#define COM_RX_PDU_BATTCURRENT                                  51u
#define COM_RX_PDU_BATTCAPDISCHARGE                             52u
#define COM_RX_PDU_BATTCAPRES                                   53u
#define COM_SIG_TX_VEHICLESTATE_VEHICLESTATUS                                      0u
#define COM_SIG_TX_DISPLAYOUTTEMP_COMMANDDISPLAYOUTSIDETEMPERATURE                   1u
#define COM_SIG_TX_STATUSBODYDATA1_BD1_CENTRALLOCKCOMMAND                             2u
#define COM_SIG_TX_STATUSBODYDATA1_BD1_TURNSIGNALCOMMAND                              3u
#define COM_SIG_TX_STATUSBODYDATA1_BD1_RLSCOMMAND                                     4u
#define COM_SIG_TX_STATUSBODYDATA1_BD1_LIGTHSENSOR                                    5u
#define COM_SIG_TX_STATUSBODYDATA1_BD1_IGNITION                                       6u
#define COM_SIG_TX_STATUSBODYDATA1_BD1_HIGHBEAMCOMMAND                                7u
#define COM_SIG_TX_STATUSBODYDATA1_BD1_GEARBOX                                        8u
#define COM_SIG_TX_STATUSBODYDATA1_BD1_FOGLIGHTSCOMMAND                               9u
#define COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYRECIRCULATION                        10u
#define COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYCLIMATEAUTO                          11u
#define COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYCLIMATEMP                            12u
#define COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYERRORMESSAGE                         13u
#define COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYDSCSTATUS                            14u
#define COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYVEHICLESPEED                         15u
#define COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYRPM                                  16u
#define COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYMODE                                 17u
#define COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYCLIMAFAN                             18u
#define COM_SIG_TX_SDAT_CSB_SDAT_YEAR                                      19u
#define COM_SIG_TX_SDAT_CSB_SDAT_SECOND                                    20u
#define COM_SIG_TX_SDAT_CSB_SDAT_MONTH                                     21u
#define COM_SIG_TX_SDAT_CSB_SDAT_MINUTE                                    22u
#define COM_SIG_TX_SDAT_CSB_SDAT_MILLISECOND                               23u
#define COM_SIG_TX_SDAT_CSB_SDAT_HOUR                                      24u
#define COM_SIG_TX_SDAT_CSB_SDAT_DAY                                       25u
#define COM_SIG_TX_SDAT_CSB_SDAT_TIMESOURCE                                2300u
#define COM_SIG_TX_SDAT_CSB_SDAT_UTCVALID                                  2301u
#define COM_SIG_TX_SDAT_CSB_SDAT_DEFAULTTIME                               2302u
#define COM_SIG_TX_SDAT_CSB_SDAT_NVMRESTORED                               2306u
#define COM_SIG_TX_NM3_NM3_PN1                                            26u
#define COM_SIG_TX_LOADREQUEST_LOADREQUESTSTATUS                                  27u
#define COM_SIG_TX_LOADREQUEST_LOADREQUESTDATAID                                  28u
#define COM_SIG_TX_LOADREQUEST_LOADREQUESTCRC                                     29u
#define COM_SIG_TX_LOADREQUEST_LOADREQUESTALIVECOUNTER                            30u
#define COM_SIG_RX_CENTRALLOCKDATA_VIBRATIONSENSORSTATUS                              31u
#define COM_SIG_RX_CENTRALLOCKDATA_CENTRALLOCKSTATUS                                  32u
#define COM_SIG_RX_CENTRALLOCKDATA_CENTRALLOCKBUZZER                                  33u
#define COM_SIG_RX_LIGHTDATA1_REVERSELIGTHSTATUS                                 34u
#define COM_SIG_RX_LIGHTDATA1_POSITIONLIGHTSTATUS                                35u
#define COM_SIG_RX_LIGHTDATA1_LOWBEAMSTATUS                                      36u
#define COM_SIG_RX_LIGHTDATA1_INTERIORLIGHTSTATUS                                37u
#define COM_SIG_RX_LIGHTDATA1_HIGHBEAMSTATUS                                     38u
#define COM_SIG_RX_LIGHTDATA1_FOGLIGHTSSTATUS                                    39u
#define COM_SIG_RX_LIGHTDATA1_TURNSIGNALSSTATUS                                  40u
#define COM_SIG_RX_LIGHTDATA1_BRAKELIGHTSTATUS                                   41u
#define COM_SIG_RX_STATUSACTUATOR_STATUSWIPERS                                       42u
#define COM_SIG_RX_STATUSACTUATOR_STATUSDOORS                                        43u
#define COM_SIG_RX_OUTSIDETEMPERATURESTATUS_OUTSIDETEMPERATURE                                 44u
#define COM_SIG_RX_CENTRALCOMMAND1_HIGHBEAMCOMMAND                                    45u
#define COM_SIG_RX_CENTRALCOMMAND1_HC05CONNECTIONSTATUS                               46u
#define COM_SIG_RX_CENTRALCOMMAND1_GEARBOX                                            47u
#define COM_SIG_RX_CENTRALCOMMAND1_RECIRCULATIONCOMMAND                               48u
#define COM_SIG_RX_CENTRALCOMMAND1_TURNSIGNALCOMMAND                                  49u
#define COM_SIG_RX_CENTRALCOMMAND1_IGNITION                                           50u
#define COM_SIG_RX_CENTRALCOMMAND1_CLIMATEFANCOMMAND                                  51u
#define COM_SIG_RX_CENTRALCOMMAND1_FOGLIGHTSCOMMAND                                   52u
#define COM_SIG_RX_CENTRALCOMMAND1_RLSCOMMAND                                         53u
#define COM_SIG_RX_CENTRALCOMMAND1_CLIMATEAUTOCOMMAND                                 54u
#define COM_SIG_RX_CENTRALCOMMAND1_CLIMATETEMPERATURECOMMAND                          55u
#define COM_SIG_RX_CENTRALCOMMAND1_WIPERSTOCKCOMMAND                                  56u
#define COM_SIG_RX_CENTRALCOMMAND1_LIGTHSENSOR                                        57u
#define COM_SIG_RX_CENTRALCOMMAND1_CENTRALLOCKCOMMAND                                 58u
#define COM_SIG_RX_DMUSTATUS_DISPLAYCAMERASTATUS                                59u
#define COM_SIG_RX_BATTFULLSTAT_RUNTIMEREMAINING                                   62u
#define COM_SIG_RX_BATTFULLSTAT_TIMETOFULL                                         63u
#define COM_SIG_RX_PDCSTAT_PDCDISTANCEREAR                                    92u
#define COM_SIG_RX_PDCSTAT_PDCDISTANCEFRONT                                   93u
#define COM_SIG_RX_PDCSTAT_PDCBUZZERFRONTREAR                                 94u
#define COM_SIG_RX_MILEAGE_MILEAGETRIP                                        95u
#define COM_SIG_RX_MILEAGE_MILEAGETOTAL                                       96u
#define COM_SIG_RX_DMU_ALIVE_ALIVEINDICATION                                    97u
#define COM_SIG_RX_VOLTAGECURRENT_L1VOLTAGE                                          98u
#define COM_SIG_RX_VOLTAGECURRENT_T30VOLTAGE                                         99u
#define COM_SIG_RX_VOLTAGECURRENT_L1ISENSE                                           100u
#define COM_SIG_RX_TEMPMEAS_L1TEMP                                             101u
#define COM_SIG_RX_TEMPMEAS_MCUTEMP                                            102u
#define COM_SIG_RX_LOADSTATUS_LOADSTATUS                                         103u
#define COM_SIG_RX_L1_I2T_COUNTER_I2TCOUNTER                                         104u
#define COM_SIG_RX_ERROR_706_DTCID_706                                          105u
#define COM_SIG_RX_ERROR_704_DTCID_704                                          106u
#define COM_SIG_RX_ERROR_702_DTCID_702                                          107u
#define COM_SIG_RX_ERROR_700_DTCID_700                                          108u
#define COM_SIG_RX_BATTSOCSOH_SOH                                                109u
#define COM_SIG_RX_BATTSOCSOH_SOCHYBRID                                          110u
#define COM_SIG_RX_BATTSOC_SOCOCV                                             111u
#define COM_SIG_RX_BATTSOC_SOCCOULOMB                                         112u
#define COM_SIG_RX_BATTDIAGNOSIS_WEAKBATTERY                                        113u
#define COM_SIG_RX_BATTDIAGNOSIS_VALIDOCVCALIBRATION                                114u
#define COM_SIG_RX_BATTDIAGNOSIS_DISCHARGING                                        115u
#define COM_SIG_RX_BATTDIAGNOSIS_DEEPDISCHARGE                                      116u
#define COM_SIG_RX_BATTDIAGNOSIS_CRANKINGEVENT                                      117u
#define COM_SIG_RX_BATTDIAGNOSIS_CONFINDENCE                                        118u
#define COM_SIG_RX_BATTDIAGNOSIS_CHARGING                                           119u
#define COM_SIG_RX_BATTDIAGNOSIS_BATTREST                                           120u
#define COM_SIG_RX_BATTCURRENT_AVGDISCHARGECURRENT                                121u
#define COM_SIG_RX_BATTCURRENT_AVGCHARGECURRENT                                   122u
#define COM_SIG_RX_BATTCAPDISCHARGE_DISCHARGEAH                                        123u
#define COM_SIG_RX_BATTCAPDISCHARGE_CHARGEAH                                           124u
#define COM_SIG_RX_BATTCAPRES_INTRESISTANCE                                      125u
#define COM_SIG_RX_BATTCAPRES_CAPACITYAH                                         126u

/* DBC generated CAN-FD COM I-PDU IDs and Signal IDs. Existing CAN Classic IDs above are untouched. */
#define COM_TX_PDU_CANFD_INFOTAINMENTDATA1                                       100u
#define COM_TX_PDU_CANFD_ENERGYMANAGEMENTDATA2                                   101u
#define COM_TX_PDU_CANFD_ENERGYMANAGEMENTDATA1                                   102u
#define COM_TX_PDU_CANFD_VEHICLESTATE                                            103u
#define COM_TX_PDU_CANFD_NM3                                                     104u
#define COM_TX_PDU_CANFD_SDAT                                                    105u
#define COM_TX_PDU_CANFD_LIGHTDATA1                                              106u
#define COM_TX_PDU_CANFD_BODYDATA1                                               109u
#define COM_TX_PDU_CANFD_PDM1_DIAGREQUEST                                        110u
#define COM_TX_PDU_CANFD_COMMANDLOAD_PDM1                                        114u
#define COM_TX_PDU_CANFD_ENERGYMANAGEMENTDATA3                                   118u
#define COM_RX_PDU_CANFD_PDM1_LOADSTATUS                                         103u
#define COM_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_1                                  104u
#define COM_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_2                                  105u
#define COM_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_3                                  106u
#define COM_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_4                                  107u
#define COM_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_5                                  108u
#define COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_1                                  109u
#define COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_2                                  110u
#define COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_3                                  111u
#define COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_4                                  112u
#define COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_5                                  113u
#define COM_RX_PDU_CANFD_PDM1_STUCKATONEVENT                                     114u
#define COM_RX_PDU_CANFD_PDM1_STUCKATOFFEVENT                                    115u
#define COM_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_1                              116u
#define COM_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_2                              117u
#define COM_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_3                              118u
#define COM_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_4                              119u
#define COM_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_5                              120u
#define COM_RX_PDU_CANFD_PDM1_DIAGRESPONSE                                       172u
#define COM_SIG_TX_CANFD_INFOTAINMENTDATA1_IT1_MILEAGETRIP                                                   1000u
#define COM_SIG_TX_CANFD_INFOTAINMENTDATA1_IT1_MILEAGETOTAL                                                  1001u
#define COM_SIG_TX_CANFD_INFOTAINMENTDATA1_IT1_DISPLAYCAMERASTATUS                                           1002u
#define COM_SIG_TX_CANFD_INFOTAINMENTDATA1_IT1_COMMANDDISPLAYERRORMESSAGE                                    1003u
#define COM_SIG_TX_CANFD_INFOTAINMENTDATA1_IT1_ALIVEINDICATION                                               1004u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_WEAKBATTERY                                                   1005u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_VALIDOCVCALIBRATION                                           1006u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_LOADSTATUS                                                    1007u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_DISCHARGING                                                   1008u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_DEEPDISCHARGE                                                 1009u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_CRANKINGEVENT                                                 1010u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_CONFINDENCE                                                   1011u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_CHARGING                                                      1012u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_CAPACITYAH                                                    1013u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_BATTREST                                                      1014u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_TIMETOFULL                                                    1015u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_T30VOLTAGE                                                    1016u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_SOH                                                           1017u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_SOCOCV                                                        1018u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_SOCHYBRID                                                     1019u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_SOCCOULOMB                                                    1020u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_RUNTIMEREMAINING                                              1021u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_MCUTEMP                                                       1022u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_L1VOLTAGE                                                     1023u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_L1TEMP                                                        1024u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_L1ISENSE                                                      1025u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_INTRESISTANCE                                                 1026u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_I2TCOUNTER                                                    1027u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_DISCHARGEAH                                                   1028u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_CHARGEAH                                                      1029u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_AVGDISCHARGECURRENT                                           1030u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_AVGCHARGECURRENT                                              1031u
#define COM_SIG_TX_CANFD_VEHICLESTATE_VEHICLESTATUS                                                     1032u
#define COM_SIG_TX_CANFD_NM3_NM3_PN1                                                           1033u
#define COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_YEAR                                                   1034u
#define COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_SECOND                                                 1035u
#define COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_MONTH                                                  1036u
#define COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_MINUTE                                                 1037u
#define COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_MILLISECOND                                            1038u
#define COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_HOUR                                                   1039u
#define COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_DAY                                                    1040u
#define COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_TIMESOURCE                                             2303u
#define COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_UTCVALID                                               2304u
#define COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_DEFAULTTIME                                            2305u
#define COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_NVMRESTORED                                            2307u
#define COM_SIG_TX_CANFD_LIGHTDATA1_LD1_TURNSIGNALSSTATUS                                             1041u
#define COM_SIG_TX_CANFD_LIGHTDATA1_LD1_TURNSIGNALCOMMAND                                             1042u
#define COM_SIG_TX_CANFD_LIGHTDATA1_LD1_REVERSELIGTHSTATUS                                            1043u
#define COM_SIG_TX_CANFD_LIGHTDATA1_LD1_POSITIONLIGHTSTATUS                                           1044u
#define COM_SIG_TX_CANFD_LIGHTDATA1_LD1_LOWBEAMSTATUS                                                 1045u
#define COM_SIG_TX_CANFD_LIGHTDATA1_LD1_INTERIORLIGHTSTATUS                                           1046u
#define COM_SIG_TX_CANFD_LIGHTDATA1_LD1_HIGHBEAMSTATUS                                                1047u
#define COM_SIG_TX_CANFD_LIGHTDATA1_LD1_FOGLIGHTSSTATUS                                               1048u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_WIPERSTOCKCOMMAND                                             1079u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_VIBRATIONSENSORSTATUS                                         1080u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_STATUSWIPERS                                                  1081u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_STATUSDOORS                                                   1082u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_RLSCOMMAND                                                    1083u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_RECIRCULATIONCOMMAND                                          1084u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_PDCDISTANCEREAR                                               1085u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_PDCDISTANCEFRONT                                              1086u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_PDCBUZZERFRONTREAR                                            1087u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_OUTSIDETEMPERATURE                                            1088u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_LIGTHSENSOR                                                   1089u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_IGNITION                                                      1090u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_HIGHBEAMCOMMAND                                               1091u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_HC05CONNECTIONSTATUS                                          1092u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_GEARBOX                                                       1093u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_FOGLIGHTSCOMMAND                                              1094u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_CLIMATETEMPERATURECOMMAND                                     1095u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_CLIMATEFANCOMMAND                                             1096u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_CLIMATEAUTOCOMMAND                                            1097u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_CENTRALLOCKSTATUS                                             1098u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_CENTRALLOCKCOMMAND                                            1099u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_CENTRALLOCKBUZZER                                             1100u
#define COM_SIG_TX_CANFD_BODYDATA1_BD1_BRAKELIGHTSTATUS                                              1101u
#define COM_SIG_TX_CANFD_PDM1_DIAGREQUEST_PDM1_DIAGREQ_BYTE00                                               1102u
#define COM_SIG_TX_CANFD_PDM1_DIAGREQUEST_PDM1_DIAGREQ_BYTE01                                               1103u
#define COM_SIG_TX_CANFD_PDM1_DIAGREQUEST_PDM1_DIAGREQ_BYTE02                                               1104u
#define COM_SIG_TX_CANFD_PDM1_DIAGREQUEST_PDM1_DIAGREQ_BYTE03                                               1105u
#define COM_SIG_TX_CANFD_PDM1_DIAGREQUEST_PDM1_DIAGREQ_BYTE04                                               1106u
#define COM_SIG_TX_CANFD_PDM1_DIAGREQUEST_PDM1_DIAGREQ_BYTE05                                               1107u
#define COM_SIG_TX_CANFD_PDM1_DIAGREQUEST_PDM1_DIAGREQ_BYTE06                                               1108u
#define COM_SIG_TX_CANFD_PDM1_DIAGREQUEST_PDM1_DIAGREQ_BYTE07                                               1109u
#define COM_SIG_TX_CANFD_COMMANDLOAD_PDM1_PDM1_COMMANDLOAD_01                                               1134u
#define COM_SIG_TX_CANFD_COMMANDLOAD_PDM1_PDM1_COMMANDLOAD_02                                               1135u
#define COM_SIG_TX_CANFD_COMMANDLOAD_PDM1_PDM1_COMMANDLOAD_03                                               1136u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_HVDCDC_RESPONSEERROR                                              1153u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_HVDCDC_LV_VOLTAGE                                                 1154u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_HVDCDC_LV_CURRENT                                                 1155u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_HVDCDC_HV_VOLTAGE                                                 1156u
#define COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_HVDCDC_HV_CURRENT                                                 1157u
#define COM_SIG_RX_CANFD_PDM1_LOADSTATUS_PDM1_LOADSTATUS_01                                                1172u
#define COM_SIG_RX_CANFD_PDM1_LOADSTATUS_PDM1_LOADSTATUS_02                                                1173u
#define COM_SIG_RX_CANFD_PDM1_LOADSTATUS_PDM1_LOADSTATUS_03                                                1174u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_001                                                   1175u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_002                                                   1176u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_003                                                   1177u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_004                                                   1178u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_005                                                   1179u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_006                                                   1180u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_007                                                   1181u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_008                                                   1182u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_009                                                   1183u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_010                                                   1184u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_011                                                   1185u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_012                                                   1186u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_013                                                   1187u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_014                                                   1188u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_015                                                   1189u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_1_PDM1_VOLTFB_016                                                   1190u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_017                                                   1191u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_018                                                   1192u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_019                                                   1193u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_020                                                   1194u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_021                                                   1195u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_022                                                   1196u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_023                                                   1197u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_024                                                   1198u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_025                                                   1199u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_026                                                   1200u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_027                                                   1201u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_028                                                   1202u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_029                                                   1203u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_030                                                   1204u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_031                                                   1205u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_2_PDM1_VOLTFB_032                                                   1206u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_033                                                   1207u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_034                                                   1208u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_035                                                   1209u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_036                                                   1210u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_037                                                   1211u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_038                                                   1212u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_039                                                   1213u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_040                                                   1214u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_041                                                   1215u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_042                                                   1216u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_043                                                   1217u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_044                                                   1218u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_045                                                   1219u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_046                                                   1220u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_047                                                   1221u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_3_PDM1_VOLTFB_048                                                   1222u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_049                                                   1223u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_050                                                   1224u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_051                                                   1225u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_052                                                   1226u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_053                                                   1227u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_054                                                   1228u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_055                                                   1229u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_056                                                   1230u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_057                                                   1231u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_058                                                   1232u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_059                                                   1233u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_060                                                   1234u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_061                                                   1235u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_062                                                   1236u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_063                                                   1237u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_4_PDM1_VOLTFB_064                                                   1238u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_5_PDM1_VOLTFB_065                                                   1239u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_5_PDM1_VOLTFB_066                                                   1240u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_5_PDM1_VOLTFB_067                                                   1241u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_5_PDM1_VOLTFB_068                                                   1242u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_5_PDM1_VOLTFB_069                                                   1243u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_5_PDM1_VOLTFB_070                                                   1244u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_5_PDM1_VOLTFB_071                                                   1245u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_5_PDM1_VOLTFB_072                                                   1246u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_5_PDM1_VOLTFB_073                                                   1247u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_5_PDM1_VOLTFB_074                                                   1248u
#define COM_SIG_RX_CANFD_PDM1_VOLTAGEFEEDBACK_5_PDM1_VOLTFB_075                                                   1249u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_001                                                   1250u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_002                                                   1251u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_003                                                   1252u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_004                                                   1253u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_005                                                   1254u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_006                                                   1255u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_007                                                   1256u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_008                                                   1257u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_009                                                   1258u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_010                                                   1259u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_011                                                   1260u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_012                                                   1261u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_013                                                   1262u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_014                                                   1263u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_015                                                   1264u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_016                                                   1265u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_017                                                   1266u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_018                                                   1267u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_019                                                   1268u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_020                                                   1269u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_021                                                   1270u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_022                                                   1271u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_023                                                   1272u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_024                                                   1273u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_025                                                   1274u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_026                                                   1275u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_027                                                   1276u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_028                                                   1277u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_029                                                   1278u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_030                                                   1279u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_031                                                   1280u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_2_PDM1_CURRFB_032                                                   1281u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_033                                                   1282u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_034                                                   1283u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_035                                                   1284u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_036                                                   1285u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_037                                                   1286u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_038                                                   1287u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_039                                                   1288u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_040                                                   1289u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_041                                                   1290u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_042                                                   1291u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_043                                                   1292u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_044                                                   1293u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_045                                                   1294u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_046                                                   1295u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_047                                                   1296u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_3_PDM1_CURRFB_048                                                   1297u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_049                                                   1298u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_050                                                   1299u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_051                                                   1300u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_052                                                   1301u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_053                                                   1302u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_054                                                   1303u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_055                                                   1304u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_056                                                   1305u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_057                                                   1306u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_058                                                   1307u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_059                                                   1308u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_060                                                   1309u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_061                                                   1310u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_062                                                   1311u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_063                                                   1312u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_4_PDM1_CURRFB_064                                                   1313u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_065                                                   1314u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_066                                                   1315u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_067                                                   1316u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_068                                                   1317u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_069                                                   1318u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_070                                                   1319u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_071                                                   1320u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_072                                                   1321u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_073                                                   1322u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_074                                                   1323u
#define COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_075                                                   1324u
#define COM_SIG_RX_CANFD_PDM1_STUCKATONEVENT_PDM1_STUCKATON_01                                                 1325u
#define COM_SIG_RX_CANFD_PDM1_STUCKATONEVENT_PDM1_STUCKATON_02                                                 1326u
#define COM_SIG_RX_CANFD_PDM1_STUCKATONEVENT_PDM1_STUCKATON_03                                                 1327u
#define COM_SIG_RX_CANFD_PDM1_STUCKATOFFEVENT_PDM1_STUCKATOFF_01                                                1328u
#define COM_SIG_RX_CANFD_PDM1_STUCKATOFFEVENT_PDM1_STUCKATOFF_02                                                1329u
#define COM_SIG_RX_CANFD_PDM1_STUCKATOFFEVENT_PDM1_STUCKATOFF_03                                                1330u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_001                                                   1331u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_002                                                   1332u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_003                                                   1333u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_004                                                   1334u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_005                                                   1335u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_006                                                   1336u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_007                                                   1337u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_008                                                   1338u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_009                                                   1339u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_010                                                   1340u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_011                                                   1341u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_012                                                   1342u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_013                                                   1343u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_014                                                   1344u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_015                                                   1345u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_1_PDM1_TEMPFB_016                                                   1346u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_017                                                   1347u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_018                                                   1348u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_019                                                   1349u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_020                                                   1350u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_021                                                   1351u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_022                                                   1352u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_023                                                   1353u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_024                                                   1354u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_025                                                   1355u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_026                                                   1356u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_027                                                   1357u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_028                                                   1358u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_029                                                   1359u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_030                                                   1360u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_031                                                   1361u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_2_PDM1_TEMPFB_032                                                   1362u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_033                                                   1363u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_034                                                   1364u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_035                                                   1365u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_036                                                   1366u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_037                                                   1367u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_038                                                   1368u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_039                                                   1369u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_040                                                   1370u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_041                                                   1371u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_042                                                   1372u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_043                                                   1373u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_044                                                   1374u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_045                                                   1375u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_046                                                   1376u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_047                                                   1377u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_3_PDM1_TEMPFB_048                                                   1378u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_049                                                   1379u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_050                                                   1380u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_051                                                   1381u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_052                                                   1382u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_053                                                   1383u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_054                                                   1384u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_055                                                   1385u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_056                                                   1386u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_057                                                   1387u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_058                                                   1388u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_059                                                   1389u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_060                                                   1390u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_061                                                   1391u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_062                                                   1392u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_063                                                   1393u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_4_PDM1_TEMPFB_064                                                   1394u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_5_PDM1_TEMPFB_065                                                   1395u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_5_PDM1_TEMPFB_066                                                   1396u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_5_PDM1_TEMPFB_067                                                   1397u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_5_PDM1_TEMPFB_068                                                   1398u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_5_PDM1_TEMPFB_069                                                   1399u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_5_PDM1_TEMPFB_070                                                   1400u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_5_PDM1_TEMPFB_071                                                   1401u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_5_PDM1_TEMPFB_072                                                   1402u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_5_PDM1_TEMPFB_073                                                   1403u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_5_PDM1_TEMPFB_074                                                   1404u
#define COM_SIG_RX_CANFD_PDM1_TEMPERATUREFEEDBACK_5_PDM1_TEMPFB_075                                                   1405u
#define COM_SIG_RX_CANFD_PDM1_DIAGRESPONSE_PDM1_DIAGRESP_BYTE00                                              2099u
#define COM_SIG_RX_CANFD_PDM1_DIAGRESPONSE_PDM1_DIAGRESP_BYTE01                                              2100u
#define COM_SIG_RX_CANFD_PDM1_DIAGRESPONSE_PDM1_DIAGRESP_BYTE02                                              2101u
#define COM_SIG_RX_CANFD_PDM1_DIAGRESPONSE_PDM1_DIAGRESP_BYTE03                                              2102u
#define COM_SIG_RX_CANFD_PDM1_DIAGRESPONSE_PDM1_DIAGRESP_BYTE04                                              2103u
#define COM_SIG_RX_CANFD_PDM1_DIAGRESPONSE_PDM1_DIAGRESP_BYTE05                                              2104u
#define COM_SIG_RX_CANFD_PDM1_DIAGRESPONSE_PDM1_DIAGRESP_BYTE06                                              2105u
#define COM_SIG_RX_CANFD_PDM1_DIAGRESPONSE_PDM1_DIAGRESP_BYTE07                                              2106u

/* DBC/LDF generated LIN COM I-PDU IDs and Signal IDs. */
#define COM_TX_PDU_LIN_ZGW_REQUEST_HVDCDC                  202u
#define COM_RX_PDU_LIN_HVDCDC_STATUS                       201u
#define COM_SIG_TX_LIN_ZGW_REQUEST_HVDCDC_ZGW_ENABLE_HVDCDC                                  2205u
#define COM_SIG_TX_LIN_ZGW_REQUEST_HVDCDC_ZGW_TARGETVOLTAGE_HVDCDC                           2206u
#define COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_RESPONSEERROR                               2215u
#define COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_LV_VOLTAGE                                  2216u
#define COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_LV_CURRENT                                  2217u
#define COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_HV_VOLTAGE                                  2218u
#define COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_HV_CURRENT                                  2219u

#endif
