#include "NvM_Cfg.h"

uint8 NvM_AppData_Ram[NVM_BLOCK_APP_DATA_LENGTH];
uint8 NvM_TimeBase_Ram[NVM_BLOCK_TIMEBASE_LENGTH];

const uint8 NvM_DemPrimary_Rom[NVM_BLOCK_DEM_PRIMARY_LENGTH] = { 0u };
const uint8 NvM_AppData_Rom[NVM_BLOCK_APP_DATA_LENGTH] = { 0u };
const uint8 NvM_TimeBase_Rom[NVM_BLOCK_TIMEBASE_LENGTH] = { 0u };

const NvM_BlockDescriptorType NvM_BlockDescriptor[NVM_TOTAL_BLOCKS] =
{
    {
        NVM_BLOCK_ID_DEM_PRIMARY,
        NVM_BLOCK_DEM_PRIMARY_LENGTH,
        (uint8 *)&Dem_NvImage,
        NvM_DemPrimary_Rom,
        NVM_BLOCK_NATIVE,
        TRUE,
        FALSE,
        FALSE
    },
    {
        NVM_BLOCK_ID_APP_DATA,
        NVM_BLOCK_APP_DATA_LENGTH,
        NvM_AppData_Ram,
        NvM_AppData_Rom,
        NVM_BLOCK_REDUNDANT,
        TRUE,
        FALSE,
        FALSE
    },
    {
        NVM_BLOCK_ID_TIMEBASE,
        NVM_BLOCK_TIMEBASE_LENGTH,
        NvM_TimeBase_Ram,
        NvM_TimeBase_Rom,
        NVM_BLOCK_REDUNDANT,
        TRUE,
        FALSE,
        FALSE
    }
};
