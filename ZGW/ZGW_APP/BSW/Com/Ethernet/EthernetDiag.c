#include "EthernetDiag.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "EthSM.h"
#include "SoAd.h"
#include "TcpIpH.h"
#include "GETH_LWIP_ILLD/lwip_geth_conf.h"
#include "GETH_LWIP_ILLD/port/include/lwip_geth_lwip.h"
#if (PHY_DEVICE_NAME == PHY_DP83825I)
#include "GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.h"
#endif
#include "APP/TimeSync/TimeBase.h"
#include <string.h>

#define ETHERNETDIAG_LINK_DEBOUNCE_MS       3000u
#define ETHERNETDIAG_LINK_ARM_STABLE_MS     5000u
#define ETHERNETDIAG_CTRL_DEBOUNCE_MS       1000u
#define ETHERNETDIAG_ERROR_WINDOW_MS        10000u
#define ETHERNETDIAG_RX_ERROR_THRESHOLD     8u
#define ETHERNETDIAG_TX_ERROR_THRESHOLD     5u
#define ETHERNETDIAG_RESOURCE_THRESHOLD     8u
#define ETHERNETDIAG_DEFAULT_HEALING_MS     30000u
#define ETHERNETDIAG_DOIP_DEBOUNCE_MS       25000u
#define ETHERNETDIAG_PHY_DEBOUNCE_MS        1000u
#define ETHERNETDIAG_NEGOTIATION_DEBOUNCE_MS 5000u
#define ETHERNETDIAG_LINK_MODE_DEBOUNCE_MS  10000u
#define ETHERNETDIAG_RESOURCE_DEBOUNCE_MS   30000u
#define ETHERNETDIAG_SNAPSHOT_DATA_SIZE     102u
#define ETHERNETDIAG_SNAPSHOT_LOCAL_IP_OFFSET           22u
#define ETHERNETDIAG_SNAPSHOT_REMOTE_IP_OFFSET          26u
#define ETHERNETDIAG_SNAPSHOT_LOCAL_PORT_OFFSET         30u
#define ETHERNETDIAG_SNAPSHOT_REMOTE_PORT_OFFSET        32u
#define ETHERNETDIAG_SNAPSHOT_SOCON_ID_OFFSET           34u
#define ETHERNETDIAG_SNAPSHOT_CONNECTION_ID_OFFSET      35u
#define ETHERNETDIAG_SNAPSHOT_PROTOCOL_OFFSET           36u
#define ETHERNETDIAG_SNAPSHOT_LINK_UP_OFFSET            37u
#define ETHERNETDIAG_SNAPSHOT_RX_ERRORS_OFFSET          38u
#define ETHERNETDIAG_SNAPSHOT_TX_ERRORS_OFFSET          42u
#define ETHERNETDIAG_SNAPSHOT_DMA_ERRORS_OFFSET         46u
#define ETHERNETDIAG_SNAPSHOT_TCP_CLOSES_OFFSET         50u
#define ETHERNETDIAG_SNAPSHOT_OPEN_FAILS_OFFSET         54u
#define ETHERNETDIAG_SNAPSHOT_LINK_DOWNS_OFFSET         58u
#define ETHERNETDIAG_SNAPSHOT_LISTEN_SOCKET_OFFSET      62u
#define ETHERNETDIAG_SNAPSHOT_ACTIVE_SOCKET_OFFSET      66u
#define ETHERNETDIAG_SNAPSHOT_SOAD_STATE_OFFSET         70u
#define ETHERNETDIAG_SNAPSHOT_PHY_STATE_OFFSET          71u
#define ETHERNETDIAG_SNAPSHOT_PHY_ADDR_OFFSET           72u
#define ETHERNETDIAG_SNAPSHOT_PHY_INIT_DONE_OFFSET      73u
#define ETHERNETDIAG_SNAPSHOT_PHY_AUTONEG_DONE_OFFSET   74u
#define ETHERNETDIAG_SNAPSHOT_PHY_SPEED100_OFFSET       75u
#define ETHERNETDIAG_SNAPSHOT_PHY_FULL_DUPLEX_OFFSET    76u
#define ETHERNETDIAG_SNAPSHOT_PHY_RESET_TIMEOUTS_OFFSET 77u
#define ETHERNETDIAG_SNAPSHOT_PHY_AUTONEG_TIMEOUTS_OFFSET 81u
#define ETHERNETDIAG_SNAPSHOT_PHY_MDIO_ERRORS_OFFSET    85u
#define ETHERNETDIAG_SNAPSHOT_RESOURCE_ERRORS_OFFSET    89u
#define ETHERNETDIAG_SNAPSHOT_LAST_RESOURCE_FLAGS_OFFSET 93u
#define ETHERNETDIAG_SNAPSHOT_NETIF_FLAGS_OFFSET         95u
#define ETHERNETDIAG_SNAPSHOT_ETHSM_REQUESTED_OFFSET     96u
#define ETHERNETDIAG_SNAPSHOT_ETHSM_CURRENT_OFFSET       97u
#define ETHERNETDIAG_SNAPSHOT_PHY_BMSR_OFFSET            98u
#define ETHERNETDIAG_SNAPSHOT_PHY_PHYSTS_OFFSET          100u

typedef struct
{
    Dem_EventIdType eventId;
    uint32 debounceMs;
    uint32 healingMs;
    boolean failedInput;
    boolean reportedFailed;
    uint32 failStartMs;
    uint32 passStartMs;
} EthernetDiagEventRuntimeType;

typedef struct
{
    EthernetDiagConnectionStateType state;
    uint8 intentionalClosePending;
    uint8 connectedOnce;
    uint8 openFailCount;
    uint32 startupUntilMs;
    uint32 lastRxMs;
    uint32 lastTxMs;
    uint32 lastAliveMs;
    uint32 lossStartMs;
} EthernetDiagConnectionRuntimeType;

typedef struct
{
    uint8 available;
    uint8 availableOnce;
    uint32 startupUntilMs;
    uint32 lossStartMs;
} EthernetDiagServiceRuntimeType;

static EthernetDiagEventRuntimeType EthernetDiag_Events[ETHERNETDIAG_EVENT_COUNT];
static EthernetDiagConnectionRuntimeType EthernetDiag_Connections[8u];
static EthernetDiagServiceRuntimeType EthernetDiag_Services[4u];
static uint8 EthernetDiag_Initialized;
static uint8 EthernetDiag_LinkUp;
static uint8 EthernetDiag_LinkUpSeen;
static uint32 EthernetDiag_LinkStableStartMs;
static uint32 EthernetDiag_RxWindowStartMs;
static uint32 EthernetDiag_TxWindowStartMs;
static uint16 EthernetDiag_RxWindowErrors;
static uint16 EthernetDiag_TxWindowErrors;
static uint32 EthernetDiag_RxErrorCounter;
static uint32 EthernetDiag_TxErrorCounter;
static uint32 EthernetDiag_DmaErrorCounter;
static uint32 EthernetDiag_TcpUnexpectedCloseCounter;
static uint32 EthernetDiag_SocketOpenFailCounter;
static uint32 EthernetDiag_LinkDownTransitionCounter;
static uint16 EthernetDiag_ResourceWindowErrors;
static uint32 EthernetDiag_ResourceWindowStartMs;
static uint32 EthernetDiag_ResourceExhaustionCounter;
static uint32 EthernetDiag_LastResourceFlags;
static uint8 EthernetDiag_DoipActive;
static uint8 EthernetDiag_DoipTimedOut;
static uint8 EthernetDiag_LinkLostSnapshotValid;
static uint16 EthernetDiag_LinkLostSnapshotLength;
static uint8 EthernetDiag_LinkLostSnapshot[ETHERNETDIAG_SNAPSHOT_DATA_SIZE];

Std_ReturnType EthernetDiag_CaptureSnapshotData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length);

static void EthernetDiag_StoreU16(uint8 *buffer, uint16 offset, uint16 value)
{
    buffer[offset] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)(value & 0xFFu);
}

static void EthernetDiag_StoreU32(uint8 *buffer, uint16 offset, uint32 value)
{
    buffer[offset] = (uint8)((value >> 24u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)((value >> 16u) & 0xFFu);
    buffer[(uint16)(offset + 2u)] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 3u)] = (uint8)(value & 0xFFu);
}

static uint32 EthernetDiag_NowMs(void)
{
    return (uint32)(TimeBase_PlatformGetCounterNs() / TIMEBASE_NS_PER_MS);
}

static boolean EthernetDiag_Elapsed(uint32 nowMs, uint32 startMs, uint32 durationMs)
{
    return ((uint32)(nowMs - startMs) >= durationMs) ? TRUE : FALSE;
}

static boolean EthernetDiag_DeadlineReached(uint32 nowMs, uint32 deadlineMs)
{
    return (((sint32)(nowMs - deadlineMs)) >= 0) ? TRUE : FALSE;
}

static void EthernetDiag_SetupEvent(EthernetDiagEventType eventType,
        Dem_EventIdType eventId,
        uint32 debounceMs,
        uint32 healingMs)
{
    EthernetDiag_Events[eventType].eventId = eventId;
    EthernetDiag_Events[eventType].debounceMs = debounceMs;
    EthernetDiag_Events[eventType].healingMs = healingMs;
    EthernetDiag_Events[eventType].failedInput = FALSE;
    EthernetDiag_Events[eventType].reportedFailed = FALSE;
    EthernetDiag_Events[eventType].failStartMs = 0u;
    EthernetDiag_Events[eventType].passStartMs = 0u;
}

static void EthernetDiag_SetEventInput(EthernetDiagEventType eventType, boolean failed)
{
    if (eventType < ETHERNETDIAG_EVENT_COUNT)
    {
        EthernetDiag_Events[eventType].failedInput = failed;
    }
}

static void EthernetDiag_EvaluateEvent(EthernetDiagEventRuntimeType *event, uint32 nowMs, boolean allowed)
{
    if ((event == NULL_PTR) || (event->eventId == 0u) || (Dem_IsReady() == FALSE))
    {
        return;
    }

    if ((allowed != FALSE) && (event->failedInput != FALSE))
    {
        event->passStartMs = 0u;
        if (event->failStartMs == 0u)
        {
            event->failStartMs = nowMs;
        }

        (void)Dem_ReportErrorStatus(event->eventId, DEM_EVENT_STATUS_PREFAILED);
        if ((event->reportedFailed == FALSE) &&
                (EthernetDiag_Elapsed(nowMs, event->failStartMs, event->debounceMs) != FALSE))
        {
            if ((event->eventId == DEM_EVENT_ID_ETH_LINK_LOST) &&
                    (EthernetDiag_LinkLostSnapshotValid == FALSE))
            {
                uint16 snapshotLength = ETHERNETDIAG_SNAPSHOT_DATA_SIZE;

                if (EthernetDiag_CaptureSnapshotData(event->eventId,
                        EthernetDiag_LinkLostSnapshot, &snapshotLength) == E_OK)
                {
                    EthernetDiag_LinkLostSnapshotLength = snapshotLength;
                    EthernetDiag_LinkLostSnapshotValid = TRUE;
                }
            }
            (void)Dem_ReportErrorStatus(event->eventId, DEM_EVENT_STATUS_FAILED);
            event->reportedFailed = TRUE;
        }
    }
    else
    {
        event->failStartMs = 0u;
        if (event->passStartMs == 0u)
        {
            event->passStartMs = nowMs;
        }

        if (EthernetDiag_Elapsed(nowMs, event->passStartMs, event->healingMs) != FALSE)
        {
            (void)Dem_ReportErrorStatus(event->eventId, DEM_EVENT_STATUS_PASSED);
            event->reportedFailed = FALSE;
        }
    }
}

static const EthernetDiagConnectionConfigType *EthernetDiag_GetConnectionConfig(EthernetDiagConnectionId id)
{
    uint8 i;

    for (i = 0u; i < EthernetDiag_Config.connectionCount; i++)
    {
        if (EthernetDiag_Config.connections[i].connectionId == id)
        {
            return &EthernetDiag_Config.connections[i];
        }
    }

    return NULL_PTR;
}

static boolean EthernetDiag_IsConnectionInGrace(const EthernetDiagConnectionConfigType *cfg,
        const EthernetDiagConnectionRuntimeType *rt,
        uint32 nowMs)
{
    if ((cfg == NULL_PTR) || (rt == NULL_PTR))
    {
        return TRUE;
    }

    return (EthernetDiag_DeadlineReached(nowMs, rt->startupUntilMs) == FALSE) ? TRUE : FALSE;
}

void EthernetDiag_Init(void)
{
    uint8 i;
    uint32 nowMs;

    nowMs = EthernetDiag_NowMs();
    memset(EthernetDiag_Connections, 0, sizeof(EthernetDiag_Connections));
    memset(EthernetDiag_Services, 0, sizeof(EthernetDiag_Services));

    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_LINK_LOST, DEM_EVENT_ID_ETH_LINK_LOST,
            ETHERNETDIAG_LINK_DEBOUNCE_MS, ETHERNETDIAG_DEFAULT_HEALING_MS);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_CTRL_DMA_FAILURE, DEM_EVENT_ID_ETH_CTRL_DMA_FAILURE,
            ETHERNETDIAG_CTRL_DEBOUNCE_MS, 60000u);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_RX_COMM_FAILURE, DEM_EVENT_ID_ETH_RX_COMM_FAILURE,
            ETHERNETDIAG_ERROR_WINDOW_MS, ETHERNETDIAG_DEFAULT_HEALING_MS);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_TX_COMM_FAILURE, DEM_EVENT_ID_ETH_TX_COMM_FAILURE,
            ETHERNETDIAG_ERROR_WINDOW_MS, ETHERNETDIAG_DEFAULT_HEALING_MS);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_TCP_UNEXPECTED_TERMINATION, DEM_EVENT_ID_ETH_TCP_UNEXPECTED_TERMINATION,
            25000u, 45000u);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_TCP_ESTABLISHMENT_FAILURE, DEM_EVENT_ID_ETH_TCP_ESTABLISHMENT_FAILURE,
            25000u, 45000u);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_UDP_SUPERVISION_TIMEOUT, DEM_EVENT_ID_ETH_UDP_SUPERVISION_TIMEOUT,
            25000u, 45000u);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_SERVICE_AVAILABILITY_FAILURE, DEM_EVENT_ID_ETH_SERVICE_AVAILABILITY_FAILURE,
            25000u, 45000u);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_DOIP_COMM_FAILURE, DEM_EVENT_ID_ETH_DOIP_COMM_FAILURE,
            ETHERNETDIAG_DOIP_DEBOUNCE_MS, 45000u);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_PARTNER_COMM_TERMINATED, DEM_EVENT_ID_ETH_PARTNER_COMM_TERMINATED,
            25000u, 45000u);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_PHY_COMMUNICATION_FAULT, DEM_EVENT_ID_ETH_PHY_COMMUNICATION_FAULT,
            ETHERNETDIAG_PHY_DEBOUNCE_MS, ETHERNETDIAG_DEFAULT_HEALING_MS);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_PHY_FAULT, DEM_EVENT_ID_ETH_PHY_FAULT,
            ETHERNETDIAG_PHY_DEBOUNCE_MS, 60000u);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_NEGOTIATION_FAILURE, DEM_EVENT_ID_ETH_NEGOTIATION_FAILURE,
            ETHERNETDIAG_NEGOTIATION_DEBOUNCE_MS, ETHERNETDIAG_DEFAULT_HEALING_MS);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_UNEXPECTED_LINK_MODE, DEM_EVENT_ID_ETH_UNEXPECTED_LINK_MODE,
            ETHERNETDIAG_LINK_MODE_DEBOUNCE_MS, ETHERNETDIAG_DEFAULT_HEALING_MS);
    EthernetDiag_SetupEvent(ETHERNETDIAG_EVENT_RESOURCE_EXHAUSTION, DEM_EVENT_ID_ETH_RESOURCE_EXHAUSTION,
            ETHERNETDIAG_RESOURCE_DEBOUNCE_MS, 60000u);

    for (i = 0u; (i < EthernetDiag_Config.connectionCount) && (i < 8u); i++)
    {
        EthernetDiag_Connections[i].state = ETHERNETDIAG_CONN_STARTUP_GRACE;
        EthernetDiag_Connections[i].startupUntilMs = nowMs + EthernetDiag_Config.connections[i].startupGraceMs;
        EthernetDiag_Connections[i].lastAliveMs = nowMs;
        EthernetDiag_Connections[i].lastRxMs = nowMs;
        EthernetDiag_Connections[i].lastTxMs = nowMs;
    }

    for (i = 0u; (i < EthernetDiag_Config.serviceCount) && (i < 4u); i++)
    {
        EthernetDiag_Services[i].startupUntilMs = nowMs + EthernetDiag_Config.services[i].startupGraceMs;
    }

    EthernetDiag_RxWindowStartMs = nowMs;
    EthernetDiag_TxWindowStartMs = nowMs;
    EthernetDiag_ResourceWindowStartMs = nowMs;
    EthernetDiag_Initialized = TRUE;
}

boolean EthernetDiag_IsMonitoringAllowed(void)
{
    EthSM_ComModeType requestedMode;

    if ((EthernetDiag_Initialized == FALSE) || (Dem_IsReady() == FALSE))
    {
        return FALSE;
    }

    if (EthSM_GetRequestedComMode(0u, &requestedMode) != E_OK)
    {
        return FALSE;
    }

    if (requestedMode != ETHSM_FULL_COMMUNICATION)
    {
        return FALSE;
    }

    if (EthSM_IsStackInitialized(0u) == FALSE)
    {
        return FALSE;
    }

    return TRUE;
}

void EthernetDiag_MainFunction(void)
{
    uint8 i;
    uint32 nowMs;
    boolean allowed;
    boolean tcpUnexpected;
    boolean tcpEstFail;
    boolean udpTimeout;
    boolean serviceLost;
    boolean partnerLost;
    boolean phyCommFault;
    boolean phyFault;
    boolean negotiationFailure;
    boolean unexpectedLinkMode;

    if (EthernetDiag_Initialized == FALSE)
    {
        EthernetDiag_Init();
    }

    nowMs = EthernetDiag_NowMs();
    allowed = EthernetDiag_IsMonitoringAllowed();
    if (allowed == FALSE)
    {
        EthernetDiag_LinkUpSeen = FALSE;
        EthernetDiag_LinkStableStartMs = 0u;
    }
    tcpUnexpected = FALSE;
    tcpEstFail = FALSE;
    udpTimeout = FALSE;
    serviceLost = FALSE;
    partnerLost = FALSE;
    phyCommFault = FALSE;
    phyFault = FALSE;
    negotiationFailure = FALSE;
    unexpectedLinkMode = FALSE;

#if (PHY_DEVICE_NAME == PHY_DP83825I)
    {
        const lwip_geth_PhyDp83825i_StatusType *phyStatus;

        phyStatus = lwip_geth_private_Phy_Dp83825i_getStatus();
        if (phyStatus != NULL_PTR)
        {
            phyCommFault = ((phyStatus->mdioErrorCnt >= 3u) ||
                    (phyStatus->resetTimeoutCnt > 0u)) ? TRUE : FALSE;
            phyFault = (phyStatus->state == LWIP_GETH_PHY_DP83825I_STATE_ERROR) ? TRUE : FALSE;
            negotiationFailure = ((phyStatus->initDone != 0u) &&
                    (phyStatus->autonegTimeoutCnt > 0u) &&
                    (phyStatus->linkUp == 0u)) ? TRUE : FALSE;
            unexpectedLinkMode = ((phyStatus->linkUp != 0u) &&
                    ((phyStatus->speed100 == 0u) || (phyStatus->fullDuplex == 0u))) ? TRUE : FALSE;
        }
    }
#endif

    EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_LINK_LOST,
            ((allowed != FALSE) && (EthernetDiag_LinkUpSeen != FALSE) && (EthernetDiag_LinkUp == FALSE)) ? TRUE : FALSE);

    if (EthernetDiag_Elapsed(nowMs, EthernetDiag_RxWindowStartMs, ETHERNETDIAG_ERROR_WINDOW_MS) != FALSE)
    {
        EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_RX_COMM_FAILURE,
                (EthernetDiag_RxWindowErrors >= ETHERNETDIAG_RX_ERROR_THRESHOLD) ? TRUE : FALSE);
        EthernetDiag_RxWindowErrors = 0u;
        EthernetDiag_RxWindowStartMs = nowMs;
    }

    if (EthernetDiag_Elapsed(nowMs, EthernetDiag_TxWindowStartMs, ETHERNETDIAG_ERROR_WINDOW_MS) != FALSE)
    {
        EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_TX_COMM_FAILURE,
                (EthernetDiag_TxWindowErrors >= ETHERNETDIAG_TX_ERROR_THRESHOLD) ? TRUE : FALSE);
        EthernetDiag_TxWindowErrors = 0u;
        EthernetDiag_TxWindowStartMs = nowMs;
    }

    if (EthernetDiag_Elapsed(nowMs, EthernetDiag_ResourceWindowStartMs, ETHERNETDIAG_ERROR_WINDOW_MS) != FALSE)
    {
        EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_RESOURCE_EXHAUSTION,
                (EthernetDiag_ResourceWindowErrors >= ETHERNETDIAG_RESOURCE_THRESHOLD) ? TRUE : FALSE);
        EthernetDiag_ResourceWindowErrors = 0u;
        EthernetDiag_ResourceWindowStartMs = nowMs;
    }

    for (i = 0u; (i < EthernetDiag_Config.connectionCount) && (i < 8u); i++)
    {
        const EthernetDiagConnectionConfigType *cfg = &EthernetDiag_Config.connections[i];
        EthernetDiagConnectionRuntimeType *rt = &EthernetDiag_Connections[i];

        if ((cfg->mandatory == FALSE) || (EthernetDiag_IsConnectionInGrace(cfg, rt, nowMs) != FALSE))
        {
            continue;
        }

        if ((cfg->diagnosticConnection == FALSE) &&
                (cfg->protocol == TCPIP_PROTOCOL_TCP) &&
                (rt->state == ETHERNETDIAG_CONN_FAULT_PENDING))
        {
            tcpUnexpected = TRUE;
            partnerLost = TRUE;
        }

        if ((cfg->diagnosticConnection == FALSE) &&
                (cfg->protocol == TCPIP_PROTOCOL_TCP) &&
                (rt->openFailCount >= cfg->retryLimit) &&
                (cfg->retryLimit > 0u))
        {
            tcpEstFail = TRUE;
            partnerLost = TRUE;
        }

        if ((cfg->protocol == TCPIP_PROTOCOL_UDP) &&
                (cfg->supervisionTimeoutMs > 0u) &&
                (EthernetDiag_Elapsed(nowMs, rt->lastRxMs, cfg->supervisionTimeoutMs) != FALSE))
        {
            udpTimeout = TRUE;
            partnerLost = TRUE;
        }
    }

    for (i = 0u; (i < EthernetDiag_Config.serviceCount) && (i < 4u); i++)
    {
        const EthernetDiagServiceConfigType *cfg = &EthernetDiag_Config.services[i];
        EthernetDiagServiceRuntimeType *rt = &EthernetDiag_Services[i];

        if ((cfg->mandatory != FALSE) &&
                (rt->availableOnce != FALSE) &&
                (rt->available == FALSE) &&
                (EthernetDiag_DeadlineReached(nowMs, rt->startupUntilMs) != FALSE))
        {
            serviceLost = TRUE;
        }
    }

    EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_TCP_UNEXPECTED_TERMINATION, tcpUnexpected);
    EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_TCP_ESTABLISHMENT_FAILURE, tcpEstFail);
    EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_UDP_SUPERVISION_TIMEOUT, udpTimeout);
    EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_SERVICE_AVAILABILITY_FAILURE, serviceLost);
    EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_DOIP_COMM_FAILURE,
            ((EthernetDiag_DoipTimedOut != FALSE) ||
             ((EthernetDiag_DoipActive != FALSE) && (tcpUnexpected != FALSE))) ? TRUE : FALSE);
    EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_PARTNER_COMM_TERMINATED,
            ((EthernetDiag_LinkUp == FALSE) ||
             (EthernetDiag_Events[ETHERNETDIAG_EVENT_CTRL_DMA_FAILURE].reportedFailed != FALSE)) ? FALSE : partnerLost);
    EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_PHY_COMMUNICATION_FAULT, phyCommFault);
    EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_PHY_FAULT, phyFault);
    EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_NEGOTIATION_FAILURE,
            ((EthernetDiag_LinkUpSeen != FALSE) && (EthernetDiag_LinkUp == FALSE)) ? FALSE : negotiationFailure);
    EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_UNEXPECTED_LINK_MODE, unexpectedLinkMode);

    for (i = 0u; i < ETHERNETDIAG_EVENT_COUNT; i++)
    {
        EthernetDiag_EvaluateEvent(&EthernetDiag_Events[i], nowMs, allowed);
    }

    if (EthernetDiag_Events[ETHERNETDIAG_EVENT_DOIP_COMM_FAILURE].reportedFailed == FALSE)
    {
        EthernetDiag_DoipTimedOut = FALSE;
    }
}

void EthernetDiag_ReportPhyLink(boolean linkUp)
{
    uint32 nowMs;

    nowMs = EthernetDiag_NowMs();
    if ((EthernetDiag_LinkUp != FALSE) && (linkUp == FALSE) &&
            (EthernetDiag_LinkDownTransitionCounter < 0xFFFFFFFFu))
    {
        EthernetDiag_LinkDownTransitionCounter++;
    }

    EthernetDiag_LinkUp = (linkUp != FALSE) ? TRUE : FALSE;
    if (linkUp != FALSE)
    {
        if (EthernetDiag_LinkStableStartMs == 0u)
        {
            EthernetDiag_LinkStableStartMs = nowMs;
        }
        else if (EthernetDiag_Elapsed(nowMs, EthernetDiag_LinkStableStartMs,
                ETHERNETDIAG_LINK_ARM_STABLE_MS) != FALSE)
        {
            EthernetDiag_LinkUpSeen = TRUE;
        }
    }
    else
    {
        EthernetDiag_LinkStableStartMs = 0u;
    }
}

void EthernetDiag_ReportDmaError(uint32 errorFlags)
{
    if (errorFlags != 0u)
    {
        if (EthernetDiag_DmaErrorCounter < 0xFFFFFFFFu)
        {
            EthernetDiag_DmaErrorCounter++;
        }
        EthernetDiag_SetEventInput(ETHERNETDIAG_EVENT_CTRL_DMA_FAILURE, TRUE);
    }
}

void EthernetDiag_ReportRxError(uint32 errorFlags)
{
    if (errorFlags != 0u)
    {
        if (EthernetDiag_RxWindowErrors < 0xFFFFu)
        {
            EthernetDiag_RxWindowErrors++;
        }
        if (EthernetDiag_RxErrorCounter < 0xFFFFFFFFu)
        {
            EthernetDiag_RxErrorCounter++;
        }
    }
}

void EthernetDiag_ReportTxError(uint32 errorFlags)
{
    if (errorFlags != 0u)
    {
        if (EthernetDiag_TxWindowErrors < 0xFFFFu)
        {
            EthernetDiag_TxWindowErrors++;
        }
        if (EthernetDiag_TxErrorCounter < 0xFFFFFFFFu)
        {
            EthernetDiag_TxErrorCounter++;
        }
    }
}

void EthernetDiag_ReportResourceExhaustion(uint32 reasonFlags)
{
    if (reasonFlags != 0u)
    {
        EthernetDiag_LastResourceFlags = reasonFlags;
        if (EthernetDiag_ResourceWindowErrors < 0xFFFFu)
        {
            EthernetDiag_ResourceWindowErrors++;
        }
        if (EthernetDiag_ResourceExhaustionCounter < 0xFFFFFFFFu)
        {
            EthernetDiag_ResourceExhaustionCounter++;
        }
    }
}

void EthernetDiag_ReportSocketConnected(EthernetDiagConnectionId id)
{
    const EthernetDiagConnectionConfigType *cfg;
    EthernetDiagConnectionRuntimeType *rt;
    uint32 nowMs;

    cfg = EthernetDiag_GetConnectionConfig(id);
    if ((cfg == NULL_PTR) || (id >= 8u))
    {
        return;
    }

    nowMs = EthernetDiag_NowMs();
    rt = &EthernetDiag_Connections[id];
    rt->state = ETHERNETDIAG_CONN_CONNECTED;
    rt->intentionalClosePending = FALSE;
    rt->connectedOnce = TRUE;
    rt->openFailCount = 0u;
    rt->lastAliveMs = nowMs;
    rt->lastRxMs = nowMs;
    rt->lastTxMs = nowMs;
    rt->lossStartMs = 0u;
}

void EthernetDiag_ReportSocketOpenFailed(EthernetDiagConnectionId id)
{
    EthernetDiagConnectionRuntimeType *rt;

    if (id >= 8u)
    {
        return;
    }

    rt = &EthernetDiag_Connections[id];
    if (rt->openFailCount < 0xFFu)
    {
        rt->openFailCount++;
    }
    if (EthernetDiag_SocketOpenFailCounter < 0xFFFFFFFFu)
    {
        EthernetDiag_SocketOpenFailCounter++;
    }
    rt->state = ETHERNETDIAG_CONN_CONNECTING;
}

void EthernetDiag_ReportSocketCloseRequested(EthernetDiagConnectionId id)
{
    if (id < 8u)
    {
        EthernetDiag_Connections[id].intentionalClosePending = TRUE;
        EthernetDiag_Connections[id].state = ETHERNETDIAG_CONN_INTENTIONAL_CLOSE_PENDING;
    }
}

void EthernetDiag_ReportSocketClosed(EthernetDiagConnectionId id, EthernetDiagCloseReasonType reason)
{
    EthernetDiagConnectionRuntimeType *rt;

    if (id >= 8u)
    {
        return;
    }

    rt = &EthernetDiag_Connections[id];
    if ((rt->intentionalClosePending != FALSE) ||
            (reason == ETHERNETDIAG_CLOSE_INTENTIONAL) ||
            (reason == ETHERNETDIAG_CLOSE_NORMAL) ||
            (reason == ETHERNETDIAG_CLOSE_PEER_FIN))
    {
        rt->state = ETHERNETDIAG_CONN_DISCONNECTED;
        rt->intentionalClosePending = FALSE;
        return;
    }

    if (rt->connectedOnce != FALSE)
    {
        rt->state = ETHERNETDIAG_CONN_FAULT_PENDING;
        if (EthernetDiag_TcpUnexpectedCloseCounter < 0xFFFFFFFFu)
        {
            EthernetDiag_TcpUnexpectedCloseCounter++;
        }
        if (rt->lossStartMs == 0u)
        {
            rt->lossStartMs = EthernetDiag_NowMs();
        }
    }
}

void EthernetDiag_ReportRxActivity(EthernetDiagConnectionId id)
{
    if (id < 8u)
    {
        EthernetDiag_Connections[id].lastRxMs = EthernetDiag_NowMs();
        EthernetDiag_ReportPartnerAlive(id);
    }
}

void EthernetDiag_ReportTxActivity(EthernetDiagConnectionId id)
{
    if (id < 8u)
    {
        EthernetDiag_Connections[id].lastTxMs = EthernetDiag_NowMs();
    }
}

void EthernetDiag_ReportPartnerAlive(EthernetDiagConnectionId id)
{
    if (id < 8u)
    {
        EthernetDiag_Connections[id].lastAliveMs = EthernetDiag_NowMs();
        if (EthernetDiag_Connections[id].state == ETHERNETDIAG_CONN_FAULT_PENDING)
        {
            EthernetDiag_Connections[id].state = ETHERNETDIAG_CONN_CONNECTED;
        }
    }
}

void EthernetDiag_ReportServiceAvailable(EthernetDiagServiceId id, boolean available)
{
    if (id < 4u)
    {
        EthernetDiag_Services[id].available = (available != FALSE) ? TRUE : FALSE;
        if (available != FALSE)
        {
            EthernetDiag_Services[id].availableOnce = TRUE;
        }
    }
}

void EthernetDiag_ReportDoipActive(boolean active)
{
    EthernetDiag_DoipActive = (active != FALSE) ? TRUE : FALSE;
}

void EthernetDiag_ReportDoipTimeout(void)
{
    EthernetDiag_DoipTimedOut = TRUE;
}

EthernetDiagConnectionId EthernetDiag_GetConnectionIdForSoCon(uint8 soConId)
{
    uint8 i;

    for (i = 0u; i < EthernetDiag_Config.connectionCount; i++)
    {
        if (EthernetDiag_Config.connections[i].soConId == soConId)
        {
            return EthernetDiag_Config.connections[i].connectionId;
        }
    }

    return ETHERNETDIAG_CONNECTION_INVALID;
}

Std_ReturnType EthernetDiag_CaptureSnapshotData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length)
{
    uint8 i;
    uint8 selectedIndex;
    SoAd_DiagSnapshotType soAdSnapshot;
    EthSM_ComModeType requestedMode = ETHSM_NO_COMMUNICATION;
    EthSM_ComModeType currentMode = ETHSM_NO_COMMUNICATION;
    const netif_t *netif;

    if ((buffer == NULL_PTR) || (length == NULL_PTR) ||
            (*length < ETHERNETDIAG_SNAPSHOT_DATA_SIZE))
    {
        return E_NOT_OK;
    }

    if ((eventId == DEM_EVENT_ID_ETH_LINK_LOST) &&
            (EthernetDiag_LinkLostSnapshotValid != FALSE))
    {
        (void)memcpy(buffer, EthernetDiag_LinkLostSnapshot,
                EthernetDiag_LinkLostSnapshotLength);
        *length = EthernetDiag_LinkLostSnapshotLength;
        EthernetDiag_LinkLostSnapshotValid = FALSE;
        return E_OK;
    }

    (void)memset(buffer, 0, ETHERNETDIAG_SNAPSHOT_DATA_SIZE);

    if ((eventId < DEM_EVENT_ID_ETH_LINK_LOST) ||
            (eventId > DEM_EVENT_ID_ETH_DIAG_LAST))
    {
        return E_NOT_OK;
    }

    if (Dem_Cfg_CaptureTimestampTemperatureData(buffer, length,
            DEM_SNAPSHOT_KIND_ETHERNET) != E_OK)
    {
        return E_NOT_OK;
    }

    selectedIndex = 0u;
    for (i = 0u; (i < EthernetDiag_Config.connectionCount) && (i < 8u); i++)
    {
        const EthernetDiagConnectionConfigType *cfg = &EthernetDiag_Config.connections[i];

        if (((eventId == cfg->tcpTerminationEventId) ||
                (eventId == cfg->tcpEstablishmentEventId) ||
                (eventId == cfg->udpTimeoutEventId) ||
                (eventId == cfg->partnerLostEventId)) &&
                (cfg->connectionId < 8u))
        {
            selectedIndex = i;
            break;
        }
    }

    (void)memset(&soAdSnapshot, 0, sizeof(soAdSnapshot));
    soAdSnapshot.listenSock = TCPIP_INVALID_SOCKET;
    soAdSnapshot.activeSock = TCPIP_INVALID_SOCKET;
    if ((selectedIndex < EthernetDiag_Config.connectionCount) &&
            (SoAd_GetDiagSnapshot(EthernetDiag_Config.connections[selectedIndex].soConId,
                    &soAdSnapshot) == E_OK))
    {
        EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_LOCAL_IP_OFFSET, soAdSnapshot.localAddr.addr);
        EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_REMOTE_IP_OFFSET, soAdSnapshot.remoteAddr.addr);
        EthernetDiag_StoreU16(buffer, ETHERNETDIAG_SNAPSHOT_LOCAL_PORT_OFFSET, soAdSnapshot.localAddr.port);
        EthernetDiag_StoreU16(buffer, ETHERNETDIAG_SNAPSHOT_REMOTE_PORT_OFFSET, soAdSnapshot.remoteAddr.port);
        buffer[ETHERNETDIAG_SNAPSHOT_SOAD_STATE_OFFSET] = (uint8)soAdSnapshot.state;
    }

    buffer[ETHERNETDIAG_SNAPSHOT_SOCON_ID_OFFSET] = EthernetDiag_Config.connections[selectedIndex].soConId;
    buffer[ETHERNETDIAG_SNAPSHOT_CONNECTION_ID_OFFSET] = EthernetDiag_Config.connections[selectedIndex].connectionId;
    buffer[ETHERNETDIAG_SNAPSHOT_PROTOCOL_OFFSET] = EthernetDiag_Config.connections[selectedIndex].protocol;
    buffer[ETHERNETDIAG_SNAPSHOT_LINK_UP_OFFSET] = EthernetDiag_LinkUp;
    EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_RX_ERRORS_OFFSET, EthernetDiag_RxErrorCounter);
    EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_TX_ERRORS_OFFSET, EthernetDiag_TxErrorCounter);
    EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_DMA_ERRORS_OFFSET, EthernetDiag_DmaErrorCounter);
    EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_TCP_CLOSES_OFFSET, EthernetDiag_TcpUnexpectedCloseCounter);
    EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_OPEN_FAILS_OFFSET, EthernetDiag_SocketOpenFailCounter);
    EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_LINK_DOWNS_OFFSET, EthernetDiag_LinkDownTransitionCounter);
    EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_LISTEN_SOCKET_OFFSET, (uint32)soAdSnapshot.listenSock);
    EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_ACTIVE_SOCKET_OFFSET, (uint32)soAdSnapshot.activeSock);

#if (PHY_DEVICE_NAME == PHY_DP83825I)
    {
        const lwip_geth_PhyDp83825i_StatusType *phyStatus;

        phyStatus = lwip_geth_private_Phy_Dp83825i_getStatus();
        if (phyStatus != NULL_PTR)
        {
            buffer[ETHERNETDIAG_SNAPSHOT_PHY_STATE_OFFSET] = (uint8)phyStatus->state;
            buffer[ETHERNETDIAG_SNAPSHOT_PHY_ADDR_OFFSET] = 0u;
            buffer[ETHERNETDIAG_SNAPSHOT_PHY_INIT_DONE_OFFSET] = (uint8)phyStatus->initDone;
            buffer[ETHERNETDIAG_SNAPSHOT_PHY_AUTONEG_DONE_OFFSET] = (uint8)phyStatus->autonegDone;
            buffer[ETHERNETDIAG_SNAPSHOT_PHY_SPEED100_OFFSET] = (uint8)phyStatus->speed100;
            buffer[ETHERNETDIAG_SNAPSHOT_PHY_FULL_DUPLEX_OFFSET] = (uint8)phyStatus->fullDuplex;
            EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_PHY_RESET_TIMEOUTS_OFFSET,
                    phyStatus->resetTimeoutCnt);
            EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_PHY_AUTONEG_TIMEOUTS_OFFSET,
                    phyStatus->autonegTimeoutCnt);
            EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_PHY_MDIO_ERRORS_OFFSET,
                    phyStatus->mdioErrorCnt);
        }
    }
#endif
    EthernetDiag_StoreU32(buffer, ETHERNETDIAG_SNAPSHOT_RESOURCE_ERRORS_OFFSET,
            EthernetDiag_ResourceExhaustionCounter);
    EthernetDiag_StoreU16(buffer, ETHERNETDIAG_SNAPSHOT_LAST_RESOURCE_FLAGS_OFFSET,
            (uint16)(EthernetDiag_LastResourceFlags & 0xFFFFu));
    netif = lwip_geth_Lwip_getNetIf();
    buffer[ETHERNETDIAG_SNAPSHOT_NETIF_FLAGS_OFFSET] =
            (netif != NULL_PTR) ? netif->flags : 0u;
    (void)EthSM_GetRequestedComMode(0u, &requestedMode);
    (void)EthSM_GetCurrentComMode(0u, &currentMode);
    buffer[ETHERNETDIAG_SNAPSHOT_ETHSM_REQUESTED_OFFSET] = (uint8)requestedMode;
    buffer[ETHERNETDIAG_SNAPSHOT_ETHSM_CURRENT_OFFSET] = (uint8)currentMode;

#if (PHY_DEVICE_NAME == PHY_DP83825I)
    {
        const lwip_geth_PhyDp83825i_StatusType *phyStatus;

        phyStatus = lwip_geth_private_Phy_Dp83825i_getStatus();
        if (phyStatus != NULL_PTR)
        {
            EthernetDiag_StoreU16(buffer, ETHERNETDIAG_SNAPSHOT_PHY_BMSR_OFFSET,
                    phyStatus->lastBmsr);
            EthernetDiag_StoreU16(buffer, ETHERNETDIAG_SNAPSHOT_PHY_PHYSTS_OFFSET,
                    phyStatus->lastPhysts);
        }
    }
#endif

    *length = ETHERNETDIAG_SNAPSHOT_DATA_SIZE;
    return E_OK;
}
