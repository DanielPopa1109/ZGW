#include "GatewaySwc.h"
#include "CanIf.h"
#include "Com.h"
#include "ComM.h"
#include "Dem.h"
#include "LinIf.h"
#include "SoAd.h"
#include "Crc.h"
#include "Nm.h"
#include "SysMgr.h"
#include "IfxCpu.h"
#include "APP/CodingApp/CodingApp.h"
#include "BSW/Time/TimeBase.h"
#include "BSW/Sys/SmM/SafetyKit_Main.h"

#define GATEWAYSWC_MAGIC0                             0x5Au
#define GATEWAYSWC_MAGIC1                             0x47u
#define GATEWAYSWC_MAGIC2                             0x57u

#define GATEWAYSWC_FRAME_SUMMARY                      0x01u
#define GATEWAYSWC_FRAME_COMMAND_SIGNAL               0x02u
#define GATEWAYSWC_FRAME_COMMAND_BLOCK                0x03u
#define GATEWAYSWC_FRAME_DTC_TRANSITION               0x04u

#define GATEWAYSWC_HEADER_LEN                         12u
#define GATEWAYSWC_ENTRY_LEN                          7u
#define GATEWAYSWC_DTC_TRANSITION_LEN                 17u
#define GATEWAYSWC_DTC_TRANSITION_QUEUE_SIZE          8u
#define GATEWAYSWC_DTC_QUEUE_LOCK_TIMEOUT             100000u
#define GATEWAYSWC_CPU_CORE_COUNT                     6u

#define GATEWAYSWC_DIAG_PERIOD_MS                     1000u
#define GATEWAYSWC_ETH_PERIOD_NS                      ((uint64)GATEWAYSWC_ETH_PERIOD_MS * TIMEBASE_NS_PER_MS)

#define GATEWAYSWC_MCU_STATUS_MAGIC                   0x5A4D4355u
#define GATEWAYSWC_MCU_STATUS_VERSION                 1u
#define GATEWAYSWC_MCU_STATUS_MESSAGE_STATUS          2u
#define GATEWAYSWC_MCU_STATUS_CRC_OFFSET              60u
#define GATEWAYSWC_MCU_STATUS_TX_PERIOD_NS            ((uint64)GATEWAYSWC_MCU_STATUS_UDP_PERIOD_MS * TIMEBASE_NS_PER_MS)

#define GATEWAYSWC_SDAT_YEAR_BASE                     2000u
#define GATEWAYSWC_SDAT_YEAR_MAX_RAW                  63u
#define GATEWAYSWC_SDAT_MS_RESOLUTION                 4u
#define GATEWAYSWC_SDAT_MS_MAX_RAW                    255u

#define GATEWAYSWC_SDAT_SOURCE_DEFAULT                0u
#define GATEWAYSWC_SDAT_SOURCE_UDS                    1u
#define GATEWAYSWC_SDAT_SOURCE_GPTP                   2u
#define GATEWAYSWC_SDAT_SOURCE_INVALID                3u

#ifndef GATEWAYSWC_ETH_RX_CAPTURE_SLOT_COUNT
#define GATEWAYSWC_ETH_RX_CAPTURE_SLOT_COUNT           8u
#endif

#ifndef GATEWAYSWC_ETH_RX_CAPTURE_MAX_PAYLOAD
#define GATEWAYSWC_ETH_RX_CAPTURE_MAX_PAYLOAD          256u
#endif

#ifndef GATEWAYSWC_ETH_RX_CAPTURE_DEFAULT_ENABLE
#define GATEWAYSWC_ETH_RX_CAPTURE_DEFAULT_ENABLE       1u
#endif

#define GATEWAYSWC_ETH_RX_CAPTURE_FILTER_OFF           0u
#define GATEWAYSWC_ETH_RX_CAPTURE_FILTER_SOCON         1u


#define GATEWAYSWC_DEM_STATE_UNKNOWN                   0xFFu

#define GATEWAYSWC_COMM_UNKNOWN                       0xFFu
#define GATEWAYSWC_COMM_NO                            0u
#define GATEWAYSWC_COMM_FULL                          1u

#define GATEWAYSWC_PDM_CMD_IDX_PDM1                   0u
#define GATEWAYSWC_PDM_CMD_IDX_PDM2                   1u
#define GATEWAYSWC_PDM_CMD_IDX_PDM3                   2u
#define GATEWAYSWC_PDM_CMD_IDX_PDM4                   3u

typedef struct
{
        Com_SignalIdType rxSignalId;
        Com_SignalIdType txSignalId;
} GatewaySwc_RouteMapType;

typedef struct
{
        Com_SignalIdType firstSignalId;
        Com_SignalIdType lastSignalId;
        GatewaySwc_BusType bus;
} GatewaySwc_SignalRangeType;

typedef struct
{
        PduIdType firstPduId;
        PduIdType lastPduId;
        GatewaySwc_BusType bus;
} GatewaySwc_PduRangeType;

typedef struct
{
        Dem_DTCType dtc;
        Dem_UdsStatusByteType status;
} GatewaySwc_DtcTransitionType;

typedef struct
{
        uint32 year;
        uint32 month;
        uint32 day;
        uint32 hour;
        uint32 minute;
        uint32 second;
        uint32 millisecond;
        uint32 utcValid;
        uint32 timeSource;
        uint32 defaultTime;
        uint32 nvmRestored;
} GatewaySwc_SdatTimeType;

static uint8 GatewaySwc_EthBuffer[GATEWAYSWC_ETH_MAX_PAYLOAD];
static uint8 GatewaySwc_DtcTransitionPayload[GATEWAYSWC_DTC_TRANSITION_LEN];

static GatewaySwc_StatusType GatewaySwc_Status;
static uint8 GatewaySwc_Initialized = 0u;
static GatewaySwc_CommandType GatewaySwc_Command;
static GatewaySwc_DtcTransitionType GatewaySwc_DtcTransitionQueue[GATEWAYSWC_DTC_TRANSITION_QUEUE_SIZE];
static volatile uint8 GatewaySwc_DtcTransitionHead;
static volatile uint8 GatewaySwc_DtcTransitionTail;
static IfxCpu_spinLock GatewaySwc_DtcQueueLock;
static boolean GatewaySwc_DtcQueueIrqState[GATEWAYSWC_CPU_CORE_COUNT];
static volatile uint8 GatewaySwc_DtcQueueLockOwned[GATEWAYSWC_CPU_CORE_COUNT];

static uint16 GatewaySwc_RouteTimerMs;
static uint16 GatewaySwc_OutputTimerMs;
static uint64 GatewaySwc_NextEthPublishTimeNs;
static TcpIp_SocketIdType GatewaySwc_McuStatusSocket = TCPIP_INVALID_SOCKET;
static uint64 GatewaySwc_McuStatusNextTxTimeNs = 0ull;
static uint32 GatewaySwc_McuStatusSequenceCounter = 0u;
static uint8 GatewaySwc_McuStatusStatus = GATEWAYSWC_MCU_STATUS_DISABLED;
static uint32 GatewaySwc_McuStatusTxCounter = 0u;
static uint16 GatewaySwc_DiagTimerMs;
static uint8 GatewaySwc_LoadRequestAliveCounter;
static uint8 GatewaySwc_LoadRequest_Status = 0u;
static uint8 GatewaySwc_LoadRequest_DataId = 7;
static uint8 GatewaySwc_RxMessageDiagStatus[GATEWAYSWC_RX_MESSAGE_DIAG_COUNT];
static uint8 GatewaySwc_RxSignalDiagStatus[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
static uint32 GatewaySwc_RxSignalDiagValue[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
static uint32 GatewaySwc_RxSignalDiagInvalidValue[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
static uint8 GatewaySwc_DemMessageTimeoutState[GATEWAYSWC_RX_MESSAGE_DIAG_COUNT];
static uint8 GatewaySwc_DemSignalInvalidState[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
static uint8 GatewaySwc_ComModeRequestState = GATEWAYSWC_COMM_UNKNOWN;

volatile uint8 GatewaySwc_ComFullRequested = 0u;
volatile uint16 GatewaySwc_RxMessageDiagDebugPduId[GATEWAYSWC_RX_MESSAGE_DIAG_COUNT];
volatile uint8 GatewaySwc_RxMessageDiagDebugStatus[GATEWAYSWC_RX_MESSAGE_DIAG_COUNT];
volatile uint32 GatewaySwc_RxMessageTimeoutCounter[GATEWAYSWC_RX_MESSAGE_DIAG_COUNT];
volatile uint32 GatewaySwc_RxMessageTimeoutActiveSamples[GATEWAYSWC_RX_MESSAGE_DIAG_COUNT];
volatile uint16 GatewaySwc_RxSignalDiagDebugSignalId[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
volatile uint8 GatewaySwc_RxSignalDiagDebugStatus[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
volatile uint32 GatewaySwc_RxSignalDiagDebugValue[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
volatile uint32 GatewaySwc_RxSignalDiagDebugInvalidValue[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
volatile uint32 GatewaySwc_RxSignalTimeoutCounter[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
volatile uint32 GatewaySwc_RxSignalTimeoutActiveSamples[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
volatile uint32 GatewaySwc_RxSignalInvalidCounter[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
volatile uint32 GatewaySwc_RxSignalInvalidActiveSamples[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
volatile uint16 GatewaySwc_DebugPublishFirstSignalId = 0u;
volatile uint16 GatewaySwc_DebugPublishLastSignalId = 0u;
volatile uint16 GatewaySwc_DebugPublishSignalId = 0u;
volatile uint16 GatewaySwc_DebugPublishLen = 0u;
volatile uint8 GatewaySwc_DebugPublishBus = 0u;
volatile uint8 GatewaySwc_DebugPublishReceiveResult = E_NOT_OK;
volatile uint32 GatewaySwc_DebugPublishValue = 0u;
volatile uint32 GatewaySwc_DebugLoadRequestCounter = 0u;
volatile uint32 GatewaySwc_DebugLoadRequestCrc = 0u;
volatile uint32 GatewaySwc_DebugLoadRequestAlive = 0u;
volatile uint8 GatewaySwc_DebugLoadRequestChanged = 0u;
volatile uint8 GatewaySwc_DebugPublishInvalidRange = 0u;
volatile uint8 GatewaySwc_DebugDtcQueueHead = 0u;
volatile uint8 GatewaySwc_DebugDtcQueueTail = 0u;
volatile uint8 GatewaySwc_DebugDtcQueueCount = 0u;
volatile uint32 GatewaySwc_DebugDtcDropped = 0u;
volatile uint32 GatewaySwc_DebugDtcFlushed = 0u;
volatile uint32 GatewaySwc_DebugDtcLastDtc = 0u;
volatile uint8 GatewaySwc_DebugDtcLastStatus = 0u;
volatile uint8 GatewaySwc_DebugDtcLastOpenResult = 0u;
volatile uint8 GatewaySwc_DebugDtcLastTxResult = SOAD_NOT_OK;
volatile uint32 GatewaySwc_DebugDtcQueueLockTimeout = 0u;

volatile uint8 GatewaySwc_EthRxCaptureEnable = GATEWAYSWC_ETH_RX_CAPTURE_DEFAULT_ENABLE;
volatile uint8 GatewaySwc_EthRxCaptureFilterMode = GATEWAYSWC_ETH_RX_CAPTURE_FILTER_OFF;
volatile uint8 GatewaySwc_EthRxCaptureFilterSoConId = GATEWAYSWC_ETH_SOCON_ID;
volatile uint16 GatewaySwc_EthRxCaptureMaxLen = GATEWAYSWC_ETH_RX_CAPTURE_MAX_PAYLOAD;
volatile uint8 GatewaySwc_EthRxCaptureWriteIndex = 0u;
volatile uint8 GatewaySwc_EthRxCaptureLastIndex = 0u;
volatile uint32 GatewaySwc_EthRxCaptureCounter = 0u;
volatile uint32 GatewaySwc_EthRxCaptureDroppedCounter = 0u;
volatile uint32 GatewaySwc_EthRxCaptureTruncatedCounter = 0u;
volatile uint8 GatewaySwc_EthRxCaptureSoConId[GATEWAYSWC_ETH_RX_CAPTURE_SLOT_COUNT];
volatile uint16 GatewaySwc_EthRxCaptureLen[GATEWAYSWC_ETH_RX_CAPTURE_SLOT_COUNT];
volatile uint16 GatewaySwc_EthRxCaptureCopiedLen[GATEWAYSWC_ETH_RX_CAPTURE_SLOT_COUNT];
volatile uint32 GatewaySwc_EthRxCaptureMainCycle[GATEWAYSWC_ETH_RX_CAPTURE_SLOT_COUNT];
volatile uint32 GatewaySwc_EthRxCaptureRemoteAddrPtr[GATEWAYSWC_ETH_RX_CAPTURE_SLOT_COUNT];
volatile uint8 GatewaySwc_EthRxCaptureData[GATEWAYSWC_ETH_RX_CAPTURE_SLOT_COUNT][GATEWAYSWC_ETH_RX_CAPTURE_MAX_PAYLOAD];

long long GatewaySwc_MainFunction_Counter = 0;

Std_ReturnType GatewaySwc_RequestComSendSignal(Com_SignalIdType signalId, const void *data)
{
    return Com_SendSignal(signalId, data);
}

void GatewaySwc_RequestComMainFunctionTx(void)
{
    Com_MainFunctionTx();
}

Std_ReturnType GatewaySwc_RequestCanIfTransmit(PduIdType txPduId, const uint8 *data, PduLengthType len)
{
    return CanIf_Transmit(txPduId, data, len);
}

Std_ReturnType GatewaySwc_RequestLinIfTransmit(PduIdType txPduId, const uint8 *data, PduLengthType len)
{
    return LinIf_Transmit(txPduId, data, len);
}

SoAd_ReturnType GatewaySwc_RequestSoAdIfTransmit(SoAd_SoConIdType soConId,
                                                 const TcpIp_SockAddrType *remoteAddr,
                                                 const uint8 *data,
                                                 uint16 len)
{
    return SoAd_IfTransmit(soConId, remoteAddr, data, len);
}

sint32 GatewaySwc_RequestTcpIpSendTo(TcpIp_SocketIdType sock,
                                     const TcpIp_SockAddrType *remoteAddr,
                                     const uint8 *data,
                                     uint16 len)
{
    return TcpIp_SendTo(sock, remoteAddr, data, len);
}

void GatewaySwc_RequestLinIfMainFunction(void)
{
    LinIf_MainFunction();
}

static const GatewaySwc_RouteMapType GatewaySwc_RouteMap[] =
{
        { COM_SIG_RX_CENTRALCOMMAND1_CENTRALLOCKCOMMAND,       COM_SIG_TX_STATUSBODYDATA1_BD1_CENTRALLOCKCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_TURNSIGNALCOMMAND,        COM_SIG_TX_STATUSBODYDATA1_BD1_TURNSIGNALCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_RLSCOMMAND,               COM_SIG_TX_STATUSBODYDATA1_BD1_RLSCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_LIGTHSENSOR,              COM_SIG_TX_STATUSBODYDATA1_BD1_LIGTHSENSOR },
        { COM_SIG_RX_CENTRALCOMMAND1_IGNITION,                 COM_SIG_TX_STATUSBODYDATA1_BD1_IGNITION },
        { COM_SIG_RX_CENTRALCOMMAND1_HIGHBEAMCOMMAND,          COM_SIG_TX_STATUSBODYDATA1_BD1_HIGHBEAMCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_GEARBOX,                  COM_SIG_TX_STATUSBODYDATA1_BD1_GEARBOX },
        { COM_SIG_RX_CENTRALCOMMAND1_FOGLIGHTSCOMMAND,         COM_SIG_TX_STATUSBODYDATA1_BD1_FOGLIGHTSCOMMAND },

        { COM_SIG_RX_CENTRALCOMMAND1_RECIRCULATIONCOMMAND,      COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYRECIRCULATION },
        { COM_SIG_RX_CENTRALCOMMAND1_CLIMATEAUTOCOMMAND,       COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYCLIMATEAUTO },
        { COM_SIG_RX_CENTRALCOMMAND1_CLIMATETEMPERATURECOMMAND, COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYCLIMATEMP },
        { COM_SIG_RX_CENTRALCOMMAND1_CLIMATEFANCOMMAND,        COM_SIG_TX_COMMANDDISPLAYSTATUS_COMMANDDISPLAYCLIMAFAN },

        { COM_SIG_RX_MILEAGE_MILEAGETRIP,                      COM_SIG_TX_CANFD_INFOTAINMENTDATA1_IT1_MILEAGETRIP },
        { COM_SIG_RX_MILEAGE_MILEAGETOTAL,                     COM_SIG_TX_CANFD_INFOTAINMENTDATA1_IT1_MILEAGETOTAL },
        { COM_SIG_RX_DMUSTATUS_DISPLAYCAMERASTATUS,            COM_SIG_TX_CANFD_INFOTAINMENTDATA1_IT1_DISPLAYCAMERASTATUS },
        { COM_SIG_RX_DMU_ALIVE_ALIVEINDICATION,                COM_SIG_TX_CANFD_INFOTAINMENTDATA1_IT1_ALIVEINDICATION },

        { COM_SIG_RX_CENTRALLOCKDATA_VIBRATIONSENSORSTATUS,    COM_SIG_TX_CANFD_BODYDATA1_BD1_VIBRATIONSENSORSTATUS },
        { COM_SIG_RX_CENTRALLOCKDATA_CENTRALLOCKSTATUS,        COM_SIG_TX_CANFD_BODYDATA1_BD1_CENTRALLOCKSTATUS },
        { COM_SIG_RX_CENTRALLOCKDATA_CENTRALLOCKBUZZER,        COM_SIG_TX_CANFD_BODYDATA1_BD1_CENTRALLOCKBUZZER },
        { COM_SIG_RX_LIGHTDATA1_REVERSELIGTHSTATUS,            COM_SIG_TX_CANFD_LIGHTDATA1_LD1_REVERSELIGTHSTATUS },
        { COM_SIG_RX_LIGHTDATA1_POSITIONLIGHTSTATUS,           COM_SIG_TX_CANFD_LIGHTDATA1_LD1_POSITIONLIGHTSTATUS },
        { COM_SIG_RX_LIGHTDATA1_LOWBEAMSTATUS,                 COM_SIG_TX_CANFD_LIGHTDATA1_LD1_LOWBEAMSTATUS },
        { COM_SIG_RX_LIGHTDATA1_INTERIORLIGHTSTATUS,           COM_SIG_TX_CANFD_LIGHTDATA1_LD1_INTERIORLIGHTSTATUS },
        { COM_SIG_RX_LIGHTDATA1_HIGHBEAMSTATUS,                COM_SIG_TX_CANFD_LIGHTDATA1_LD1_HIGHBEAMSTATUS },
        { COM_SIG_RX_LIGHTDATA1_FOGLIGHTSSTATUS,               COM_SIG_TX_CANFD_LIGHTDATA1_LD1_FOGLIGHTSSTATUS },
        { COM_SIG_RX_LIGHTDATA1_TURNSIGNALSSTATUS,             COM_SIG_TX_CANFD_LIGHTDATA1_LD1_TURNSIGNALSSTATUS },
        { COM_SIG_RX_LIGHTDATA1_BRAKELIGHTSTATUS,              COM_SIG_TX_CANFD_BODYDATA1_BD1_BRAKELIGHTSTATUS },
        { COM_SIG_RX_STATUSACTUATOR_STATUSWIPERS,              COM_SIG_TX_CANFD_BODYDATA1_BD1_STATUSWIPERS },
        { COM_SIG_RX_STATUSACTUATOR_STATUSDOORS,               COM_SIG_TX_CANFD_BODYDATA1_BD1_STATUSDOORS },
        { COM_SIG_RX_OUTSIDETEMPERATURESTATUS_OUTSIDETEMPERATURE, COM_SIG_TX_CANFD_BODYDATA1_BD1_OUTSIDETEMPERATURE },

        { COM_SIG_RX_CENTRALCOMMAND1_HIGHBEAMCOMMAND,          COM_SIG_TX_CANFD_BODYDATA1_BD1_HIGHBEAMCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_HC05CONNECTIONSTATUS,     COM_SIG_TX_CANFD_BODYDATA1_BD1_HC05CONNECTIONSTATUS },
        { COM_SIG_RX_CENTRALCOMMAND1_GEARBOX,                  COM_SIG_TX_CANFD_BODYDATA1_BD1_GEARBOX },
        { COM_SIG_RX_CENTRALCOMMAND1_RECIRCULATIONCOMMAND,     COM_SIG_TX_CANFD_BODYDATA1_BD1_RECIRCULATIONCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_TURNSIGNALCOMMAND,        COM_SIG_TX_CANFD_LIGHTDATA1_LD1_TURNSIGNALCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_IGNITION,                 COM_SIG_TX_CANFD_BODYDATA1_BD1_IGNITION },
        { COM_SIG_RX_CENTRALCOMMAND1_CLIMATEFANCOMMAND,        COM_SIG_TX_CANFD_BODYDATA1_BD1_CLIMATEFANCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_FOGLIGHTSCOMMAND,         COM_SIG_TX_CANFD_BODYDATA1_BD1_FOGLIGHTSCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_RLSCOMMAND,               COM_SIG_TX_CANFD_BODYDATA1_BD1_RLSCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_CLIMATEAUTOCOMMAND,       COM_SIG_TX_CANFD_BODYDATA1_BD1_CLIMATEAUTOCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_CLIMATETEMPERATURECOMMAND, COM_SIG_TX_CANFD_BODYDATA1_BD1_CLIMATETEMPERATURECOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_WIPERSTOCKCOMMAND,        COM_SIG_TX_CANFD_BODYDATA1_BD1_WIPERSTOCKCOMMAND },
        { COM_SIG_RX_CENTRALCOMMAND1_LIGTHSENSOR,              COM_SIG_TX_CANFD_BODYDATA1_BD1_LIGTHSENSOR },
        { COM_SIG_RX_CENTRALCOMMAND1_CENTRALLOCKCOMMAND,       COM_SIG_TX_CANFD_BODYDATA1_BD1_CENTRALLOCKCOMMAND },

        { COM_SIG_RX_DSCDATA2_VEHSPEED,                        COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_VEHSPEED },
        { COM_SIG_RX_DSCDATA1_TRACTIONCONTROLSTATUS,           COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_TRACTIONCONTROLSTATUS },
        { COM_SIG_RX_ENGINEDATA1_TORQUEVALUE,                  COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_TORQUEVALUE },
        { COM_SIG_RX_ASGDATA1_SHIFTPHASE,                      COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_SHIFTPHASE },
        { COM_SIG_RX_ENGINEDATA1_PEDALSTATUS,                  COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_PEDALSTATUS },
        { COM_SIG_RX_ENGINEDATA1_ENGINERPM,                    COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_ENGINERPM },
        { COM_SIG_RX_DSCDATA1_DSCSTATUS,                       COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_DSCSTATUS },
        { COM_SIG_RX_ASGDATA1_CURRENTGEAR,                     COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_CURRENTGEAR },
        { COM_SIG_RX_ASGDATA1_CLUTCHSTATUS,                    COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_CLUTCHSTATUS },
        { COM_SIG_RX_DSCDATA1_BRAKEWHEELSTATUS,                COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_BRAKEWHEELSTATUS },
        { COM_SIG_RX_ASGDATA1_ASGSTATUS,                       COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_ASGSTATUS },
        { COM_SIG_RX_ENGINEDATA1_ACTUALTORQUE,                 COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_ACTUALTORQUE },
        { COM_SIG_RX_DSCDATA1_ABSSTATUS,                       COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_ABSSTATUS },
        { COM_SIG_RX_DSCDATA1_BRAKEPRESSUREVALUE,              COM_SIG_TX_CANFD_POWERTRAINDATA2_PT2_BRAKEPRESSUREVALUE },

        { COM_SIG_RX_DSCDATA3_LATACC,                          COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_LATACC },
        { COM_SIG_RX_ENGINEDATA7_INTAKETEMPERATURE,            COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_INTAKETEMPERATURE },
        { COM_SIG_RX_ENGINEDATA7_COOLANTTEMPERATURE,           COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_COOLANTTEMPERATURE },
        { COM_SIG_RX_DSCDATA2_YAWRATE,                         COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_YAWRATE },
        { COM_SIG_RX_ENGINEDATA2_SHORTTERMFUELTRIMBANK2,       COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_SHORTTERMFUELTRIMBANK2 },
        { COM_SIG_RX_ENGINEDATA2_SHORTTERMFUELTRIMBANK1,       COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_SHORTTERMFUELTRIMBANK1 },
        { COM_SIG_RX_ENGINEDATA3_O2SENSORVOLTAGE2BANK2,        COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_O2SENSORVOLTAGE2BANK2 },
        { COM_SIG_RX_ENGINEDATA3_O2SENSORVOLTAGE2BANK1,        COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_O2SENSORVOLTAGE2BANK1 },
        { COM_SIG_RX_ENGINEDATA4_O2SENSORVOLTAGE1BANK2,        COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_O2SENSORVOLTAGE1BANK2 },
        { COM_SIG_RX_ENGINEDATA4_O2SENSORVOLTAGE1BANK1,        COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_O2SENSORVOLTAGE1BANK1 },
        { COM_SIG_RX_ENGINEDATA5_O2SENSORBANK1_SHORTTERMFUELTRIM, COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_O2SENSB1_SHORTTERMFUELTRIM },
        { COM_SIG_RX_ENGINEDATA5_O2SENSORBANK2_SHORTTERMFUELTRIM, COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_O2SENB2_SHORTTERMFUELTRIM },
        { COM_SIG_RX_ENGINEDATA1_MAFVALUE,                     COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_MAFVALUE },
        { COM_SIG_RX_ENGINEDATA6_LONGTERMFUELTRIMBANK2,        COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_LONGTERMFUELTRIMBANK2 },
        { COM_SIG_RX_ENGINEDATA6_LONGTERMFUELTRIMBANK1,        COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_LONGTERMFUELTRIMBANK1 },
        { COM_SIG_RX_DSCDATA3_LONGACC,                         COM_SIG_TX_CANFD_POWERTRAINDATA1_PT1_LONGACC },

        { COM_SIG_RX_PDCSTAT_PDCDISTANCEREAR,                  COM_SIG_TX_CANFD_BODYDATA1_BD1_PDCDISTANCEREAR },
        { COM_SIG_RX_PDCSTAT_PDCDISTANCEFRONT,                 COM_SIG_TX_CANFD_BODYDATA1_BD1_PDCDISTANCEFRONT },
        { COM_SIG_RX_PDCSTAT_PDCBUZZERFRONTREAR,               COM_SIG_TX_CANFD_BODYDATA1_BD1_PDCBUZZERFRONTREAR },

        { COM_SIG_RX_BATTFULLSTAT_TIMETOFULL,                  COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_TIMETOFULL },
        { COM_SIG_RX_VOLTAGECURRENT_T30VOLTAGE,                COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_T30VOLTAGE },
        { COM_SIG_RX_BATTSOCSOH_SOH,                           COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_SOH },
        { COM_SIG_RX_BATTSOC_SOCOCV,                           COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_SOCOCV },
        { COM_SIG_RX_BATTSOCSOH_SOCHYBRID,                     COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_SOCHYBRID },
        { COM_SIG_RX_BATTSOC_SOCCOULOMB,                       COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_SOCCOULOMB },
        { COM_SIG_RX_BATTFULLSTAT_RUNTIMEREMAINING,            COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_RUNTIMEREMAINING },
        { COM_SIG_RX_TEMPMEAS_MCUTEMP,                         COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_MCUTEMP },
        { COM_SIG_RX_VOLTAGECURRENT_L1VOLTAGE,                 COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_L1VOLTAGE },
        { COM_SIG_RX_TEMPMEAS_L1TEMP,                          COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_L1TEMP },
        { COM_SIG_RX_VOLTAGECURRENT_L1ISENSE,                  COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_L1ISENSE },
        { COM_SIG_RX_BATTCAPRES_INTRESISTANCE,                 COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_INTRESISTANCE },
        { COM_SIG_RX_L1_I2T_COUNTER_I2TCOUNTER,                COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_I2TCOUNTER },
        { COM_SIG_RX_BATTCAPDISCHARGE_DISCHARGEAH,             COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_DISCHARGEAH },
        { COM_SIG_RX_BATTCAPDISCHARGE_CHARGEAH,                COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_CHARGEAH },
        { COM_SIG_RX_BATTCURRENT_AVGDISCHARGECURRENT,          COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_AVGDISCHARGECURRENT },
        { COM_SIG_RX_BATTCURRENT_AVGCHARGECURRENT,             COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA1_EM1_AVGCHARGECURRENT },

        { COM_SIG_RX_BATTDIAGNOSIS_WEAKBATTERY,                COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_WEAKBATTERY },
        { COM_SIG_RX_BATTDIAGNOSIS_VALIDOCVCALIBRATION,        COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_VALIDOCVCALIBRATION },
        { COM_SIG_RX_LOADSTATUS_LOADSTATUS,                    COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_LOADSTATUS },
        { COM_SIG_RX_BATTDIAGNOSIS_DISCHARGING,                COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_DISCHARGING },
        { COM_SIG_RX_BATTDIAGNOSIS_DEEPDISCHARGE,              COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_DEEPDISCHARGE },
        { COM_SIG_RX_BATTDIAGNOSIS_CRANKINGEVENT,              COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_CRANKINGEVENT },
        { COM_SIG_RX_BATTDIAGNOSIS_CONFINDENCE,                COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_CONFINDENCE },
        { COM_SIG_RX_BATTDIAGNOSIS_CHARGING,                   COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_CHARGING },
        { COM_SIG_RX_BATTCAPRES_CAPACITYAH,                    COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_CAPACITYAH },
        { COM_SIG_RX_BATTDIAGNOSIS_BATTREST,                   COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA2_EM1_BATTREST },

        { COM_SIG_RX_LIN_ALT_STATUS_ALT_RESPONSEERROR,         COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_ALT_RESPONSEERROR },
        { COM_SIG_RX_LIN_ALT_STATUS_ALT_CHARGINGACTIVE,        COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_ALT_CHARGINGACTIVE },
        { COM_SIG_RX_LIN_ALT_STATUS_ALT_OUTPUTCURRENT,         COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_ALT_OUTPUTCURRENT },
        { COM_SIG_RX_LIN_ALT_STATUS_ALT_OUTPUTVOLTAGE,         COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_ALT_OUTPUTVOLTAGE },
        { COM_SIG_RX_LIN_ALT_STATUS_ALT_FIELDDUTYACTUAL,       COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_ALT_FIELDDUTYACTUAL },
        { COM_SIG_RX_LIN_ALT_STATUS_ALT_POWER,                 COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_ALT_POWER },
        { COM_SIG_RX_LIN_ALT_STATUS_ALT_TEMPERATURE,           COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_ALT_TEMPERATURE },

        { COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_RESPONSEERROR,   COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_HVDCDC_RESPONSEERROR },
        { COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_LV_VOLTAGE,      COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_HVDCDC_LV_VOLTAGE },
        { COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_LV_CURRENT,      COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_HVDCDC_LV_CURRENT },
        { COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_HV_VOLTAGE,      COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_HVDCDC_HV_VOLTAGE },
        { COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_HV_CURRENT,      COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_HVDCDC_HV_CURRENT },

        { COM_SIG_RX_LIN_PCU48_STATUS_PCU48_RESPONSEERROR,     COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_PCU48_RESPONSEERROR },
        { COM_SIG_RX_LIN_PCU48_STATUS_PCU48_SOH,               COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_PCU48_SOH },
        { COM_SIG_RX_LIN_PCU48_STATUS_PCU48_SOC,               COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_PCU48_SOC },
        { COM_SIG_RX_LIN_PCU48_STATUS_PCU48_VOLTAGE,           COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_PCU48_VOLTAGE },
        { COM_SIG_RX_LIN_PCU48_STATUS_PCU48_CHARGESTATE,       COM_SIG_TX_CANFD_ENERGYMANAGEMENTDATA3_PCU48_CHARGESTATE }
};

static const GatewaySwc_PduRangeType GatewaySwc_RxMessageDiagRanges[] =
{
        { COM_RX_PDU_CENTRALLOCKDATA,        COM_RX_PDU_DMU_ALIVE,                       GATEWAYSWC_BUS_CAN },
        { COM_RX_PDU_VOLTAGECURRENT,         COM_RX_PDU_L1_I2T_COUNTER,                  GATEWAYSWC_BUS_CAN },
        { COM_RX_PDU_BATTSOCSOH,             COM_RX_PDU_BATTCAPRES,                      GATEWAYSWC_BUS_CAN },
        { COM_RX_PDU_CANFD_PDM4_LOADSTATUS,  COM_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_5, GATEWAYSWC_BUS_CANFD },
        { COM_RX_PDU_LIN_ALT_STATUS,         COM_RX_PDU_LIN_PCU48_STATUS,                GATEWAYSWC_BUS_LIN }
};

static const GatewaySwc_SignalRangeType GatewaySwc_EthSignalRanges[] =
{
        { GATEWAYSWC_CAN_RX_FIRST_0, GATEWAYSWC_CAN_RX_LAST_0, GATEWAYSWC_BUS_CAN },
        { GATEWAYSWC_CAN_RX_FIRST_1, GATEWAYSWC_CAN_RX_LAST_1, GATEWAYSWC_BUS_CAN },
        { GATEWAYSWC_CANFD_RX_FIRST, GATEWAYSWC_CANFD_RX_LAST, GATEWAYSWC_BUS_CANFD },
        { GATEWAYSWC_LIN_RX_FIRST,   GATEWAYSWC_LIN_RX_LAST,   GATEWAYSWC_BUS_LIN }
};

static void GatewaySwc_LoadDefaultCommands(void);
static void GatewaySwc_RouteSignals(void);
static void GatewaySwc_GenerateOwnedOutputs(void);
static void GatewaySwc_GenerateSdatOutputs(void);
static Std_ReturnType GatewaySwc_GetSdatTime(GatewaySwc_SdatTimeType *time);
static uint32 GatewaySwc_MapSdatTimeSource(uint8 timeSource);
static void GatewaySwc_SendCanSdat(const GatewaySwc_SdatTimeType *time);
static void GatewaySwc_SendCanFdSdat(const GatewaySwc_SdatTimeType *time);
static void GatewaySwc_GenerateCanOutputs(void);
static void GatewaySwc_GenerateCanFdOutputs(void);
static void GatewaySwc_GenerateLinOutputs(void);
static void GatewaySwc_PublishEthernetSummary(void);
static void GatewaySwc_McuStatusInit(void);
static void GatewaySwc_McuStatusMainFunction(uint64 nowNs);
static void GatewaySwc_McuStatusOpenSocket(void);
static void GatewaySwc_McuStatusCloseSocket(void);
static void GatewaySwc_McuStatusBuildPacket(uint8 *packet);
static void GatewaySwc_McuStatusSendPacket(void);
static void GatewaySwc_McuStatusUpdateNextTxDeadline(uint64 nowNs);
static uint16 GatewaySwc_McuStatusScaleMilliVolt(float32 voltage);
static sint16 GatewaySwc_McuStatusScaleCentiDeg(float32 temperature);
static void GatewaySwc_PublishRange(GatewaySwc_BusType bus,
        Com_SignalIdType firstSignalId,
        Com_SignalIdType lastSignalId);
static void GatewaySwc_ResetRxDiagnostics(void);
static void GatewaySwc_UpdateRxDiagnostics(void);
static void GatewaySwc_UpdateComModeFromBusActivity(void);
static void GatewaySwc_RequestFullCom(void);
static void GatewaySwc_RequestNoCom(void);
static void GatewaySwc_ReportRxMessageTimeoutToDem(uint16 index, uint8 status);
static void GatewaySwc_ReportRxSignalInvalidToDem(uint16 index, uint8 status);
static Std_ReturnType GatewaySwc_GetPduDiagConfig(uint16 index, PduIdType *pduId, GatewaySwc_BusType *bus);
static Std_ReturnType GatewaySwc_GetSignalDiagConfig(uint16 index, Com_SignalIdType *signalId, GatewaySwc_BusType *bus);
static uint8 GatewaySwc_IsNmActiveForBus(GatewaySwc_BusType bus);
static uint8 GatewaySwc_IsEthComActive(void);
static void GatewaySwc_ResetEthFrame(uint16 *len, GatewaySwc_BusType bus);
static void GatewaySwc_FlushEthFrame(uint16 *len);
static void GatewaySwc_UpdateDtcDebugQueueState(void);
static void GatewaySwc_FlushDtcTransitionQueue(void);
static uint8 GatewaySwc_EnterDtcQueueCritical(void);
static void GatewaySwc_ExitDtcQueueCritical(void);
static uint8 GatewaySwc_PopDtcTransition(GatewaySwc_DtcTransitionType *transition);
static void GatewaySwc_PublishDtcTransition(Dem_DTCType dtc, Dem_UdsStatusByteType status);
static void GatewaySwc_AppendU8(uint16 *len, uint8 value);
static void GatewaySwc_AppendU16(uint16 *len, uint16 value);
static void GatewaySwc_AppendU32(uint16 *len, uint32 value);
static void GatewaySwc_StoreU16(uint8 *buffer, uint16 offset, uint16 value);
static void GatewaySwc_StoreS16(uint8 *buffer, uint16 offset, sint16 value);
static void GatewaySwc_StoreU32(uint8 *buffer, uint16 offset, uint32 value);
static void GatewaySwc_StoreU64(uint8 *buffer, uint16 offset, uint64 value);
static Std_ReturnType GatewaySwc_SendU32(Com_SignalIdType signalId, uint32 value);
static Std_ReturnType GatewaySwc_ReceiveU32(Com_SignalIdType signalId, uint32 *value);
static void GatewaySwc_CaptureEthRxMessage(uint8 soConId,
        const TcpIp_SockAddrType *remoteAddr,
        const uint8 *data,
        uint16 len);

void GatewaySwc_Init(void)
{
    GatewaySwc_Initialized = 0u;
    GatewaySwc_Status.mainCycles = 0u;
    GatewaySwc_Status.routedSignals = 0u;
    GatewaySwc_Status.generatedSignals = 0u;
    GatewaySwc_Status.sendSignalOk = 0u;
    GatewaySwc_Status.sendSignalFailed = 0u;
    GatewaySwc_Status.receiveSignalFailed = 0u;
    GatewaySwc_Status.ethFramesSent = 0u;
    GatewaySwc_Status.ethFramesFailed = 0u;
    GatewaySwc_Status.ethSignalsPublished = 0u;
    GatewaySwc_Status.ethLastOpenResult = 0u;
    GatewaySwc_Status.ethLastSoAdResult = (uint8)SOAD_NOT_OK;
    GatewaySwc_Status.ethLastTransmitOk = 0u;
    GatewaySwc_Status.ethLastPayloadLength = 0u;
    GatewaySwc_Status.configuredRoutes = (uint16)(sizeof(GatewaySwc_RouteMap) / sizeof(GatewaySwc_RouteMap[0]));
    GatewaySwc_Status.rxDiagConfiguredMessages = (uint16)GATEWAYSWC_RX_MESSAGE_DIAG_COUNT;
    GatewaySwc_Status.rxDiagConfiguredSignals = (uint16)GATEWAYSWC_RX_SIGNAL_DIAG_COUNT;
    GatewaySwc_Status.rxDiagTimedOutMessages = 0u;
    GatewaySwc_Status.rxDiagInvalidSignals = 0u;
    GatewaySwc_Status.rxDiagTimeoutEvents = 0u;
    GatewaySwc_Status.rxDiagInvalidEvents = 0u;
    GatewaySwc_Status.rxDiagLastTimeoutPduId = 0u;
    GatewaySwc_Status.rxDiagLastInvalidSignalId = 0u;
    GatewaySwc_Status.rxDiagLastInvalidValue = 0u;

    GatewaySwc_RouteTimerMs = 0u;
    GatewaySwc_OutputTimerMs = 0u;
    GatewaySwc_NextEthPublishTimeNs = 0ull;
    GatewaySwc_McuStatusInit();
    GatewaySwc_DiagTimerMs = 0u;
    GatewaySwc_LoadRequestAliveCounter = 0u;
    GatewaySwc_DtcTransitionHead = 0u;
    GatewaySwc_DtcTransitionTail = 0u;
    GatewaySwc_DebugDtcDropped = 0u;
    GatewaySwc_DebugDtcFlushed = 0u;
    GatewaySwc_DebugDtcLastDtc = 0u;
    GatewaySwc_DebugDtcLastStatus = 0u;
    GatewaySwc_DebugDtcLastOpenResult = 0u;
    GatewaySwc_DebugDtcLastTxResult = SOAD_NOT_OK;
    GatewaySwc_DebugDtcQueueLockTimeout = 0u;
    GatewaySwc_UpdateDtcDebugQueueState();
    GatewaySwc_ComModeRequestState = GATEWAYSWC_COMM_UNKNOWN;
    GatewaySwc_ComFullRequested = 0u;
    GatewaySwc_ResetRxDiagnostics();

    GatewaySwc_EthRxCaptureEnable = GATEWAYSWC_ETH_RX_CAPTURE_DEFAULT_ENABLE;
    GatewaySwc_EthRxCaptureFilterMode = GATEWAYSWC_ETH_RX_CAPTURE_FILTER_OFF;
    GatewaySwc_EthRxCaptureFilterSoConId = GATEWAYSWC_ETH_SOCON_ID;
    GatewaySwc_EthRxCaptureMaxLen = GATEWAYSWC_ETH_RX_CAPTURE_MAX_PAYLOAD;
    GatewaySwc_EthRxCaptureWriteIndex = 0u;
    GatewaySwc_EthRxCaptureLastIndex = 0u;
    GatewaySwc_EthRxCaptureCounter = 0u;
    GatewaySwc_EthRxCaptureDroppedCounter = 0u;
    GatewaySwc_EthRxCaptureTruncatedCounter = 0u;

    GatewaySwc_LoadDefaultCommands();

    (void)Com_IpduGroupStart(COM_IPDU_GROUP_0);
    (void)Com_IpduGroupStart(COM_IPDU_GROUP_1);
    (void)SoAd_OpenSoCon((SoAd_SoConIdType)GATEWAYSWC_ETH_SOCON_ID);

    GatewaySwc_GenerateOwnedOutputs();
    GatewaySwc_RouteSignals();
    GatewaySwc_Initialized = 1u;
}

void GatewaySwc_MainFunction(void)
{
    uint64 nowNs;

    if (GatewaySwc_Initialized == 0u)
    {
        return;
    }

    GatewaySwc_Status.mainCycles++;

    GatewaySwc_UpdateComModeFromBusActivity();

    GatewaySwc_RouteTimerMs += GATEWAYSWC_MAIN_PERIOD_MS;
    GatewaySwc_OutputTimerMs += GATEWAYSWC_MAIN_PERIOD_MS;
    GatewaySwc_DiagTimerMs += GATEWAYSWC_MAIN_PERIOD_MS;
    nowNs = TimeBase_PlatformGetCounterNs();

    GatewaySwc_McuStatusMainFunction(nowNs);

    if (GatewaySwc_RouteTimerMs >= GATEWAYSWC_ROUTE_PERIOD_MS)
    {
        GatewaySwc_RouteTimerMs = 0u;
        GatewaySwc_RouteSignals();
    }

    if (GatewaySwc_OutputTimerMs >= GATEWAYSWC_OUTPUT_PERIOD_MS)
    {
        GatewaySwc_OutputTimerMs = 0u;
        GatewaySwc_GenerateOwnedOutputs();
    }

    if (GatewaySwc_NextEthPublishTimeNs == 0ull)
    {
        GatewaySwc_NextEthPublishTimeNs = nowNs + GATEWAYSWC_ETH_PERIOD_NS;
    }
    else if (nowNs >= GatewaySwc_NextEthPublishTimeNs)
    {
        GatewaySwc_PublishEthernetSummary();
        GatewaySwc_NextEthPublishTimeNs = nowNs + GATEWAYSWC_ETH_PERIOD_NS;
    }

    if (GatewaySwc_DiagTimerMs >= GATEWAYSWC_DIAG_PERIOD_MS)
    {
        GatewaySwc_DiagTimerMs = 0u;
        GatewaySwc_UpdateRxDiagnostics();
    }

    GatewaySwc_FlushDtcTransitionQueue();

    GatewaySwc_MainFunction_Counter++;
}

void GatewaySwc_SetCommands(const GatewaySwc_CommandType *cmd)
{
    if (cmd != NULL_PTR)
    {
        GatewaySwc_Command = *cmd;
    }
}

void GatewaySwc_GetCommands(GatewaySwc_CommandType *cmd)
{
    if (cmd != NULL_PTR)
    {
        *cmd = GatewaySwc_Command;
    }
}

void GatewaySwc_GetStatus(GatewaySwc_StatusType *status)
{
    if (status != NULL_PTR)
    {
        *status = GatewaySwc_Status;
    }
}

uint8 GatewaySwc_GetMcuStatusStatus(void)
{
    return GatewaySwc_McuStatusStatus;
}

uint32 GatewaySwc_GetMcuStatusTxCounter(void)
{
    return GatewaySwc_McuStatusTxCounter;
}

uint16 GatewaySwc_GetRxMessageDiagCount(void)
{
    return (uint16)GATEWAYSWC_RX_MESSAGE_DIAG_COUNT;
}

uint16 GatewaySwc_GetRxSignalDiagCount(void)
{
    return (uint16)GATEWAYSWC_RX_SIGNAL_DIAG_COUNT;
}

Std_ReturnType GatewaySwc_GetRxMessageDiag(uint16 index, GatewaySwc_RxMessageDiagType *diag)
{
    PduIdType pduId;
    GatewaySwc_BusType bus;
    uint16 cycleTicks = 0u;
    uint16 timeoutTicks = 0u;

    if ((diag == NULL_PTR) ||
            (GatewaySwc_GetPduDiagConfig(index, &pduId, &bus) != E_OK))
    {
        return E_NOT_OK;
    }

    diag->pduId = (uint16)pduId;
    diag->bus = bus;
    diag->status = GatewaySwc_RxMessageDiagStatus[index];
    (void)Com_GetRxPduDiagConfig(pduId, &cycleTicks, &timeoutTicks);
    diag->cycleTicks = cycleTicks;
    diag->timeoutTicks = timeoutTicks;

    return E_OK;
}

Std_ReturnType GatewaySwc_GetRxSignalDiag(uint16 index, GatewaySwc_RxSignalDiagType *diag)
{
    Com_SignalIdType signalId;
    GatewaySwc_BusType bus;

    if ((diag == NULL_PTR) ||
            (GatewaySwc_GetSignalDiagConfig(index, &signalId, &bus) != E_OK))
    {
        return E_NOT_OK;
    }

    diag->signalId = (uint16)signalId;
    diag->bus = bus;
    diag->status = GatewaySwc_RxSignalDiagStatus[index];
    diag->value = GatewaySwc_RxSignalDiagValue[index];
    diag->invalidValue = GatewaySwc_RxSignalDiagInvalidValue[index];

    return E_OK;
}

Std_ReturnType GatewaySwc_GetRxMessageDiagSnapshot(uint16 index, GatewaySwc_RxDiagSnapshotType *snapshot)
{
    PduIdType pduId;
    GatewaySwc_BusType bus;
    uint16 cycleTicks = 0u;
    uint16 timeoutTicks = 0u;

    if ((snapshot == NULL_PTR) ||
            (GatewaySwc_GetPduDiagConfig(index, &pduId, &bus) != E_OK))
    {
        return E_NOT_OK;
    }

    (void)Com_GetRxPduDiagConfig(pduId, &cycleTicks, &timeoutTicks);

    snapshot->kind = GATEWAYSWC_RX_DIAG_KIND_MESSAGE_TIMEOUT;
    snapshot->diagIndex = index;
    snapshot->objectId = (uint16)pduId;
    snapshot->bus = bus;
    snapshot->status = GatewaySwc_RxMessageDiagStatus[index];
    snapshot->cycleTicks = cycleTicks;
    snapshot->timeoutTicks = timeoutTicks;
    snapshot->value = 0u;
    snapshot->thresholdValue = (uint32)timeoutTicks;
    snapshot->mainCycles = GatewaySwc_Status.mainCycles;
    snapshot->activeCount = GatewaySwc_Status.rxDiagTimedOutMessages;
    snapshot->eventCount = GatewaySwc_Status.rxDiagTimeoutEvents;

    return E_OK;
}

Std_ReturnType GatewaySwc_GetRxSignalDiagSnapshot(uint16 index, GatewaySwc_RxDiagSnapshotType *snapshot)
{
    Com_SignalIdType signalId;
    GatewaySwc_BusType bus;

    if ((snapshot == NULL_PTR) ||
            (GatewaySwc_GetSignalDiagConfig(index, &signalId, &bus) != E_OK))
    {
        return E_NOT_OK;
    }

    snapshot->kind = GATEWAYSWC_RX_DIAG_KIND_SIGNAL_INVALID;
    snapshot->diagIndex = index;
    snapshot->objectId = (uint16)signalId;
    snapshot->bus = bus;
    snapshot->status = GatewaySwc_RxSignalDiagStatus[index];
    snapshot->cycleTicks = 0u;
    snapshot->timeoutTicks = 0u;
    snapshot->value = GatewaySwc_RxSignalDiagValue[index];
    snapshot->thresholdValue = GatewaySwc_RxSignalDiagInvalidValue[index];
    snapshot->mainCycles = GatewaySwc_Status.mainCycles;
    snapshot->activeCount = GatewaySwc_Status.rxDiagInvalidSignals;
    snapshot->eventCount = GatewaySwc_Status.rxDiagInvalidEvents;

    return E_OK;
}

Std_ReturnType GatewaySwc_CaptureRxDiagFreezeFrame(Dem_EventIdType eventId, uint8 *buffer, uint16 *length)
{
    GatewaySwc_RxDiagSnapshotType snapshot;
    uint16 index;
    uint16 i;

    if ((buffer == NULL_PTR) || (length == NULL_PTR) || (*length < 32u))
    {
        return E_NOT_OK;
    }

    if ((eventId >= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST) &&
            (eventId <= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_LAST))
    {
        index = (uint16)(eventId - DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST);

        if (GatewaySwc_GetRxMessageDiagSnapshot(index, &snapshot) != E_OK)
        {
            return E_NOT_OK;
        }
    }
    else if ((eventId >= DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST) &&
            (eventId <= DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_LAST))
    {
        index = (uint16)(eventId - DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST);

        if (GatewaySwc_GetRxSignalDiagSnapshot(index, &snapshot) != E_OK)
        {
            return E_NOT_OK;
        }
    }
    else
    {
        return E_NOT_OK;
    }

    for (i = 0u; i < 32u; i++)
    {
        buffer[i] = 0u;
    }

    buffer[0u] = snapshot.kind;
    buffer[1u] = (uint8)snapshot.bus;
    GatewaySwc_StoreU16(buffer, 2u, snapshot.diagIndex);
    GatewaySwc_StoreU16(buffer, 4u, snapshot.objectId);
    buffer[6u] = snapshot.status;
    GatewaySwc_StoreU32(buffer, 8u, snapshot.mainCycles);
    GatewaySwc_StoreU16(buffer, 12u, snapshot.cycleTicks);
    GatewaySwc_StoreU16(buffer, 14u, snapshot.timeoutTicks);
    GatewaySwc_StoreU32(buffer, 16u, snapshot.value);
    GatewaySwc_StoreU32(buffer, 20u, snapshot.thresholdValue);
    GatewaySwc_StoreU16(buffer, 24u, snapshot.activeCount);
    GatewaySwc_StoreU32(buffer, 26u, snapshot.eventCount);

    *length = 32u;
    return E_OK;
}

Std_ReturnType GatewaySwc_CaptureRxDiagExtendedData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length)
{
    GatewaySwc_RxDiagSnapshotType snapshot;
    uint16 index;
    uint16 i;

    if ((buffer == NULL_PTR) || (length == NULL_PTR) || (*length < 16u))
    {
        return E_NOT_OK;
    }

    if ((eventId >= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST) &&
            (eventId <= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_LAST))
    {
        index = (uint16)(eventId - DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST);

        if (GatewaySwc_GetRxMessageDiagSnapshot(index, &snapshot) != E_OK)
        {
            return E_NOT_OK;
        }
    }
    else if ((eventId >= DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST) &&
            (eventId <= DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_LAST))
    {
        index = (uint16)(eventId - DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST);

        if (GatewaySwc_GetRxSignalDiagSnapshot(index, &snapshot) != E_OK)
        {
            return E_NOT_OK;
        }
    }
    else
    {
        return E_NOT_OK;
    }

    for (i = 0u; i < 16u; i++)
    {
        buffer[i] = 0u;
    }

    buffer[0u] = snapshot.kind;
    buffer[1u] = (uint8)snapshot.bus;
    GatewaySwc_StoreU16(buffer, 2u, snapshot.objectId);
    GatewaySwc_StoreU32(buffer, 4u, snapshot.value);
    GatewaySwc_StoreU32(buffer, 8u, snapshot.thresholdValue);
    GatewaySwc_StoreU32(buffer, 12u, snapshot.mainCycles);

    *length = 16u;
    return E_OK;
}

void GatewaySwc_EthRxIndication(uint8 soConId,
        const TcpIp_SockAddrType *remoteAddr,
        const uint8 *data,
        uint16 len)
{
    uint16 signalId;
    uint32 value;
    GatewaySwc_CommandType cmd;

    GatewaySwc_CaptureEthRxMessage(soConId, remoteAddr, data, len);

    if ((soConId != GATEWAYSWC_ETH_SOCON_ID) || (data == NULL_PTR) || (len < 4u))
    {
        return;
    }

    if ((data[0] != GATEWAYSWC_MAGIC0) ||
            (data[1] != GATEWAYSWC_MAGIC1) ||
            (data[2] != GATEWAYSWC_MAGIC2))
    {
        return;
    }

    if ((data[3] == GATEWAYSWC_FRAME_COMMAND_SIGNAL) && (len >= 10u))
    {
        signalId = (uint16)(((uint16)data[4] << 8u) | (uint16)data[5]);

        value = ((uint32)data[6] << 24u) |
                ((uint32)data[7] << 16u) |
                ((uint32)data[8] << 8u) |
                ((uint32)data[9]);

        (void)GatewaySwc_SendU32((Com_SignalIdType)signalId, value);
    }
    else if ((data[3] == GATEWAYSWC_FRAME_COMMAND_BLOCK) && (len >= 88u))
    {
        uint16 idx = 4u;
        uint8 p;
        uint8 l;

        cmd = GatewaySwc_Command;

        for (p = 0u; p < 4u; p++)
        {
            for (l = 0u; l < GATEWAYSWC_PDM_LOADS_PER_PDM; l++)
            {
                cmd.pdmCommandLoad[p][l] = ((uint32)data[idx] << 24u) |
                        ((uint32)data[idx + 1u] << 16u) |
                        ((uint32)data[idx + 2u] << 8u) |
                        ((uint32)data[idx + 3u]);
                idx += 4u;
            }
        }

        cmd.linAltTargetCurrent = ((uint32)data[idx] << 24u) | ((uint32)data[idx + 1u] << 16u) | ((uint32)data[idx + 2u] << 8u) | ((uint32)data[idx + 3u]); idx += 4u;
        cmd.linAltTargetVoltage = ((uint32)data[idx] << 24u) | ((uint32)data[idx + 1u] << 16u) | ((uint32)data[idx + 2u] << 8u) | ((uint32)data[idx + 3u]); idx += 4u;
        cmd.linAltEnable = ((uint32)data[idx] << 24u) | ((uint32)data[idx + 1u] << 16u) | ((uint32)data[idx + 2u] << 8u) | ((uint32)data[idx + 3u]); idx += 4u;
        cmd.linAltFieldDutyCommand = ((uint32)data[idx] << 24u) | ((uint32)data[idx + 1u] << 16u) | ((uint32)data[idx + 2u] << 8u) | ((uint32)data[idx + 3u]); idx += 4u;
        cmd.linHvDcdcEnable = ((uint32)data[idx] << 24u) | ((uint32)data[idx + 1u] << 16u) | ((uint32)data[idx + 2u] << 8u) | ((uint32)data[idx + 3u]); idx += 4u;
        cmd.linHvDcdcTargetVoltage = ((uint32)data[idx] << 24u) | ((uint32)data[idx + 1u] << 16u) | ((uint32)data[idx + 2u] << 8u) | ((uint32)data[idx + 3u]); idx += 4u;
        cmd.linPcu48RequestAvailability = ((uint32)data[idx] << 24u) | ((uint32)data[idx + 1u] << 16u) | ((uint32)data[idx + 2u] << 8u) | ((uint32)data[idx + 3u]); idx += 4u;
        cmd.vehicleStatus = ((uint32)data[idx] << 24u) | ((uint32)data[idx + 1u] << 16u) | ((uint32)data[idx + 2u] << 8u) | ((uint32)data[idx + 3u]); idx += 4u;
        cmd.nmPn1 = ((uint32)data[idx] << 24u) | ((uint32)data[idx + 1u] << 16u) | ((uint32)data[idx + 2u] << 8u) | ((uint32)data[idx + 3u]);

        GatewaySwc_Command = cmd;
        GatewaySwc_GenerateOwnedOutputs();
    }
    else
    {
        /* Unknown command frame. */
    }
}


static void GatewaySwc_CaptureEthRxMessage(uint8 soConId,
        const TcpIp_SockAddrType *remoteAddr,
        const uint8 *data,
        uint16 len)
{
    uint8 slot;
    uint16 i;
    uint16 copyLen;
    uint16 configuredMaxLen;

    if (GatewaySwc_EthRxCaptureEnable == 0u)
    {
        return;
    }

    if (data == NULL_PTR)
    {
        GatewaySwc_EthRxCaptureDroppedCounter++;
        return;
    }

    if ((GatewaySwc_EthRxCaptureFilterMode == GATEWAYSWC_ETH_RX_CAPTURE_FILTER_SOCON) &&
            (soConId != GatewaySwc_EthRxCaptureFilterSoConId))
    {
        return;
    }

    configuredMaxLen = GatewaySwc_EthRxCaptureMaxLen;

    if (configuredMaxLen > GATEWAYSWC_ETH_RX_CAPTURE_MAX_PAYLOAD)
    {
        configuredMaxLen = GATEWAYSWC_ETH_RX_CAPTURE_MAX_PAYLOAD;
    }

    if (configuredMaxLen == 0u)
    {
        GatewaySwc_EthRxCaptureDroppedCounter++;
        return;
    }

    copyLen = len;

    if (copyLen > configuredMaxLen)
    {
        copyLen = configuredMaxLen;
        GatewaySwc_EthRxCaptureTruncatedCounter++;
    }

    slot = GatewaySwc_EthRxCaptureWriteIndex;

    if (slot >= GATEWAYSWC_ETH_RX_CAPTURE_SLOT_COUNT)
    {
        slot = 0u;
    }

    for (i = 0u; i < copyLen; i++)
    {
        GatewaySwc_EthRxCaptureData[slot][i] = data[i];
    }

    for (i = copyLen; i < GATEWAYSWC_ETH_RX_CAPTURE_MAX_PAYLOAD; i++)
    {
        GatewaySwc_EthRxCaptureData[slot][i] = 0u;
    }

    GatewaySwc_EthRxCaptureSoConId[slot] = soConId;
    GatewaySwc_EthRxCaptureLen[slot] = len;
    GatewaySwc_EthRxCaptureCopiedLen[slot] = copyLen;
    GatewaySwc_EthRxCaptureMainCycle[slot] = GatewaySwc_Status.mainCycles;
    GatewaySwc_EthRxCaptureRemoteAddrPtr[slot] = (uint32)remoteAddr;
    GatewaySwc_EthRxCaptureLastIndex = slot;
    GatewaySwc_EthRxCaptureCounter++;

    slot++;

    if (slot >= GATEWAYSWC_ETH_RX_CAPTURE_SLOT_COUNT)
    {
        slot = 0u;
    }

    GatewaySwc_EthRxCaptureWriteIndex = slot;
}


static void GatewaySwc_LoadDefaultCommands(void)
{
    uint8 p;
    uint8 l;

    for (p = 0u; p < 4u; p++)
    {
        for (l = 0u; l < GATEWAYSWC_PDM_LOADS_PER_PDM; l++)
        {
            GatewaySwc_Command.pdmCommandLoad[p][l] = 0u;
        }
    }

    GatewaySwc_Command.linAltTargetCurrent = 0u;
    GatewaySwc_Command.linAltTargetVoltage = 14500u;
    GatewaySwc_Command.linAltEnable = 1u;
    GatewaySwc_Command.linAltFieldDutyCommand = 0u;

    GatewaySwc_Command.linHvDcdcEnable = 1u;
    GatewaySwc_Command.linHvDcdcTargetVoltage = 48000u;

    GatewaySwc_Command.linPcu48RequestAvailability = 1u;

    GatewaySwc_Command.vehicleStatus = 6u;
    GatewaySwc_Command.nmPn1 = 0xFFu;
}

static void GatewaySwc_RouteSignals(void)
{
    uint16 i;
    uint32 value;

    for (i = 0u; i < (uint16)(sizeof(GatewaySwc_RouteMap) / sizeof(GatewaySwc_RouteMap[0])); i++)
    {
        if (GatewaySwc_ReceiveU32(GatewaySwc_RouteMap[i].rxSignalId, &value) == E_OK)
        {
            if (GatewaySwc_SendU32(GatewaySwc_RouteMap[i].txSignalId, value) == E_OK)
            {
                GatewaySwc_Status.routedSignals++;
            }
        }
    }
}

static void GatewaySwc_GenerateOwnedOutputs(void)
{
    GatewaySwc_GenerateCanOutputs();
    GatewaySwc_GenerateCanFdOutputs();
    GatewaySwc_GenerateSdatOutputs();
    GatewaySwc_GenerateLinOutputs();
}

static void GatewaySwc_GenerateSdatOutputs(void)
{
    GatewaySwc_SdatTimeType time;

    if (GatewaySwc_GetSdatTime(&time) == E_OK)
    {
        GatewaySwc_SendCanSdat(&time);
        GatewaySwc_SendCanFdSdat(&time);
    }
}

static Std_ReturnType GatewaySwc_GetSdatTime(GatewaySwc_SdatTimeType *time)
{
    TimeBase_TimestampSnapshotType snapshot;
    TimeBase_DateTimeType dateTime;
    uint32 yearRaw;
    uint32 millisecondRaw;

    if (time == NULL_PTR)
    {
        return E_NOT_OK;
    }

    TimeBase_GetTimestampSnapshot(&snapshot);
    time->utcValid = (snapshot.utc_valid != FALSE) ? 1u : 0u;
    time->timeSource = GatewaySwc_MapSdatTimeSource(snapshot.time_source);
    time->defaultTime = (snapshot.time_source == (uint8)TIMEBASE_SOURCE_DEFAULT_COMPILETIME) ? 1u : 0u;
    time->nvmRestored = ((snapshot.sync_status & TIMEBASE_SYNC_STATUS_NVM_RESTORED) != 0u) ? 1u : 0u;

    if ((snapshot.utc_valid == FALSE) ||
        (TimeBase_ConvertUtcNsToDateTime(snapshot.utc_time_ns, &dateTime) != E_OK))
    {
        time->year = 0u;
        time->month = 0u;
        time->day = 0u;
        time->hour = 0u;
        time->minute = 0u;
        time->second = 0u;
        time->millisecond = 0u;
        time->utcValid = 0u;
        time->timeSource = GATEWAYSWC_SDAT_SOURCE_INVALID;
        time->defaultTime = 0u;
        time->nvmRestored = 0u;
        return E_OK;
    }

    if (dateTime.year <= (uint16)GATEWAYSWC_SDAT_YEAR_BASE)
    {
        yearRaw = 0u;
    }
    else
    {
        yearRaw = (uint32)dateTime.year - GATEWAYSWC_SDAT_YEAR_BASE;
        if (yearRaw > GATEWAYSWC_SDAT_YEAR_MAX_RAW)
        {
            yearRaw = GATEWAYSWC_SDAT_YEAR_MAX_RAW;
        }
    }

    millisecondRaw = ((uint32)dateTime.millisecond + (GATEWAYSWC_SDAT_MS_RESOLUTION / 2u)) /
            GATEWAYSWC_SDAT_MS_RESOLUTION;
    if (millisecondRaw > GATEWAYSWC_SDAT_MS_MAX_RAW)
    {
        millisecondRaw = GATEWAYSWC_SDAT_MS_MAX_RAW;
    }

    time->year = yearRaw;
    time->month = (uint32)dateTime.month;
    time->day = (uint32)dateTime.day;
    time->hour = (uint32)dateTime.hour;
    time->minute = (uint32)dateTime.minute;
    time->second = (uint32)dateTime.second;
    time->millisecond = millisecondRaw;

    return E_OK;
}

static uint32 GatewaySwc_MapSdatTimeSource(uint8 timeSource)
{
    switch ((TimeBase_TimeSourceType)timeSource)
    {
        case TIMEBASE_SOURCE_DEFAULT_COMPILETIME:
            return GATEWAYSWC_SDAT_SOURCE_DEFAULT;

        case TIMEBASE_SOURCE_UDS_ROUTINE:
            return GATEWAYSWC_SDAT_SOURCE_UDS;

        case TIMEBASE_SOURCE_GPTP_MASTER:
            return GATEWAYSWC_SDAT_SOURCE_GPTP;

        default:
            return GATEWAYSWC_SDAT_SOURCE_INVALID;
    }
}

static void GatewaySwc_SendCanSdat(const GatewaySwc_SdatTimeType *time)
{
    if (time == NULL_PTR)
    {
        return;
    }

    (void)GatewaySwc_SendU32(COM_SIG_TX_SDAT_CSB_SDAT_YEAR, time->year);
    (void)GatewaySwc_SendU32(COM_SIG_TX_SDAT_CSB_SDAT_SECOND, time->second);
    (void)GatewaySwc_SendU32(COM_SIG_TX_SDAT_CSB_SDAT_MONTH, time->month);
    (void)GatewaySwc_SendU32(COM_SIG_TX_SDAT_CSB_SDAT_MINUTE, time->minute);
    (void)GatewaySwc_SendU32(COM_SIG_TX_SDAT_CSB_SDAT_MILLISECOND, time->millisecond);
    (void)GatewaySwc_SendU32(COM_SIG_TX_SDAT_CSB_SDAT_HOUR, time->hour);
    (void)GatewaySwc_SendU32(COM_SIG_TX_SDAT_CSB_SDAT_DAY, time->day);
    (void)GatewaySwc_SendU32(COM_SIG_TX_SDAT_CSB_SDAT_TIMESOURCE, time->timeSource);
    (void)GatewaySwc_SendU32(COM_SIG_TX_SDAT_CSB_SDAT_UTCVALID, time->utcValid);
    (void)GatewaySwc_SendU32(COM_SIG_TX_SDAT_CSB_SDAT_DEFAULTTIME, time->defaultTime);
    (void)GatewaySwc_SendU32(COM_SIG_TX_SDAT_CSB_SDAT_NVMRESTORED, time->nvmRestored);
}

static void GatewaySwc_SendCanFdSdat(const GatewaySwc_SdatTimeType *time)
{
    if (time == NULL_PTR)
    {
        return;
    }

    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_YEAR, time->year);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_SECOND, time->second);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_MONTH, time->month);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_MINUTE, time->minute);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_MILLISECOND, time->millisecond);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_HOUR, time->hour);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_DAY, time->day);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_TIMESOURCE, time->timeSource);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_UTCVALID, time->utcValid);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_DEFAULTTIME, time->defaultTime);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_SDAT_CANFD_SDAT_NVMRESTORED, time->nvmRestored);
}

static void GatewaySwc_GenerateCanOutputs(void)
{
    static uint32 counter = 0u;
    static uint8 prevStat_LoadRequestStatus = 0u;
    uint32 alive;
    uint32 crc;
    uint8 hasChanged = 0u;
    uint8 crcarr[3];

    counter++;
    GatewaySwc_DebugLoadRequestCounter = counter;

    if(prevStat_LoadRequestStatus != GatewaySwc_LoadRequest_Status)
    {
        prevStat_LoadRequestStatus = GatewaySwc_LoadRequest_Status;
        hasChanged = 1;
    }

    GatewaySwc_DebugLoadRequestChanged = hasChanged;

    GatewaySwc_LoadRequestAliveCounter++;
    GatewaySwc_LoadRequestAliveCounter &= 0x0Fu;

    alive = (uint32)GatewaySwc_LoadRequestAliveCounter;
    crcarr[0] = GatewaySwc_LoadRequestAliveCounter;
    crcarr[1] = GatewaySwc_LoadRequest_Status;
    crcarr[2] = GatewaySwc_LoadRequest_DataId;
    crc = Crc_CalculateCRC32(&crcarr[0u], 3, 0xFFFFFFFFu, 1u);
    GatewaySwc_DebugLoadRequestAlive = alive;
    GatewaySwc_DebugLoadRequestCrc = crc;

    (void)GatewaySwc_SendU32(COM_SIG_TX_VEHICLESTATE_VEHICLESTATUS, GatewaySwc_Command.vehicleStatus);
    (void)GatewaySwc_SendU32(COM_SIG_TX_NM3_NM3_PN1, GatewaySwc_Command.nmPn1);

    (void)GatewaySwc_SendU32(COM_SIG_TX_LOADREQUEST_LOADREQUESTSTATUS, GatewaySwc_LoadRequest_Status);
    (void)GatewaySwc_SendU32(COM_SIG_TX_LOADREQUEST_LOADREQUESTDATAID, GatewaySwc_LoadRequest_DataId);
    (void)GatewaySwc_SendU32(COM_SIG_TX_LOADREQUEST_LOADREQUESTCRC, crc);
    (void)GatewaySwc_SendU32(COM_SIG_TX_LOADREQUEST_LOADREQUESTALIVECOUNTER, alive);
}

static void GatewaySwc_GenerateCanFdOutputs(void)
{
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_VEHICLESTATE_VEHICLESTATUS, GatewaySwc_Command.vehicleStatus);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_NM3_NM3_PN1, GatewaySwc_Command.nmPn1);

    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM1_PDM1_COMMANDLOAD_01, GatewaySwc_Command.pdmCommandLoad[0u][0u]);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM1_PDM1_COMMANDLOAD_02, GatewaySwc_Command.pdmCommandLoad[0u][1u]);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM1_PDM1_COMMANDLOAD_03, GatewaySwc_Command.pdmCommandLoad[0u][2u]);

    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM2_PDM2_COMMANDLOAD_01, GatewaySwc_Command.pdmCommandLoad[1u][0u]);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM2_PDM2_COMMANDLOAD_02, GatewaySwc_Command.pdmCommandLoad[1u][1u]);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM2_PDM2_COMMANDLOAD_03, GatewaySwc_Command.pdmCommandLoad[1u][2u]);

    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM3_PDM3_COMMANDLOAD_01, GatewaySwc_Command.pdmCommandLoad[2u][0u]);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM3_PDM3_COMMANDLOAD_02, GatewaySwc_Command.pdmCommandLoad[2u][1u]);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM3_PDM3_COMMANDLOAD_03, GatewaySwc_Command.pdmCommandLoad[2u][2u]);

    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM4_PDM4_COMMANDLOAD_01, GatewaySwc_Command.pdmCommandLoad[3u][0u]);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM4_PDM4_COMMANDLOAD_02, GatewaySwc_Command.pdmCommandLoad[3u][1u]);
    (void)GatewaySwc_SendU32(COM_SIG_TX_CANFD_COMMANDLOAD_PDM4_PDM4_COMMANDLOAD_03, GatewaySwc_Command.pdmCommandLoad[3u][2u]);
}

static void GatewaySwc_GenerateLinOutputs(void)
{
    (void)GatewaySwc_SendU32(COM_SIG_TX_LIN_ZGW_NM3_ZGW_NM3_PN1, GatewaySwc_Command.nmPn1);

    (void)GatewaySwc_SendU32(COM_SIG_TX_LIN_ZGW_REQUEST_ALT_ZGW_TARGETCURRENT_ALT, GatewaySwc_Command.linAltTargetCurrent);
    (void)GatewaySwc_SendU32(COM_SIG_TX_LIN_ZGW_REQUEST_ALT_ZGW_TARGETVOLTAGE_ALT, GatewaySwc_Command.linAltTargetVoltage);
    (void)GatewaySwc_SendU32(COM_SIG_TX_LIN_ZGW_REQUEST_ALT_ZGW_ENABLE_ALT, GatewaySwc_Command.linAltEnable);
    (void)GatewaySwc_SendU32(COM_SIG_TX_LIN_ZGW_REQUEST_ALT_ZGW_FIELDDUTYCOMMAND_ALT, GatewaySwc_Command.linAltFieldDutyCommand);

    (void)GatewaySwc_SendU32(COM_SIG_TX_LIN_ZGW_REQUEST_HVDCDC_ZGW_ENABLE_HVDCDC, GatewaySwc_Command.linHvDcdcEnable);
    (void)GatewaySwc_SendU32(COM_SIG_TX_LIN_ZGW_REQUEST_HVDCDC_ZGW_TARGETVOLTAGE_HVDCDC, GatewaySwc_Command.linHvDcdcTargetVoltage);

    (void)GatewaySwc_SendU32(COM_SIG_TX_LIN_ZGW_REQUEST_PCU48_ZGW_REQUESTAVAILABILITY_PCU48, GatewaySwc_Command.linPcu48RequestAvailability);
}

static void GatewaySwc_PublishEthernetSummary(void)
{
    uint16 i;

    for (i = 0u; i < (uint16)(sizeof(GatewaySwc_EthSignalRanges) / sizeof(GatewaySwc_EthSignalRanges[0])); i++)
    {
        GatewaySwc_PublishRange(GatewaySwc_EthSignalRanges[i].bus,
                GatewaySwc_EthSignalRanges[i].firstSignalId,
                GatewaySwc_EthSignalRanges[i].lastSignalId);
    }
}

static void GatewaySwc_McuStatusInit(void)
{
#if (GATEWAYSWC_MCU_STATUS_UDP_ENABLE == STD_ON)
    GatewaySwc_McuStatusNextTxTimeNs = 0ull;
    GatewaySwc_McuStatusSequenceCounter = 0u;
    GatewaySwc_McuStatusTxCounter = 0u;
    GatewaySwc_McuStatusStatus = GATEWAYSWC_MCU_STATUS_WAIT_LINK;
    GatewaySwc_McuStatusOpenSocket();
#else
    GatewaySwc_McuStatusStatus = GATEWAYSWC_MCU_STATUS_DISABLED;
#endif
}

static void GatewaySwc_McuStatusMainFunction(uint64 nowNs)
{
#if (GATEWAYSWC_MCU_STATUS_UDP_ENABLE == STD_ON)
    if (TcpIp_IsLinkAvailable() == 0u)
    {
        GatewaySwc_McuStatusNextTxTimeNs = 0ull;
        GatewaySwc_McuStatusStatus = GATEWAYSWC_MCU_STATUS_WAIT_LINK;
        return;
    }

    if (GatewaySwc_McuStatusSocket == TCPIP_INVALID_SOCKET)
    {
        GatewaySwc_McuStatusOpenSocket();
    }

    if (GatewaySwc_McuStatusNextTxTimeNs == 0ull)
    {
        GatewaySwc_McuStatusNextTxTimeNs = nowNs + GATEWAYSWC_MCU_STATUS_TX_PERIOD_NS;
    }
    else if (nowNs >= GatewaySwc_McuStatusNextTxTimeNs)
    {
        GatewaySwc_McuStatusSendPacket();
        GatewaySwc_McuStatusUpdateNextTxDeadline(nowNs);
    }
#else
    (void)nowNs;
#endif
}

static void GatewaySwc_McuStatusOpenSocket(void)
{
#if (GATEWAYSWC_MCU_STATUS_UDP_ENABLE == STD_ON)
    if (GatewaySwc_McuStatusSocket != TCPIP_INVALID_SOCKET)
    {
        return;
    }

    if (TcpIp_IsLinkAvailable() == 0u)
    {
        GatewaySwc_McuStatusStatus = GATEWAYSWC_MCU_STATUS_WAIT_LINK;
        return;
    }

    GatewaySwc_McuStatusSocket = TcpIp_Create(0u);
    if (GatewaySwc_McuStatusSocket == TCPIP_INVALID_SOCKET)
    {
        GatewaySwc_McuStatusStatus = GATEWAYSWC_MCU_STATUS_TX_ERROR;
        return;
    }

    if (TcpIp_Bind(GatewaySwc_McuStatusSocket, 0u) < 0)
    {
        GatewaySwc_McuStatusCloseSocket();
        GatewaySwc_McuStatusStatus = GATEWAYSWC_MCU_STATUS_TX_ERROR;
        return;
    }

    GatewaySwc_McuStatusStatus = GATEWAYSWC_MCU_STATUS_SOCKET_READY;
#endif
}

static void GatewaySwc_McuStatusCloseSocket(void)
{
#if (GATEWAYSWC_MCU_STATUS_UDP_ENABLE == STD_ON)
    if (GatewaySwc_McuStatusSocket != TCPIP_INVALID_SOCKET)
    {
        TcpIp_Close(GatewaySwc_McuStatusSocket);
        GatewaySwc_McuStatusSocket = TCPIP_INVALID_SOCKET;
    }
#endif
}

static void GatewaySwc_McuStatusBuildPacket(uint8 *packet)
{
    TimeBase_TimestampSnapshotType snapshot;
    uint32 crc;
    uint16 i;

    TimeBase_GetTimestampSnapshot(&snapshot);

    for (i = 0u; i < GATEWAYSWC_MCU_STATUS_PACKET_LENGTH; i++)
    {
        packet[i] = 0u;
    }

    GatewaySwc_StoreU32(packet, 0u, GATEWAYSWC_MCU_STATUS_MAGIC);
    packet[4u] = GATEWAYSWC_MCU_STATUS_VERSION;
    packet[5u] = GATEWAYSWC_MCU_STATUS_MESSAGE_STATUS;
    GatewaySwc_StoreU16(packet, 6u, GATEWAYSWC_MCU_STATUS_PACKET_LENGTH);
    GatewaySwc_StoreU32(packet, 8u, GatewaySwc_McuStatusSequenceCounter);
    GatewaySwc_StoreU64(packet, 12u, snapshot.vehicle_time_ns);
    packet[20u] = (uint8)g_SafetyKitStatus.safetyKitInitDone;
    packet[21u] = (uint8)g_SafetyKitStatus.wakeupFromStandby;
    GatewaySwc_StoreU16(packet, 22u, (uint16)g_SafetyKitStatus.resetCode.resetType);
    GatewaySwc_StoreU16(packet, 24u, (uint16)g_SafetyKitStatus.resetCode.resetTrigger);
    GatewaySwc_StoreU16(packet, 26u, g_SafetyKitStatus.resetCode.resetReason);
    GatewaySwc_StoreU16(packet, 28u, GatewaySwc_McuStatusScaleMilliVolt(g_SafetyKitStatus.voltStatus.vextVoltage));
    GatewaySwc_StoreU16(packet, 30u, GatewaySwc_McuStatusScaleMilliVolt(g_SafetyKitStatus.voltStatus.vddp3Voltage));
    GatewaySwc_StoreU16(packet, 32u, GatewaySwc_McuStatusScaleMilliVolt(g_SafetyKitStatus.voltStatus.coreVoltage));
    GatewaySwc_StoreU16(packet, 34u, GatewaySwc_McuStatusScaleMilliVolt(g_SafetyKitStatus.voltStatus.coreVoltageHighest));
    GatewaySwc_StoreU16(packet, 36u, GatewaySwc_McuStatusScaleMilliVolt(g_SafetyKitStatus.voltStatus.coreVoltageLowest));
    GatewaySwc_StoreU16(packet, 38u, GatewaySwc_McuStatusScaleMilliVolt(g_SafetyKitStatus.voltStatus.coreVoltageUvLimit));
    GatewaySwc_StoreS16(packet, 40u, GatewaySwc_McuStatusScaleCentiDeg(g_SafetyKitStatus.dieTempStatus.dieTemperaturePms));
    GatewaySwc_StoreS16(packet, 42u, GatewaySwc_McuStatusScaleCentiDeg(g_SafetyKitStatus.dieTempStatus.dieTemperatureCore));
    GatewaySwc_StoreS16(packet, 44u, GatewaySwc_McuStatusScaleCentiDeg(g_SafetyKitStatus.dieTempStatus.dieTempDifference));
    GatewaySwc_StoreS16(packet, 46u, GatewaySwc_McuStatusScaleCentiDeg(g_SafetyKitStatus.dieTempStatus.dieTempHighest));
    GatewaySwc_StoreS16(packet, 48u, GatewaySwc_McuStatusScaleCentiDeg(g_SafetyKitStatus.dieTempStatus.dieTempLowest));

    crc = Crc_CalculateCRC32(packet, GATEWAYSWC_MCU_STATUS_CRC_OFFSET, 0u, TRUE);
    GatewaySwc_StoreU32(packet, GATEWAYSWC_MCU_STATUS_CRC_OFFSET, crc);
}

static void GatewaySwc_McuStatusSendPacket(void)
{
#if (GATEWAYSWC_MCU_STATUS_UDP_ENABLE == STD_ON)
    uint8 packet[GATEWAYSWC_MCU_STATUS_PACKET_LENGTH];
    TcpIp_SockAddrType remoteAddr;
    sint32 sentLength;

    if ((GatewaySwc_McuStatusSocket == TCPIP_INVALID_SOCKET) ||
            (TcpIp_IsSocketOpen(GatewaySwc_McuStatusSocket) == 0u))
    {
        GatewaySwc_McuStatusSocket = TCPIP_INVALID_SOCKET;
        GatewaySwc_McuStatusOpenSocket();
        return;
    }

    remoteAddr.addr = GATEWAYSWC_MCU_STATUS_UDP_BROADCAST_ADDR;
    remoteAddr.port = GATEWAYSWC_MCU_STATUS_UDP_PORT;

    GatewaySwc_McuStatusBuildPacket(packet);
    sentLength = GatewaySwc_RequestTcpIpSendTo(GatewaySwc_McuStatusSocket,
            &remoteAddr,
            packet,
            GATEWAYSWC_MCU_STATUS_PACKET_LENGTH);
    GatewaySwc_Status.ethLastPayloadLength = GATEWAYSWC_MCU_STATUS_PACKET_LENGTH;
    if (sentLength == (sint32)GATEWAYSWC_MCU_STATUS_PACKET_LENGTH)
    {
        GatewaySwc_McuStatusSequenceCounter++;
        GatewaySwc_McuStatusTxCounter++;
        GatewaySwc_McuStatusStatus = GATEWAYSWC_MCU_STATUS_SOCKET_READY;
        GatewaySwc_Status.ethLastTransmitOk = 1u;
        GatewaySwc_Status.ethFramesSent++;
    }
    else
    {
        GatewaySwc_McuStatusStatus = GATEWAYSWC_MCU_STATUS_TX_ERROR;
        GatewaySwc_Status.ethLastTransmitOk = 0u;
        GatewaySwc_Status.ethFramesFailed++;
    }
#endif
}

static void GatewaySwc_McuStatusUpdateNextTxDeadline(uint64 nowNs)
{
    GatewaySwc_McuStatusNextTxTimeNs = nowNs + GATEWAYSWC_MCU_STATUS_TX_PERIOD_NS;
}

static uint16 GatewaySwc_McuStatusScaleMilliVolt(float32 voltage)
{
    float32 millivolts = voltage * 1000.0f;

    if (millivolts <= 0.0f)
    {
        return 0u;
    }

    if (millivolts >= 65535.0f)
    {
        return 0xFFFFu;
    }

    return (uint16)(millivolts + 0.5f);
}

static sint16 GatewaySwc_McuStatusScaleCentiDeg(float32 temperature)
{
    float32 centiDeg = temperature * 100.0f;

    if (centiDeg >= 32767.0f)
    {
        return 32767;
    }

    if (centiDeg <= -32768.0f)
    {
        return -32768;
    }

    if (centiDeg >= 0.0f)
    {
        return (sint16)(centiDeg + 0.5f);
    }

    return (sint16)(centiDeg - 0.5f);
}

static void GatewaySwc_PublishRange(GatewaySwc_BusType bus,
        Com_SignalIdType firstSignalId,
        Com_SignalIdType lastSignalId)
{
    Com_SignalIdType signalId;
    uint16 len;
    uint32 value;
    Std_ReturnType receiveResult;

    GatewaySwc_ResetEthFrame(&len, bus);
    GatewaySwc_DebugPublishBus = (uint8)bus;
    GatewaySwc_DebugPublishFirstSignalId = firstSignalId;
    GatewaySwc_DebugPublishLastSignalId = lastSignalId;
    GatewaySwc_DebugPublishInvalidRange = 0u;

    if (firstSignalId > lastSignalId)
    {
        GatewaySwc_DebugPublishInvalidRange = 1u;
        return;
    }

    for (signalId = firstSignalId; signalId <= lastSignalId; signalId++)
    {
        GatewaySwc_DebugPublishSignalId = signalId;
        receiveResult = GatewaySwc_ReceiveU32(signalId, &value);
        GatewaySwc_DebugPublishReceiveResult = (uint8)receiveResult;

        if (receiveResult != E_OK)
        {
            continue;
        }
        GatewaySwc_DebugPublishValue = value;
        GatewaySwc_DebugPublishLen = len;

        if ((uint16)(len + GATEWAYSWC_ENTRY_LEN) > GATEWAYSWC_ETH_MAX_PAYLOAD)
        {
            GatewaySwc_FlushEthFrame(&len);
            GatewaySwc_ResetEthFrame(&len, bus);
            GatewaySwc_DebugPublishLen = len;
        }

        GatewaySwc_AppendU16(&len, signalId);
        GatewaySwc_AppendU8(&len, (uint8)bus);
        GatewaySwc_AppendU32(&len, value);

        GatewaySwc_Status.ethSignalsPublished++;
        GatewaySwc_DebugPublishLen = len;
    }

    GatewaySwc_FlushEthFrame(&len);
    GatewaySwc_DebugPublishLen = len;
}

static void GatewaySwc_ResetRxDiagnostics(void)
{
    uint16 i;

    for (i = 0u; i < (uint16)GATEWAYSWC_RX_MESSAGE_DIAG_COUNT; i++)
    {
        GatewaySwc_RxMessageDiagStatus[i] = GATEWAYSWC_RX_DIAG_STATUS_OK;
        GatewaySwc_DemMessageTimeoutState[i] = GATEWAYSWC_DEM_STATE_UNKNOWN;
        GatewaySwc_RxMessageDiagDebugPduId[i] = 0u;
        GatewaySwc_RxMessageDiagDebugStatus[i] = GATEWAYSWC_RX_DIAG_STATUS_OK;
        GatewaySwc_RxMessageTimeoutCounter[i] = 0u;
        GatewaySwc_RxMessageTimeoutActiveSamples[i] = 0u;
    }

    for (i = 0u; i < (uint16)GATEWAYSWC_RX_SIGNAL_DIAG_COUNT; i++)
    {
        GatewaySwc_RxSignalDiagStatus[i] = GATEWAYSWC_RX_DIAG_STATUS_OK;
        GatewaySwc_RxSignalDiagValue[i] = 0u;
        GatewaySwc_RxSignalDiagInvalidValue[i] = 0u;
        GatewaySwc_DemSignalInvalidState[i] = GATEWAYSWC_DEM_STATE_UNKNOWN;
        GatewaySwc_RxSignalDiagDebugSignalId[i] = 0u;
        GatewaySwc_RxSignalDiagDebugStatus[i] = GATEWAYSWC_RX_DIAG_STATUS_OK;
        GatewaySwc_RxSignalDiagDebugValue[i] = 0u;
        GatewaySwc_RxSignalDiagDebugInvalidValue[i] = 0u;
        GatewaySwc_RxSignalTimeoutCounter[i] = 0u;
        GatewaySwc_RxSignalTimeoutActiveSamples[i] = 0u;
        GatewaySwc_RxSignalInvalidCounter[i] = 0u;
        GatewaySwc_RxSignalInvalidActiveSamples[i] = 0u;
    }
}

static uint8 GatewaySwc_IsNmActiveForBus(GatewaySwc_BusType bus)
{
    Nm_StateType state;
    Nm_ModeType mode;
    uint8 channel;

    switch (bus)
    {
        case GATEWAYSWC_BUS_CAN:
            channel = COMM_CH_CAN;
            break;

        case GATEWAYSWC_BUS_CANFD:
            channel = COMM_CH_CANFD;
            break;

        case GATEWAYSWC_BUS_LIN:
            channel = COMM_CH_LIN;
            break;

        default:
            return FALSE;
    }

    if (Nm_GetState(channel, &state, &mode) != E_OK)
    {
        return FALSE;
    }

    return ((mode == NM_MODE_NETWORK) &&
            (state != NM_STATE_BUS_SLEEP)) ? TRUE : FALSE;
}

static uint8 GatewaySwc_IsEthComActive(void)
{
    ComM_ModeType mode;

    if (ComM_GetCurrentComMode(COMM_CH_ETH, &mode) != E_OK)
    {
        return FALSE;
    }

    return (mode == COMM_FULL_COMMUNICATION) ? TRUE : FALSE;
}

static void GatewaySwc_UpdateRxDiagnostics(void)
{
    uint16 i;
    uint8 comStatus;
    uint8 status;
    uint8 previousStatus;
    uint16 timeoutCount = 0u;
    uint16 invalidCount = 0u;
    PduIdType pduId;
    Com_SignalIdType signalId;
    GatewaySwc_BusType bus;
    uint32 value;
    uint32 invalidValue;

    for (i = 0u; i < (uint16)GATEWAYSWC_RX_MESSAGE_DIAG_COUNT; i++)
    {
        status = GATEWAYSWC_RX_DIAG_STATUS_OK;
        pduId = 0u;
        bus = (GatewaySwc_BusType)0u;

        if ((GatewaySwc_GetPduDiagConfig(i, &pduId, &bus) == E_OK) &&
                (Com_GetRxPduDiagStatus(pduId, &comStatus) == E_OK))
        {
            if ((comStatus & COM_RX_DIAG_STATUS_TIMEOUT) != 0u)
            {
                status |= GATEWAYSWC_RX_DIAG_STATUS_TIMEOUT;
            }
        }
        else
        {
            pduId = 0u;
            bus = (GatewaySwc_BusType)0u;
        }

        if ((CodingApp_IsRxMessageExpected(i) == FALSE) ||
                (GatewaySwc_IsNmActiveForBus(bus) == FALSE))
        {
            status = GATEWAYSWC_RX_DIAG_STATUS_OK;
        }

        previousStatus = GatewaySwc_RxMessageDiagStatus[i];
        GatewaySwc_RxMessageDiagStatus[i] = status;
        GatewaySwc_RxMessageDiagDebugPduId[i] = (uint16)pduId;
        GatewaySwc_RxMessageDiagDebugStatus[i] = status;

        if ((status & GATEWAYSWC_RX_DIAG_STATUS_TIMEOUT) != 0u)
        {
            timeoutCount++;
            GatewaySwc_RxMessageTimeoutActiveSamples[i]++;

            if ((previousStatus & GATEWAYSWC_RX_DIAG_STATUS_TIMEOUT) == 0u)
            {
                GatewaySwc_RxMessageTimeoutCounter[i]++;
                GatewaySwc_Status.rxDiagTimeoutEvents++;
                GatewaySwc_Status.rxDiagLastTimeoutPduId = (uint16)pduId;
            }
        }
    }

    for (i = 0u; i < (uint16)GATEWAYSWC_RX_SIGNAL_DIAG_COUNT; i++)
    {
        status = GATEWAYSWC_RX_DIAG_STATUS_OK;
        value = 0u;
        invalidValue = 0u;
        signalId = 0u;
        bus = (GatewaySwc_BusType)0u;

        if ((GatewaySwc_GetSignalDiagConfig(i, &signalId, &bus) == E_OK) &&
                (Com_GetRxSignalDiagStatus(signalId, &value, &invalidValue, &comStatus) == E_OK))
        {
            if ((comStatus & COM_RX_DIAG_STATUS_TIMEOUT) != 0u)
            {
                status |= GATEWAYSWC_RX_DIAG_STATUS_TIMEOUT;
            }

            if ((comStatus & COM_RX_DIAG_STATUS_INVALID) != 0u)
            {
                status |= GATEWAYSWC_RX_DIAG_STATUS_INVALID;
            }
        }
        else
        {
            signalId = 0u;
            bus = (GatewaySwc_BusType)0u;
        }

        if ((CodingApp_IsCoded() == FALSE) ||
                (GatewaySwc_IsNmActiveForBus(bus) == FALSE))
        {
            status = GATEWAYSWC_RX_DIAG_STATUS_OK;
        }

        previousStatus = GatewaySwc_RxSignalDiagStatus[i];
        GatewaySwc_RxSignalDiagStatus[i] = status;
        GatewaySwc_RxSignalDiagValue[i] = value;
        GatewaySwc_RxSignalDiagInvalidValue[i] = invalidValue;
        GatewaySwc_RxSignalDiagDebugSignalId[i] = (uint16)signalId;
        GatewaySwc_RxSignalDiagDebugStatus[i] = status;
        GatewaySwc_RxSignalDiagDebugValue[i] = value;
        GatewaySwc_RxSignalDiagDebugInvalidValue[i] = invalidValue;

        if ((status & GATEWAYSWC_RX_DIAG_STATUS_TIMEOUT) != 0u)
        {
            GatewaySwc_RxSignalTimeoutActiveSamples[i]++;

            if ((previousStatus & GATEWAYSWC_RX_DIAG_STATUS_TIMEOUT) == 0u)
            {
                GatewaySwc_RxSignalTimeoutCounter[i]++;
            }
        }

        if ((status & GATEWAYSWC_RX_DIAG_STATUS_INVALID) != 0u)
        {
            invalidCount++;
            GatewaySwc_RxSignalInvalidActiveSamples[i]++;

            if ((previousStatus & GATEWAYSWC_RX_DIAG_STATUS_INVALID) == 0u)
            {
                GatewaySwc_RxSignalInvalidCounter[i]++;
                GatewaySwc_Status.rxDiagInvalidEvents++;
                GatewaySwc_Status.rxDiagLastInvalidSignalId = (uint16)signalId;
                GatewaySwc_Status.rxDiagLastInvalidValue = value;
            }
        }
    }

    GatewaySwc_Status.rxDiagTimedOutMessages = timeoutCount;
    GatewaySwc_Status.rxDiagInvalidSignals = invalidCount;

    for (i = 0u; i < (uint16)GATEWAYSWC_RX_MESSAGE_DIAG_COUNT; i++)
    {
        GatewaySwc_ReportRxMessageTimeoutToDem(i, GatewaySwc_RxMessageDiagStatus[i]);
    }

    for (i = 0u; i < (uint16)GATEWAYSWC_RX_SIGNAL_DIAG_COUNT; i++)
    {
        GatewaySwc_ReportRxSignalInvalidToDem(i, GatewaySwc_RxSignalDiagStatus[i]);
    }
}

static void GatewaySwc_UpdateComModeFromBusActivity(void)
{
    if (SysMgr_BusActivityCounter > 0u)
    {
        GatewaySwc_RequestFullCom();
    }
    else
    {
        GatewaySwc_RequestNoCom();
    }
}

static void GatewaySwc_RequestFullCom(void)
{
    if (GatewaySwc_ComModeRequestState == GATEWAYSWC_COMM_FULL)
    {
        return;
    }

    if (ComM_RequestComMode(COMM_USER_GATEWAY,
            COMM_FULL_COMMUNICATION) == E_OK)
    {
        GatewaySwc_ComModeRequestState = GATEWAYSWC_COMM_FULL;
        GatewaySwc_ComFullRequested = 1u;
    }
}

static void GatewaySwc_RequestNoCom(void)
{
    if (GatewaySwc_ComModeRequestState == GATEWAYSWC_COMM_NO)
    {
        return;
    }

    if (ComM_RequestComMode(COMM_USER_GATEWAY,
            COMM_NO_COMMUNICATION) == E_OK)
    {
        GatewaySwc_ComModeRequestState = GATEWAYSWC_COMM_NO;
        GatewaySwc_ComFullRequested = 0u;
    }
}

static void GatewaySwc_ReportRxMessageTimeoutToDem(uint16 index, uint8 status)
{
    Dem_EventStatusType desiredStatus;
    Dem_EventIdType eventId;

    if (Dem_IsReady() == FALSE)
    {
        return;
    }

    if (index >= (uint16)GATEWAYSWC_RX_MESSAGE_DIAG_COUNT)
    {
        return;
    }

    desiredStatus = ((status & GATEWAYSWC_RX_DIAG_STATUS_TIMEOUT) != 0u) ?
            DEM_EVENT_STATUS_FAILED :
            DEM_EVENT_STATUS_PASSED;

    if ((GatewaySwc_DemMessageTimeoutState[index] == GATEWAYSWC_DEM_STATE_UNKNOWN) &&
            (desiredStatus == DEM_EVENT_STATUS_PASSED))
    {
        GatewaySwc_DemMessageTimeoutState[index] = (uint8)desiredStatus;
        return;
    }

    if (GatewaySwc_DemMessageTimeoutState[index] != (uint8)desiredStatus)
    {
        eventId = (Dem_EventIdType)(DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST + index);

        if (Dem_ReportErrorStatus(eventId, desiredStatus) == E_OK)
        {
            GatewaySwc_DemMessageTimeoutState[index] = (uint8)desiredStatus;
        }
    }
}

static void GatewaySwc_ReportRxSignalInvalidToDem(uint16 index, uint8 status)
{
    Dem_EventStatusType desiredStatus;
    Dem_EventIdType eventId;

    if (Dem_IsReady() == FALSE)
    {
        return;
    }

    if (index >= (uint16)GATEWAYSWC_RX_SIGNAL_DIAG_COUNT)
    {
        return;
    }

    desiredStatus = ((status & GATEWAYSWC_RX_DIAG_STATUS_INVALID) != 0u) ?
            DEM_EVENT_STATUS_FAILED :
            DEM_EVENT_STATUS_PASSED;

    if ((GatewaySwc_DemSignalInvalidState[index] == GATEWAYSWC_DEM_STATE_UNKNOWN) &&
            (desiredStatus == DEM_EVENT_STATUS_PASSED))
    {
        GatewaySwc_DemSignalInvalidState[index] = (uint8)desiredStatus;
        return;
    }

    if (GatewaySwc_DemSignalInvalidState[index] != (uint8)desiredStatus)
    {
        eventId = (Dem_EventIdType)(DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST + index);

        if (Dem_ReportErrorStatus(eventId, desiredStatus) == E_OK)
        {
            GatewaySwc_DemSignalInvalidState[index] = (uint8)desiredStatus;
        }
    }
}

static Std_ReturnType GatewaySwc_GetPduDiagConfig(uint16 index, PduIdType *pduId, GatewaySwc_BusType *bus)
{
    uint16 i;
    uint16 rangeSize;
    uint16 offset = index;

    if ((pduId == NULL_PTR) || (bus == NULL_PTR))
    {
        return E_NOT_OK;
    }

    for (i = 0u; i < (uint16)(sizeof(GatewaySwc_RxMessageDiagRanges) / sizeof(GatewaySwc_RxMessageDiagRanges[0])); i++)
    {
        rangeSize = GATEWAYSWC_RANGE_SIZE(GatewaySwc_RxMessageDiagRanges[i].firstPduId,
                GatewaySwc_RxMessageDiagRanges[i].lastPduId);

        if (offset < rangeSize)
        {
            *pduId = (PduIdType)((uint16)GatewaySwc_RxMessageDiagRanges[i].firstPduId + offset);
            *bus = GatewaySwc_RxMessageDiagRanges[i].bus;
            return E_OK;
        }

        offset = (uint16)(offset - rangeSize);
    }

    return E_NOT_OK;
}

static Std_ReturnType GatewaySwc_GetSignalDiagConfig(uint16 index, Com_SignalIdType *signalId, GatewaySwc_BusType *bus)
{
    uint16 i;
    uint16 rangeSize;
    uint16 offset = index;

    if ((signalId == NULL_PTR) || (bus == NULL_PTR))
    {
        return E_NOT_OK;
    }

    for (i = 0u; i < (uint16)(sizeof(GatewaySwc_EthSignalRanges) / sizeof(GatewaySwc_EthSignalRanges[0])); i++)
    {
        rangeSize = GATEWAYSWC_RANGE_SIZE(GatewaySwc_EthSignalRanges[i].firstSignalId,
                GatewaySwc_EthSignalRanges[i].lastSignalId);

        if (offset < rangeSize)
        {
            *signalId = (Com_SignalIdType)((uint16)GatewaySwc_EthSignalRanges[i].firstSignalId + offset);
            *bus = GatewaySwc_EthSignalRanges[i].bus;
            return E_OK;
        }

        offset = (uint16)(offset - rangeSize);
    }

    return E_NOT_OK;
}

static void GatewaySwc_ResetEthFrame(uint16 *len, GatewaySwc_BusType bus)
{
    *len = 0u;

    GatewaySwc_AppendU8(len, GATEWAYSWC_MAGIC0);
    GatewaySwc_AppendU8(len, GATEWAYSWC_MAGIC1);
    GatewaySwc_AppendU8(len, GATEWAYSWC_MAGIC2);
    GatewaySwc_AppendU8(len, GATEWAYSWC_FRAME_SUMMARY);

    GatewaySwc_AppendU32(len, GatewaySwc_Status.mainCycles);
    GatewaySwc_AppendU8(len, (uint8)bus);
    GatewaySwc_AppendU8(len, 0u);
    GatewaySwc_AppendU8(len, 0u);
    GatewaySwc_AppendU8(len, 0u);
}

static void GatewaySwc_FlushEthFrame(uint16 *len)
{
    SoAd_ReturnType txResult;

    if (*len <= GATEWAYSWC_HEADER_LEN)
    {
        *len = 0u;
        return;
    }

    if (GatewaySwc_IsEthComActive() == FALSE)
    {
        GatewaySwc_Status.ethLastOpenResult = 0u;
        GatewaySwc_Status.ethLastPayloadLength = *len;
        GatewaySwc_Status.ethLastSoAdResult = SOAD_NOT_OK;
        GatewaySwc_Status.ethLastTransmitOk = 0u;
        *len = 0u;
        return;
    }

    GatewaySwc_Status.ethLastOpenResult =
        SoAd_OpenSoCon((SoAd_SoConIdType)GATEWAYSWC_ETH_SOCON_ID);
    GatewaySwc_Status.ethLastPayloadLength = *len;

    txResult = GatewaySwc_RequestSoAdIfTransmit((SoAd_SoConIdType)GATEWAYSWC_ETH_SOCON_ID,
            NULL_PTR,
            GatewaySwc_EthBuffer,
            *len);
    GatewaySwc_Status.ethLastSoAdResult = (uint8)txResult;

    if (txResult == SOAD_OK)
    {
        GatewaySwc_Status.ethLastTransmitOk = 1u;
        GatewaySwc_Status.ethFramesSent++;
    }
    else
    {
        GatewaySwc_Status.ethLastTransmitOk = 0u;
        GatewaySwc_Status.ethFramesFailed++;
    }

    *len = 0u;
}

void GatewaySwc_ReportDtcTransition(Dem_DTCType dtc, Dem_UdsStatusByteType status)
{
    uint8 nextHead;

    if (GatewaySwc_EnterDtcQueueCritical() == FALSE)
    {
        GatewaySwc_DebugDtcDropped++;
        return;
    }

    nextHead = (uint8)((GatewaySwc_DtcTransitionHead + 1u) % GATEWAYSWC_DTC_TRANSITION_QUEUE_SIZE);
    if (nextHead == GatewaySwc_DtcTransitionTail)
    {
        GatewaySwc_Status.ethFramesFailed++;
        GatewaySwc_DebugDtcDropped++;
        GatewaySwc_UpdateDtcDebugQueueState();
        GatewaySwc_ExitDtcQueueCritical();
        return;
    }

    GatewaySwc_DtcTransitionQueue[GatewaySwc_DtcTransitionHead].dtc = dtc & 0x00FFFFFFu;
    GatewaySwc_DtcTransitionQueue[GatewaySwc_DtcTransitionHead].status = status;
    __dsync();
    GatewaySwc_DtcTransitionHead = nextHead;
    GatewaySwc_UpdateDtcDebugQueueState();
    GatewaySwc_ExitDtcQueueCritical();
}

void GatewaySwc_OnDemEventCleared(Dem_EventIdType eventId)
{
    if ((eventId >= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST) &&
        (eventId <= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_LAST))
    {
        uint16 index = (uint16)(eventId - DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST);

        if (index < (uint16)GATEWAYSWC_RX_MESSAGE_DIAG_COUNT)
        {
            GatewaySwc_DemMessageTimeoutState[index] = GATEWAYSWC_DEM_STATE_UNKNOWN;
        }
    }
    else if ((eventId >= DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST) &&
        (eventId <= DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_LAST))
    {
        uint16 index = (uint16)(eventId - DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST);

        if (index < (uint16)GATEWAYSWC_RX_SIGNAL_DIAG_COUNT)
        {
            GatewaySwc_DemSignalInvalidState[index] = GATEWAYSWC_DEM_STATE_UNKNOWN;
        }
    }
}

static uint8 GatewaySwc_EnterDtcQueueCritical(void)
{
    uint32 coreIndex;

    coreIndex = (uint32)IfxCpu_getCoreIndex();

    if (coreIndex >= GATEWAYSWC_CPU_CORE_COUNT)
    {
        coreIndex = 0u;
    }

    GatewaySwc_DtcQueueIrqState[coreIndex] = IfxCpu_disableInterrupts();

    if (IfxCpu_setSpinLock(&GatewaySwc_DtcQueueLock, GATEWAYSWC_DTC_QUEUE_LOCK_TIMEOUT) != FALSE)
    {
        GatewaySwc_DtcQueueLockOwned[coreIndex] = TRUE;
        __dsync();
    }
    else
    {
        GatewaySwc_DebugDtcQueueLockTimeout++;
        IfxCpu_restoreInterrupts(GatewaySwc_DtcQueueIrqState[coreIndex]);
        return FALSE;
    }

    return TRUE;
}

static void GatewaySwc_ExitDtcQueueCritical(void)
{
    uint32 coreIndex;

    coreIndex = (uint32)IfxCpu_getCoreIndex();

    if (coreIndex >= GATEWAYSWC_CPU_CORE_COUNT)
    {
        coreIndex = 0u;
    }

    __dsync();

    if (GatewaySwc_DtcQueueLockOwned[coreIndex] != FALSE)
    {
        GatewaySwc_DtcQueueLockOwned[coreIndex] = FALSE;
        IfxCpu_resetSpinLock(&GatewaySwc_DtcQueueLock);
    }

    IfxCpu_restoreInterrupts(GatewaySwc_DtcQueueIrqState[coreIndex]);
}

static void GatewaySwc_UpdateDtcDebugQueueState(void)
{
    GatewaySwc_DebugDtcQueueHead = GatewaySwc_DtcTransitionHead;
    GatewaySwc_DebugDtcQueueTail = GatewaySwc_DtcTransitionTail;

    if (GatewaySwc_DtcTransitionHead >= GatewaySwc_DtcTransitionTail)
    {
        GatewaySwc_DebugDtcQueueCount =
            (uint8)(GatewaySwc_DtcTransitionHead - GatewaySwc_DtcTransitionTail);
    }
    else
    {
        GatewaySwc_DebugDtcQueueCount =
            (uint8)(GATEWAYSWC_DTC_TRANSITION_QUEUE_SIZE -
                    GatewaySwc_DtcTransitionTail +
                    GatewaySwc_DtcTransitionHead);
    }
}

static void GatewaySwc_FlushDtcTransitionQueue(void)
{
    GatewaySwc_DtcTransitionType transition;

    while (GatewaySwc_PopDtcTransition(&transition) != FALSE)
    {
        GatewaySwc_PublishDtcTransition(transition.dtc, transition.status);
        GatewaySwc_DebugDtcFlushed++;
    }
}

static uint8 GatewaySwc_PopDtcTransition(GatewaySwc_DtcTransitionType *transition)
{
    uint8 popped;

    if (transition == NULL_PTR)
    {
        return FALSE;
    }

    popped = FALSE;
    if (GatewaySwc_EnterDtcQueueCritical() == FALSE)
    {
        return FALSE;
    }

    if (GatewaySwc_DtcTransitionTail != GatewaySwc_DtcTransitionHead)
    {
        *transition = GatewaySwc_DtcTransitionQueue[GatewaySwc_DtcTransitionTail];
        GatewaySwc_DtcTransitionTail =
            (uint8)((GatewaySwc_DtcTransitionTail + 1u) % GATEWAYSWC_DTC_TRANSITION_QUEUE_SIZE);
        GatewaySwc_UpdateDtcDebugQueueState();
        popped = TRUE;
    }

    GatewaySwc_ExitDtcQueueCritical();

    return popped;
}

static void GatewaySwc_PublishDtcTransition(Dem_DTCType dtc, Dem_UdsStatusByteType status)
{
    uint8 *payload;
    SoAd_ReturnType txResult;

    payload = GatewaySwc_DtcTransitionPayload;
    GatewaySwc_DebugDtcLastDtc = dtc & 0x00FFFFFFu;
    GatewaySwc_DebugDtcLastStatus = status;

    payload[0] = GATEWAYSWC_MAGIC0;
    payload[1] = GATEWAYSWC_MAGIC1;
    payload[2] = GATEWAYSWC_MAGIC2;
    payload[3] = GATEWAYSWC_FRAME_DTC_TRANSITION;
    GatewaySwc_StoreU32(payload, 4u, GatewaySwc_Status.mainCycles);
    payload[8] = 0u;
    payload[9] = 0u;
    payload[10] = 0u;
    payload[11] = 0u;
    GatewaySwc_StoreU32(payload, 12u, dtc & 0x00FFFFFFu);
    payload[16] = status;

    if (GatewaySwc_IsEthComActive() == FALSE)
    {
        GatewaySwc_Status.ethLastOpenResult = 0u;
        GatewaySwc_DebugDtcLastOpenResult = 0u;
        GatewaySwc_Status.ethLastPayloadLength = GATEWAYSWC_DTC_TRANSITION_LEN;
        GatewaySwc_DebugDtcLastTxResult = SOAD_NOT_OK;
        GatewaySwc_Status.ethLastSoAdResult = SOAD_NOT_OK;
        GatewaySwc_Status.ethLastTransmitOk = 0u;
        return;
    }

    GatewaySwc_Status.ethLastOpenResult =
        SoAd_OpenSoCon((SoAd_SoConIdType)GATEWAYSWC_ETH_SOCON_ID);
    GatewaySwc_DebugDtcLastOpenResult = GatewaySwc_Status.ethLastOpenResult;
    GatewaySwc_Status.ethLastPayloadLength = GATEWAYSWC_DTC_TRANSITION_LEN;

    txResult = GatewaySwc_RequestSoAdIfTransmit((SoAd_SoConIdType)GATEWAYSWC_ETH_SOCON_ID,
            NULL_PTR,
            payload,
            GATEWAYSWC_DTC_TRANSITION_LEN);
    GatewaySwc_DebugDtcLastTxResult = (uint8)txResult;
    GatewaySwc_Status.ethLastSoAdResult = (uint8)txResult;

    if (txResult == SOAD_OK)
    {
        GatewaySwc_Status.ethLastTransmitOk = 1u;
        GatewaySwc_Status.ethFramesSent++;
    }
    else
    {
        GatewaySwc_Status.ethLastTransmitOk = 0u;
        GatewaySwc_Status.ethFramesFailed++;
    }
}

static void GatewaySwc_AppendU8(uint16 *len, uint8 value)
{
    if (*len < GATEWAYSWC_ETH_MAX_PAYLOAD)
    {
        GatewaySwc_EthBuffer[*len] = value;
        (*len)++;
    }
}

static void GatewaySwc_AppendU16(uint16 *len, uint16 value)
{
    GatewaySwc_AppendU8(len, (uint8)((value >> 8u) & 0xFFu));
    GatewaySwc_AppendU8(len, (uint8)(value & 0xFFu));
}

static void GatewaySwc_AppendU32(uint16 *len, uint32 value)
{
    GatewaySwc_AppendU8(len, (uint8)((value >> 24u) & 0xFFu));
    GatewaySwc_AppendU8(len, (uint8)((value >> 16u) & 0xFFu));
    GatewaySwc_AppendU8(len, (uint8)((value >> 8u) & 0xFFu));
    GatewaySwc_AppendU8(len, (uint8)(value & 0xFFu));
}

static void GatewaySwc_StoreU16(uint8 *buffer, uint16 offset, uint16 value)
{
    buffer[offset] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)(value & 0xFFu);
}

static void GatewaySwc_StoreS16(uint8 *buffer, uint16 offset, sint16 value)
{
    GatewaySwc_StoreU16(buffer, offset, (uint16)value);
}

static void GatewaySwc_StoreU32(uint8 *buffer, uint16 offset, uint32 value)
{
    buffer[offset] = (uint8)((value >> 24u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)((value >> 16u) & 0xFFu);
    buffer[(uint16)(offset + 2u)] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 3u)] = (uint8)(value & 0xFFu);
}

static void GatewaySwc_StoreU64(uint8 *buffer, uint16 offset, uint64 value)
{
    buffer[offset] = (uint8)((value >> 56u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)((value >> 48u) & 0xFFu);
    buffer[(uint16)(offset + 2u)] = (uint8)((value >> 40u) & 0xFFu);
    buffer[(uint16)(offset + 3u)] = (uint8)((value >> 32u) & 0xFFu);
    buffer[(uint16)(offset + 4u)] = (uint8)((value >> 24u) & 0xFFu);
    buffer[(uint16)(offset + 5u)] = (uint8)((value >> 16u) & 0xFFu);
    buffer[(uint16)(offset + 6u)] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 7u)] = (uint8)(value & 0xFFu);
}

static Std_ReturnType GatewaySwc_SendU32(Com_SignalIdType signalId, uint32 value)
{
    Std_ReturnType ret;

    ret = GatewaySwc_RequestComSendSignal(signalId, &value);

    if (ret == E_OK)
    {
        GatewaySwc_Status.sendSignalOk++;
        GatewaySwc_Status.generatedSignals++;
    }
    else
    {
        GatewaySwc_Status.sendSignalFailed++;
    }

    return ret;
}

static Std_ReturnType GatewaySwc_ReceiveU32(Com_SignalIdType signalId, uint32 *value)
{
    Std_ReturnType ret;

    if (value == NULL_PTR)
    {
        return E_NOT_OK;
    }

    ret = Com_ReceiveSignal(signalId, value);

    if (ret != E_OK)
    {
        GatewaySwc_Status.receiveSignalFailed++;
    }

    return ret;
}
