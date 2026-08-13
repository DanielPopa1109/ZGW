#include "EthernetDiag.h"
#include "SoAd.h"
#include "Dem_Cfg.h"

#define ETHERNETDIAG_SOCON_DOIP_UDP        0u
#define ETHERNETDIAG_SOCON_DOIP_TCP        1u
#define ETHERNETDIAG_SOCON_SOMEIP_SD_UDP   2u
#define ETHERNETDIAG_SOCON_SOMEIP_UDP      3u
#define ETHERNETDIAG_SOCON_PDUR_IF_UDP     4u

static const EthernetDiagConnectionConfigType EthernetDiag_Connections[] =
{
    {
        0u,
        ETHERNETDIAG_SOCON_DOIP_TCP,
        TRUE,
        TCPIP_PROTOCOL_TCP,
        5000u,
        0u,
        25000u,
        45000u,
        3u,
        DEM_EVENT_ID_ETH_TCP_UNEXPECTED_TERMINATION,
        DEM_EVENT_ID_ETH_TCP_ESTABLISHMENT_FAILURE,
        0u,
        DEM_EVENT_ID_ETH_PARTNER_COMM_TERMINATED
    },
    {
        1u,
        ETHERNETDIAG_SOCON_PDUR_IF_UDP,
        FALSE,
        TCPIP_PROTOCOL_UDP,
        5000u,
        0u,
        25000u,
        45000u,
        0u,
        0u,
        0u,
        DEM_EVENT_ID_ETH_UDP_SUPERVISION_TIMEOUT,
        DEM_EVENT_ID_ETH_PARTNER_COMM_TERMINATED
    }
};

static const EthernetDiagServiceConfigType EthernetDiag_Services[] =
{
    {
        0u,
        0x1234u,
        0x0001u,
        TRUE,
        5000u,
        25000u,
        45000u,
        DEM_EVENT_ID_ETH_SERVICE_AVAILABILITY_FAILURE
    }
};

const EthernetDiag_ConfigType EthernetDiag_Config =
{
    EthernetDiag_Connections,
    (uint8)(sizeof(EthernetDiag_Connections) / sizeof(EthernetDiag_Connections[0])),
    EthernetDiag_Services,
    (uint8)(sizeof(EthernetDiag_Services) / sizeof(EthernetDiag_Services[0]))
};
