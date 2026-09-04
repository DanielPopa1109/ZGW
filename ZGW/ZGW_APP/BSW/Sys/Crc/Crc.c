#include "Crc.h"
#include "BSW/Sys/CpuPerf/CpuPerf.h"

#define CRC32_POLY_REFLECTED (0xEDB88320u)
#define CRC32_INIT_VALUE     (0xFFFFFFFFu)

uint32 Crc_CalculateCRC32(const uint8 *Crc_DataPtr,
                          uint32 Crc_Length,
                          uint32 Crc_StartValue32,
                          boolean Crc_IsFirstCall)
{
    CpuPerf_ContextType cpuPerfCtx;
    uint32 crc;
    uint32 i;
    uint8 bit;

    if (Crc_DataPtr == NULL_PTR)
    {
        return 0u;
    }

    CpuPerf_Start(CPUPERF_ID_CRC32, &cpuPerfCtx);
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

    CpuPerf_Stop(CPUPERF_ID_CRC32, &cpuPerfCtx);
    CpuPerf_AddBytes(CPUPERF_ID_CRC32, Crc_Length);
    return ~crc;
}
