#ifndef SOMEIP_H_
#define SOMEIP_H_

#include "Ifx_Types.h"
#include "SoAd.h"

#define SOMEIP_HEADER_LEN                  16u
#define SOMEIP_MAX_PAYLOAD_LEN             1400u
#define SOMEIP_TCP_STREAM_BUF_LEN          4096u
#define SOMEIP_PROTOCOL_VERSION            0x01u

#define SOMEIP_MSG_REQUEST                 0x00u
#define SOMEIP_MSG_REQUEST_NO_RET          0x01u
#define SOMEIP_MSG_NOTIFICATION            0x02u
#define SOMEIP_MSG_RESPONSE                0x80u
#define SOMEIP_MSG_ERROR                   0x81u

#define SOMEIP_RET_OK                      0x00u
#define SOMEIP_RET_NOT_OK                  0x01u
#define SOMEIP_RET_UNKNOWN_SERVICE         0x02u
#define SOMEIP_RET_UNKNOWN_METHOD          0x03u
#define SOMEIP_RET_NOT_READY               0x04u
#define SOMEIP_RET_NOT_REACHABLE           0x05u
#define SOMEIP_RET_TIMEOUT                 0x06u
#define SOMEIP_RET_WRONG_PROTOCOL_VERSION  0x07u
#define SOMEIP_RET_WRONG_INTERFACE_VERSION 0x08u
#define SOMEIP_RET_MALFORMED_MESSAGE       0x09u

typedef struct
{
    uint16 serviceId;
    uint16 methodId;
    uint16 clientId;
    uint16 sessionId;
    uint8 protocolVersion;
    uint8 interfaceVersion;
    uint8 messageType;
    uint8 returnCode;
    const uint8 *payload;
    uint32 payloadLen;
} SomeIp_MessageType;

typedef void (*SomeIp_MethodHandlerType)(SoAd_SoConIdType soConId,
                                         const TcpIp_SockAddrType *remoteAddr,
                                         const SomeIp_MessageType *request);

typedef struct
{
    uint16 serviceId;
    uint16 methodId;
    uint8 interfaceVersion;
    SomeIp_MethodHandlerType handler;
} SomeIp_MethodConfigType;

typedef struct
{
    SoAd_SoConIdType udpSoConId;
    SoAd_SoConIdType tcpSoConId;
    const SomeIp_MethodConfigType *methods;
    uint16 methodCount;
} SomeIp_ConfigType;

void SomeIp_Init(const SomeIp_ConfigType *config);
void SomeIp_MainFunction(uint32 elapsedMs);

void SomeIp_SoAdRxIndication(SoAd_SoConIdType soConId,
                             const TcpIp_SockAddrType *remoteAddr,
                             const uint8 *data,
                             uint16 len);

void SomeIp_SoAdTcpConnected(SoAd_SoConIdType soConId);
void SomeIp_SoAdTcpDisconnected(SoAd_SoConIdType soConId);

uint8 SomeIp_SendResponse(SoAd_SoConIdType soConId,
                          const TcpIp_SockAddrType *remoteAddr,
                          const SomeIp_MessageType *request,
                          const uint8 *payload,
                          uint32 payloadLen,
                          uint8 returnCode);

uint8 SomeIp_SendNotification(SoAd_SoConIdType soConId,
                              const TcpIp_SockAddrType *remoteAddr,
                              uint16 serviceId,
                              uint16 eventId,
                              uint16 clientId,
                              uint16 sessionId,
                              uint8 interfaceVersion,
                              const uint8 *payload,
                              uint32 payloadLen);

#endif
