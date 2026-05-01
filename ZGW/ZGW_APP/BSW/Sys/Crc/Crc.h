#ifndef CRC_H
#define CRC_H

#include "Std_Types.h"
#include "MemStack_Cfg.h"

#define CRC_MODULE_ID                       (MEMSTACK_MODULE_ID_CRC)
#define CRC_VENDOR_ID                       (0u)
#define CRC_SW_MAJOR_VERSION                (1u)
#define CRC_SW_MINOR_VERSION                (0u)
#define CRC_SW_PATCH_VERSION                (0u)

uint32 Crc_CalculateCRC32(const uint8 *Crc_DataPtr,
                          uint32 Crc_Length,
                          uint32 Crc_StartValue32,
                          boolean Crc_IsFirstCall);

#endif /* CRC_H */
