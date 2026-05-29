#include "NvM_Cfg.h"

uint8 NvM_AppData_Ram[NVM_BLOCK_APP_DATA_LENGTH];
uint8 NvM_TimeBase_Ram[NVM_BLOCK_TIMEBASE_LENGTH];

const uint8 NvM_AppData_Rom[NVM_BLOCK_APP_DATA_LENGTH] = { 0u };
const uint8 NvM_TimeBase_Rom[NVM_BLOCK_TIMEBASE_LENGTH] =
{
    0x54u, 0x49u, 0x4Du, 0x45u, /* magic: "TIME" */
    0x01u,                         /* version */
    0x01u,                         /* valid */
    0x00u,                         /* source: default ROM time */
    0x00u,                         /* reserved */
    0x18u, 0xB1u, 0x7Fu, 0xCCu, 0x03u, 0xB0u, 0x54u, 0x00u, /* UTC ns */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, /* vehicle ns */
    0x00u, 0x00u, 0x00u, 0x01u, /* sync status: default UTC */
    0x00u, 0x00u, 0x00u, 0x00u  /* store counter */
};

const NvM_BlockDescriptorType NvM_BlockDescriptor[NVM_TOTAL_BLOCKS] =
{
    {
        NVM_BLOCK_ID_DEM_PRIMARY,
        NVM_BLOCK_DEM_PRIMARY_LENGTH,
        (uint8 *)&Dem_NvImage,
        NULL_PTR,
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
