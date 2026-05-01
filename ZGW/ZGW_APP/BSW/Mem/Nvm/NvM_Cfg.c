#include "NvM_Cfg.h"

uint8 NvM_Block1_Ram[NVM_BLOCK_1_LENGTH];
uint8 NvM_Block2_Ram[NVM_BLOCK_2_LENGTH];

const uint8 NvM_Block1_Rom[NVM_BLOCK_1_LENGTH] = { 0u };
const uint8 NvM_Block2_Rom[NVM_BLOCK_2_LENGTH] = { 0u };

const NvM_BlockDescriptorType NvM_BlockDescriptor[NVM_TOTAL_BLOCKS] =
{
    {
        NVM_BLOCK_ID_1,
        NVM_BLOCK_1_LENGTH,
        NvM_Block1_Ram,
        NvM_Block1_Rom,
        NVM_BLOCK_NATIVE,
        TRUE,
        FALSE,
        TRUE
    },
    {
        NVM_BLOCK_ID_2,
        NVM_BLOCK_2_LENGTH,
        NvM_Block2_Ram,
        NvM_Block2_Rom,
        NVM_BLOCK_REDUNDANT,
        TRUE,
        FALSE,
        FALSE
    }
};
