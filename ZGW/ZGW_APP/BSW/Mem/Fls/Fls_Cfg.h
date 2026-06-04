#ifndef FLS_CFG_H
#define FLS_CFG_H

#include "Std_Types.h"

#define FLS_DFLASH0_BASE_ADDRESS            (0xAF000000u)
#define FLS_DFLASH0_TOTAL_SIZE              (256u * 1024u)
#define FLS_DFLASH0_SECTOR_SIZE             (4u * 1024u)
#define FLS_DFLASH0_PAGE_SIZE               (8u)

#define FLS_ERASED_VALUE                    (0x00u)
#define FLS_FLASH_MODULE                    (0u)
#define FLS_VERIFY_WRITE                    (STD_OFF)
#define FLS_VERIFY_ERASE                    (STD_ON)
#define FLS_DISABLE_INTERRUPTS_FOR_COMMAND  (STD_ON)

#endif /* FLS_CFG_H */
