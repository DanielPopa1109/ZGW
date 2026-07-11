#ifndef LIN_LOWLEVEL_H_
#define LIN_LOWLEVEL_H_

#include "Std_Types.h"
#include "Lin_Protocol.h"

void Lin_LowLevel_Init(void);

void Lin_LowLevel_SendBreak(uint8 Channel);
void Lin_LowLevel_SendByte(uint8 Channel, uint8 Byte);

void Lin_LowLevel_EnableRx(uint8 Channel);
void Lin_LowLevel_DisableRx(uint8 Channel);

Lin_ResultType Lin_LowLevel_TransferFrame(uint8 Channel,
                                          uint8 Pid,
                                          uint8 Length,
                                          uint8 Direction,
                                          Lin_ChecksumType ChecksumType,
                                          const uint8* TxData,
                                          uint8* RxData);

void Lin_LowLevel_WakeupPulse(uint8 Channel);

void Lin_LowLevel_SetResponseLength(uint8 Length);
void Lin_LowLevel_SetChecksumType(Lin_ChecksumType ChecksumType);

#endif
