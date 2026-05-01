#include "Crc.h"

#define CRC32_POLY_REFLECTED (0xEDB88320u)
#define CRC32_INIT_VALUE     (0xFFFFFFFFu)

uint32 Crc_CalculateCRC32(const uint8 *Crc_DataPtr,
                          uint32 Crc_Length,
                          uint32 Crc_StartValue32,
                          boolean Crc_IsFirstCall)
{
    uint32 crc;
    uint32 i;
    uint8 bit;

    if (Crc_DataPtr == NULL_PTR)
    {
        return 0u;
    }

    crc = (Crc_IsFirstCall == TRUE) ? CRC32_INIT_VALUE : (~Crc_StartValue32);

    for (i = 0u; i < Crc_Length; i++)
    {
        crc ^= (uint32)Crc_DataPtr[i];
        for (bit = 0u; bit < 8u; bit++)
        {
            if ((crc & 1u) != 0u)
            {
                crc = (crc >> 1u) ^ CRC32_POLY_REFLECTED;
            }
            else
            {
                crc >>= 1u;
            }
        }
    }

    return ~crc;
}
