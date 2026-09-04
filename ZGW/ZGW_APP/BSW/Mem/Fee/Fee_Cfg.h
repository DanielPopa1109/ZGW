#ifndef FEE_CFG_H
#define FEE_CFG_H

#include "Std_Types.h"
#include "Fls_Cfg.h"
#include "Dem_Int.h"
#include "APP/CodingApp/CodingApp.h"
#include "APP/TimeSync/TimeBase.h"
#include "APP/GatewaySwc/GatewaySwc.h"
#include "APP/AiModel/AiModel.h"
#include "BSW/Com/Ethernet/EthStartupTiming.h"
#include "BSW/Mem/Nvm/NvMTiming.h"
#include "BSW/Mem/Nvm/NvMStats.h"

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

#define FEE_CONFIGURED_BLOCKS               (8u)
#define FEE_ALIGN8_SIZE(size)               (((size) + 7u) & ~7u)
#define FEE_MAX2(a, b)                      (((a) > (b)) ? (a) : (b))
#define FEE_BLOCK_DEM_PRIMARY_SIZE          ((uint32)sizeof(Dem_NvImageType))
#define FEE_BLOCK_APP_DATA_SIZE             CODINGAPP_NVM_IMAGE_SIZE
#define FEE_BLOCK_APP_DATA_LEGACY_NUMBER    (2u)
#define FEE_BLOCK_APP_DATA_LEGACY_SIZE      (64u)
#define FEE_BLOCK_TIMEBASE_SIZE             TIMEBASE_NVM_IMAGE_SIZE
#define FEE_BLOCK_ETH_STARTUP_TIMING_SIZE   ETHSTARTUPTIMING_NVM_IMAGE_SIZE
#define FEE_BLOCK_NVM_TIMING_SIZE           NVMTIMING_NVM_IMAGE_SIZE
#define FEE_BLOCK_MCU_STATUS_SIZE           GATEWAYSWC_MCU_STATUS_NVM_IMAGE_SIZE
#define FEE_BLOCK_AIMODEL_SIZE              AIMODEL_NVM_IMAGE_SIZE
#define FEE_BLOCK_NVM_STATS_SIZE            NVMSTATS_NVM_IMAGE_SIZE
#define FEE_MAX_CONFIGURED_BLOCK_SIZE       FEE_MAX2(FEE_BLOCK_DEM_PRIMARY_SIZE, \
                                                     FEE_MAX2(FEE_BLOCK_APP_DATA_SIZE, \
                                                              FEE_MAX2(FEE_BLOCK_TIMEBASE_SIZE, \
                                                                       FEE_MAX2(FEE_BLOCK_ETH_STARTUP_TIMING_SIZE, \
                                                                                FEE_MAX2(FEE_BLOCK_NVM_TIMING_SIZE, \
                                                                                         FEE_MAX2(FEE_BLOCK_MCU_STATUS_SIZE, \
                                                                                                  FEE_MAX2(FEE_BLOCK_AIMODEL_SIZE, \
                                                                                                           FEE_BLOCK_NVM_STATS_SIZE)))))))
#define FEE_MAX_BLOCK_SIZE                  FEE_ALIGN8_SIZE(FEE_MAX_CONFIGURED_BLOCK_SIZE)
#define FEE_VIRTUAL_SECTOR_COUNT            (2u)
#define FEE_VIRTUAL_SECTOR_SIZE             (128u * 1024u)
#define FEE_VIRTUAL_SECTOR0_OFFSET          (0u)
#define FEE_VIRTUAL_SECTOR1_OFFSET          (FEE_VIRTUAL_SECTOR_SIZE)
#define FEE_DATAFLASH_SIZE                  (FLS_DFLASH0_TOTAL_SIZE)

extern const Fee_BlockConfigType Fee_BlockConfig[FEE_CONFIGURED_BLOCKS];

#endif /* FEE_CFG_H */
