#include "Fee_Cfg.h"
#include "NvM_Cfg.h"

const Fee_BlockConfigType Fee_BlockConfig[FEE_CONFIGURED_BLOCKS] =
{
    {
        NVM_BLOCK_ID_DEM_PRIMARY,
        NVM_BLOCK_DEM_PRIMARY_LENGTH,
        FEE_BLOCK_NATIVE,
        FALSE
    },
    {
        NVM_BLOCK_ID_APP_DATA,
        NVM_BLOCK_APP_DATA_LENGTH,
        FEE_BLOCK_REDUNDANT,
        FALSE
    }
};
