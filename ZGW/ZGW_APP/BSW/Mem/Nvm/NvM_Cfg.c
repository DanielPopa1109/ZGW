#include "NvM_Cfg.h"

uint8 NvM_AppData_Ram[NVM_BLOCK_APP_DATA_LENGTH];
uint8 NvM_TimeBase_Ram[NVM_BLOCK_TIMEBASE_LENGTH];

const uint8 NvM_AppData_Rom[NVM_BLOCK_APP_DATA_LENGTH] = { 0u };
const uint8 NvM_TimeBase_Rom[TIMEBASE_NVM_IMAGE_SIZE] =
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
        NVM_BLOCK_REDUNDANT,
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

/*
 * Compile-time consistency guards.
 *
 * Every layer that records a block's size must agree, so each block occupies
 * exactly the size of the data it stores and a layout change made in one image
 * is reflected everywhere - the NvM length, the Fee block size, and the RAM
 * mirror / ROM default can never silently diverge (no mismatch).
 */
#define NVM_ASSERT_CONCAT_(a, b) a##b
#define NVM_ASSERT_CONCAT(a, b)  NVM_ASSERT_CONCAT_(a, b)
#define NVM_STATIC_ASSERT(cond) \
    typedef char NVM_ASSERT_CONCAT(NvM_StaticAssert_, __LINE__)[(cond) ? 1 : -1]

/* NvM block length == exact size of the data image it stores. */
NVM_STATIC_ASSERT(NVM_BLOCK_DEM_PRIMARY_LENGTH == sizeof(Dem_NvImageType));
NVM_STATIC_ASSERT(NVM_BLOCK_APP_DATA_LENGTH == sizeof(CodingApp_NvImageType));
NVM_STATIC_ASSERT(NVM_BLOCK_TIMEBASE_LENGTH == TIMEBASE_NVM_IMAGE_SIZE);

/* NvM length == the Fee block size handed to the lower layer. */
NVM_STATIC_ASSERT(NVM_BLOCK_DEM_PRIMARY_LENGTH == FEE_BLOCK_DEM_PRIMARY_SIZE);
NVM_STATIC_ASSERT(NVM_BLOCK_APP_DATA_LENGTH == FEE_BLOCK_APP_DATA_SIZE);
NVM_STATIC_ASSERT(NVM_BLOCK_TIMEBASE_LENGTH == FEE_BLOCK_TIMEBASE_SIZE);

/* RAM mirror and ROM default occupy exactly the block length. */
NVM_STATIC_ASSERT(sizeof(NvM_AppData_Ram) == NVM_BLOCK_APP_DATA_LENGTH);
NVM_STATIC_ASSERT(sizeof(NvM_AppData_Rom) == NVM_BLOCK_APP_DATA_LENGTH);
NVM_STATIC_ASSERT(sizeof(NvM_TimeBase_Ram) == NVM_BLOCK_TIMEBASE_LENGTH);
NVM_STATIC_ASSERT(sizeof(NvM_TimeBase_Rom) == NVM_BLOCK_TIMEBASE_LENGTH);

/* Every block fits the shared maximum used to size the Fee/NvM buffers. */
NVM_STATIC_ASSERT(NVM_BLOCK_DEM_PRIMARY_LENGTH <= FEE_MAX_BLOCK_SIZE);
NVM_STATIC_ASSERT(NVM_BLOCK_APP_DATA_LENGTH <= FEE_MAX_BLOCK_SIZE);
NVM_STATIC_ASSERT(NVM_BLOCK_TIMEBASE_LENGTH <= FEE_MAX_BLOCK_SIZE);
NVM_STATIC_ASSERT(NVM_MAX_BLOCK_LENGTH == FEE_MAX_BLOCK_SIZE);
