#ifndef NVM_CFG_H
#define NVM_CFG_H

#include "Std_Types.h"
#include "Dem_Int.h"
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

#define NVM_TOTAL_BLOCKS                    (3u)

#define NVM_BLOCK_ID_DEM_PRIMARY            (1u)
#define NVM_BLOCK_ID_APP_DATA               (2u)
#define NVM_BLOCK_ID_TIMEBASE               (3u)

#define NVM_BLOCK_DEM_PRIMARY_LENGTH        FEE_BLOCK_DEM_PRIMARY_SIZE
#define NVM_BLOCK_APP_DATA_LENGTH           FEE_BLOCK_APP_DATA_SIZE
#define NVM_BLOCK_TIMEBASE_LENGTH           FEE_BLOCK_TIMEBASE_SIZE

#define NVM_MAX_BLOCK_LENGTH                FEE_MAX_BLOCK_SIZE

extern uint8 NvM_AppData_Ram[NVM_BLOCK_APP_DATA_LENGTH];
extern uint8 NvM_TimeBase_Ram[NVM_BLOCK_TIMEBASE_LENGTH];

extern const uint8 NvM_AppData_Rom[NVM_BLOCK_APP_DATA_LENGTH];
extern const uint8 NvM_TimeBase_Rom[NVM_BLOCK_TIMEBASE_LENGTH];

extern const NvM_BlockDescriptorType NvM_BlockDescriptor[NVM_TOTAL_BLOCKS];

#endif /* NVM_CFG_H */
