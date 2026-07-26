#ifndef FBLRAM_LWIPHOOKS_H
#define FBLRAM_LWIPHOOKS_H

#include "Ifx_Types.h"

void *FblRam_Memcpy(void *destination, const void *source, uint32 length);
void *FblRam_Memset(void *destination, uint8 value, uint32 length);
void *FblRam_Memmove(void *destination, const void *source, uint32 length);
sint32 FblRam_Memcmp(const void *a, const void *b, uint32 length);
uint32 FblRam_Strlen(const char *text);

#define MEMCPY(dst, src, len)  FblRam_Memcpy((dst), (src), (uint32)(len))
#define SMEMCPY(dst, src, len) FblRam_Memcpy((dst), (src), (uint32)(len))
#define MEMSET(dst, val, len)  FblRam_Memset((dst), (uint8)(val), (uint32)(len))
#define MEMMOVE(dst, src, len) FblRam_Memmove((dst), (src), (uint32)(len))
#define MEMCMP(a, b, len)      FblRam_Memcmp((a), (b), (uint32)(len))
#define STRLEN(text)           FblRam_Strlen((text))

#endif
