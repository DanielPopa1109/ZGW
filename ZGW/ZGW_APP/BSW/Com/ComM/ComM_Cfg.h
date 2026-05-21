#ifndef COMM_CFG_H
#define COMM_CFG_H

#include "ComM.h"

#define COMM_CHANNEL_MASK_CAN   (1u << COMM_CH_CAN)
#define COMM_CHANNEL_MASK_CANFD (1u << COMM_CH_CANFD)
#define COMM_CHANNEL_MASK_LIN   (1u << COMM_CH_LIN)
#define COMM_CHANNEL_MASK_ETH   (1u << COMM_CH_ETH)
#define COMM_CHANNEL_MASK_ALL   (COMM_CHANNEL_MASK_CAN | \
                                 COMM_CHANNEL_MASK_CANFD | \
                                 COMM_CHANNEL_MASK_LIN | \
                                 COMM_CHANNEL_MASK_ETH)

extern const uint8 ComM_UserChannelMap[COMM_NUM_USERS];

#endif
