#include "PduR.h"

#include "CanIf.h"
#include "CanTp.h"
#include "Com.h"
#include "../ComM/ComM.h"
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "DoIP.h"
#include "GatewaySwc.h"
#include "APP/ParallelFlashSwc/ParallelFlashSwc.h"
#include "LinIf.h"
#include "LinTp.h"
#include "McuSm.h"
#include "SoAd.h"
#include "../UdpNm/UdpNm.h"

#include <string.h>

#ifndef TRUE
#define TRUE  1u
#endif

#ifndef FALSE
#define FALSE 0u
#endif

/* Keep these IDs aligned with EthStack_Cfg.c. */
#define PDUR_SOAD_SOCON_PDUR_IF_UDP     4u
#define PDUR_DOIP_TX_STATE_EMPTY        0u
#define PDUR_DOIP_TX_STATE_PENDING      1u
#define PDUR_DOIP_TX_STATE_CONFIRM      2u
#define PDUR_DOIP_TX_RETRY_LIMIT        20u
#define PDUR_DOIP_RX_SHORT_QUEUE_DEPTH  17u
#define PDUR_DOIP_RX_SHORT_QUEUE_USABLE 16u
#define PDUR_DOIP_RX_SHORT_BUFFER       64u
#define PDUR_DOIP_RX_LARGE_QUEUE_DEPTH  49u
#define PDUR_DOIP_RX_LARGE_QUEUE_USABLE 48u
#define PDUR_DOIP_RX_LARGE_BUFFER       320u
#define PDUR_DOIP_RX_DRAIN_BUDGET       (PDUR_DOIP_RX_SHORT_QUEUE_USABLE + PDUR_DOIP_RX_LARGE_QUEUE_USABLE)
#define PDUR_DOIP_FORWARD_NOT_ROUTED    0u
#define PDUR_DOIP_FORWARD_ACCEPTED      1u
#define PDUR_DOIP_FORWARD_RETRY         2u
#define PDUR_DOIP_FORWARD_DROPPED       3u
/* Core2 runs this monitor every 5 ms; 400 ticks gives core0 about 2 seconds
 * to consume a DoIP TCP reconnect/session-reset request before we reset. */
#define PDUR_DOIP_SESSION_RESET_STALL_TICKS 400u


/* ===================== Routing types ===================== */

typedef enum
{
    PDUR_IF_DEST_NONE = 0u,
    PDUR_IF_DEST_COM,
    PDUR_IF_DEST_CANIF,
    PDUR_IF_DEST_LINIF,
    PDUR_IF_DEST_SOAD,
    PDUR_IF_DEST_GATEWAY
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
    uint8 valid;
    uint16 sourceAddress;
    uint16 targetAddress;
} PduR_DoIPContextType;

typedef struct
{
    volatile uint8 valid;
    uint32 sessionGeneration;
    uint16 sourceAddress;
    uint16 targetAddress;
    uint16 udsLen;
    uint8 data[PDUR_MAX_TP_BUFFER];
} PduR_DoIPRxMailboxType;

typedef struct
{
    volatile uint8 valid;
    uint32 sessionGeneration;
    uint16 sourceAddress;
    uint16 targetAddress;
    uint16 udsLen;
    uint8 data[PDUR_DOIP_RX_SHORT_BUFFER];
} PduR_DoIPRxQueueEntryType;

typedef struct
{
    volatile uint8 head;
    volatile uint8 tail;
    PduR_DoIPRxQueueEntryType entries[PDUR_DOIP_RX_SHORT_QUEUE_DEPTH];
} PduR_DoIPRxQueueType;

typedef struct
{
    volatile uint8 valid;
    uint32 sessionGeneration;
    uint16 sourceAddress;
    uint16 targetAddress;
    uint16 udsLen;
    uint8 data[PDUR_DOIP_RX_LARGE_BUFFER];
} PduR_DoIPRxLargeQueueEntryType;

typedef struct
{
    volatile uint8 head;
    volatile uint8 tail;
    PduR_DoIPRxLargeQueueEntryType entries[PDUR_DOIP_RX_LARGE_QUEUE_DEPTH];
} PduR_DoIPRxLargeQueueType;

typedef struct
{
    volatile uint8 state;
    uint8 keepContext;
    uint8 confirmDcm;
    PduIdType txPduId;
    uint16 sourceAddress;
    uint16 targetAddress;
    uint16 udsLen;
    uint8 txRetries;
    Std_ReturnType result;
    uint8 data[PDUR_MAX_TP_BUFFER];
} PduR_DoIPTxMailboxType;

typedef struct
{
    volatile uint8 valid;
    uint8 keepContext;
    PduIdType txPduId;
    Std_ReturnType result;
} PduR_DoIPTxConfirmationMailboxType;

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
    { TRUE, PDUR_SOAD_SOCON_PDUR_IF_UDP, PDUR_IF_DEST_GATEWAY, 0u, PDUR_SOAD_SOCON_PDUR_IF_UDP }
};

/* ===================== Diagnostic TP routes ===================== */

static const PduR_TpRxRouteType PduR_TpRxRoutes[] =
{
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CAN_PHYS,        DCM_RX_CAN_PHYS        },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CANFD_PHYS,      DCM_RX_CANFD_PHYS      },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CAN_EXT_PHYS,    DCM_RX_CAN_EXT_PHYS    },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CAN_EXT_PHYS_2,  DCM_RX_CAN_EXT_PHYS_2  },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CAN_EXT_PHYS_3,  DCM_RX_CAN_EXT_PHYS_3  },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CAN_EXT_PHYS_4,  DCM_RX_CAN_EXT_PHYS_4  },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CANFD_EXT_PHYS,  DCM_RX_CANFD_EXT_PHYS  },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CANFD_EXT_PHYS_2, DCM_RX_CANFD_EXT_PHYS_2 },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CANFD_EXT_PHYS_3, DCM_RX_CANFD_EXT_PHYS_3 },
    { TRUE, PDUR_TP_LOWER_CANTP, DCM_RX_CANFD_EXT_PHYS_4, DCM_RX_CANFD_EXT_PHYS_4 },
    { TRUE, PDUR_TP_LOWER_LINTP, DCM_RX_LIN_PHYS,        DCM_RX_LIN_PHYS        },
    { TRUE, PDUR_TP_LOWER_DOIP,  DCM_RX_ETH_PHYS,        DCM_RX_ETH_PHYS        }
};

static const PduR_DcmTxRouteType PduR_DcmTxRoutes[] =
{
    { TRUE, DCM_TX_CAN_PHYS,        PDUR_TP_LOWER_CANTP, DCM_TX_CAN_PHYS        },
    { TRUE, DCM_TX_CANFD_PHYS,      PDUR_TP_LOWER_CANTP, DCM_TX_CANFD_PHYS      },
    { TRUE, DCM_TX_CAN_EXT_PHYS,    PDUR_TP_LOWER_CANTP, DCM_TX_CAN_EXT_PHYS    },
    { TRUE, DCM_TX_CAN_EXT_PHYS_2,  PDUR_TP_LOWER_CANTP, DCM_TX_CAN_EXT_PHYS_2  },
    { TRUE, DCM_TX_CAN_EXT_PHYS_3,  PDUR_TP_LOWER_CANTP, DCM_TX_CAN_EXT_PHYS_3  },
    { TRUE, DCM_TX_CAN_EXT_PHYS_4,  PDUR_TP_LOWER_CANTP, DCM_TX_CAN_EXT_PHYS_4  },
    { TRUE, DCM_TX_CANFD_EXT_PHYS,  PDUR_TP_LOWER_CANTP, DCM_TX_CANFD_EXT_PHYS  },
    { TRUE, DCM_TX_CANFD_EXT_PHYS_2, PDUR_TP_LOWER_CANTP, DCM_TX_CANFD_EXT_PHYS_2 },
    { TRUE, DCM_TX_CANFD_EXT_PHYS_3, PDUR_TP_LOWER_CANTP, DCM_TX_CANFD_EXT_PHYS_3 },
    { TRUE, DCM_TX_CANFD_EXT_PHYS_4, PDUR_TP_LOWER_CANTP, DCM_TX_CANFD_EXT_PHYS_4 },
    { TRUE, DCM_TX_LIN_PHYS,        PDUR_TP_LOWER_LINTP, DCM_TX_LIN_PHYS        },
    { TRUE, DCM_TX_ETH_PHYS,        PDUR_TP_LOWER_DOIP,  DCM_TX_ETH_PHYS        }
};

static PduR_DoIPContextType PduR_DoIPCtx;
static PduR_DoIPRxMailboxType PduR_DoIPRxMailbox;
static PduR_DoIPRxQueueType PduR_DoIPRxQueue;
static PduR_DoIPRxLargeQueueType PduR_DoIPRxLargeQueue;
static PduR_DoIPTxMailboxType PduR_DoIPTxMailbox;
static PduR_DoIPTxConfirmationMailboxType PduR_DoIPTxConfirmationMailbox;
static uint8 PduR_Initialized;

/* Set by the DoIP TCP connect/disconnect callbacks (core2), consumed by
 * PduR_DoIPCore0MainFunction (core0). The DoIP TP context and mailboxes are
 * core0-owned, so the actual clear runs on core0 - core2 only raises this
 * request, avoiding a cross-core write that could clobber an in-flight
 * transaction. */
static volatile uint8 PduR_DoIPSessionResetPending;
static volatile uint32 PduR_DoIPSessionGeneration;
volatile uint32 PduR_DoIPSessionResetAgeTicks;
volatile uint32 PduR_DoIPSessionResetStallCounter;

/* Return path for relayed routed responses. A routed request (extended-address
 * forward) is fire-and-forget and never sets up PduR_DoIPCtx, so the tester's DoIP
 * addresses are captured here when a routed request is accepted. A slave reply that
 * later arrives on a bus is relayed back to this tester via PduR_DoIPRelayForwardedResponse.
 * One tester/connection at a time, so a single slot is sufficient. */
static volatile uint8 PduR_RoutedReturnValid;
static uint16 PduR_RoutedReturnZgwAddr;
static uint16 PduR_RoutedReturnTesterAddr;

volatile uint32 PduR_DoIPRoutedRelayPostedCounter;
volatile uint32 PduR_DoIPRoutedRelayDroppedCounter;
volatile uint32 PduR_DoIPRoutedForwardDroppedCounter;

volatile uint32 PduR_DoIPRxMailboxFullCounter;
volatile uint32 PduR_DoIPRxDroppedBusyCounter;
volatile uint32 PduR_DoIPTxMailboxFullCounter;
volatile uint32 PduR_DoIPTxConfirmFullCounter;
volatile uint32 PduR_DoIPTxSentCounter;
volatile uint32 PduR_DoIPTxFailCounter;
volatile uint32 PduR_DoIPFastConfirmCounter;
volatile uint8 PduR_DebugInitialized;
volatile uint8 PduR_DebugInitFailure;
volatile uint8 PduR_DebugTpRxRouteCount;
volatile uint8 PduR_DebugMaxTpRxRoutes;

static void PduR_DoIPRxQueueReset(void)
{
    uint8 idx;

    PduR_DoIPRxQueue.head = 0u;
    PduR_DoIPRxQueue.tail = 0u;

    for (idx = 0u; idx < PDUR_DOIP_RX_SHORT_QUEUE_DEPTH; idx++)
    {
        PduR_DoIPRxQueue.entries[idx].valid = FALSE;
        PduR_DoIPRxQueue.entries[idx].sessionGeneration = 0u;
        PduR_DoIPRxQueue.entries[idx].sourceAddress = 0u;
        PduR_DoIPRxQueue.entries[idx].targetAddress = 0u;
        PduR_DoIPRxQueue.entries[idx].udsLen = 0u;
    }

    __dsync();
}

static uint8 PduR_DoIPRxQueueNextIndex(uint8 idx)
{
    idx++;
    if (idx >= PDUR_DOIP_RX_SHORT_QUEUE_DEPTH)
    {
        idx = 0u;
    }

    return idx;
}

static void PduR_DoIPRxQueueDropStale(uint32 sessionGeneration)
{
    uint8 idx;

    for (idx = 0u; idx < PDUR_DOIP_RX_SHORT_QUEUE_DEPTH; idx++)
    {
        if ((PduR_DoIPRxQueue.entries[idx].valid != FALSE) &&
                (PduR_DoIPRxQueue.entries[idx].sessionGeneration != sessionGeneration))
        {
            PduR_DoIPRxQueue.entries[idx].valid = FALSE;
            PduR_DoIPRxQueue.entries[idx].sessionGeneration = 0u;
            PduR_DoIPRxQueue.entries[idx].sourceAddress = 0u;
            PduR_DoIPRxQueue.entries[idx].targetAddress = 0u;
            PduR_DoIPRxQueue.entries[idx].udsLen = 0u;
        }
    }

    while ((PduR_DoIPRxQueue.head != PduR_DoIPRxQueue.tail) &&
            (PduR_DoIPRxQueue.entries[PduR_DoIPRxQueue.head].valid == FALSE))
    {
        PduR_DoIPRxQueue.head = PduR_DoIPRxQueueNextIndex(PduR_DoIPRxQueue.head);
    }

    __dsync();
}

static Std_ReturnType PduR_DoIPRxQueuePush(uint16 sourceAddress,
                                           uint16 targetAddress,
                                           const uint8* uds,
                                           uint16 udsLen,
                                           uint32 sessionGeneration)
{
    PduR_DoIPRxQueueEntryType* entry;
    uint8 tail;
    uint8 nextTail;

    if ((uds == NULL_PTR) ||
        (udsLen == 0u) ||
        (udsLen > PDUR_DOIP_RX_SHORT_BUFFER))
    {
        return E_NOT_OK;
    }

    tail = PduR_DoIPRxQueue.tail;
    nextTail = PduR_DoIPRxQueueNextIndex(tail);

    if (nextTail == PduR_DoIPRxQueue.head)
    {
        return E_NOT_OK;
    }

    entry = &PduR_DoIPRxQueue.entries[tail];

    if (entry->valid != FALSE)
    {
        return E_NOT_OK;
    }

    entry->sessionGeneration = sessionGeneration;
    entry->sourceAddress = sourceAddress;
    entry->targetAddress = targetAddress;
    entry->udsLen = udsLen;
    memcpy(entry->data, uds, udsLen);
    __dsync();
    entry->valid = TRUE;

    PduR_DoIPRxQueue.tail = nextTail;
    __dsync();

    return E_OK;
}

static PduR_DoIPRxQueueEntryType* PduR_DoIPRxQueuePeek(void)
{
    PduR_DoIPRxQueueEntryType* entry;

    while (PduR_DoIPRxQueue.head != PduR_DoIPRxQueue.tail)
    {
        entry = &PduR_DoIPRxQueue.entries[PduR_DoIPRxQueue.head];

        if (entry->valid != FALSE)
        {
            return entry;
        }

        PduR_DoIPRxQueue.head = PduR_DoIPRxQueueNextIndex(PduR_DoIPRxQueue.head);
    }

    return NULL_PTR;
}

static void PduR_DoIPRxQueuePop(void)
{
    PduR_DoIPRxQueueEntryType* entry;
    uint8 head;
    uint8 nextHead;

    if (PduR_DoIPRxQueue.head == PduR_DoIPRxQueue.tail)
    {
        return;
    }

    head = PduR_DoIPRxQueue.head;
    entry = &PduR_DoIPRxQueue.entries[head];
    entry->valid = FALSE;
    entry->sessionGeneration = 0u;
    entry->sourceAddress = 0u;
    entry->targetAddress = 0u;
    entry->udsLen = 0u;

    __dsync();
    nextHead = PduR_DoIPRxQueueNextIndex(head);
    PduR_DoIPRxQueue.head = nextHead;
    __dsync();
}

static void PduR_DoIPRxLargeQueueReset(void)
{
    uint8 idx;

    PduR_DoIPRxLargeQueue.head = 0u;
    PduR_DoIPRxLargeQueue.tail = 0u;

    for (idx = 0u; idx < PDUR_DOIP_RX_LARGE_QUEUE_DEPTH; idx++)
    {
        PduR_DoIPRxLargeQueue.entries[idx].valid = FALSE;
        PduR_DoIPRxLargeQueue.entries[idx].sessionGeneration = 0u;
        PduR_DoIPRxLargeQueue.entries[idx].sourceAddress = 0u;
        PduR_DoIPRxLargeQueue.entries[idx].targetAddress = 0u;
        PduR_DoIPRxLargeQueue.entries[idx].udsLen = 0u;
    }

    __dsync();
}

static uint8 PduR_DoIPRxLargeQueueNextIndex(uint8 idx)
{
    idx++;
    if (idx >= PDUR_DOIP_RX_LARGE_QUEUE_DEPTH)
    {
        idx = 0u;
    }

    return idx;
}

static void PduR_DoIPRxLargeQueueDropStale(uint32 sessionGeneration)
{
    uint8 idx;

    for (idx = 0u; idx < PDUR_DOIP_RX_LARGE_QUEUE_DEPTH; idx++)
    {
        if ((PduR_DoIPRxLargeQueue.entries[idx].valid != FALSE) &&
                (PduR_DoIPRxLargeQueue.entries[idx].sessionGeneration != sessionGeneration))
        {
            PduR_DoIPRxLargeQueue.entries[idx].valid = FALSE;
            PduR_DoIPRxLargeQueue.entries[idx].sessionGeneration = 0u;
            PduR_DoIPRxLargeQueue.entries[idx].sourceAddress = 0u;
            PduR_DoIPRxLargeQueue.entries[idx].targetAddress = 0u;
            PduR_DoIPRxLargeQueue.entries[idx].udsLen = 0u;
        }
    }

    while ((PduR_DoIPRxLargeQueue.head != PduR_DoIPRxLargeQueue.tail) &&
            (PduR_DoIPRxLargeQueue.entries[PduR_DoIPRxLargeQueue.head].valid == FALSE))
    {
        PduR_DoIPRxLargeQueue.head = PduR_DoIPRxLargeQueueNextIndex(PduR_DoIPRxLargeQueue.head);
    }

    __dsync();
}

static Std_ReturnType PduR_DoIPRxLargeQueuePush(uint16 sourceAddress,
                                                uint16 targetAddress,
                                                const uint8* uds,
                                                uint16 udsLen,
                                                uint32 sessionGeneration)
{
    PduR_DoIPRxLargeQueueEntryType* entry;
    uint8 tail;
    uint8 nextTail;

    if ((uds == NULL_PTR) ||
        (udsLen == 0u) ||
        (udsLen > PDUR_DOIP_RX_LARGE_BUFFER))
    {
        return E_NOT_OK;
    }

    tail = PduR_DoIPRxLargeQueue.tail;
    nextTail = PduR_DoIPRxLargeQueueNextIndex(tail);

    if (nextTail == PduR_DoIPRxLargeQueue.head)
    {
        return E_NOT_OK;
    }

    entry = &PduR_DoIPRxLargeQueue.entries[tail];

    if (entry->valid != FALSE)
    {
        return E_NOT_OK;
    }

    entry->sessionGeneration = sessionGeneration;
    entry->sourceAddress = sourceAddress;
    entry->targetAddress = targetAddress;
    entry->udsLen = udsLen;
    memcpy(entry->data, uds, udsLen);
    __dsync();
    entry->valid = TRUE;

    PduR_DoIPRxLargeQueue.tail = nextTail;
    __dsync();

    return E_OK;
}

static PduR_DoIPRxLargeQueueEntryType* PduR_DoIPRxLargeQueuePeek(void)
{
    PduR_DoIPRxLargeQueueEntryType* entry;

    while (PduR_DoIPRxLargeQueue.head != PduR_DoIPRxLargeQueue.tail)
    {
        entry = &PduR_DoIPRxLargeQueue.entries[PduR_DoIPRxLargeQueue.head];

        if (entry->valid != FALSE)
        {
            return entry;
        }

        PduR_DoIPRxLargeQueue.head = PduR_DoIPRxLargeQueueNextIndex(PduR_DoIPRxLargeQueue.head);
    }

    return NULL_PTR;
}

static void PduR_DoIPRxLargeQueuePop(void)
{
    PduR_DoIPRxLargeQueueEntryType* entry;
    uint8 head;
    uint8 nextHead;

    if (PduR_DoIPRxLargeQueue.head == PduR_DoIPRxLargeQueue.tail)
    {
        return;
    }

    head = PduR_DoIPRxLargeQueue.head;
    entry = &PduR_DoIPRxLargeQueue.entries[head];
    entry->valid = FALSE;
    entry->sessionGeneration = 0u;
    entry->sourceAddress = 0u;
    entry->targetAddress = 0u;
    entry->udsLen = 0u;

    __dsync();
    nextHead = PduR_DoIPRxLargeQueueNextIndex(head);
    PduR_DoIPRxLargeQueue.head = nextHead;
    __dsync();
}

static uint8 PduR_DoIPIsRoutedExtendedAddress(uint8 firstByte)
{
    if ((firstByte >= 0x42u) && (firstByte <= 0x4Fu))
    {
        return TRUE;
    }

    if ((firstByte >= 0x50u) && (firstByte <= 0x52u))
    {
        return TRUE;
    }

    if ((firstByte >= 0x53u) && (firstByte <= 0x59u))
    {
        return TRUE;
    }

    if ((firstByte >= 0x60u) && (firstByte <= 0x7Fu))
    {
        return TRUE;
    }

    if ((firstByte >= 0x90u) && (firstByte <= 0xAFu))
    {
        return TRUE;
    }

    return FALSE;
}

static uint8 PduR_DoIPForwardRoutedRequest(const uint8* uds, uint16 udsLen)
{
    uint8 response[4u];
    Dcm_PduLengthType responseLength;
    Dcm_ReturnType forwardResult;

    if ((uds == NULL_PTR) ||
        (udsLen < 2u) ||
        (PduR_DoIPIsRoutedExtendedAddress(uds[0u]) == FALSE))
    {
        return PDUR_DOIP_FORWARD_NOT_ROUTED;
    }

    responseLength = 0u;
    forwardResult = ParallelFlashSwc_ForwardCodingRequest(uds[0u],
                                                          DCM_INITIAL,
                                                          &uds[1u],
                                                          (Dcm_PduLengthType)(udsLen - 1u),
                                                          response,
                                                          &responseLength);
    if ((forwardResult == DCM_E_OK) && (responseLength == 0u))
    {
        return PDUR_DOIP_FORWARD_ACCEPTED;
    }

    /* Routed external-node requests are fire-and-forget from DoIP's point of
     * view. If forwarding rejects one while the target bus is saturated or the
     * per-bus queue is full, do not keep it at the head of the DoIP queue:
     * that stalls later local ZGW requests behind an external node that the
     * tester is not waiting on. */
    PduR_DoIPRoutedForwardDroppedCounter++;
    return PDUR_DOIP_FORWARD_DROPPED;
}

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

static uint8 PduR_ComMChannelForCanIfTx(PduIdType canIfTxPduId,
                                        ComM_ChannelType* channel)
{
    if (channel == NULL_PTR)
    {
        return FALSE;
    }

    if ((canIfTxPduId == CANIF_PDU_FD_PHYS_TX) ||
        ((canIfTxPduId >= CANIF_TX_PDU_CANFD_INFOTAINMENTDATA1) &&
         (canIfTxPduId <= CANIF_TX_PDU_CANFD_ENERGYMANAGEMENTDATA3)))
    {
        *channel = COMM_CH_CANFD;
        return TRUE;
    }

    if ((canIfTxPduId == CANIF_PDU_CLASSIC_PHYS_TX) ||
        ((canIfTxPduId >= CANIF_TX_PDU_VEHICLESTATE) &&
         (canIfTxPduId <= CANIF_TX_PDU_DIAGREQUEST_700)))
    {
        *channel = COMM_CH_CAN;
        return TRUE;
    }

    return FALSE;
}

static uint8 PduR_ComMChannelForCanIfRx(PduIdType canIfRxPduId,
                                        ComM_ChannelType* channel)
{
    if (channel == NULL_PTR)
    {
        return FALSE;
    }

    if ((canIfRxPduId >= CANIF_RX_PDU_CANFD_PDM4_LOADSTATUS) &&
        (canIfRxPduId <= CANIF_RX_PDU_CANFD_PDM4_DIAGRESPONSE))
    {
        *channel = COMM_CH_CANFD;
        return TRUE;
    }

    if ((canIfRxPduId >= CANIF_RX_PDU_CENTRALLOCKDATA) &&
        (canIfRxPduId <= CANIF_RX_PDU_BATTCAPRES))
    {
        *channel = COMM_CH_CAN;
        return TRUE;
    }

    return FALSE;
}

static uint8 PduR_ComMChannelForIfLower(PduR_IfLowerType lower,
                                        PduIdType lowerPduId,
                                        ComM_ChannelType* channel)
{
    if (channel == NULL_PTR)
    {
        return FALSE;
    }

    switch (lower)
    {
        case PDUR_IF_LOWER_CANIF:
            return PduR_ComMChannelForCanIfTx(lowerPduId, channel);

        case PDUR_IF_LOWER_LINIF:
            *channel = COMM_CH_LIN;
            return TRUE;

        case PDUR_IF_LOWER_SOAD:
            *channel = COMM_CH_ETH;
            return TRUE;

        default:
            return FALSE;
    }
}

static uint8 PduR_ComMChannelForIfDest(PduR_IfDestType dest,
                                       PduIdType destPduId,
                                       ComM_ChannelType* channel)
{
    if (channel == NULL_PTR)
    {
        return FALSE;
    }

    switch (dest)
    {
        case PDUR_IF_DEST_CANIF:
            return PduR_ComMChannelForCanIfTx(destPduId, channel);

        case PDUR_IF_DEST_LINIF:
            *channel = COMM_CH_LIN;
            return TRUE;

        case PDUR_IF_DEST_SOAD:
        case PDUR_IF_DEST_GATEWAY:
            *channel = COMM_CH_ETH;
            return TRUE;

        default:
            return FALSE;
    }
}

static uint8 PduR_ComMChannelForDcmRoute(const PduR_DcmTxRouteType* route,
                                         ComM_ChannelType* channel)
{
    if ((route == NULL_PTR) || (channel == NULL_PTR))
    {
        return FALSE;
    }

    if (route->lower == PDUR_TP_LOWER_CANTP)
    {
        if ((route->dcmTxPduId == DCM_TX_CANFD_PHYS) ||
            (route->dcmTxPduId == DCM_TX_CANFD_EXT_PHYS) ||
            (route->dcmTxPduId == DCM_TX_CANFD_EXT_PHYS_2) ||
            (route->dcmTxPduId == DCM_TX_CANFD_EXT_PHYS_3) ||
            (route->dcmTxPduId == DCM_TX_CANFD_EXT_PHYS_4))
        {
            *channel = COMM_CH_CANFD;
        }
        else
        {
            *channel = COMM_CH_CAN;
        }
        return TRUE;
    }

    if (route->lower == PDUR_TP_LOWER_LINTP)
    {
        *channel = COMM_CH_LIN;
        return TRUE;
    }

    if (route->lower == PDUR_TP_LOWER_DOIP)
    {
        *channel = COMM_CH_ETH;
        return TRUE;
    }

    return FALSE;
}

static Std_ReturnType PduR_TransmitIf(PduR_IfLowerType lower,
                                      PduIdType lowerPduId,
                                      uint8 soConId,
                                      const uint8* data,
                                      PduLengthType len)
{
    ComM_ChannelType channel;

    if ((data == NULL_PTR) || (len == 0u))
    {
        return E_NOT_OK;
    }

    if ((PduR_ComMChannelForIfLower(lower, lowerPduId, &channel) != FALSE) &&
        (ComM_IsTxAllowed(channel) == FALSE))
    {
        return E_NOT_OK;
    }

    switch (lower)
    {
        case PDUR_IF_LOWER_CANIF:
            return GatewaySwc_RequestCanIfTransmit(lowerPduId, data, len);

        case PDUR_IF_LOWER_LINIF:
            return GatewaySwc_RequestLinIfTransmit(lowerPduId, data, len);

        case PDUR_IF_LOWER_SOAD:
            if ((soConId == PDUR_SOAD_INVALID_SOCON) || (len > 0xFFFFu))
            {
                return E_NOT_OK;
            }
            return (GatewaySwc_RequestSoAdIfTransmit(soConId, NULL_PTR, data, (uint16)len) == SOAD_OK) ? E_OK : E_NOT_OK;

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
    ComM_ChannelType channel;

    if ((data == NULL_PTR) || (len == 0u))
    {
        return E_NOT_OK;
    }

    if ((PduR_ComMChannelForIfDest(dest, destPduId, &channel) != FALSE) &&
        (ComM_IsTxAllowed(channel) == FALSE))
    {
        return E_NOT_OK;
    }

    switch (dest)
    {
        case PDUR_IF_DEST_COM:
            Com_RxIndication(destPduId, data, len);
            return E_OK;

        case PDUR_IF_DEST_CANIF:
            return GatewaySwc_RequestCanIfTransmit(destPduId, data, len);

        case PDUR_IF_DEST_LINIF:
            return GatewaySwc_RequestLinIfTransmit(destPduId, data, len);

        case PDUR_IF_DEST_SOAD:
            if ((destSoConId == PDUR_SOAD_INVALID_SOCON) || (len > 0xFFFFu))
            {
                return E_NOT_OK;
            }
            return (GatewaySwc_RequestSoAdIfTransmit(destSoConId, NULL_PTR, data, (uint16)len) == SOAD_OK) ? E_OK : E_NOT_OK;

        case PDUR_IF_DEST_GATEWAY:
            if ((destSoConId == PDUR_SOAD_INVALID_SOCON) || (len > 0xFFFFu))
            {
                return E_NOT_OK;
            }
            GatewaySwc_EthRxIndication(destSoConId, NULL_PTR, data, (uint16)len);
            return E_OK;

        default:
            return E_NOT_OK;
    }
}

static Std_ReturnType PduR_TpStartOfReception(PduR_TpLowerType lower,
                                              PduIdType lowerRxPduId,
                                              PduLengthType len)
{
    uint8 routeIdx;
    const PduR_TpRxRouteType* route;
    Dcm_PduInfoType info;
    Dcm_PduLengthType available;

    if ((PduR_Initialized == FALSE) || (len == 0u) || (len > PDUR_MAX_TP_BUFFER))
    {
        return E_NOT_OK;
    }

    route = PduR_FindTpRxRoute(lower, lowerRxPduId, &routeIdx);

    if (route == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (routeIdx >= PDUR_MAX_TP_RX_ROUTES)
    {
        return E_NOT_OK;
    }

    info.SduDataPtr = NULL_PTR;
    info.SduLength = 0u;
    available = 0u;

    return (Dcm_StartOfReception(route->dcmRxPduId,
                                 &info,
                                 (Dcm_PduLengthType)len,
                                 &available) == BUFREQ_OK) ? E_OK : E_NOT_OK;
}

static Std_ReturnType PduR_TpCopyRxData(PduR_TpLowerType lower,
                                        PduIdType lowerRxPduId,
                                        const uint8* data,
                                        PduLengthType len)
{
    uint8 routeIdx;
    const PduR_TpRxRouteType* route;
    Dcm_PduInfoType info;
    Dcm_PduLengthType available;

    if ((PduR_Initialized == FALSE) || (data == NULL_PTR))
    {
        return E_NOT_OK;
    }

    route = PduR_FindTpRxRoute(lower, lowerRxPduId, &routeIdx);

    if (route == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (routeIdx >= PDUR_MAX_TP_RX_ROUTES)
    {
        return E_NOT_OK;
    }

    info.SduDataPtr = (uint8*)data;
    info.SduLength = (Dcm_PduLengthType)len;
    available = 0u;

    return (Dcm_CopyRxData(route->dcmRxPduId, &info, &available) == BUFREQ_OK) ? E_OK : E_NOT_OK;
}

static void PduR_TpRxIndication(PduR_TpLowerType lower,
                                PduIdType lowerRxPduId,
                                Std_ReturnType result)
{
    uint8 routeIdx;
    const PduR_TpRxRouteType* route;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    route = PduR_FindTpRxRoute(lower, lowerRxPduId, &routeIdx);

    if ((route == NULL_PTR) || (routeIdx >= PDUR_MAX_TP_RX_ROUTES))
    {
        return;
    }

    Dcm_TpRxIndication(route->dcmRxPduId,
                       (result == E_OK) ? NTFRSLT_OK : NTFRSLT_E_NOT_OK);
}

/* ===================== Public API ===================== */

void PduR_Init(void)
{
    PduR_Initialized = FALSE;
    PduR_DebugInitialized = FALSE;
    PduR_DebugInitFailure = PDUR_INIT_FAIL_NONE;
    PduR_DebugTpRxRouteCount = PduR_GetTpRxRouteCount();
    PduR_DebugMaxTpRxRoutes = PDUR_MAX_TP_RX_ROUTES;

    PduR_DoIPCtx.valid = FALSE;
    PduR_DoIPCtx.sourceAddress = 0u;
    PduR_DoIPCtx.targetAddress = 0u;
    PduR_DoIPRxMailbox.valid = FALSE;
    PduR_DoIPRxMailbox.sessionGeneration = 0u;
    PduR_DoIPRxMailbox.sourceAddress = 0u;
    PduR_DoIPRxMailbox.targetAddress = 0u;
    PduR_DoIPRxMailbox.udsLen = 0u;
    PduR_DoIPRxQueueReset();
    PduR_DoIPRxLargeQueueReset();
    PduR_DoIPTxMailbox.state = PDUR_DOIP_TX_STATE_EMPTY;
    PduR_DoIPTxMailbox.keepContext = FALSE;
    PduR_DoIPTxMailbox.confirmDcm = FALSE;
    PduR_DoIPTxMailbox.txPduId = 0u;
    PduR_DoIPTxMailbox.sourceAddress = 0u;
    PduR_DoIPTxMailbox.targetAddress = 0u;
    PduR_DoIPTxMailbox.udsLen = 0u;
    PduR_DoIPTxMailbox.txRetries = 0u;
    PduR_DoIPTxMailbox.result = E_NOT_OK;
    PduR_DoIPTxConfirmationMailbox.valid = FALSE;
    PduR_DoIPTxConfirmationMailbox.keepContext = FALSE;
    PduR_DoIPTxConfirmationMailbox.txPduId = 0u;
    PduR_DoIPTxConfirmationMailbox.result = E_NOT_OK;
    PduR_DoIPSessionResetPending = 0u;
    PduR_DoIPSessionGeneration = 0u;
    PduR_DoIPSessionResetAgeTicks = 0u;
    PduR_DoIPSessionResetStallCounter = 0u;

    PduR_RoutedReturnValid = FALSE;
    PduR_RoutedReturnZgwAddr = 0u;
    PduR_RoutedReturnTesterAddr = 0u;
    PduR_DoIPRoutedRelayPostedCounter = 0u;
    PduR_DoIPRoutedRelayDroppedCounter = 0u;
    PduR_DoIPRoutedForwardDroppedCounter = 0u;

    if (PduR_DebugTpRxRouteCount > PDUR_MAX_TP_RX_ROUTES)
    {
        PduR_DebugInitFailure = PDUR_INIT_FAIL_TP_RX_ROUTES;
        return;
    }

    PduR_Initialized = TRUE;
    PduR_DebugInitialized = TRUE;
}

uint8 PduR_IsInitialized(void)
{
    return PduR_Initialized;
}

void PduR_DoIPResetSession(void)
{
    /* Called from the DoIP TCP connect/disconnect callbacks, which run on core2.
     * The DoIP TP context and mailboxes are owned by core0 (set/cleared by the
     * RX drain and PduR_DcmTransmit), so we only raise a request here and let
     * core0 perform the actual clear in PduR_DoIPCore0MainFunction. Writing the
     * core0-owned state directly from core2 could clobber a transaction that is
     * legitimately in flight on the freshly (re)established connection. */
    PduR_DoIPSessionGeneration++;
    PduR_DoIPSessionResetAgeTicks = 0u;
    __dsync();
    PduR_DoIPSessionResetPending = 1u;
    __dsync();
}

static void PduR_DoIPApplySessionReset(void)
{
    PduIdType staleTxPduId;
    Std_ReturnType staleTxResult;
    uint8 confirmStaleTx;
    uint32 sessionGeneration;

    /* Runs on core0. Clears the DoIP TP request context and TX/confirmation
     * mailboxes so a new (or re-established) DoIP TCP connection starts clean.
     * RX entries are generation-tagged: stale entries from an older TCP session
     * are dropped, while requests already received for the current session are
     * kept. Otherwise a delayed reset can delete the first fresh UDS request
     * (for example 0x10 0x03) before DCM ever sees it. */
    staleTxPduId = 0u;
    staleTxResult = E_NOT_OK;
    confirmStaleTx = FALSE;
    sessionGeneration = PduR_DoIPSessionGeneration;

    if ((PduR_DoIPTxMailbox.state != PDUR_DOIP_TX_STATE_EMPTY) &&
            (PduR_DoIPTxMailbox.confirmDcm != FALSE))
    {
        staleTxPduId = PduR_DoIPTxMailbox.txPduId;
        staleTxResult = (PduR_DoIPTxMailbox.state == PDUR_DOIP_TX_STATE_CONFIRM) ?
                PduR_DoIPTxMailbox.result :
                E_NOT_OK;
        confirmStaleTx = TRUE;
    }

    PduR_DoIPTxMailbox.state = PDUR_DOIP_TX_STATE_EMPTY;
    PduR_DoIPTxMailbox.keepContext = FALSE;
    PduR_DoIPTxMailbox.confirmDcm = FALSE;
    PduR_DoIPTxMailbox.txPduId = 0u;
    PduR_DoIPTxMailbox.sourceAddress = 0u;
    PduR_DoIPTxMailbox.targetAddress = 0u;
    PduR_DoIPTxMailbox.udsLen = 0u;
    PduR_DoIPTxMailbox.txRetries = 0u;
    PduR_DoIPTxMailbox.result = E_NOT_OK;

    PduR_DoIPTxConfirmationMailbox.valid = FALSE;
    PduR_DoIPTxConfirmationMailbox.keepContext = FALSE;
    PduR_DoIPTxConfirmationMailbox.txPduId = 0u;
    PduR_DoIPTxConfirmationMailbox.result = E_NOT_OK;

    PduR_DoIPCtx.valid = FALSE;
    PduR_DoIPCtx.sourceAddress = 0u;
    PduR_DoIPCtx.targetAddress = 0u;
    if ((PduR_DoIPRxMailbox.valid == FALSE) ||
            (PduR_DoIPRxMailbox.sessionGeneration != sessionGeneration))
    {
        PduR_DoIPRxMailbox.valid = FALSE;
        PduR_DoIPRxMailbox.sessionGeneration = 0u;
        PduR_DoIPRxMailbox.sourceAddress = 0u;
        PduR_DoIPRxMailbox.targetAddress = 0u;
        PduR_DoIPRxMailbox.udsLen = 0u;
    }
    PduR_DoIPRxQueueDropStale(sessionGeneration);
    PduR_DoIPRxLargeQueueDropStale(sessionGeneration);

    /* A new/reset TCP session invalidates the captured routed-response return path. */
    PduR_RoutedReturnValid = FALSE;

    if (confirmStaleTx != FALSE)
    {
        Dcm_TxConfirmation(staleTxPduId, staleTxResult);
    }

    Dcm_ResetDoIPSession();
    PduR_DoIPSessionResetAgeTicks = 0u;
    __dsync();
}

static void PduR_DoIPMonitorSessionReset(void)
{
    if (PduR_DoIPSessionResetPending == 0u)
    {
        PduR_DoIPSessionResetAgeTicks = 0u;
        return;
    }

    if (PduR_DoIPSessionResetAgeTicks < PDUR_DOIP_SESSION_RESET_STALL_TICKS)
    {
        PduR_DoIPSessionResetAgeTicks++;
        return;
    }

    PduR_DoIPSessionResetStallCounter++;
    McuSm_PerformResetHook(
            MCUSM_RESET_REASON_DOIP_CORE0_STALL,
            PduR_DoIPSessionResetAgeTicks);
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
    uint8 keepContext;
    uint8 fastConfirm;
    ComM_ChannelType channel;

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

    if ((route->lower != PDUR_TP_LOWER_CANTP) &&
        (route->lower != PDUR_TP_LOWER_DOIP) &&
        (PduR_ComMChannelForDcmRoute(route, &channel) != FALSE) &&
        (ComM_IsTxAllowed(channel) == FALSE))
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

            if (PduR_DoIPTxMailbox.state != PDUR_DOIP_TX_STATE_EMPTY)
            {
                PduR_DoIPTxMailboxFullCounter++;
                return E_NOT_OK;
            }

            keepContext = ((len == 3u) &&
                           (data[0u] == 0x7Fu) &&
                           (data[2u] == DCM_NRC_RESPONSE_PENDING)) ? TRUE : FALSE;
            fastConfirm = ((len == 2u) && (data[0u] == 0x7Eu)) ? TRUE : FALSE;

            PduR_DoIPTxMailbox.txPduId = DcmTxPduId;
            PduR_DoIPTxMailbox.sourceAddress = PduR_DoIPCtx.targetAddress;
            PduR_DoIPTxMailbox.targetAddress = PduR_DoIPCtx.sourceAddress;
            PduR_DoIPTxMailbox.udsLen = (uint16)len;
            PduR_DoIPTxMailbox.txRetries = 0u;
            PduR_DoIPTxMailbox.keepContext = keepContext;
            PduR_DoIPTxMailbox.confirmDcm = (fastConfirm == FALSE) ? TRUE : FALSE;
            memcpy(PduR_DoIPTxMailbox.data, data, len);
            __dsync();
            PduR_DoIPTxMailbox.state = PDUR_DOIP_TX_STATE_PENDING;
            __dsync();

            /* Release the request context as soon as a terminal response has been
             * captured into the TX mailbox. Its addresses are already copied above,
             * so the context is no longer needed to transmit it. Only a
             * response-pending (0x78) keeps the context, because further responses
             * for the same request still follow. Releasing here - on the same core
             * that delivers RX - closes the window in which a fast follow-up request
             * arrives after the response was handed off but before the cross-core TX
             * confirmation completes, and would otherwise be dropped as "busy".
             * Failed/suppressed responses are released via PduR_DcmReleaseNoResponse
             * from Dcm_ClearProcessing, so no path leaks the context. */
            if (keepContext == FALSE)
            {
                PduR_DoIPCtx.valid = FALSE;
                __dsync();
            }

            if (fastConfirm != FALSE)
            {
                PduR_DoIPFastConfirmCounter++;
                Dcm_TxConfirmation(DcmTxPduId, E_OK);
            }

            return E_OK;

        default:
            return E_NOT_OK;
    }
}

void PduR_DcmReleaseNoResponse(PduIdType DcmTxPduId)
{
    const PduR_DcmTxRouteType* route;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    route = PduR_FindDcmTxRoute(DcmTxPduId);

    if ((route != NULL_PTR) && (route->lower == PDUR_TP_LOWER_DOIP))
    {
        PduR_DoIPCtx.valid = FALSE;
        __dsync();
    }
}

Std_ReturnType PduR_DoIPRelayForwardedResponse(uint8 extendedAddress,
                                               const uint8* data,
                                               uint16 len)
{
    /* Relay a slave's UDS response (received on a bus EXT connection) back to the
     * DoIP tester that issued the routed request. The response is prefixed with the
     * originating node's extended address so the tester can correlate it. This reuses
     * the existing DoIP TX mailbox (drained on core2) but, unlike PduR_DcmTransmit, it
     * is driven by the captured routed-return addresses rather than PduR_DoIPCtx and
     * never confirms a Dcm TX transaction. */
    if ((PduR_Initialized == FALSE) ||
        (data == NULL_PTR) ||
        (len == 0u) ||
        (((uint32)len + 1u) > PDUR_MAX_TP_BUFFER))
    {
        return E_NOT_OK;
    }

    if (PduR_RoutedReturnValid == FALSE)
    {
        PduR_DoIPRoutedRelayDroppedCounter++;
        return E_NOT_OK;
    }

    if (PduR_DoIPTxMailbox.state != PDUR_DOIP_TX_STATE_EMPTY)
    {
        PduR_DoIPTxMailboxFullCounter++;
        PduR_DoIPRoutedRelayDroppedCounter++;
        return E_NOT_OK;
    }

    PduR_DoIPTxMailbox.txPduId = DCM_TX_ETH_PHYS;
    PduR_DoIPTxMailbox.sourceAddress = PduR_RoutedReturnZgwAddr;
    PduR_DoIPTxMailbox.targetAddress = PduR_RoutedReturnTesterAddr;
    PduR_DoIPTxMailbox.udsLen = (uint16)(len + 1u);
    PduR_DoIPTxMailbox.txRetries = 0u;
    PduR_DoIPTxMailbox.keepContext = FALSE;
    PduR_DoIPTxMailbox.confirmDcm = FALSE;
    PduR_DoIPTxMailbox.data[0] = extendedAddress;
    memcpy(&PduR_DoIPTxMailbox.data[1], data, len);
    __dsync();
    PduR_DoIPTxMailbox.state = PDUR_DOIP_TX_STATE_PENDING;
    __dsync();

    PduR_DoIPRoutedRelayPostedCounter++;
    return E_OK;
}

void PduR_CanIfRxIndication(PduIdType CanIfRxPduId,
                            const uint8* data,
                            PduLengthType len)
{
    uint8 i;
    ComM_ChannelType channel;

    if ((PduR_Initialized == FALSE) || (data == NULL_PTR) || (len == 0u))
    {
        return;
    }

    if ((PduR_ComMChannelForCanIfRx(CanIfRxPduId, &channel) != FALSE) &&
        (ComM_IsRxAllowed(channel) == FALSE))
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

    if (ComM_IsRxAllowed(COMM_CH_LIN) == FALSE)
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

    if (UdpNm_SoAdRxIndication(soConId, remoteAddr, data, len) != FALSE)
    {
        return;
    }

    if (ComM_IsRxAllowed(COMM_CH_ETH) == FALSE)
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

Std_ReturnType PduR_DoIPRxIndication(uint16 sourceAddress,
                                     uint16 targetAddress,
                                     const uint8* uds,
                                     uint16 udsLen)
{
    const PduR_TpRxRouteType* route;
    uint8 routeIdx;
    uint32 sessionGeneration;

    if ((PduR_Initialized == FALSE) ||
        (uds == NULL_PTR) ||
        (udsLen == 0u) ||
        (udsLen > PDUR_MAX_TP_BUFFER))
    {
        return E_NOT_OK;
    }

    route = PduR_FindTpRxRoute(PDUR_TP_LOWER_DOIP, DCM_RX_ETH_PHYS, &routeIdx);

    if (route == NULL_PTR)
    {
        return E_NOT_OK;
    }

    (void)routeIdx;
    sessionGeneration = PduR_DoIPSessionGeneration;

    if (udsLen <= PDUR_DOIP_RX_SHORT_BUFFER)
    {
        if (PduR_DoIPRxQueuePush(sourceAddress,
                targetAddress,
                uds,
                udsLen,
                sessionGeneration) != E_OK)
        {
            PduR_DoIPRxMailboxFullCounter++;
            return E_NOT_OK;
        }
        return E_OK;
    }

    if (udsLen <= PDUR_DOIP_RX_LARGE_BUFFER)
    {
        if (PduR_DoIPRxLargeQueuePush(sourceAddress,
                targetAddress,
                uds,
                udsLen,
                sessionGeneration) != E_OK)
        {
            PduR_DoIPRxMailboxFullCounter++;
            return E_NOT_OK;
        }
        return E_OK;
    }

    if (PduR_DoIPRxMailbox.valid != FALSE)
    {
        PduR_DoIPRxMailboxFullCounter++;
        return E_NOT_OK;
    }

    PduR_DoIPRxMailbox.sessionGeneration = sessionGeneration;
    PduR_DoIPRxMailbox.sourceAddress = sourceAddress;
    PduR_DoIPRxMailbox.targetAddress = targetAddress;
    PduR_DoIPRxMailbox.udsLen = udsLen;
    memcpy(PduR_DoIPRxMailbox.data, uds, udsLen);
    __dsync();
    PduR_DoIPRxMailbox.valid = TRUE;
    __dsync();

    return E_OK;
}

void PduR_DoIPCore0MainFunction(void)
{
    const PduR_TpRxRouteType* route;
    PduIdType txPduId;
    Std_ReturnType txResult;
    uint16 sourceAddress;
    uint16 targetAddress;
    uint16 udsLen;
    const uint8* udsData;
    PduR_DoIPRxQueueEntryType* rxQueueEntry;
    PduR_DoIPRxLargeQueueEntryType* rxLargeQueueEntry;
    uint8 rxFromShortQueue;
    uint8 rxFromLargeQueue;
    uint8 rxDrainCount;
    uint8 routeIdx;
    uint8 forwardResult;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    if (PduR_DoIPTxConfirmationMailbox.valid != FALSE)
    {
        __dsync();
        txPduId = PduR_DoIPTxConfirmationMailbox.txPduId;
        txResult = PduR_DoIPTxConfirmationMailbox.result;
        PduR_DoIPTxConfirmationMailbox.valid = FALSE;
        __dsync();

        /* The request context is released in PduR_DcmTransmit (for a terminal
         * response) or PduR_DcmReleaseNoResponse (suppressed / failed response via
         * Dcm_ClearProcessing), never here, so that a late confirmation cannot clear
         * the context of a newer request that is already in flight. */
        Dcm_TxConfirmation(txPduId, txResult);
    }

    if (PduR_DoIPSessionResetPending != 0u)
    {
        PduR_DoIPSessionResetPending = 0u;
        __dsync();
        PduR_DoIPApplySessionReset();
    }

    for (rxDrainCount = 0u; rxDrainCount < PDUR_DOIP_RX_DRAIN_BUDGET; rxDrainCount++)
    {
        if (PduR_DoIPSessionResetPending != 0u)
        {
            PduR_DoIPSessionResetPending = 0u;
            __dsync();
            PduR_DoIPApplySessionReset();
        }

        if (PduR_DoIPCtx.valid != FALSE)
        {
            return;
        }

        __dsync();

        rxQueueEntry = PduR_DoIPRxQueuePeek();
        rxLargeQueueEntry = NULL_PTR;
        rxFromShortQueue = FALSE;
        rxFromLargeQueue = FALSE;

        if (rxQueueEntry != NULL_PTR)
        {
            sourceAddress = rxQueueEntry->sourceAddress;
            targetAddress = rxQueueEntry->targetAddress;
            udsLen = rxQueueEntry->udsLen;
            udsData = rxQueueEntry->data;
            rxFromShortQueue = TRUE;
        }
        else
        {
            rxLargeQueueEntry = PduR_DoIPRxLargeQueuePeek();

            if (rxLargeQueueEntry != NULL_PTR)
            {
                sourceAddress = rxLargeQueueEntry->sourceAddress;
                targetAddress = rxLargeQueueEntry->targetAddress;
                udsLen = rxLargeQueueEntry->udsLen;
                udsData = rxLargeQueueEntry->data;
                rxFromLargeQueue = TRUE;
            }
            else if (PduR_DoIPRxMailbox.valid == FALSE)
            {
                return;
            }
            else
            {
                sourceAddress = PduR_DoIPRxMailbox.sourceAddress;
                targetAddress = PduR_DoIPRxMailbox.targetAddress;
                udsLen = PduR_DoIPRxMailbox.udsLen;
                udsData = PduR_DoIPRxMailbox.data;
            }
        }

        forwardResult = PduR_DoIPForwardRoutedRequest(udsData, udsLen);
        if ((forwardResult == PDUR_DOIP_FORWARD_ACCEPTED) ||
                (forwardResult == PDUR_DOIP_FORWARD_DROPPED))
        {
            if (forwardResult == PDUR_DOIP_FORWARD_ACCEPTED)
            {
                /* Remember where to relay the slave's reply. The forward itself is
                 * fire-and-forget (no PduR_DoIPCtx), so capture the tester addresses here:
                 * the response is sent from the ZGW (request target) to the tester
                 * (request source). */
                PduR_RoutedReturnZgwAddr = targetAddress;
                PduR_RoutedReturnTesterAddr = sourceAddress;
                PduR_RoutedReturnValid = TRUE;
                __dsync();
            }

            if (rxFromShortQueue != FALSE)
            {
                PduR_DoIPRxQueuePop();
            }
            else if (rxFromLargeQueue != FALSE)
            {
                PduR_DoIPRxLargeQueuePop();
            }
            else
            {
                PduR_DoIPRxMailbox.valid = FALSE;
                PduR_DoIPRxMailbox.sessionGeneration = 0u;
                PduR_DoIPRxMailbox.sourceAddress = 0u;
                PduR_DoIPRxMailbox.targetAddress = 0u;
                PduR_DoIPRxMailbox.udsLen = 0u;
            }
            __dsync();
            continue;
        }
        else if (forwardResult == PDUR_DOIP_FORWARD_RETRY)
        {
            return;
        }

        route = PduR_FindTpRxRoute(PDUR_TP_LOWER_DOIP, DCM_RX_ETH_PHYS, &routeIdx);

        if (route == NULL_PTR)
        {
            if (rxFromShortQueue != FALSE)
            {
                PduR_DoIPRxQueuePop();
            }
            else if (rxFromLargeQueue != FALSE)
            {
                PduR_DoIPRxLargeQueuePop();
            }
            else
            {
                PduR_DoIPRxMailbox.valid = FALSE;
                PduR_DoIPRxMailbox.sessionGeneration = 0u;
                PduR_DoIPRxMailbox.sourceAddress = 0u;
                PduR_DoIPRxMailbox.targetAddress = 0u;
                PduR_DoIPRxMailbox.udsLen = 0u;
            }
            __dsync();
            return;
        }

        (void)routeIdx;

        PduR_DoIPCtx.sourceAddress = sourceAddress;
        PduR_DoIPCtx.targetAddress = targetAddress;
        __dsync();
        PduR_DoIPCtx.valid = TRUE;
        __dsync();

        if (Dcm_RxIndication(route->dcmRxPduId, udsData, (PduLengthType)udsLen) != E_OK)
        {
            uint8 busyResponse[3];

            busyResponse[0u] = 0x7Fu;
            busyResponse[1u] = (udsLen > 0u) ? udsData[0u] : 0x00u;
            busyResponse[2u] = DCM_NRC_BUSY_REPEAT_REQUEST;

            if (PduR_DcmTransmit(DCM_TX_ETH_PHYS, busyResponse, (PduLengthType)sizeof(busyResponse)) != E_OK)
            {
                PduR_DoIPCtx.valid = FALSE;
                __dsync();
            }
        }

        if (rxFromShortQueue != FALSE)
        {
            PduR_DoIPRxQueuePop();
        }
        else if (rxFromLargeQueue != FALSE)
        {
            PduR_DoIPRxLargeQueuePop();
        }
        else
        {
            PduR_DoIPRxMailbox.valid = FALSE;
            PduR_DoIPRxMailbox.sessionGeneration = 0u;
            PduR_DoIPRxMailbox.sourceAddress = 0u;
            PduR_DoIPRxMailbox.targetAddress = 0u;
            PduR_DoIPRxMailbox.udsLen = 0u;
        }
        __dsync();
    }
}

void PduR_DoIPCore2MainFunction(void)
{
    DoIP_ReturnType doipRet;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    PduR_DoIPMonitorSessionReset();

    if (PduR_DoIPTxMailbox.state == PDUR_DOIP_TX_STATE_PENDING)
    {
        __dsync();
        doipRet = DoIP_SendDiagnosticResponse(PduR_DoIPTxMailbox.sourceAddress,
                                              PduR_DoIPTxMailbox.targetAddress,
                                              PduR_DoIPTxMailbox.data,
                                              PduR_DoIPTxMailbox.udsLen);
        PduR_DoIPTxMailbox.result = (doipRet == DOIP_OK) ? E_OK : E_NOT_OK;
        if (doipRet == DOIP_OK)
        {
            PduR_DoIPTxSentCounter++;
        }
        else
        {
            if ((DoIP_GetTcpState() == DOIP_TCP_ROUTING_ACTIVE) &&
                    (PduR_DoIPTxMailbox.txRetries < PDUR_DOIP_TX_RETRY_LIMIT))
            {
                PduR_DoIPTxMailbox.txRetries++;
                __dsync();
                return;
            }

            PduR_DoIPTxFailCounter++;
        }

        if (PduR_DoIPTxMailbox.confirmDcm == FALSE)
        {
            PduR_DoIPTxMailbox.state = PDUR_DOIP_TX_STATE_EMPTY;
            __dsync();
            return;
        }

        __dsync();
        PduR_DoIPTxMailbox.state = PDUR_DOIP_TX_STATE_CONFIRM;
        __dsync();
    }

    if (PduR_DoIPTxMailbox.state != PDUR_DOIP_TX_STATE_CONFIRM)
    {
        return;
    }

    if (PduR_DoIPTxConfirmationMailbox.valid != FALSE)
    {
        PduR_DoIPTxConfirmFullCounter++;
        return;
    }

    PduR_DoIPTxConfirmationMailbox.txPduId = PduR_DoIPTxMailbox.txPduId;
    PduR_DoIPTxConfirmationMailbox.result = PduR_DoIPTxMailbox.result;
    PduR_DoIPTxConfirmationMailbox.keepContext = PduR_DoIPTxMailbox.keepContext;
    __dsync();
    PduR_DoIPTxConfirmationMailbox.valid = TRUE;
    __dsync();
    PduR_DoIPTxMailbox.state = PDUR_DOIP_TX_STATE_EMPTY;
    __dsync();
}
