#ifndef GATEWAYSWC_H
#define GATEWAYSWC_H

#include "Com.h"
#include "ComStack_Types.h"
#include "Dem_Types.h"
#include "TcpIpH.h"

#define GATEWAYSWC_VERSION_MAJOR        1u
#define GATEWAYSWC_VERSION_MINOR        4u

#define GATEWAYSWC_MAIN_PERIOD_MS       5u
#define GATEWAYSWC_ROUTE_PERIOD_MS      10u
#define GATEWAYSWC_OUTPUT_PERIOD_MS     10u
#define GATEWAYSWC_ETH_PERIOD_MS        100u

#define GATEWAYSWC_ETH_SOCON_ID         4u
#define GATEWAYSWC_ETH_MAX_PAYLOAD      256u

#define GATEWAYSWC_PDM_LOADS_PER_PDM    3u

#define GATEWAYSWC_RX_DIAG_STATUS_OK       0x00u
#define GATEWAYSWC_RX_DIAG_STATUS_TIMEOUT  0x01u
#define GATEWAYSWC_RX_DIAG_STATUS_INVALID  0x02u

#define GATEWAYSWC_RX_DIAG_KIND_MESSAGE_TIMEOUT  0x01u
#define GATEWAYSWC_RX_DIAG_KIND_SIGNAL_INVALID   0x02u

#define GATEWAYSWC_CAN_RX_FIRST_0          COM_SIG_RX_CENTRALLOCKDATA_VIBRATIONSENSORSTATUS
#define GATEWAYSWC_CAN_RX_LAST_0           COM_SIG_RX_L1_I2T_COUNTER_I2TCOUNTER

#define GATEWAYSWC_CAN_RX_FIRST_1          COM_SIG_RX_BATTSOCSOH_SOH
#define GATEWAYSWC_CAN_RX_LAST_1           COM_SIG_RX_BATTCAPRES_CAPACITYAH

#define GATEWAYSWC_CANFD_RX_FIRST          COM_SIG_RX_CANFD_PDM4_LOADSTATUS_PDM4_LOADSTATUS_01
#define GATEWAYSWC_CANFD_RX_LAST           COM_SIG_RX_CANFD_PDM4_TEMPERATUREFEEDBACK_5_PDM4_TEMPFB_075

#define GATEWAYSWC_LIN_RX_FIRST            COM_SIG_RX_LIN_ALT_STATUS_ALT_RESPONSEERROR
#define GATEWAYSWC_LIN_RX_LAST             COM_SIG_RX_LIN_PCU48_STATUS_PCU48_CHARGESTATE

#define GATEWAYSWC_RANGE_SIZE(first, last)  (((uint16)(last) - (uint16)(first)) + 1u)

#define GATEWAYSWC_RX_MESSAGE_DIAG_COUNT    (GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_CENTRALLOCKDATA, COM_RX_PDU_DMU_ALIVE) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_VOLTAGECURRENT, COM_RX_PDU_L1_I2T_COUNTER) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_BATTSOCSOH, COM_RX_PDU_BATTCAPRES) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_CANFD_PDM4_LOADSTATUS, COM_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_5) + \
        GATEWAYSWC_RANGE_SIZE(COM_RX_PDU_LIN_ALT_STATUS, COM_RX_PDU_LIN_PCU48_STATUS))

#define GATEWAYSWC_RX_SIGNAL_DIAG_COUNT     (GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_CAN_RX_FIRST_0, GATEWAYSWC_CAN_RX_LAST_0) + \
        GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_CAN_RX_FIRST_1, GATEWAYSWC_CAN_RX_LAST_1) + \
        GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_CANFD_RX_FIRST, GATEWAYSWC_CANFD_RX_LAST) + \
        GATEWAYSWC_RANGE_SIZE(GATEWAYSWC_LIN_RX_FIRST, GATEWAYSWC_LIN_RX_LAST))

typedef enum
{
    GATEWAYSWC_BUS_CAN    = 1u,
    GATEWAYSWC_BUS_CANFD  = 2u,
    GATEWAYSWC_BUS_LIN    = 3u
} GatewaySwc_BusType;

typedef struct
{
    uint32 mainCycles;
    uint32 routedSignals;
    uint32 generatedSignals;
    uint32 sendSignalOk;
    uint32 sendSignalFailed;
    uint32 receiveSignalFailed;
    uint32 ethFramesSent;
    uint32 ethFramesFailed;
    uint32 ethSignalsPublished;
    uint16 configuredRoutes;
    uint16 rxDiagConfiguredMessages;
    uint16 rxDiagConfiguredSignals;
    uint16 rxDiagTimedOutMessages;
    uint16 rxDiagInvalidSignals;
    uint32 rxDiagTimeoutEvents;
    uint32 rxDiagInvalidEvents;
    uint16 rxDiagLastTimeoutPduId;
    uint16 rxDiagLastInvalidSignalId;
    uint32 rxDiagLastInvalidValue;
} GatewaySwc_StatusType;

typedef struct
{
    uint16 pduId;
    GatewaySwc_BusType bus;
    uint8 status;
    uint16 cycleTicks;
    uint16 timeoutTicks;
} GatewaySwc_RxMessageDiagType;

typedef struct
{
    uint16 signalId;
    GatewaySwc_BusType bus;
    uint8 status;
    uint32 value;
    uint32 invalidValue;
} GatewaySwc_RxSignalDiagType;

typedef struct
{
    uint8 kind;
    uint16 diagIndex;
    uint16 objectId;
    GatewaySwc_BusType bus;
    uint8 status;
    uint16 cycleTicks;
    uint16 timeoutTicks;
    uint32 value;
    uint32 thresholdValue;
    uint32 mainCycles;
    uint16 activeCount;
    uint32 eventCount;
} GatewaySwc_RxDiagSnapshotType;

typedef struct
{
    uint32 pdmCommandLoad[4u][GATEWAYSWC_PDM_LOADS_PER_PDM];

    uint32 linAltTargetCurrent;
    uint32 linAltTargetVoltage;
    uint32 linAltEnable;
    uint32 linAltFieldDutyCommand;

    uint32 linHvDcdcEnable;
    uint32 linHvDcdcTargetVoltage;

    uint32 linPcu48RequestAvailability;

    uint32 vehicleStatus;
    uint32 nmPn1;
} GatewaySwc_CommandType;

void GatewaySwc_Init(void);
void GatewaySwc_MainFunction(void);

void GatewaySwc_SetCommands(const GatewaySwc_CommandType *cmd);
void GatewaySwc_GetCommands(GatewaySwc_CommandType *cmd);
void GatewaySwc_GetStatus(GatewaySwc_StatusType *status);
uint16 GatewaySwc_GetRxMessageDiagCount(void);
uint16 GatewaySwc_GetRxSignalDiagCount(void);
Std_ReturnType GatewaySwc_GetRxMessageDiag(uint16 index, GatewaySwc_RxMessageDiagType *diag);
Std_ReturnType GatewaySwc_GetRxSignalDiag(uint16 index, GatewaySwc_RxSignalDiagType *diag);
Std_ReturnType GatewaySwc_GetRxMessageDiagSnapshot(uint16 index, GatewaySwc_RxDiagSnapshotType *snapshot);
Std_ReturnType GatewaySwc_GetRxSignalDiagSnapshot(uint16 index, GatewaySwc_RxDiagSnapshotType *snapshot);
Std_ReturnType GatewaySwc_CaptureRxDiagFreezeFrame(Dem_EventIdType eventId, uint8 *buffer, uint16 *length);
Std_ReturnType GatewaySwc_CaptureRxDiagExtendedData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length);

void GatewaySwc_EthRxIndication(uint8 soConId,
                                const TcpIp_SockAddrType *remoteAddr,
                                const uint8 *data,
                                uint16 len);

#endif
