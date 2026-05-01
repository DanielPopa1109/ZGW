#include "Fee_Cfg.h"

const Fee_BlockConfigType Fee_BlockConfig[FEE_CONFIGURED_BLOCKS] =
{
    { 1u, 1024u, FEE_BLOCK_NATIVE,    TRUE  },
    { 2u, 2048u, FEE_BLOCK_REDUNDANT, FALSE }
};
