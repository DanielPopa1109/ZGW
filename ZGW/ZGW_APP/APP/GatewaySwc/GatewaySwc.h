#ifndef GATEWAYSWC_H
#define GATEWAYSWC_H

#include "Com.h"
#include "ComStack_Types.h"
#include "Dcm.h"
#include "Dem_Types.h"
#include "SoAd.h"
#include "TcpIpH.h"

#define GATEWAYSWC_VERSION_MAJOR        1u
#define GATEWAYSWC_VERSION_MINOR        4u

#ifndef GATEWAYSWC_DEBUG_INSTRUMENTATION
#define GATEWAYSWC_DEBUG_INSTRUMENTATION 0
#endif

#define GATEWAYSWC_MAIN_PERIOD_MS       5u
#define GATEWAYSWC_ROUTE_PERIOD_MS      10u
#define GATEWAYSWC_OUTPUT_PERIOD_MS     10u
#define GATEWAYSWC_ETH_PERIOD_MS        1000u

#define GATEWAYSWC_ETH_SOCON_ID         4u
#define GATEWAYSWC_ETH_MAX_PAYLOAD      256u
#define GATEWAYSWC_ETH_RX_MESSAGE_COUNT 2u
#define GATEWAYSWC_ETH_RX_PDU_COMMAND_SIGNAL ((PduIdType)0x8102u)
#define GATEWAYSWC_ETH_RX_PDU_COMMAND_BLOCK  ((PduIdType)0x8103u)

#define GATEWAYSWC_MCU_STATUS_UDP_ENABLE          STD_ON
#define GATEWAYSWC_MCU_STATUS_UDP_PORT            35001u
#define GATEWAYSWC_MCU_STATUS_UDP_PERIOD_MS       100u
#define GATEWAYSWC_MCU_STATUS_UDP_BROADCAST_ADDR  0xFFFFFFFFu
#define GATEWAYSWC_MCU_STATUS_PACKET_LENGTH       64u
#define GATEWAYSWC_MCU_STATUS_NVM_IMAGE_SIZE      GATEWAYSWC_MCU_STATUS_PACKET_LENGTH

#define GATEWAYSWC_MCU_STATUS_DISABLED            0u
#define GATEWAYSWC_MCU_STATUS_WAIT_LINK           1u
#define GATEWAYSWC_MCU_STATUS_SOCKET_READY        2u
#define GATEWAYSWC_MCU_STATUS_TX_ERROR            3u

#define GATEWAYSWC_PDM_LOADS_PER_PDM    3u

#define GATEWAYSWC_RX_DIAG_STATUS_OK       0x00u
#define GATEWAYSWC_RX_DIAG_STATUS_TIMEOUT  0x01u
#define GATEWAYSWC_RX_DIAG_STATUS_INVALID  0x02u

#define GATEWAYSWC_CAN_RX_FIRST_0          COM_SIG_RX_CENTRALLOCKDATA_VIBRATIONSENSORSTATUS
#define GATEWAYSWC_CAN_RX_LAST_0           COM_SIG_RX_DMUSTATUS_DISPLAYCAMERASTATUS

#define GATEWAYSWC_CAN_RX_FIRST_1          COM_SIG_RX_BATTFULLSTAT_RUNTIMEREMAINING
#define GATEWAYSWC_CAN_RX_LAST_1           COM_SIG_RX_BATTFULLSTAT_TIMETOFULL

#define GATEWAYSWC_CAN_RX_FIRST_2          COM_SIG_RX_PDCSTAT_PDCDISTANCEREAR
#define GATEWAYSWC_CAN_RX_LAST_2           COM_SIG_RX_L1_I2T_COUNTER_I2TCOUNTER

#define GATEWAYSWC_CAN_RX_FIRST_3          COM_SIG_RX_BATTSOCSOH_SOH
#define GATEWAYSWC_CAN_RX_LAST_3           COM_SIG_RX_BATTCAPRES_CAPACITYAH

#define GATEWAYSWC_CANFD_LOAD_RX_FIRST     COM_SIG_RX_CANFD_PDM1_LOADSTATUS_PDM1_LOADSTATUS_01
#define GATEWAYSWC_CANFD_LOAD_RX_LAST      COM_SIG_RX_CANFD_PDM1_LOADSTATUS_PDM1_LOADSTATUS_03
#define GATEWAYSWC_CANFD_INPUTT30_RX_FIRST COM_SIG_RX_CANFD_PDM1_INPUTT30_INPUTT30
#define GATEWAYSWC_CANFD_INPUTT30_RX_LAST  COM_SIG_RX_CANFD_PDM1_INPUTT30_INPUTT30
#define GATEWAYSWC_CANFD_CURRENT_RX_FIRST  COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_1_PDM1_CURRFB_001
#define GATEWAYSWC_CANFD_CURRENT_RX_LAST   COM_SIG_RX_CANFD_PDM1_CURRENTFEEDBACK_5_PDM1_CURRFB_075

#define GATEWAYSWC_LIN_RX_FIRST            COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_RESPONSEERROR
#define GATEWAYSWC_LIN_RX_LAST             COM_SIG_RX_LIN_HVDCDC_STATUS_HVDCDC_HV_CURRENT

#define GATEWAYSWC_RANGE_SIZE(first, last)  (((uint16)(last) - (uint16)(first)) + 1u)

#define GATEWAYSWC_RX_MESSAGE_DIAG_COUNT    (GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_CENTRALLOCKDATA, COM_RX_PDU_DMUSTATUS) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_BATTFULLSTAT, COM_RX_PDU_BATTFULLSTAT) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_PDCSTAT, COM_RX_PDU_DMU_ALIVE) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_VOLTAGECURRENT, COM_RX_PDU_L1_I2T_COUNTER) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_BATTSOCSOH, COM_RX_PDU_BATTCAPRES) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_CANFD_PDM1_LOADSTATUS, COM_RX_PDU_CANFD_PDM1_LOADSTATUS) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_1, COM_RX_PDU_CANFD_PDM1_CURRENTFEEDBACK_5) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_CANFD_PDM1_INPUTT30, COM_RX_PDU_CANFD_PDM1_INPUTT30) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_LIN_HVDCDC_STATUS, COM_RX_PDU_LIN_HVDCDC_STATUS) + \
        GATEWAYSWC_ETH_RX_MESSAGE_COUNT)

#define GATEWAYSWC_RX_SIGNAL_DIAG_COUNT     (GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_CAN_RX_FIRST_0, GATEWAYSWC_CAN_RX_LAST_0) + \
        GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_CAN_RX_FIRST_1, GATEWAYSWC_CAN_RX_LAST_1) + \
        GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_CAN_RX_FIRST_2, GATEWAYSWC_CAN_RX_LAST_2) + \
        GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_CAN_RX_FIRST_3, GATEWAYSWC_CAN_RX_LAST_3) + \
        GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_CANFD_LOAD_RX_FIRST, GATEWAYSWC_CANFD_LOAD_RX_LAST) + \
        GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_CANFD_INPUTT30_RX_FIRST, GATEWAYSWC_CANFD_INPUTT30_RX_LAST) + \
        GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_CANFD_CURRENT_RX_FIRST, GATEWAYSWC_CANFD_CURRENT_RX_LAST) + \
        GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_LIN_RX_FIRST, GATEWAYSWC_LIN_RX_LAST))

typedef enum
{
    GATEWAYSWC_BUS_CAN    = 1u,
    GATEWAYSWC_BUS_CANFD  = 2u,
    GATEWAYSWC_BUS_LIN    = 3u,
    GATEWAYSWC_BUS_ETH    = 4u
} GatewaySwc_BusType;

extern volatile uint32 GatewaySwc_RxMessageTimeoutCounter[GATEWAYSWC_RX_MESSAGE_DIAG_COUNT];
extern volatile uint32 GatewaySwc_RxMessageTimeoutActiveSamples[GATEWAYSWC_RX_MESSAGE_DIAG_COUNT];
extern volatile uint32 GatewaySwc_RxSignalTimeoutCounter[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
extern volatile uint32 GatewaySwc_RxSignalTimeoutActiveSamples[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
extern volatile uint32 GatewaySwc_RxSignalInvalidCounter[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
extern volatile uint32 GatewaySwc_RxSignalInvalidActiveSamples[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
#if GATEWAYSWC_DEBUG_INSTRUMENTATION
extern volatile uint16 GatewaySwc_RxMessageDiagDebugPduId[GATEWAYSWC_RX_MESSAGE_DIAG_COUNT];
extern volatile uint8 GatewaySwc_RxMessageDiagDebugStatus[GATEWAYSWC_RX_MESSAGE_DIAG_COUNT];
extern volatile uint16 GatewaySwc_RxSignalDiagDebugSignalId[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
extern volatile uint8 GatewaySwc_RxSignalDiagDebugStatus[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
extern volatile uint32 GatewaySwc_RxSignalDiagDebugValue[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
extern volatile uint32 GatewaySwc_RxSignalDiagDebugInvalidValue[GATEWAYSWC_RX_SIGNAL_DIAG_COUNT];
extern volatile uint16 GatewaySwc_DebugPublishFirstSignalId;
extern volatile uint16 GatewaySwc_DebugPublishLastSignalId;
extern volatile uint16 GatewaySwc_DebugPublishSignalId;
extern volatile uint16 GatewaySwc_DebugPublishLen;
extern volatile uint8 GatewaySwc_DebugPublishBus;
extern volatile uint8 GatewaySwc_DebugPublishReceiveResult;
extern volatile uint32 GatewaySwc_DebugPublishValue;
extern volatile uint16 GatewaySwc_DebugRouteIndex;
extern volatile uint16 GatewaySwc_DebugRouteCount;
extern volatile uint16 GatewaySwc_DebugRouteRxSignalId;
extern volatile uint16 GatewaySwc_DebugRouteTxSignalId;
extern volatile uint8 GatewaySwc_DebugRoutePhase;
extern volatile uint8 GatewaySwc_DebugRouteReceiveResult;
extern volatile uint8 GatewaySwc_DebugRouteSendResult;
extern volatile uint32 GatewaySwc_DebugRouteValue;
extern volatile uint32 GatewaySwc_DebugLoadRequestCounter;
extern volatile uint32 GatewaySwc_DebugLoadRequestCrc;
extern volatile uint32 GatewaySwc_DebugLoadRequestAlive;
extern volatile uint8 GatewaySwc_DebugLoadRequestChanged;
extern volatile uint8 GatewaySwc_DebugDtcQueueHead;
extern volatile uint8 GatewaySwc_DebugDtcQueueTail;
extern volatile uint8 GatewaySwc_DebugDtcQueueCount;
extern volatile uint32 GatewaySwc_DebugDtcDropped;
extern volatile uint32 GatewaySwc_DebugDtcFlushed;
extern volatile uint32 GatewaySwc_DebugDtcLastDtc;
extern volatile uint8 GatewaySwc_DebugDtcLastStatus;
extern volatile uint8 GatewaySwc_DebugDtcLastOpenResult;
extern volatile uint8 GatewaySwc_DebugDtcLastTxResult;
extern volatile uint32 GatewaySwc_DebugEthTxQueueDropped;
extern volatile uint32 GatewaySwc_DebugEthTxResultQueueDropped;
extern volatile uint32 GatewaySwc_DebugEthRxQueueDropped;
extern volatile uint32 GatewaySwc_DebugCrossCoreLockTimeout;
#endif

void GatewaySwc_Init(void);
void GatewaySwc_MainFunction(void);
void GatewaySwc_EthernetMainFunction(void);
Dcm_ReturnType GatewaySwc_SetDiagnosticCommunicationControl(uint8 controlType, uint8 communicationType);
boolean GatewaySwc_IsNormalCommunicationRxEnabled(void);
boolean GatewaySwc_IsNormalCommunicationTxEnabled(void);
Std_ReturnType GatewaySwc_RequestComSendSignal(Com_SignalIdType signalId, const void *data);
void GatewaySwc_RequestComMainFunctionTx(void);
Std_ReturnType GatewaySwc_RequestCanIfTransmit(PduIdType txPduId, const uint8 *data, PduLengthType len);
Std_ReturnType GatewaySwc_RequestLinIfTransmit(PduIdType txPduId, const uint8 *data, PduLengthType len);
SoAd_ReturnType GatewaySwc_RequestSoAdIfTransmit(SoAd_SoConIdType soConId,
                                                 const TcpIp_SockAddrType *remoteAddr,
                                                 const uint8 *data,
                                                 uint16 len);
sint32 GatewaySwc_RequestTcpIpSendTo(TcpIp_SocketIdType sock,
                                     const TcpIp_SockAddrType *remoteAddr,
                                     const uint8 *data,
                                     uint16 len);
void GatewaySwc_RequestLinIfMainFunction(void);

Std_ReturnType GatewaySwc_CaptureRxDiagSnapshotData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length);
void GatewaySwc_ReportDtcTransition(Dem_DTCType dtc, Dem_UdsStatusByteType status);
void GatewaySwc_OnDemEventCleared(Dem_EventIdType eventId);
Std_ReturnType GatewaySwc_ReadDid(uint16 did, uint8 *data, Dcm_PduLengthType *dataLen);

void GatewaySwc_EthRxIndication(uint8 soConId,
                                const TcpIp_SockAddrType *remoteAddr,
                                const uint8 *data,
                                uint16 len);

#endif
