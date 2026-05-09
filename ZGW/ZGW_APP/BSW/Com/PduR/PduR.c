#include "PduR.h"

#include "CanIf.h"
#include "CanTp.h"
#include "Com.h"
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "DoIP.h"
#include "LinIf.h"
#include "LinTp.h"
#include "SoAd.h"

#include <string.h>

#ifndef TRUE
#define TRUE  1u
#endif

#ifndef FALSE
#define FALSE 0u
#endif

/* Keep these IDs aligned with EthStack_Cfg.c. */
#define PDUR_SOAD_SOCON_PDUR_IF_UDP     4u


/* ===================== Routing types ===================== */

typedef enum
{
    PDUR_IF_DEST_NONE = 0u,
    PDUR_IF_DEST_COM,
    PDUR_IF_DEST_CANIF,
    PDUR_IF_DEST_LINIF,
    PDUR_IF_DEST_SOAD
} PduR_IfDestType;

typedef enum
{
    PDUR_IF_LOWER_CANIF = 0u,
    PDUR_IF_LOWER_LINIF,
    PDUR_IF_LOWER_SOAD
} PduR_IfLowerType;

typedef enum
{
    PDUR_TP_LOWER_CANTP = 0u,
    PDUR_TP_LOWER_LINTP,
    PDUR_TP_LOWER_DOIP
} PduR_TpLowerType;

typedef struct
{
    uint8 enabled;
    PduIdType upperPduId;
    PduR_IfLowerType lower;
    PduIdType lowerPduId;
    uint8 soConId;
} PduR_ComTxRouteType;

typedef struct
{
    uint8 enabled;
    PduIdType sourcePduId;
    PduR_IfDestType dest;
    PduIdType destPduId;
    uint8 destSoConId;
} PduR_IfRxRouteType;

typedef struct
{
    uint8 enabled;
    uint8 sourceSoConId;
    PduR_IfDestType dest;
    PduIdType destPduId;
    uint8 destSoConId;
} PduR_SoAdRxRouteType;

typedef struct
{
    uint8 enabled;
    PduR_TpLowerType lower;
    PduIdType lowerRxPduId;
    PduIdType dcmRxPduId;
} PduR_TpRxRouteType;

typedef struct
{
    uint8 enabled;
    PduIdType dcmTxPduId;
    PduR_TpLowerType lower;
    PduIdType lowerTxPduId;
} PduR_DcmTxRouteType;

typedef struct
{
    uint8 buffer[PDUR_MAX_TP_BUFFER];
    PduLengthType expectedLen;
    PduLengthType offset;
    uint8 active;
} PduR_TpBufferType;

typedef struct
{
    uint8 valid;
    uint16 sourceAddress;
    uint16 targetAddress;
} PduR_DoIPContextType;

/* ===================== IF routes =====================
 *
 * COM_TX route:
 *   COM I-PDU -> lower IF PDU.
 *
 * Lower IF RX routes:
 *   lower IF PDU -> COM I-PDU or raw gateway to another lower IF.
 *
 * Keep actual signal interpretation out of PduR. PduR only copies I-PDU bytes.
 */

static const PduR_ComTxRouteType PduR_ComTxRoutes[] =
{
    { TRUE, COM_TX_PDU_VEHICLESTATE                                , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_VEHICLESTATE                            , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_DISPLAYOUTTEMP                              , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_DISPLAYOUTTEMP                          , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_STATUSBODYDATA1                             , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_STATUSBODYDATA1                         , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_COMMANDDISPLAYSTATUS                        , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_COMMANDDISPLAYSTATUS                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_XCPREQUEST_7C8                              , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_XCPREQUEST_7C8                          , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_XCPREQUEST_7C6                              , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_XCPREQUEST_7C6                          , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_XCPREQUEST_7C4                              , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_XCPREQUEST_7C4                          , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_XCPREQUEST_7C2                              , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_XCPREQUEST_7C2                          , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_XCPREQUEST_7C0                              , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_XCPREQUEST_7C0                          , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_SDAT                                        , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_SDAT                                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_NM3                                         , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_NM3                                     , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_LOADREQUEST                                 , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_LOADREQUEST                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_DIAGREQUEST_706                             , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_DIAGREQUEST_706                         , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_DIAGREQUEST_704                             , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_DIAGREQUEST_704                         , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_DIAGREQUEST_702                             , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_DIAGREQUEST_702                         , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_DIAGREQUEST_700                             , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_DIAGREQUEST_700                         , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_INFOTAINMENTDATA1                                      , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_INFOTAINMENTDATA1                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_ENERGYMANAGEMENTDATA2                                  , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_ENERGYMANAGEMENTDATA2                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_ENERGYMANAGEMENTDATA1                                  , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_ENERGYMANAGEMENTDATA1                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_VEHICLESTATE                                           , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_VEHICLESTATE                                      , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_NM3                                                    , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_NM3                                               , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_SDAT                                                   , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_SDAT                                              , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_LIGHTDATA1                                             , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_LIGHTDATA1                                        , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_POWERTRAINDATA2                                        , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_POWERTRAINDATA2                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_POWERTRAINDATA1                                        , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_POWERTRAINDATA1                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_BODYDATA1                                              , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_BODYDATA1                                         , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_PDM1_DIAGREQUEST                                       , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_PDM1_DIAGREQUEST                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_PDM2_DIAGREQUEST                                       , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_PDM2_DIAGREQUEST                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_PDM3_DIAGREQUEST                                       , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_PDM3_DIAGREQUEST                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_PDM4_DIAGREQUEST                                       , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_PDM4_DIAGREQUEST                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_COMMANDLOAD_PDM1                                       , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_COMMANDLOAD_PDM1                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_COMMANDLOAD_PDM2                                       , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_COMMANDLOAD_PDM2                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_COMMANDLOAD_PDM3                                       , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_COMMANDLOAD_PDM3                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_COMMANDLOAD_PDM4                                       , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_COMMANDLOAD_PDM4                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_CANFD_ENERGYMANAGEMENTDATA3                                  , PDUR_IF_LOWER_CANIF, CANIF_TX_PDU_CANFD_ENERGYMANAGEMENTDATA3                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_LIN_ZGW_NM3                            , PDUR_IF_LOWER_LINIF, LINIF_TX_PDU_ZGW_NM3               , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_LIN_ZGW_REQUEST_ALT                    , PDUR_IF_LOWER_LINIF, LINIF_TX_PDU_ZGW_REQUEST_ALT       , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_LIN_ZGW_REQUEST_HVDCDC                 , PDUR_IF_LOWER_LINIF, LINIF_TX_PDU_ZGW_REQUEST_HVDCDC    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, COM_TX_PDU_LIN_ZGW_REQUEST_PCU48                  , PDUR_IF_LOWER_LINIF, LINIF_TX_PDU_ZGW_REQUEST_PCU48     , PDUR_SOAD_INVALID_SOCON }
};

static const PduR_IfRxRouteType PduR_CanIfRxRoutes[] =
{
    { TRUE, CANIF_RX_PDU_CENTRALLOCKDATA                         , PDUR_IF_DEST_COM, COM_RX_PDU_CENTRALLOCKDATA                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_LIGHTDATA1                              , PDUR_IF_DEST_COM, COM_RX_PDU_LIGHTDATA1                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_STATUSACTUATOR                          , PDUR_IF_DEST_COM, COM_RX_PDU_STATUSACTUATOR                              , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_OUTSIDETEMPERATURESTATUS                , PDUR_IF_DEST_COM, COM_RX_PDU_OUTSIDETEMPERATURESTATUS                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CENTRALCOMMAND1                         , PDUR_IF_DEST_COM, COM_RX_PDU_CENTRALCOMMAND1                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_DMUSTATUS                               , PDUR_IF_DEST_COM, COM_RX_PDU_DMUSTATUS                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ENGINEDATA7                             , PDUR_IF_DEST_COM, COM_RX_PDU_ENGINEDATA7                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_BATTFULLSTAT                            , PDUR_IF_DEST_COM, COM_RX_PDU_BATTFULLSTAT                                , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ENGINEDATA6                             , PDUR_IF_DEST_COM, COM_RX_PDU_ENGINEDATA6                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ENGINEDATA5                             , PDUR_IF_DEST_COM, COM_RX_PDU_ENGINEDATA5                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ENGINEDATA4                             , PDUR_IF_DEST_COM, COM_RX_PDU_ENGINEDATA4                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ENGINEDATA3                             , PDUR_IF_DEST_COM, COM_RX_PDU_ENGINEDATA3                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ENGINEDATA2                             , PDUR_IF_DEST_COM, COM_RX_PDU_ENGINEDATA2                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_DSCDATA3                                , PDUR_IF_DEST_COM, COM_RX_PDU_DSCDATA3                                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_DSCDATA2                                , PDUR_IF_DEST_COM, COM_RX_PDU_DSCDATA2                                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_DSCDATA1                                , PDUR_IF_DEST_COM, COM_RX_PDU_DSCDATA1                                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ENGINEDATA1                             , PDUR_IF_DEST_COM, COM_RX_PDU_ENGINEDATA1                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ASGDATA1                                , PDUR_IF_DEST_COM, COM_RX_PDU_ASGDATA1                                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_PDCSTAT                                 , PDUR_IF_DEST_COM, COM_RX_PDU_PDCSTAT                                     , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_MILEAGE                                 , PDUR_IF_DEST_COM, COM_RX_PDU_MILEAGE                                     , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_DMU_ALIVE                               , PDUR_IF_DEST_COM, COM_RX_PDU_DMU_ALIVE                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_XCPRESPONSE_7C7                         , PDUR_IF_DEST_COM, COM_RX_PDU_XCPRESPONSE_7C7                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_XCPRESPONSE_7C5                         , PDUR_IF_DEST_COM, COM_RX_PDU_XCPRESPONSE_7C5                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_XCPRESPONSE_7C3                         , PDUR_IF_DEST_COM, COM_RX_PDU_XCPRESPONSE_7C3                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_XCPRESPONSE_7C1                         , PDUR_IF_DEST_COM, COM_RX_PDU_XCPRESPONSE_7C1                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_VOLTAGECURRENT                          , PDUR_IF_DEST_COM, COM_RX_PDU_VOLTAGECURRENT                              , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_TEMPMEAS                                , PDUR_IF_DEST_COM, COM_RX_PDU_TEMPMEAS                                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_LOADSTATUS                              , PDUR_IF_DEST_COM, COM_RX_PDU_LOADSTATUS                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_L1_I2T_COUNTER                          , PDUR_IF_DEST_COM, COM_RX_PDU_L1_I2T_COUNTER                              , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ERROR_706                               , PDUR_IF_DEST_COM, COM_RX_PDU_ERROR_706                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ERROR_704                               , PDUR_IF_DEST_COM, COM_RX_PDU_ERROR_704                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ERROR_702                               , PDUR_IF_DEST_COM, COM_RX_PDU_ERROR_702                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_ERROR_700                               , PDUR_IF_DEST_COM, COM_RX_PDU_ERROR_700                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_DIAGREQUEST_710                         , PDUR_IF_DEST_COM, COM_RX_PDU_DIAGREQUEST_710                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_DIAGRESPONSE_707                        , PDUR_IF_DEST_COM, COM_RX_PDU_DIAGRESPONSE_707                            , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_DIAGRESPONSE_705                        , PDUR_IF_DEST_COM, COM_RX_PDU_DIAGRESPONSE_705                            , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_DIAGRESPONSE_703                        , PDUR_IF_DEST_COM, COM_RX_PDU_DIAGRESPONSE_703                            , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_DIAGRESPONSE_701                        , PDUR_IF_DEST_COM, COM_RX_PDU_DIAGRESPONSE_701                            , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_BATTSOCSOH                              , PDUR_IF_DEST_COM, COM_RX_PDU_BATTSOCSOH                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_BATTSOC                                 , PDUR_IF_DEST_COM, COM_RX_PDU_BATTSOC                                     , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_BATTDIAGNOSIS                           , PDUR_IF_DEST_COM, COM_RX_PDU_BATTDIAGNOSIS                               , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_BATTCURRENT                             , PDUR_IF_DEST_COM, COM_RX_PDU_BATTCURRENT                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_BATTCAPDISCHARGE                        , PDUR_IF_DEST_COM, COM_RX_PDU_BATTCAPDISCHARGE                            , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_BATTCAPRES                              , PDUR_IF_DEST_COM, COM_RX_PDU_BATTCAPRES                                  , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_LOADSTATUS                                   , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_LOADSTATUS                                        , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_LOADSTATUS                                   , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_LOADSTATUS                                        , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_LOADSTATUS                                   , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_LOADSTATUS                                        , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_LOADSTATUS                                   , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_LOADSTATUS                                        , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_1                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_1                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_2                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_2                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_3                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_3                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_4                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_4                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_5                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_VOLTAGEFEEDBACK_5                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_1                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_1                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_2                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_2                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_3                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_3                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_4                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_4                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_5                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_5                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_STUCKATONEVENT                               , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_STUCKATONEVENT                                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_STUCKATOFFEVENT                              , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_STUCKATOFFEVENT                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_1                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_1                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_2                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_2                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_3                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_3                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_4                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_4                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_5                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_TEMPERATUREFEEDBACK_5                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_1                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_1                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_2                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_2                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_3                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_3                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_4                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_4                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_5                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_VOLTAGEFEEDBACK_5                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_1                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_1                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_2                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_2                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_3                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_3                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_4                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_4                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_5                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_CURRENTFEEDBACK_5                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_STUCKATONEVENT                               , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_STUCKATONEVENT                                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_STUCKATOFFEVENT                              , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_STUCKATOFFEVENT                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_1                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_1                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_2                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_2                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_3                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_3                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_4                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_4                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_5                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_TEMPERATUREFEEDBACK_5                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_1                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_1                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_2                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_2                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_3                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_3                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_4                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_4                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_5                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_VOLTAGEFEEDBACK_5                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_1                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_1                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_2                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_2                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_3                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_3                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_4                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_4                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_5                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_CURRENTFEEDBACK_5                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_STUCKATONEVENT                               , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_STUCKATONEVENT                                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_STUCKATOFFEVENT                              , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_STUCKATOFFEVENT                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_1                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_1                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_2                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_2                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_3                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_3                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_4                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_4                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_5                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_TEMPERATUREFEEDBACK_5                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_1                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_1                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_2                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_2                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_3                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_3                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_4                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_4                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_5                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_VOLTAGEFEEDBACK_5                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_1                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_1                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_2                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_2                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_3                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_3                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_4                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_4                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_5                            , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_CURRENTFEEDBACK_5                                 , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_STUCKATONEVENT                               , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_STUCKATONEVENT                                    , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_STUCKATOFFEVENT                              , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_STUCKATOFFEVENT                                   , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_1                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_1                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_2                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_2                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_3                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_3                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_4                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_4                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_5                        , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_5                             , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM1_DIAGRESPONSE                                 , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM1_DIAGRESPONSE                                      , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM2_DIAGRESPONSE                                 , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM2_DIAGRESPONSE                                      , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM3_DIAGRESPONSE                                 , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM3_DIAGRESPONSE                                      , PDUR_SOAD_INVALID_SOCON },
    { TRUE, CANIF_RX_PDU_CANFD_PDM4_DIAGRESPONSE                                 , PDUR_IF_DEST_COM, COM_RX_PDU_CANFD_PDM4_DIAGRESPONSE                                      , PDUR_SOAD_INVALID_SOCON }
};

static const PduR_IfRxRouteType PduR_LinIfRxRoutes[] =
{
    { TRUE, LINIF_RX_PDU_ALT_STATUS            , PDUR_IF_DEST_COM, COM_RX_PDU_LIN_ALT_STATUS                         , PDUR_SOAD_INVALID_SOCON },
    { TRUE, LINIF_RX_PDU_HVDCDC_STATUS         , PDUR_IF_DEST_COM, COM_RX_PDU_LIN_HVDCDC_STATUS                      , PDUR_SOAD_INVALID_SOCON },
    { TRUE, LINIF_RX_PDU_PCU48_STATUS          , PDUR_IF_DEST_COM, COM_RX_PDU_LIN_PCU48_STATUS                       , PDUR_SOAD_INVALID_SOCON }
};

static const PduR_SoAdRxRouteType PduR_SoAdRxRoutes[] =
{
    { FALSE, PDUR_SOAD_SOCON_PDUR_IF_UDP, PDUR_IF_DEST_NONE, 0u, PDUR_SOAD_INVALID_SOCON }
};

/* ===================== Diagnostic TP routes ===================== */

static const PduR_TpRxRouteType PduR_TpRxRoutes[] =
{
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CAN_PHYS,   DCM_RX_CAN_PHYS   },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CAN_FUNC,   DCM_RX_CAN_FUNC   },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CANFD_PHYS, DCM_RX_CANFD_PHYS },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CANFD_FUNC, DCM_RX_CANFD_FUNC },
    { TRUE, PDUR_TP_LOWER_LINTP, DCM_RX_LIN_PHYS,   DCM_RX_LIN_PHYS   },
    { TRUE, PDUR_TP_LOWER_DOIP,  DCM_RX_ETH_PHYS,   DCM_RX_ETH_PHYS   }
};

static const PduR_DcmTxRouteType PduR_DcmTxRoutes[] =
{
    { TRUE, DCM_TX_CAN_PHYS,   PDUR_TP_LOWER_CANTP, DCM_TX_CAN_PHYS   },
    { TRUE, DCM_TX_CAN_FUNC,   PDUR_TP_LOWER_CANTP, DCM_TX_CAN_FUNC   },
    { TRUE, DCM_TX_CANFD_PHYS, PDUR_TP_LOWER_CANTP, DCM_TX_CANFD_PHYS },
    { TRUE, DCM_TX_CANFD_FUNC, PDUR_TP_LOWER_CANTP, DCM_TX_CANFD_FUNC },
    { TRUE, DCM_TX_LIN_PHYS,   PDUR_TP_LOWER_LINTP, DCM_TX_LIN_PHYS   },
    { TRUE, DCM_TX_ETH_PHYS,   PDUR_TP_LOWER_DOIP,  DCM_TX_ETH_PHYS   }
};

static PduR_TpBufferType PduR_TpRxBuf[PDUR_MAX_TP_RX_ROUTES];
static PduR_DoIPContextType PduR_DoIPCtx;
static uint8 PduR_Initialized;

/* ===================== Internal helpers ===================== */

static uint8 PduR_GetTpRxRouteCount(void)
{
    return (uint8)(sizeof(PduR_TpRxRoutes) / sizeof(PduR_TpRxRoutes[0]));
}

static const PduR_TpRxRouteType* PduR_FindTpRxRoute(PduR_TpLowerType lower,
                                                    PduIdType lowerRxPduId,
                                                    uint8* routeIdx)
{
    uint8 i;
    uint8 count;

    count = PduR_GetTpRxRouteCount();

    for (i = 0u; i < count; i++)
    {
        if ((PduR_TpRxRoutes[i].enabled != FALSE) &&
            (PduR_TpRxRoutes[i].lower == lower) &&
            (PduR_TpRxRoutes[i].lowerRxPduId == lowerRxPduId))
        {
            if (routeIdx != NULL_PTR)
            {
                *routeIdx = i;
            }
            return &PduR_TpRxRoutes[i];
        }
    }

    return NULL_PTR;
}

static const PduR_DcmTxRouteType* PduR_FindDcmTxRoute(PduIdType dcmTxPduId)
{
    uint8 i;

    for (i = 0u; i < (uint8)(sizeof(PduR_DcmTxRoutes) / sizeof(PduR_DcmTxRoutes[0])); i++)
    {
        if ((PduR_DcmTxRoutes[i].enabled != FALSE) &&
            (PduR_DcmTxRoutes[i].dcmTxPduId == dcmTxPduId))
        {
            return &PduR_DcmTxRoutes[i];
        }
    }

    return NULL_PTR;
}

static const PduR_DcmTxRouteType* PduR_FindDcmTxRouteByLower(PduR_TpLowerType lower,
                                                             PduIdType lowerOrUpperTxPduId)
{
    uint8 i;

    for (i = 0u; i < (uint8)(sizeof(PduR_DcmTxRoutes) / sizeof(PduR_DcmTxRoutes[0])); i++)
    {
        if ((PduR_DcmTxRoutes[i].enabled != FALSE) &&
            (PduR_DcmTxRoutes[i].lower == lower) &&
            ((PduR_DcmTxRoutes[i].lowerTxPduId == lowerOrUpperTxPduId) ||
             (PduR_DcmTxRoutes[i].dcmTxPduId == lowerOrUpperTxPduId)))
        {
            return &PduR_DcmTxRoutes[i];
        }
    }

    return NULL_PTR;
}

static Std_ReturnType PduR_TransmitIf(PduR_IfLowerType lower,
                                      PduIdType lowerPduId,
                                      uint8 soConId,
                                      const uint8* data,
                                      PduLengthType len)
{
    if ((data == NULL_PTR) || (len == 0u))
    {
        return E_NOT_OK;
    }

    switch (lower)
    {
        case PDUR_IF_LOWER_CANIF:
            return CanIf_Transmit(lowerPduId, data, len);

        case PDUR_IF_LOWER_LINIF:
            return LinIf_Transmit(lowerPduId, data, len);

        case PDUR_IF_LOWER_SOAD:
            if ((soConId == PDUR_SOAD_INVALID_SOCON) || (len > 0xFFFFu))
            {
                return E_NOT_OK;
            }
            return (SoAd_IfTransmit(soConId, NULL_PTR, data, (uint16)len) == SOAD_OK) ? E_OK : E_NOT_OK;

        default:
            return E_NOT_OK;
    }
}

static Std_ReturnType PduR_DispatchIfDest(PduR_IfDestType dest,
                                          PduIdType destPduId,
                                          uint8 destSoConId,
                                          const uint8* data,
                                          PduLengthType len)
{
    if ((data == NULL_PTR) || (len == 0u))
    {
        return E_NOT_OK;
    }

    switch (dest)
    {
        case PDUR_IF_DEST_COM:
            Com_RxIndication(destPduId, data, len);
            return E_OK;

        case PDUR_IF_DEST_CANIF:
            return CanIf_Transmit(destPduId, data, len);

        case PDUR_IF_DEST_LINIF:
            return LinIf_Transmit(destPduId, data, len);

        case PDUR_IF_DEST_SOAD:
            if ((destSoConId == PDUR_SOAD_INVALID_SOCON) || (len > 0xFFFFu))
            {
                return E_NOT_OK;
            }
            return (SoAd_IfTransmit(destSoConId, NULL_PTR, data, (uint16)len) == SOAD_OK) ? E_OK : E_NOT_OK;

        default:
            return E_NOT_OK;
    }
}

static void PduR_ResetTpBuffer(PduR_TpBufferType* buf)
{
    if (buf != NULL_PTR)
    {
        buf->expectedLen = 0u;
        buf->offset = 0u;
        buf->active = FALSE;
    }
}

static Std_ReturnType PduR_TpStartOfReception(PduR_TpLowerType lower,
                                              PduIdType lowerRxPduId,
                                              PduLengthType len)
{
    uint8 routeIdx;
    PduR_TpBufferType* buf;

    if ((PduR_Initialized == FALSE) || (len == 0u) || (len > PDUR_MAX_TP_BUFFER))
    {
        return E_NOT_OK;
    }

    if (PduR_FindTpRxRoute(lower, lowerRxPduId, &routeIdx) == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (routeIdx >= PDUR_MAX_TP_RX_ROUTES)
    {
        return E_NOT_OK;
    }

    buf = &PduR_TpRxBuf[routeIdx];
    buf->expectedLen = len;
    buf->offset = 0u;
    buf->active = TRUE;

    return E_OK;
}

static Std_ReturnType PduR_TpCopyRxData(PduR_TpLowerType lower,
                                        PduIdType lowerRxPduId,
                                        const uint8* data,
                                        PduLengthType len)
{
    uint8 routeIdx;
    PduR_TpBufferType* buf;

    if ((PduR_Initialized == FALSE) || (data == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (PduR_FindTpRxRoute(lower, lowerRxPduId, &routeIdx) == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (routeIdx >= PDUR_MAX_TP_RX_ROUTES)
    {
        return E_NOT_OK;
    }

    buf = &PduR_TpRxBuf[routeIdx];

    if (buf->active == FALSE)
    {
        return E_NOT_OK;
    }

    if (((PduLengthType)(buf->offset + len) < buf->offset) ||
        ((PduLengthType)(buf->offset + len) > buf->expectedLen))
    {
        PduR_ResetTpBuffer(buf);
        return E_NOT_OK;
    }

    if (len > 0u)
    {
        memcpy(&buf->buffer[buf->offset], data, len);
        buf->offset = (PduLengthType)(buf->offset + len);
    }

    return E_OK;
}

static void PduR_TpRxIndication(PduR_TpLowerType lower,
                                PduIdType lowerRxPduId,
                                Std_ReturnType result)
{
    uint8 routeIdx;
    const PduR_TpRxRouteType* route;
    PduR_TpBufferType* buf;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    route = PduR_FindTpRxRoute(lower, lowerRxPduId, &routeIdx);

    if ((route == NULL_PTR) || (routeIdx >= PDUR_MAX_TP_RX_ROUTES))
    {
        return;
    }

    buf = &PduR_TpRxBuf[routeIdx];

    if ((result == E_OK) &&
        (buf->active != FALSE) &&
        (buf->offset == buf->expectedLen))
    {
        Dcm_RxIndication(route->dcmRxPduId, buf->buffer, buf->expectedLen);
    }

    PduR_ResetTpBuffer(buf);
}

/* ===================== Public API ===================== */

void PduR_Init(void)
{
    uint8 i;

    PduR_Initialized = FALSE;

    for (i = 0u; i < PDUR_MAX_TP_RX_ROUTES; i++)
    {
        PduR_ResetTpBuffer(&PduR_TpRxBuf[i]);
    }

    PduR_DoIPCtx.valid = FALSE;
    PduR_DoIPCtx.sourceAddress = 0u;
    PduR_DoIPCtx.targetAddress = 0u;

    if (PduR_GetTpRxRouteCount() > PDUR_MAX_TP_RX_ROUTES)
    {
        return;
    }

    PduR_Initialized = TRUE;
}

uint8 PduR_IsInitialized(void)
{
    return PduR_Initialized;
}

Std_ReturnType PduR_ComTransmit(PduIdType TxPduId,
                                const uint8* data,
                                PduLengthType len)
{
    uint8 i;
    uint8 routeFound;
    Std_ReturnType ret;
    Std_ReturnType overall;

    if ((PduR_Initialized == FALSE) || (data == NULL_PTR) || (len == 0u))
    {
        return E_NOT_OK;
    }

    routeFound = FALSE;
    overall = E_OK;

    for (i = 0u; i < (uint8)(sizeof(PduR_ComTxRoutes) / sizeof(PduR_ComTxRoutes[0])); i++)
    {
        if ((PduR_ComTxRoutes[i].enabled != FALSE) &&
            (PduR_ComTxRoutes[i].upperPduId == TxPduId))
        {
            routeFound = TRUE;
            ret = PduR_TransmitIf(PduR_ComTxRoutes[i].lower,
                                  PduR_ComTxRoutes[i].lowerPduId,
                                  PduR_ComTxRoutes[i].soConId,
                                  data,
                                  len);
            if (ret != E_OK)
            {
                overall = E_NOT_OK;
            }
        }
    }

    return (routeFound != FALSE) ? overall : E_NOT_OK;
}

Std_ReturnType PduR_DcmTransmit(PduIdType DcmTxPduId,
                                const uint8* data,
                                PduLengthType len)
{
    const PduR_DcmTxRouteType* route;
    DoIP_ReturnType doipRet;

    if ((PduR_Initialized == FALSE) ||
        (data == NULL_PTR) ||
        (len == 0u) ||
        (len > PDUR_MAX_TP_BUFFER))
    {
        return E_NOT_OK;
    }

    route = PduR_FindDcmTxRoute(DcmTxPduId);

    if (route == NULL_PTR)
    {
        return E_NOT_OK;
    }

    switch (route->lower)
    {
        case PDUR_TP_LOWER_CANTP:
            return CanTp_Transmit(route->lowerTxPduId, data, len);

        case PDUR_TP_LOWER_LINTP:
            return LinTp_Transmit(route->lowerTxPduId, data, len);

        case PDUR_TP_LOWER_DOIP:
            if (PduR_DoIPCtx.valid == FALSE)
            {
                return E_NOT_OK;
            }

            doipRet = DoIP_SendDiagnosticResponse(PduR_DoIPCtx.targetAddress,
                                                  PduR_DoIPCtx.sourceAddress,
                                                  data,
                                                  (uint16)len);
            if (doipRet == DOIP_OK)
            {
                Dcm_TxConfirmation(DcmTxPduId, E_OK);
                return E_OK;
            }

            Dcm_TxConfirmation(DcmTxPduId, E_NOT_OK);
            return E_NOT_OK;

        default:
            return E_NOT_OK;
    }
}

void PduR_CanIfRxIndication(PduIdType CanIfRxPduId,
                            const uint8* data,
                            PduLengthType len)
{
    uint8 i;

    if ((PduR_Initialized == FALSE) || (data == NULL_PTR) || (len == 0u))
    {
        return;
    }

    for (i = 0u; i < (uint8)(sizeof(PduR_CanIfRxRoutes) / sizeof(PduR_CanIfRxRoutes[0])); i++)
    {
        if ((PduR_CanIfRxRoutes[i].enabled != FALSE) &&
            (PduR_CanIfRxRoutes[i].sourcePduId == CanIfRxPduId))
        {
            (void)PduR_DispatchIfDest(PduR_CanIfRxRoutes[i].dest,
                                      PduR_CanIfRxRoutes[i].destPduId,
                                      PduR_CanIfRxRoutes[i].destSoConId,
                                      data,
                                      len);
        }
    }
}

void PduR_CanIfTxConfirmation(PduIdType CanIfTxPduId)
{
    uint8 i;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    for (i = 0u; i < (uint8)(sizeof(PduR_ComTxRoutes) / sizeof(PduR_ComTxRoutes[0])); i++)
    {
        if ((PduR_ComTxRoutes[i].enabled != FALSE) &&
            (PduR_ComTxRoutes[i].lower == PDUR_IF_LOWER_CANIF) &&
            (PduR_ComTxRoutes[i].lowerPduId == CanIfTxPduId))
        {
            Com_TxConfirmation(PduR_ComTxRoutes[i].upperPduId);
        }
    }
}

void PduR_LinIfRxIndication(PduIdType LinIfRxPduId,
                            const uint8* data,
                            PduLengthType len)
{
    uint8 i;

    if ((PduR_Initialized == FALSE) || (data == NULL_PTR) || (len == 0u))
    {
        return;
    }

    for (i = 0u; i < (uint8)(sizeof(PduR_LinIfRxRoutes) / sizeof(PduR_LinIfRxRoutes[0])); i++)
    {
        if ((PduR_LinIfRxRoutes[i].enabled != FALSE) &&
            (PduR_LinIfRxRoutes[i].sourcePduId == LinIfRxPduId))
        {
            (void)PduR_DispatchIfDest(PduR_LinIfRxRoutes[i].dest,
                                      PduR_LinIfRxRoutes[i].destPduId,
                                      PduR_LinIfRxRoutes[i].destSoConId,
                                      data,
                                      len);
        }
    }
}

void PduR_LinIfTxConfirmation(PduIdType LinIfTxPduId)
{
    uint8 i;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    for (i = 0u; i < (uint8)(sizeof(PduR_ComTxRoutes) / sizeof(PduR_ComTxRoutes[0])); i++)
    {
        if ((PduR_ComTxRoutes[i].enabled != FALSE) &&
            (PduR_ComTxRoutes[i].lower == PDUR_IF_LOWER_LINIF) &&
            (PduR_ComTxRoutes[i].lowerPduId == LinIfTxPduId))
        {
            Com_TxConfirmation(PduR_ComTxRoutes[i].upperPduId);
        }
    }
}

void PduR_SoAdIfRxIndication(uint8 soConId,
                             const TcpIp_SockAddrType* remoteAddr,
                             const uint8* data,
                             uint16 len)
{
    uint8 i;

    (void)remoteAddr;

    if ((PduR_Initialized == FALSE) || (data == NULL_PTR) || (len == 0u))
    {
        return;
    }

    for (i = 0u; i < (uint8)(sizeof(PduR_SoAdRxRoutes) / sizeof(PduR_SoAdRxRoutes[0])); i++)
    {
        if ((PduR_SoAdRxRoutes[i].enabled != FALSE) &&
            (PduR_SoAdRxRoutes[i].sourceSoConId == soConId))
        {
            (void)PduR_DispatchIfDest(PduR_SoAdRxRoutes[i].dest,
                                      PduR_SoAdRxRoutes[i].destPduId,
                                      PduR_SoAdRxRoutes[i].destSoConId,
                                      data,
                                      (PduLengthType)len);
        }
    }
}

void PduR_SoAdIfTxConfirmation(uint8 soConId)
{
    uint8 i;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    for (i = 0u; i < (uint8)(sizeof(PduR_ComTxRoutes) / sizeof(PduR_ComTxRoutes[0])); i++)
    {
        if ((PduR_ComTxRoutes[i].enabled != FALSE) &&
            (PduR_ComTxRoutes[i].lower == PDUR_IF_LOWER_SOAD) &&
            (PduR_ComTxRoutes[i].soConId == soConId))
        {
            Com_TxConfirmation(PduR_ComTxRoutes[i].upperPduId);
        }
    }
}

Std_ReturnType PduR_CanTpStartOfReception(PduIdType CanTpRxPduId,
                                          PduLengthType len)
{
    return PduR_TpStartOfReception(PDUR_TP_LOWER_CANTP, CanTpRxPduId, len);
}

Std_ReturnType PduR_CanTpCopyRxData(PduIdType CanTpRxPduId,
                                    const uint8* data,
                                    PduLengthType len)
{
    return PduR_TpCopyRxData(PDUR_TP_LOWER_CANTP, CanTpRxPduId, data, len);
}

void PduR_CanTpRxIndication(PduIdType CanTpRxPduId,
                            Std_ReturnType result)
{
    PduR_TpRxIndication(PDUR_TP_LOWER_CANTP, CanTpRxPduId, result);
}

void PduR_CanTpTxConfirmation(PduIdType CanTpTxPduId,
                              Std_ReturnType result)
{
    const PduR_DcmTxRouteType* route;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    route = PduR_FindDcmTxRouteByLower(PDUR_TP_LOWER_CANTP, CanTpTxPduId);

    if (route != NULL_PTR)
    {
        Dcm_TxConfirmation(route->dcmTxPduId, result);
    }
}

Std_ReturnType PduR_LinTpStartOfReception(PduIdType LinTpRxPduId,
                                          PduLengthType len)
{
    return PduR_TpStartOfReception(PDUR_TP_LOWER_LINTP, LinTpRxPduId, len);
}

Std_ReturnType PduR_LinTpCopyRxData(PduIdType LinTpRxPduId,
                                    const uint8* data,
                                    PduLengthType len)
{
    return PduR_TpCopyRxData(PDUR_TP_LOWER_LINTP, LinTpRxPduId, data, len);
}

void PduR_LinTpRxIndication(PduIdType LinTpRxPduId,
                            Std_ReturnType result)
{
    PduR_TpRxIndication(PDUR_TP_LOWER_LINTP, LinTpRxPduId, result);
}

void PduR_LinTpTxConfirmation(PduIdType LinTpTxPduId,
                              Std_ReturnType result)
{
    const PduR_DcmTxRouteType* route;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    route = PduR_FindDcmTxRouteByLower(PDUR_TP_LOWER_LINTP, LinTpTxPduId);

    if (route != NULL_PTR)
    {
        Dcm_TxConfirmation(route->dcmTxPduId, result);
    }
}

void PduR_DoIPRxIndication(uint16 sourceAddress,
                           uint16 targetAddress,
                           const uint8* uds,
                           uint16 udsLen)
{
    const PduR_TpRxRouteType* route;
    uint8 routeIdx;

    if ((PduR_Initialized == FALSE) ||
        (uds == NULL_PTR) ||
        (udsLen == 0u) ||
        (udsLen > PDUR_MAX_TP_BUFFER))
    {
        return;
    }

    route = PduR_FindTpRxRoute(PDUR_TP_LOWER_DOIP, DCM_RX_ETH_PHYS, &routeIdx);

    if (route == NULL_PTR)
    {
        return;
    }

    (void)routeIdx;

    PduR_DoIPCtx.valid = TRUE;
    PduR_DoIPCtx.sourceAddress = sourceAddress;
    PduR_DoIPCtx.targetAddress = targetAddress;

    Dcm_RxIndication(route->dcmRxPduId, uds, (PduLengthType)udsLen);
}
