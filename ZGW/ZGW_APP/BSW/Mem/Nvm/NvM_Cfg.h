#ifndef NVM_CFG_H
#define NVM_CFG_H

#include "Std_Types.h"
#include "Fee_Cfg.h"

typedef uint16 NvM_BlockIdType;

typedef enum
{
    NVM_BLOCK_NATIVE = 0,
    NVM_BLOCK_REDUNDANT = 1,
    NVM_BLOCK_DATASET = 2
} NvM_BlockManagementType;

typedef struct
{
    NvM_BlockIdType blockId;
    uint16 nvBlockLength;
    uint8 *ramBlockDataAddress;
    const uint8 *romBlockDataAddress;
    NvM_BlockManagementType blockManagementType;
    boolean blockUseCrc;
    boolean writeOnce;
    boolean immediateData;
} NvM_BlockDescriptorType;

#define NVM_TOTAL_BLOCKS                    (2u)
#define NVM_BLOCK_ID_1                      (1u)
#define NVM_BLOCK_ID_2                      (2u)
#define NVM_BLOCK_1_LENGTH                  (1024u)
#define NVM_BLOCK_2_LENGTH                  (2048u)
#define NVM_MAX_BLOCK_LENGTH                (2048u)

extern uint8 NvM_Block1_Ram[NVM_BLOCK_1_LENGTH];
extern uint8 NvM_Block2_Ram[NVM_BLOCK_2_LENGTH];
extern const uint8 NvM_Block1_Rom[NVM_BLOCK_1_LENGTH];
extern const uint8 NvM_Block2_Rom[NVM_BLOCK_2_LENGTH];
extern const NvM_BlockDescriptorType NvM_BlockDescriptor[NVM_TOTAL_BLOCKS];

#endif /* NVM_CFG_H */
