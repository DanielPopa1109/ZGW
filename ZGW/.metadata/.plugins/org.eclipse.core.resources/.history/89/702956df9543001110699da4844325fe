/* DoIP.h */
#ifndef DOIP_H_
#define DOIP_H_

#include <stdint.h>
#include "SoAd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DOIP_TCP_PORT                         13400u
#define DOIP_UDP_PORT                         13400u

#define DOIP_PROTOCOL_VERSION                 0x02u
#define DOIP_INVERSE_PROTOCOL_VERSION         0xFDu

#define DOIP_MAX_VIN_LEN                      17u
#define DOIP_MAX_EID_LEN                      6u
#define DOIP_MAX_GID_LEN                      6u
#define DOIP_MAX_UDS_PAYLOAD_LEN              4095u
#define DOIP_HEADER_LEN                       8u

#define DOIP_PAYLOAD_VEHICLE_ID_REQ           0x0001u
#define DOIP_PAYLOAD_VEHICLE_ID_RES           0x0004u
#define DOIP_PAYLOAD_ROUTING_ACTIVATION_REQ   0x0005u
#define DOIP_PAYLOAD_ROUTING_ACTIVATION_RES   0x0006u
#define DOIP_PAYLOAD_ALIVE_CHECK_REQ          0x0007u
#define DOIP_PAYLOAD_ALIVE_CHECK_RES          0x0008u
#define DOIP_PAYLOAD_DIAG_MSG                 0x8001u
#define DOIP_PAYLOAD_DIAG_MSG_POS_ACK         0x8002u
#define DOIP_PAYLOAD_DIAG_MSG_NEG_ACK         0x8003u

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

typedef struct
{
    char vin[DOIP_MAX_VIN_LEN];
    uint8_t eid[DOIP_MAX_EID_LEN];
    uint8_t gid[DOIP_MAX_GID_LEN];
    uint16_t logicalAddress;
    SoAd_SoConIdType udpSoConId;
    SoAd_SoConIdType tcpSoConId;
} DoIP_ConfigType;

typedef void (*DoIP_DcmRxIndicationFct)(uint16_t sourceAddress,
                                        uint16_t targetAddress,
                                        const uint8_t *uds,
                                        uint16_t udsLen);

void DoIP_Init(const DoIP_ConfigType *config);
void DoIP_MainFunction(void);

void DoIP_SetDcmRxIndication(DoIP_DcmRxIndicationFct cb);

void DoIP_SoAdUdpRxIndication(SoAd_SoConIdType soConId,
                              const TcpIp_SockAddrType *remoteAddr,
                              const uint8_t *data,
                              uint16_t len);

void DoIP_SoAdTcpRxIndication(SoAd_SoConIdType soConId,
                              const TcpIp_SockAddrType *remoteAddr,
                              const uint8_t *data,
                              uint16_t len);

void DoIP_SoAdTcpConnected(SoAd_SoConIdType soConId);
void DoIP_SoAdTcpDisconnected(SoAd_SoConIdType soConId);

DoIP_ReturnType DoIP_SendDiagnosticResponse(uint16_t sourceAddress,
                                            uint16_t targetAddress,
                                            const uint8_t *uds,
                                            uint16_t udsLen);

#ifdef __cplusplus
}
#endif

#endif
