#include "CanIf.h"
#include "CanTp.h"
#include "CanSM.h"
#include "PduR.h"
#include <string.h>

static uint8 CanIf_BusOffState[CAN_NUM_CONTROLLERS];
static uint8 CanIf_ErrorPassiveState[CAN_NUM_CONTROLLERS];
static uint8 CanIf_ErrorWarningState[CAN_NUM_CONTROLLERS];

/* CAN-TP transmit uses the normal asynchronous Can_MainFunction path. Keep a
 * small synchronous drain only as a latency fast path; a large spin here can
 * starve DoIP/Core0 while FCD sends dense routed coding bursts. */
#define CANIF_CANTP_TX_POLL_LIMIT 16u

volatile uint32 CanIf_ComTxAsyncQueuedCounter = 0u;
volatile uint32 CanIf_ComTxAsyncRejectedCounter = 0u;

static const CanIf_RxPduConfigType CanIf_RxPduConfig[] =
{
    { CANIF_PDU_CLASSIC_PHYS_RX, CAN_CONTROLLER_CLASSIC, 0x710u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 1u, 8u,  CANIF_RX_TARGET_CANTP },
    { CANIF_PDU_FD_PHYS_RX,      CAN_CONTROLLER_FD,      0x710u, CAN_ID_STANDARD, CAN_FRAME_FD,      1u, 64u, CANIF_RX_TARGET_CANTP },
    { CANIF_RX_PDU_CENTRALLOCKDATA                         , CAN_CONTROLLER_CLASSIC, 0x253u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 2u, 2u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_LIGHTDATA1                              , CAN_CONTROLLER_CLASSIC, 0x252u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 3u, 3u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_STATUSACTUATOR                          , CAN_CONTROLLER_CLASSIC, 0x240u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 2u, 2u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_OUTSIDETEMPERATURESTATUS                , CAN_CONTROLLER_CLASSIC, 0x251u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 4u, 4u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CENTRALCOMMAND1                         , CAN_CONTROLLER_CLASSIC, 0x250u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_DMUSTATUS                               , CAN_CONTROLLER_CLASSIC, 0x201u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 1u, 1u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ENGINEDATA7                             , CAN_CONTROLLER_CLASSIC, 0x086u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_BATTFULLSTAT                            , CAN_CONTROLLER_CLASSIC, 0x6F6u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ENGINEDATA6                             , CAN_CONTROLLER_CLASSIC, 0x085u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ENGINEDATA5                             , CAN_CONTROLLER_CLASSIC, 0x084u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ENGINEDATA4                             , CAN_CONTROLLER_CLASSIC, 0x083u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ENGINEDATA3                             , CAN_CONTROLLER_CLASSIC, 0x082u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ENGINEDATA2                             , CAN_CONTROLLER_CLASSIC, 0x081u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_DSCDATA3                                , CAN_CONTROLLER_CLASSIC, 0x092u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_DSCDATA2                                , CAN_CONTROLLER_CLASSIC, 0x091u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_DSCDATA1                                , CAN_CONTROLLER_CLASSIC, 0x090u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 5u, 5u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ENGINEDATA1                             , CAN_CONTROLLER_CLASSIC, 0x080u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ASGDATA1                                , CAN_CONTROLLER_CLASSIC, 0x100u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 2u, 2u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_PDCSTAT                                 , CAN_CONTROLLER_CLASSIC, 0x10Du, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 3u, 3u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_MILEAGE                                 , CAN_CONTROLLER_CLASSIC, 0x299u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_DMU_ALIVE                               , CAN_CONTROLLER_CLASSIC, 0x200u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 1u, 1u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_XCPRESPONSE_7C7                         , CAN_CONTROLLER_CLASSIC, 0x7C7u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_XCPRESPONSE_7C5                         , CAN_CONTROLLER_CLASSIC, 0x7C5u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_XCPRESPONSE_7C3                         , CAN_CONTROLLER_CLASSIC, 0x7C3u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_XCPRESPONSE_7C1                         , CAN_CONTROLLER_CLASSIC, 0x7C1u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_VOLTAGECURRENT                          , CAN_CONTROLLER_CLASSIC, 0x6EFu, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_TEMPMEAS                                , CAN_CONTROLLER_CLASSIC, 0x6EEu, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_LOADSTATUS                              , CAN_CONTROLLER_CLASSIC, 0x051u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 1u, 1u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_L1_I2T_COUNTER                          , CAN_CONTROLLER_CLASSIC, 0x6F0u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 4u, 4u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ERROR_706                               , CAN_CONTROLLER_CLASSIC, 0x753u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 4u, 4u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ERROR_704                               , CAN_CONTROLLER_CLASSIC, 0x752u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 4u, 4u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ERROR_702                               , CAN_CONTROLLER_CLASSIC, 0x751u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 4u, 4u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_ERROR_700                               , CAN_CONTROLLER_CLASSIC, 0x750u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 4u, 4u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_DIAGREQUEST_710                         , CAN_CONTROLLER_CLASSIC, 0x710u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_DIAGRESPONSE_707                        , CAN_CONTROLLER_CLASSIC, 0x707u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_DIAGRESPONSE_705                        , CAN_CONTROLLER_CLASSIC, 0x705u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_DIAGRESPONSE_703                        , CAN_CONTROLLER_CLASSIC, 0x703u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_DIAGRESPONSE_701                        , CAN_CONTROLLER_CLASSIC, 0x701u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_BATTSOCSOH                              , CAN_CONTROLLER_CLASSIC, 0x6F3u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_BATTSOC                                 , CAN_CONTROLLER_CLASSIC, 0x6F2u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_BATTDIAGNOSIS                           , CAN_CONTROLLER_CLASSIC, 0x6F7u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 1u, 1u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_BATTCURRENT                             , CAN_CONTROLLER_CLASSIC, 0x6F5u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_BATTCAPDISCHARGE                        , CAN_CONTROLLER_CLASSIC, 0x6F1u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_BATTCAPRES                              , CAN_CONTROLLER_CLASSIC, 0x6F4u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_LOADSTATUS                                   , CAN_CONTROLLER_FD,      0x093u, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_LOADSTATUS                                   , CAN_CONTROLLER_FD,      0x092u, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_LOADSTATUS                                   , CAN_CONTROLLER_FD,      0x091u, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_LOADSTATUS                                   , CAN_CONTROLLER_FD,      0x090u, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_1                            , CAN_CONTROLLER_FD,      0x300u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_2                            , CAN_CONTROLLER_FD,      0x301u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_3                            , CAN_CONTROLLER_FD,      0x302u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_4                            , CAN_CONTROLLER_FD,      0x303u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_5                            , CAN_CONTROLLER_FD,      0x304u, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_1                            , CAN_CONTROLLER_FD,      0x305u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_2                            , CAN_CONTROLLER_FD,      0x306u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_3                            , CAN_CONTROLLER_FD,      0x307u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_4                            , CAN_CONTROLLER_FD,      0x308u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_5                            , CAN_CONTROLLER_FD,      0x309u, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_STUCKATONEVENT                               , CAN_CONTROLLER_FD,      0x30Au, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_STUCKATOFFEVENT                              , CAN_CONTROLLER_FD,      0x30Bu, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_1                        , CAN_CONTROLLER_FD,      0x30Cu, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_2                        , CAN_CONTROLLER_FD,      0x30Du, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_3                        , CAN_CONTROLLER_FD,      0x30Eu, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_4                        , CAN_CONTROLLER_FD,      0x30Fu, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_5                        , CAN_CONTROLLER_FD,      0x310u, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_1                            , CAN_CONTROLLER_FD,      0x311u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_2                            , CAN_CONTROLLER_FD,      0x312u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_3                            , CAN_CONTROLLER_FD,      0x313u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_4                            , CAN_CONTROLLER_FD,      0x314u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_5                            , CAN_CONTROLLER_FD,      0x315u, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_1                            , CAN_CONTROLLER_FD,      0x316u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_2                            , CAN_CONTROLLER_FD,      0x317u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_3                            , CAN_CONTROLLER_FD,      0x318u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_4                            , CAN_CONTROLLER_FD,      0x319u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_5                            , CAN_CONTROLLER_FD,      0x31Au, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_STUCKATONEVENT                               , CAN_CONTROLLER_FD,      0x31Bu, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_STUCKATOFFEVENT                              , CAN_CONTROLLER_FD,      0x31Cu, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_1                        , CAN_CONTROLLER_FD,      0x31Du, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_2                        , CAN_CONTROLLER_FD,      0x31Eu, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_3                        , CAN_CONTROLLER_FD,      0x31Fu, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_4                        , CAN_CONTROLLER_FD,      0x320u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_5                        , CAN_CONTROLLER_FD,      0x321u, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_1                            , CAN_CONTROLLER_FD,      0x322u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_2                            , CAN_CONTROLLER_FD,      0x323u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_3                            , CAN_CONTROLLER_FD,      0x324u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_4                            , CAN_CONTROLLER_FD,      0x325u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_5                            , CAN_CONTROLLER_FD,      0x326u, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_1                            , CAN_CONTROLLER_FD,      0x327u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_2                            , CAN_CONTROLLER_FD,      0x328u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_3                            , CAN_CONTROLLER_FD,      0x329u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_4                            , CAN_CONTROLLER_FD,      0x32Au, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_5                            , CAN_CONTROLLER_FD,      0x32Bu, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_STUCKATONEVENT                               , CAN_CONTROLLER_FD,      0x32Cu, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_STUCKATOFFEVENT                              , CAN_CONTROLLER_FD,      0x32Du, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_1                        , CAN_CONTROLLER_FD,      0x32Eu, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_2                        , CAN_CONTROLLER_FD,      0x32Fu, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_3                        , CAN_CONTROLLER_FD,      0x330u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_4                        , CAN_CONTROLLER_FD,      0x331u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_5                        , CAN_CONTROLLER_FD,      0x332u, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_1                            , CAN_CONTROLLER_FD,      0x333u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_2                            , CAN_CONTROLLER_FD,      0x334u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_3                            , CAN_CONTROLLER_FD,      0x335u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_4                            , CAN_CONTROLLER_FD,      0x336u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_5                            , CAN_CONTROLLER_FD,      0x337u, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_1                            , CAN_CONTROLLER_FD,      0x338u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_2                            , CAN_CONTROLLER_FD,      0x339u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_3                            , CAN_CONTROLLER_FD,      0x33Au, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_4                            , CAN_CONTROLLER_FD,      0x33Bu, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_5                            , CAN_CONTROLLER_FD,      0x33Cu, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_STUCKATONEVENT                               , CAN_CONTROLLER_FD,      0x33Du, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_STUCKATOFFEVENT                              , CAN_CONTROLLER_FD,      0x33Eu, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, 12u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_1                        , CAN_CONTROLLER_FD,      0x33Fu, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_2                        , CAN_CONTROLLER_FD,      0x340u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_3                        , CAN_CONTROLLER_FD,      0x341u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_4                        , CAN_CONTROLLER_FD,      0x342u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_5                        , CAN_CONTROLLER_FD,      0x343u, CAN_ID_STANDARD, CAN_FRAME_FD,      48u, 48u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM1_DIAGRESPONSE                                 , CAN_CONTROLLER_FD,      0x721u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM2_DIAGRESPONSE                                 , CAN_CONTROLLER_FD,      0x723u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM3_DIAGRESPONSE                                 , CAN_CONTROLLER_FD,      0x725u, CAN_ID_STANDARD, CAN_FRAME_FD,      8u, 8u, CANIF_RX_TARGET_COM },
    { CANIF_RX_PDU_CANFD_PDM4_DIAGRESPONSE                                 , CAN_CONTROLLER_FD,      0x727u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, 64u, CANIF_RX_TARGET_COM }
};

static const CanIf_TxPduConfigType CanIf_TxPduConfig[] =
{
    { CANIF_PDU_CLASSIC_PHYS_TX,      CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, CANIF_DIAGREQUEST_TESTER_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u,  CANIF_TX_TARGET_CANTP },
    { CANIF_PDU_FD_PHYS_TX,           CAN_HTH_FD,      CAN_CONTROLLER_FD,      CANIF_DIAGREQUEST_TESTER_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, CANIF_TX_TARGET_CANTP },
    { CANIF_PDU_CLASSIC_EXT_PHYS_TX0, CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, CANIF_DIAGREQUEST_TESTER_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u,  CANIF_TX_TARGET_CANTP },
    { CANIF_PDU_CLASSIC_EXT_PHYS_TX1, CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, CANIF_DIAGREQUEST_TESTER_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u,  CANIF_TX_TARGET_CANTP },
    { CANIF_PDU_FD_EXT_PHYS_TX0,      CAN_HTH_FD,      CAN_CONTROLLER_FD,      CANIF_DIAGREQUEST_TESTER_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, CANIF_TX_TARGET_CANTP },
    { CANIF_PDU_FD_EXT_PHYS_TX1,      CAN_HTH_FD,      CAN_CONTROLLER_FD,      CANIF_DIAGREQUEST_TESTER_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, CANIF_TX_TARGET_CANTP },
    { CANIF_PDU_CLASSIC_EXT_PHYS_TX2, CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, CANIF_DIAGREQUEST_TESTER_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u,  CANIF_TX_TARGET_CANTP },
    { CANIF_PDU_CLASSIC_EXT_PHYS_TX3, CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, CANIF_DIAGREQUEST_TESTER_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u,  CANIF_TX_TARGET_CANTP },
    { CANIF_PDU_FD_EXT_PHYS_TX2,      CAN_HTH_FD,      CAN_CONTROLLER_FD,      CANIF_DIAGREQUEST_TESTER_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, CANIF_TX_TARGET_CANTP },
    { CANIF_PDU_FD_EXT_PHYS_TX3,      CAN_HTH_FD,      CAN_CONTROLLER_FD,      CANIF_DIAGREQUEST_TESTER_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, CANIF_TX_TARGET_CANTP },
    { CANIF_TX_PDU_VEHICLESTATE                            , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x3A0u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 1u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_DISPLAYOUTTEMP                          , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x221u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 4u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_STATUSBODYDATA1                         , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x040u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 4u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_COMMANDDISPLAYSTATUS                    , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x220u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 6u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_XCPREQUEST_7C8                          , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x7C8u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_XCPREQUEST_7C6                          , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x7C6u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_XCPREQUEST_7C4                          , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x7C4u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_XCPREQUEST_7C2                          , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x7C2u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_XCPREQUEST_7C0                          , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x7C0u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_SDAT                                    , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x202u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 7u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_NM3                                     , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x3FFu, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 1u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_LOADREQUEST                             , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x050u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 2u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_DIAGREQUEST_706                         , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, CANIF_DIAGREQUEST_CLASSIC_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_DIAGREQUEST_704                         , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, CANIF_DIAGREQUEST_CLASSIC_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_DIAGREQUEST_702                         , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, CANIF_DIAGREQUEST_CLASSIC_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_DIAGREQUEST_700                         , CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, CANIF_DIAGREQUEST_CLASSIC_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_INFOTAINMENTDATA1                                 , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x106u, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_ENERGYMANAGEMENTDATA2                             , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x105u, CAN_ID_STANDARD, CAN_FRAME_FD,      6u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_ENERGYMANAGEMENTDATA1                             , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x104u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_VEHICLESTATE                                      , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x3A0u, CAN_ID_STANDARD, CAN_FRAME_FD,      1u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_NM3                                               , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x3FFu, CAN_ID_STANDARD, CAN_FRAME_FD,      1u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_SDAT                                              , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x202u, CAN_ID_STANDARD, CAN_FRAME_FD,      7u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_LIGHTDATA1                                        , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x103u, CAN_ID_STANDARD, CAN_FRAME_FD,      3u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_POWERTRAINDATA2                                   , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x102u, CAN_ID_STANDARD, CAN_FRAME_FD,      16u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_POWERTRAINDATA1                                   , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x101u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_BODYDATA1                                         , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x100u, CAN_ID_STANDARD, CAN_FRAME_FD,      20u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_PDM1_DIAGREQUEST                                  , CAN_HTH_FD,      CAN_CONTROLLER_FD,      CANIF_DIAGREQUEST_CANFD_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_PDM2_DIAGREQUEST                                  , CAN_HTH_FD,      CAN_CONTROLLER_FD,      CANIF_DIAGREQUEST_CANFD_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_PDM3_DIAGREQUEST                                  , CAN_HTH_FD,      CAN_CONTROLLER_FD,      CANIF_DIAGREQUEST_CANFD_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_FD,      8u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_PDM4_DIAGREQUEST                                  , CAN_HTH_FD,      CAN_CONTROLLER_FD,      CANIF_DIAGREQUEST_CANFD_CAN_ID, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_COMMANDLOAD_PDM1                                  , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x080u, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_COMMANDLOAD_PDM2                                  , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x081u, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_COMMANDLOAD_PDM3                                  , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x082u, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_COMMANDLOAD_PDM4                                  , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x083u, CAN_ID_STANDARD, CAN_FRAME_FD,      12u, CANIF_TX_TARGET_COM },
    { CANIF_TX_PDU_CANFD_ENERGYMANAGEMENTDATA3                             , CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x107u, CAN_ID_STANDARD, CAN_FRAME_FD,      20u, CANIF_TX_TARGET_COM }
};

const CanIf_ConfigType CanIf_Config =
{
    CanIf_RxPduConfig,
    (uint8)(sizeof(CanIf_RxPduConfig) / sizeof(CanIf_RxPduConfig[0])),
    CanIf_TxPduConfig,
    (uint8)(sizeof(CanIf_TxPduConfig) / sizeof(CanIf_TxPduConfig[0]))
};

static const CanIf_ConfigType* CanIf_ConfigPtr;
static uint8 CanIf_PduMode[CAN_NUM_CONTROLLERS];

static const CanIf_TxPduConfigType* CanIf_FindTxPdu(PduIdType CanIfTxSduId)
{
    uint8 i;

    if (CanIf_ConfigPtr == NULL_PTR)
    {
        return NULL_PTR;
    }

    for (i = 0u; i < CanIf_ConfigPtr->numTxPdus; i++)
    {
        if (CanIf_ConfigPtr->txPdus[i].txPduId == CanIfTxSduId)
        {
            return &CanIf_ConfigPtr->txPdus[i];
        }
    }

    return NULL_PTR;
}

void CanIf_Init(const CanIf_ConfigType* ConfigPtr)
{
    uint8 i;

    CanIf_ConfigPtr = (ConfigPtr == NULL_PTR) ? &CanIf_Config : ConfigPtr;

    for (i = 0u; i < CAN_NUM_CONTROLLERS; i++)
    {
        CanIf_PduMode[i] = CANIF_PDU_MODE_ONLINE;
        CanIf_BusOffState[i] = FALSE;
        CanIf_ErrorPassiveState[i] = FALSE;
        CanIf_ErrorWarningState[i] = FALSE;
    }
}

Std_ReturnType CanIf_SetPduMode(uint8 ControllerId, uint8 PduMode)
{
    if ((ControllerId >= CAN_NUM_CONTROLLERS) || (PduMode > CANIF_PDU_MODE_ONLINE))
    {
        return E_NOT_OK;
    }

    CanIf_PduMode[ControllerId] = PduMode;
    return E_OK;
}

uint8 CanIf_GetPduMode(uint8 ControllerId)
{
    if (ControllerId >= CAN_NUM_CONTROLLERS)
    {
        return CANIF_PDU_MODE_OFFLINE;
    }

    return CanIf_PduMode[ControllerId];
}

Std_ReturnType CanIf_Transmit(PduIdType CanIfTxSduId, const uint8* data, PduLengthType len)
{
    const CanIf_TxPduConfigType* cfg;
    Can_PduType pdu;
    Std_ReturnType writeResult;

    if ((data == NULL_PTR) || (len == 0u))
    {
        return E_NOT_OK;
    }

    cfg = CanIf_FindTxPdu(CanIfTxSduId);

    if (cfg == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (cfg->controllerId >= CAN_NUM_CONTROLLERS)
    {
        return E_NOT_OK;
    }

    if (CanIf_BusOffState[cfg->controllerId] != FALSE)
    {
        return E_NOT_OK;
    }

    if (((CanIf_PduMode[cfg->controllerId] & CANIF_PDU_MODE_TX_ONLINE) == 0u) &&
        (cfg->target != CANIF_TX_TARGET_CANTP))
    {
        return E_NOT_OK;
    }

    if (cfg->target == CANIF_TX_TARGET_CANTP)
    {
        if (len > cfg->fixedDlc)
        {
            return E_NOT_OK;
        }
    }
    else if (len != cfg->fixedDlc)
    {
        return E_NOT_OK;
    }

    if ((cfg->frameType == CAN_FRAME_CLASSIC) && (len > CAN_CLASSIC_MAX_DLC))
    {
        return E_NOT_OK;
    }

    if ((cfg->frameType == CAN_FRAME_FD) && (len > CANFD_MAX_DLC))
    {
        return E_NOT_OK;
    }

    pdu.swPduHandle = CanIfTxSduId;
    pdu.id = cfg->canId;
    pdu.idType = cfg->idType;
    pdu.controllerId = cfg->controllerId;
    pdu.frameType = cfg->frameType;
    pdu.dlc = (uint8)len;
    pdu.sdu = data;

    if (cfg->target == CANIF_TX_TARGET_CANTP)
    {
        writeResult = Can_WriteNoReplace(cfg->hth, &pdu);
    }
    else
    {
        writeResult = Can_Write(cfg->hth, &pdu);
    }

    if (writeResult != E_OK)
    {
        if (cfg->target == CANIF_TX_TARGET_COM)
        {
            CanIf_ComTxAsyncRejectedCounter++;
        }

        return E_NOT_OK;
    }

    if (cfg->target == CANIF_TX_TARGET_CANTP)
    {
        (void)Can_DrainTxPdu(CanIfTxSduId, cfg->controllerId, CANIF_CANTP_TX_POLL_LIMIT);
    }

    if (cfg->target == CANIF_TX_TARGET_COM)
    {
        CanIf_ComTxAsyncQueuedCounter++;
    }

    return E_OK;
}

void CanIf_RxIndication(const Can_FrameType* frame)
{
    uint8 i;

    if ((CanIf_ConfigPtr == NULL_PTR) || (frame == NULL_PTR))
    {
        return;
    }

    if (frame->controllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    if ((CanIf_PduMode[frame->controllerId] & CANIF_PDU_MODE_RX_ONLINE) == 0u)
    {
        return;
    }

    for (i = 0u; i < CanIf_ConfigPtr->numRxPdus; i++)
    {
        const CanIf_RxPduConfigType* cfg = &CanIf_ConfigPtr->rxPdus[i];

        if ((cfg->controllerId == frame->controllerId) &&
            (cfg->canId == frame->id) &&
            (cfg->idType == frame->idType) &&
            (cfg->frameType == frame->frameType) &&
            (frame->dlc >= cfg->minDlc) &&
            (frame->dlc <= cfg->maxDlc))
        {
            if (cfg->target == CANIF_RX_TARGET_CANTP)
            {
                CanTp_RxIndication(cfg->rxPduId, frame->data, frame->dlc);
            }
            else
            {
                PduR_CanIfRxIndication(cfg->rxPduId, frame->data, frame->dlc);
            }
        }
    }
}

void CanIf_TxConfirmation(PduIdType CanIfTxSduId)
{
    const CanIf_TxPduConfigType* cfg;

    cfg = CanIf_FindTxPdu(CanIfTxSduId);

    if (cfg == NULL_PTR)
    {
        return;
    }

    if (cfg->target == CANIF_TX_TARGET_CANTP)
    {
        CanTp_TxConfirmation(CanIfTxSduId);
    }
    else
    {
        PduR_CanIfTxConfirmation(CanIfTxSduId);
    }
}

void CanIf_ControllerBusOff(uint8 ControllerId)
{
    if (ControllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    CanIf_BusOffState[ControllerId] = TRUE;
    CanIf_ErrorPassiveState[ControllerId] = FALSE;
    CanIf_ErrorWarningState[ControllerId] = FALSE;

    (void)CanIf_SetPduMode(ControllerId, CANIF_PDU_MODE_OFFLINE);

    CanSM_ControllerBusOff(ControllerId);
    CanIf_AppBusOff(ControllerId);
}

void CanIf_ControllerRecovered(uint8 ControllerId)
{
    if (ControllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    CanIf_BusOffState[ControllerId] = FALSE;
    CanIf_ErrorPassiveState[ControllerId] = FALSE;
    CanIf_ErrorWarningState[ControllerId] = FALSE;

    (void)CanIf_SetPduMode(ControllerId, CANIF_PDU_MODE_ONLINE);

    CanIf_AppControllerRecovered(ControllerId);
}

void CanIf_ControllerErrorPassive(uint8 ControllerId)
{
    if (ControllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    CanIf_ErrorPassiveState[ControllerId] = TRUE;
    CanIf_ErrorWarningState[ControllerId] = FALSE;

    CanIf_AppErrorPassive(ControllerId);
}

void CanIf_ControllerErrorWarning(uint8 ControllerId)
{
    if (ControllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    if (CanIf_ErrorPassiveState[ControllerId] == FALSE)
    {
        CanIf_ErrorWarningState[ControllerId] = TRUE;
    }

    CanIf_AppErrorWarning(ControllerId);
}

__attribute__((weak)) void CanIf_AppBusOff(uint8 ControllerId)
{
    (void)ControllerId;
}

__attribute__((weak)) void CanIf_AppControllerRecovered(uint8 ControllerId)
{
    (void)ControllerId;
}

__attribute__((weak)) void CanIf_AppErrorPassive(uint8 ControllerId)
{
    (void)ControllerId;
}

__attribute__((weak)) void CanIf_AppErrorWarning(uint8 ControllerId)
{
    (void)ControllerId;
}
