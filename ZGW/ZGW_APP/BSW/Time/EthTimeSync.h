#ifndef ETHTIMESYNC_H
#define ETHTIMESYNC_H

#include "Std_Types.h"

#define ETHTIMESYNC_STATUS_DISABLED            0u
#define ETHTIMESYNC_STATUS_WAIT_LINK           1u
#define ETHTIMESYNC_STATUS_SOCKET_READY        2u
#define ETHTIMESYNC_STATUS_TX_ERROR            3u

void EthTimeSync_Init(void);
void EthTimeSync_MainFunction(uint32 elapsedMs);
uint8 EthTimeSync_GetStatus(void);
uint32 EthTimeSync_GetTxCounter(void);

#endif /* ETHTIMESYNC_H */
