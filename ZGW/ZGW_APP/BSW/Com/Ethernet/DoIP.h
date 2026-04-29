#ifndef DOIP_H_
#define DOIP_H_

#include "Ifx_Types.h"
#include "SoAd.h"

#define DOIP_TCP_PORT                         13400u
#define DOIP_UDP_PORT                         13400u

#define DOIP_PROTOCOL_VERSION                 0x02u
#define DOIP_INVERSE_PROTOCOL_VERSION         0xFDu

#define DOIP_MAX_VIN_LEN                      17u
#define DOIP_MAX_EID_LEN                      6u
#define DOIP_MAX_GID_LEN                      6u
#define DOIP_MAX_UDS_PAYLOAD_LEN              4095u
#define DOIP_HEADER_LEN                       8u
#define DOIP_TCP_RX_STREAM_LEN                8192u

#define DOIP_ALIVE_TIMEOUT_MS                 5000u
#define DOIP_INACTIVITY_TIMEOUT_MS            30000u

#define DOIP_PAYLOAD_GENERIC_NACK             0x0000u
#define DOIP_PAYLOAD_VEHICLE_ID_REQ           0x0001u
#define DOIP_PAYLOAD_VEHICLE_ID_RES           0x0004u
#define DOIP_PAYLOAD_ROUTING_ACTIVATION_REQ   0x0005u
#define DOIP_PAYLOAD_ROUTING_ACTIVATION_RES   0x0006u
#define DOIP_PAYLOAD_ALIVE_CHECK_REQ          0x0007u
#define DOIP_PAYLOAD_ALIVE_CHECK_RES          0x0008u
#define DOIP_PAYLOAD_ENTITY_STATUS_REQ        0x4001u
#define DOIP_PAYLOAD_ENTITY_STATUS_RES        0x4002u
#define DOIP_PAYLOAD_POWER_MODE_REQ           0x4003u
#define DOIP_PAYLOAD_POWER_MODE_RES           0x4004u
#define DOIP_PAYLOAD_DIAG_MSG                 0x8001u
#define DOIP_PAYLOAD_DIAG_MSG_POS_ACK         0x8002u
#define DOIP_PAYLOAD_DIAG_MSG_NEG_ACK         0x8003u

#define DOIP_GEN_NACK_INCORRECT_PATTERN       0x00u
#define DOIP_GEN_NACK_UNKNOWN_PAYLOAD         0x01u
#define DOIP_GEN_NACK_MESSAGE_TOO_LARGE       0x02u
#define DOIP_GEN_NACK_OUT_OF_MEMORY           0x03u
#define DOIP_GEN_NACK_INVALID_LENGTH          0x04u

#define DOIP_RA_RES_DENIED_UNKNOWN_SOURCE     0x00u
#define DOIP_RA_RES_DENIED_ALL_SOCKETS_USED   0x01u
#define DOIP_RA_RES_DENIED_SA_REGISTERED      0x02u
#define DOIP_RA_RES_DENIED_MISSING_AUTH       0x03u
#define DOIP_RA_RES_DENIED_REJECTED_CONF      0x04u
#define DOIP_RA_RES_UNSUPPORTED_ACTIVATION    0x05u
#define DOIP_RA_RES_OK                        0x10u

#define DOIP_NACK_INVALID_SOURCE_ADDR         0x02u
#define DOIP_NACK_UNKNOWN_TARGET_ADDR         0x03u
#define DOIP_NACK_DIAG_MSG_TOO_LARGE          0x04u
#define DOIP_NACK_OUT_OF_MEMORY               0x05u
#define DOIP_NACK_TARGET_UNREACHABLE          0x06u
#define DOIP_NACK_UNKNOWN_NETWORK             0x07u
#define DOIP_NACK_TRANSPORT_PROTOCOL_ERROR    0x08u

typedef enum
{
    DOIP_OK = 0,
    DOIP_NOT_OK,
    DOIP_BUSY,
    DOIP_PARAM_ERROR
} DoIP_ReturnType;

typedef enum
{
    DOIP_TCP_OFFLINE = 0,
    DOIP_TCP_CONNECTED,
    DOIP_TCP_ROUTING_ACTIVE
} DoIP_TcpStateType;

typedef struct
{
    char vin[DOIP_MAX_VIN_LEN];
    uint8 eid[DOIP_MAX_EID_LEN];
    uint8 gid[DOIP_MAX_GID_LEN];
    uint16 logicalAddress;
    SoAd_SoConIdType udpSoConId;
    SoAd_SoConIdType tcpSoConId;
} DoIP_ConfigType;

typedef void (*DoIP_DcmRxIndicationFct)(uint16 sourceAddress,
                                        uint16 targetAddress,
                                        const uint8 *uds,
                                        uint16 udsLen);

void DoIP_Init(const DoIP_ConfigType *config);
void DoIP_MainFunction(uint32 elapsedMs);
void DoIP_SetDcmRxIndication(DoIP_DcmRxIndicationFct cb);

void DoIP_SoAdUdpRxIndication(SoAd_SoConIdType soConId,
                              const TcpIp_SockAddrType *remoteAddr,
                              const uint8 *data,
                              uint16 len);

void DoIP_SoAdTcpRxIndication(SoAd_SoConIdType soConId,
                              const TcpIp_SockAddrType *remoteAddr,
                              const uint8 *data,
                              uint16 len);

void DoIP_SoAdTcpConnected(SoAd_SoConIdType soConId);
void DoIP_SoAdTcpDisconnected(SoAd_SoConIdType soConId);

DoIP_ReturnType DoIP_SendDiagnosticResponse(uint16 sourceAddress,
                                            uint16 targetAddress,
                                            const uint8 *uds,
                                            uint16 udsLen);

DoIP_TcpStateType DoIP_GetTcpState(void);

#endif
