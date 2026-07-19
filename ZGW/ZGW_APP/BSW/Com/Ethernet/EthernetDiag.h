#ifndef ETHERNETDIAG_H
#define ETHERNETDIAG_H

#include "Std_Types.h"
#include "Dem_Types.h"

typedef uint8 EthernetDiagConnectionId;
typedef uint8 EthernetDiagServiceId;

#define ETHERNETDIAG_CONNECTION_INVALID ((EthernetDiagConnectionId)0xFFu)
#define ETHERNETDIAG_SERVICE_INVALID    ((EthernetDiagServiceId)0xFFu)

typedef enum
{
    ETHERNETDIAG_EVENT_LINK_LOST = 0u,
    ETHERNETDIAG_EVENT_CTRL_DMA_FAILURE,
    ETHERNETDIAG_EVENT_RX_COMM_FAILURE,
    ETHERNETDIAG_EVENT_TX_COMM_FAILURE,
    ETHERNETDIAG_EVENT_TCP_UNEXPECTED_TERMINATION,
    ETHERNETDIAG_EVENT_TCP_ESTABLISHMENT_FAILURE,
    ETHERNETDIAG_EVENT_UDP_SUPERVISION_TIMEOUT,
    ETHERNETDIAG_EVENT_SERVICE_AVAILABILITY_FAILURE,
    ETHERNETDIAG_EVENT_DOIP_COMM_FAILURE,
    ETHERNETDIAG_EVENT_PARTNER_COMM_TERMINATED,
    ETHERNETDIAG_EVENT_COUNT
} EthernetDiagEventType;

typedef enum
{
    ETHERNETDIAG_CLOSE_NORMAL = 0u,
    ETHERNETDIAG_CLOSE_INTENTIONAL,
    ETHERNETDIAG_CLOSE_PEER_FIN,
    ETHERNETDIAG_CLOSE_RESET,
    ETHERNETDIAG_CLOSE_TIMEOUT,
    ETHERNETDIAG_CLOSE_PIPE,
    ETHERNETDIAG_CLOSE_NOT_CONNECTED,
    ETHERNETDIAG_CLOSE_SOCKET_LOST,
    ETHERNETDIAG_CLOSE_SEND_FAILURE,
    ETHERNETDIAG_CLOSE_LINK_LOST
} EthernetDiagCloseReasonType;

typedef enum
{
    ETHERNETDIAG_CONN_DISABLED = 0u,
    ETHERNETDIAG_CONN_STARTUP_GRACE,
    ETHERNETDIAG_CONN_CONNECTING,
    ETHERNETDIAG_CONN_CONNECTED,
    ETHERNETDIAG_CONN_INTENTIONAL_CLOSE_PENDING,
    ETHERNETDIAG_CONN_DISCONNECTED,
    ETHERNETDIAG_CONN_FAULT_PENDING,
    ETHERNETDIAG_CONN_FAULT_CONFIRMED,
    ETHERNETDIAG_CONN_RECOVERY_PENDING
} EthernetDiagConnectionStateType;

typedef struct
{
    EthernetDiagConnectionId connectionId;
    uint8 soConId;
    uint8 mandatory;
    uint8 protocol;
    uint32 startupGraceMs;
    uint32 supervisionTimeoutMs;
    uint32 failureDebounceMs;
    uint32 healingMs;
    uint8 retryLimit;
    Dem_EventIdType tcpTerminationEventId;
    Dem_EventIdType tcpEstablishmentEventId;
    Dem_EventIdType udpTimeoutEventId;
    Dem_EventIdType partnerLostEventId;
} EthernetDiagConnectionConfigType;

typedef struct
{
    EthernetDiagServiceId serviceId;
    uint16 service;
    uint16 instance;
    uint8 mandatory;
    uint32 startupGraceMs;
    uint32 failureDebounceMs;
    uint32 healingMs;
    Dem_EventIdType eventId;
} EthernetDiagServiceConfigType;

typedef struct
{
    const EthernetDiagConnectionConfigType *connections;
    uint8 connectionCount;
    const EthernetDiagServiceConfigType *services;
    uint8 serviceCount;
} EthernetDiag_ConfigType;

extern const EthernetDiag_ConfigType EthernetDiag_Config;

void EthernetDiag_Init(void);
void EthernetDiag_MainFunction(void);
boolean EthernetDiag_IsMonitoringAllowed(void);

void EthernetDiag_ReportPhyLink(boolean linkUp);
void EthernetDiag_ReportDmaError(uint32 errorFlags);
void EthernetDiag_ReportRxError(uint32 errorFlags);
void EthernetDiag_ReportTxError(uint32 errorFlags);
void EthernetDiag_ReportSocketConnected(EthernetDiagConnectionId id);
void EthernetDiag_ReportSocketOpenFailed(EthernetDiagConnectionId id);
void EthernetDiag_ReportSocketCloseRequested(EthernetDiagConnectionId id);
void EthernetDiag_ReportSocketClosed(EthernetDiagConnectionId id, EthernetDiagCloseReasonType reason);
void EthernetDiag_ReportRxActivity(EthernetDiagConnectionId id);
void EthernetDiag_ReportTxActivity(EthernetDiagConnectionId id);
void EthernetDiag_ReportPartnerAlive(EthernetDiagConnectionId id);
void EthernetDiag_ReportServiceAvailable(EthernetDiagServiceId id, boolean available);
void EthernetDiag_ReportDoipActive(boolean active);
void EthernetDiag_ReportDoipTimeout(void);

EthernetDiagConnectionId EthernetDiag_GetConnectionIdForSoCon(uint8 soConId);
Std_ReturnType EthernetDiag_CaptureSnapshotData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length);

#endif
