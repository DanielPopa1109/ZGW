#ifndef FEE_CFG_H
#define FEE_CFG_H

#include "Std_Types.h"
#include "Fls_Cfg.h"

typedef enum
{
    FEE_BLOCK_NATIVE = 0,
    FEE_BLOCK_REDUNDANT = 1
} Fee_BlockManagementType;

typedef struct
{
    uint16 blockNumber;
    uint16 blockSize;
    Fee_BlockManagementType managementType;
    boolean immediateData;
} Fee_BlockConfigType;

#define FEE_CONFIGURED_BLOCKS               (2u)
#define FEE_MAX_BLOCK_SIZE                  (32u * 1024u)
#define FEE_VIRTUAL_SECTOR_COUNT            (2u)
#define FEE_VIRTUAL_SECTOR_SIZE             (128u * 1024u)
#define FEE_VIRTUAL_SECTOR0_OFFSET          (0u)
#define FEE_VIRTUAL_SECTOR1_OFFSET          (FEE_VIRTUAL_SECTOR_SIZE)
#define FEE_DATAFLASH_SIZE                  (FLS_DFLASH0_TOTAL_SIZE)

extern const Fee_BlockConfigType Fee_BlockConfig[FEE_CONFIGURED_BLOCKS];

#endif /* FEE_CFG_H */
