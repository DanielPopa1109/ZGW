#ifndef GATEWAYSWC_H
#define GATEWAYSWC_H

#include "ComStack_Types.h"
#include "TcpIpH.h"

#define GATEWAYSWC_VERSION_MAJOR        1u
#define GATEWAYSWC_VERSION_MINOR        3u

#define GATEWAYSWC_MAIN_PERIOD_MS       10u
#define GATEWAYSWC_ROUTE_PERIOD_MS      10u
#define GATEWAYSWC_OUTPUT_PERIOD_MS     10u
#define GATEWAYSWC_ETH_PERIOD_MS        100u

#define GATEWAYSWC_ETH_SOCON_ID         4u
#define GATEWAYSWC_ETH_MAX_PAYLOAD      256u

#define GATEWAYSWC_PDM_LOADS_PER_PDM    3u

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
} GatewaySwc_StatusType;

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

void GatewaySwc_EthRxIndication(uint8 soConId,
                                const TcpIp_SockAddrType *remoteAddr,
                                const uint8 *data,
                                uint16 len);

#endif
