#ifndef UDPNM_H
#define UDPNM_H

#include "../Nm/Nm.h"
#include "../Ethernet/SoAd.h"

#define UDPNM_USER_DATA_LEN 8u

void UdpNm_Init(void);
void UdpNm_MainFunction(void);

Std_ReturnType UdpNm_NetworkRequest(uint8 channel);
Std_ReturnType UdpNm_NetworkRelease(uint8 channel);
Std_ReturnType UdpNm_GetState(uint8 channel, Nm_StateType* state, Nm_ModeType* mode);
Std_ReturnType UdpNm_PassiveStartUp(uint8 channel);
Std_ReturnType UdpNm_SetUserData(uint8 channel, const uint8* data, uint8 len);
Std_ReturnType UdpNm_GetUserData(uint8 channel, uint8* data, uint8* len);

uint8 UdpNm_SoAdRxIndication(SoAd_SoConIdType soConId,
                             const TcpIp_SockAddrType* remoteAddr,
                             const uint8* data,
                             uint16 len);

#endif
