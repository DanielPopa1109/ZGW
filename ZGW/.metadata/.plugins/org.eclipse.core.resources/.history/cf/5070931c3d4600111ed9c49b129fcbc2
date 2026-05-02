#include "Com.h"
#include "PduR.h"
#include <string.h>

#define COM_TRUE  1u
#define COM_FALSE 0u

#define COM_TX_PDU_APP_STATUS  2u
#define COM_RX_PDU_APP_STATUS  0u

typedef struct
{
    PduIdType pduId;
    Com_IpduGroupIdType groupId;
    uint8 txMode;
    uint16 periodTicks;
    uint16 mdtTicks;
    uint8 repetitions;
    uint16 repetitionPeriodTicks;
    uint16 deadlineTicks;
    uint8 len;
    uint8 init[COM_MAX_IPDU_LEN];
} Com_TxIpduConfigType;

typedef struct
{
    PduIdType pduId;
    Com_IpduGroupIdType groupId;
    uint16 deadlineTicks;
    uint8 len;
    uint8 init[COM_MAX_IPDU_LEN];
} Com_RxIpduConfigType;

typedef struct
{
    Com_SignalIdType signalId;
    uint8 isTx;
    PduIdType pduId;
    uint16 bitPosition;
    uint8 bitSize;
    uint8 dataSize;
    uint8 hasUpdateBit;
    uint16 updateBitPosition;
    uint32 invalidValue;
    uint32 initValue;
} Com_SignalConfigType;

typedef struct
{
    uint8 buffer[COM_MAX_IPDU_LEN];
    uint8 active;
    uint8 dirty;
    uint8 txInProgress;
    uint16 periodTimer;
    uint16 mdtTimer;
    uint8 repetitionsLeft;
    uint16 repetitionTimer;
    uint16 deadlineTimer;
} Com_TxIpduRuntimeType;

typedef struct
{
    uint8 buffer[COM_MAX_IPDU_LEN];
    uint8 active;
    uint16 deadlineTimer;
    uint8 timeout;
} Com_RxIpduRuntimeType;

static const Com_TxIpduConfigType Com_TxIpduCfg[] =
{
    {
        COM_TX_PDU_APP_STATUS,
        COM_IPDU_GROUP_0,
        COM_TX_MODE_MIXED,
        20u,
        2u,
        3u,
        2u,
        100u,
        8u,
        {0u}
    }
};

static const Com_RxIpduConfigType Com_RxIpduCfg[] =
{
    {
        COM_RX_PDU_APP_STATUS,
        COM_IPDU_GROUP_0,
        100u,
        8u,
        {0u}
    }
};

static const Com_SignalConfigType Com_SignalCfg[] =
{
    { 0u, COM_TRUE,  COM_TX_PDU_APP_STATUS, 0u,  8u,  COM_SIGNAL_U8,  COM_TRUE,  63u, 0xFFu,       0u },
    { 1u, COM_TRUE,  COM_TX_PDU_APP_STATUS, 8u,  16u, COM_SIGNAL_U16, COM_FALSE, 0u,  0xFFFFu,     0u },
    { 2u, COM_TRUE,  COM_TX_PDU_APP_STATUS, 24u, 32u, COM_SIGNAL_U32, COM_FALSE, 0u,  0xFFFFFFFFu, 0u },

    { 10u, COM_FALSE, COM_RX_PDU_APP_STATUS, 0u,  8u,  COM_SIGNAL_U8,  COM_TRUE,  63u, 0xFFu,       0u },
    { 11u, COM_FALSE, COM_RX_PDU_APP_STATUS, 8u,  16u, COM_SIGNAL_U16, COM_FALSE, 0u,  0xFFFFu,     0u },
    { 12u, COM_FALSE, COM_RX_PDU_APP_STATUS, 24u, 32u, COM_SIGNAL_U32, COM_FALSE, 0u,  0xFFFFFFFFu, 0u }
};

static Com_TxIpduRuntimeType Com_TxRt[sizeof(Com_TxIpduCfg) / sizeof(Com_TxIpduCfg[0])];
static Com_RxIpduRuntimeType Com_RxRt[sizeof(Com_RxIpduCfg) / sizeof(Com_RxIpduCfg[0])];

static uint8 Com_FindTxIpdu(PduIdType pduId)
{
    uint8 i;

    for (i = 0u; i < (uint8)(sizeof(Com_TxIpduCfg) / sizeof(Com_TxIpduCfg[0])); i++)
    {
        if (Com_TxIpduCfg[i].pduId == pduId)
        {
            return i;
        }
    }

    return 0xFFu;
}

static uint8 Com_FindRxIpdu(PduIdType pduId)
{
    uint8 i;

    for (i = 0u; i < (uint8)(sizeof(Com_RxIpduCfg) / sizeof(Com_RxIpduCfg[0])); i++)
    {
        if (Com_RxIpduCfg[i].pduId == pduId)
        {
            return i;
        }
    }

    return 0xFFu;
}

static const Com_SignalConfigType* Com_FindSignal(Com_SignalIdType signalId)
{
    uint8 i;

    for (i = 0u; i < (uint8)(sizeof(Com_SignalCfg) / sizeof(Com_SignalCfg[0])); i++)
    {
        if (Com_SignalCfg[i].signalId == signalId)
        {
            return &Com_SignalCfg[i];
        }
    }

    return NULL_PTR;
}

static void Com_WriteBits(uint8* buffer, uint16 bitPos, uint8 bitSize, uint32 value)
{
    uint8 i;
    uint16 bitIndex;
    uint8 byteIndex;
    uint8 bitInByte;

    for (i = 0u; i < bitSize; i++)
    {
        bitIndex = (uint16)(bitPos + i);
        byteIndex = (uint8)(bitIndex / 8u);
        bitInByte = (uint8)(bitIndex % 8u);

        if (((value >> i) & 1u) != 0u)
        {
            buffer[byteIndex] |= (uint8)(1u << bitInByte);
        }
        else
        {
            buffer[byteIndex] &= (uint8)~(uint8)(1u << bitInByte);
        }
    }
}

static uint32 Com_ReadBits(const uint8* buffer, uint16 bitPos, uint8 bitSize)
{
    uint8 i;
    uint32 value = 0u;
    uint16 bitIndex;
    uint8 byteIndex;
    uint8 bitInByte;

    for (i = 0u; i < bitSize; i++)
    {
        bitIndex = (uint16)(bitPos + i);
        byteIndex = (uint8)(bitIndex / 8u);
        bitInByte = (uint8)(bitIndex % 8u);

        if ((buffer[byteIndex] & (uint8)(1u << bitInByte)) != 0u)
        {
            value |= (uint32)(1UL << i);
        }
    }

    return value;
}

static uint32 Com_LoadValue(const void* ptr, uint8 size)
{
    uint32 value = 0u;

    if (size == COM_SIGNAL_U8)
    {
        value = *((const uint8*)ptr);
    }
    else if (size == COM_SIGNAL_U16)
    {
        value = *((const uint16*)ptr);
    }
    else if (size == COM_SIGNAL_U32)
    {
        value = *((const uint32*)ptr);
    }

    return value;
}

static void Com_StoreValue(void* ptr, uint8 size, uint32 value)
{
    if (size == COM_SIGNAL_U8)
    {
        *((uint8*)ptr) = (uint8)value;
    }
    else if (size == COM_SIGNAL_U16)
    {
        *((uint16*)ptr) = (uint16)value;
    }
    else if (size == COM_SIGNAL_U32)
    {
        *((uint32*)ptr) = value;
    }
}

static Std_ReturnType Com_TriggerTransmit(uint8 txIdx)
{
    const Com_TxIpduConfigType* cfg;

    if (txIdx >= (uint8)(sizeof(Com_TxIpduCfg) / sizeof(Com_TxIpduCfg[0])))
    {
        return E_NOT_OK;
    }

    cfg = &Com_TxIpduCfg[txIdx];

    if (Com_TxRt[txIdx].active == COM_FALSE)
    {
        return E_NOT_OK;
    }

    if (Com_TxRt[txIdx].txInProgress != COM_FALSE)
    {
        return E_NOT_OK;
    }

    if (Com_TxRt[txIdx].mdtTimer > 0u)
    {
        return E_NOT_OK;
    }

    if (PduR_ComTransmit(cfg->pduId, Com_TxRt[txIdx].buffer, cfg->len) == E_OK)
    {
        Com_TxRt[txIdx].txInProgress = COM_TRUE;
        Com_TxRt[txIdx].mdtTimer = cfg->mdtTicks;
        Com_TxRt[txIdx].deadlineTimer = cfg->deadlineTicks;
        return E_OK;
    }

    return E_NOT_OK;
}

void Com_Init(void)
{
    uint8 i;

    memset(Com_TxRt, 0, sizeof(Com_TxRt));
    memset(Com_RxRt, 0, sizeof(Com_RxRt));

    for (i = 0u; i < (uint8)(sizeof(Com_TxIpduCfg) / sizeof(Com_TxIpduCfg[0])); i++)
    {
        memcpy(Com_TxRt[i].buffer, Com_TxIpduCfg[i].init, Com_TxIpduCfg[i].len);
        Com_TxRt[i].periodTimer = Com_TxIpduCfg[i].periodTicks;
        Com_TxRt[i].deadlineTimer = Com_TxIpduCfg[i].deadlineTicks;
        Com_TxRt[i].active = COM_TRUE;
    }

    for (i = 0u; i < (uint8)(sizeof(Com_RxIpduCfg) / sizeof(Com_RxIpduCfg[0])); i++)
    {
        memcpy(Com_RxRt[i].buffer, Com_RxIpduCfg[i].init, Com_RxIpduCfg[i].len);
        Com_RxRt[i].deadlineTimer = Com_RxIpduCfg[i].deadlineTicks;
        Com_RxRt[i].active = COM_TRUE;
    }
}

Std_ReturnType Com_IpduGroupStart(Com_IpduGroupIdType groupId)
{
    uint8 i;

    for (i = 0u; i < (uint8)(sizeof(Com_TxIpduCfg) / sizeof(Com_TxIpduCfg[0])); i++)
    {
        if (Com_TxIpduCfg[i].groupId == groupId)
        {
            Com_TxRt[i].active = COM_TRUE;
        }
    }

    for (i = 0u; i < (uint8)(sizeof(Com_RxIpduCfg) / sizeof(Com_RxIpduCfg[0])); i++)
    {
        if (Com_RxIpduCfg[i].groupId == groupId)
        {
            Com_RxRt[i].active = COM_TRUE;
        }
    }

    return E_OK;
}

Std_ReturnType Com_IpduGroupStop(Com_IpduGroupIdType groupId)
{
    uint8 i;

    for (i = 0u; i < (uint8)(sizeof(Com_TxIpduCfg) / sizeof(Com_TxIpduCfg[0])); i++)
    {
        if (Com_TxIpduCfg[i].groupId == groupId)
        {
            Com_TxRt[i].active = COM_FALSE;
        }
    }

    for (i = 0u; i < (uint8)(sizeof(Com_RxIpduCfg) / sizeof(Com_RxIpduCfg[0])); i++)
    {
        if (Com_RxIpduCfg[i].groupId == groupId)
        {
            Com_RxRt[i].active = COM_FALSE;
        }
    }

    return E_OK;
}

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    const Com_SignalConfigType* sig;
    uint8 txIdx;
    uint32 value;

    if (SignalDataPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    sig = Com_FindSignal(SignalId);

    if ((sig == NULL_PTR) || (sig->isTx == COM_FALSE))
    {
        return E_NOT_OK;
    }

    txIdx = Com_FindTxIpdu(sig->pduId);

    if ((txIdx == 0xFFu) || (Com_TxRt[txIdx].active == COM_FALSE))
    {
        return E_NOT_OK;
    }

    value = Com_LoadValue(SignalDataPtr, sig->dataSize);
    Com_WriteBits(Com_TxRt[txIdx].buffer, sig->bitPosition, sig->bitSize, value);

    if (sig->hasUpdateBit != COM_FALSE)
    {
        Com_WriteBits(Com_TxRt[txIdx].buffer, sig->updateBitPosition, 1u, 1u);
    }

    Com_TxRt[txIdx].dirty = COM_TRUE;

    if ((Com_TxIpduCfg[txIdx].txMode == COM_TX_MODE_DIRECT) ||
        (Com_TxIpduCfg[txIdx].txMode == COM_TX_MODE_MIXED))
    {
        Com_TxRt[txIdx].repetitionsLeft = Com_TxIpduCfg[txIdx].repetitions;
        Com_TxRt[txIdx].repetitionTimer = 0u;
        (void)Com_TriggerTransmit(txIdx);
    }

    return E_OK;
}

Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    const Com_SignalConfigType* sig;
    uint8 rxIdx;
    uint32 value;

    if (SignalDataPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    sig = Com_FindSignal(SignalId);

    if ((sig == NULL_PTR) || (sig->isTx != COM_FALSE))
    {
        return E_NOT_OK;
    }

    rxIdx = Com_FindRxIpdu(sig->pduId);

    if ((rxIdx == 0xFFu) || (Com_RxRt[rxIdx].active == COM_FALSE))
    {
        return E_NOT_OK;
    }

    if (Com_RxRt[rxIdx].timeout != COM_FALSE)
    {
        return E_NOT_OK;
    }

    if (sig->hasUpdateBit != COM_FALSE)
    {
        if (Com_ReadBits(Com_RxRt[rxIdx].buffer, sig->updateBitPosition, 1u) == 0u)
        {
            return E_NOT_OK;
        }
    }

    value = Com_ReadBits(Com_RxRt[rxIdx].buffer, sig->bitPosition, sig->bitSize);
    Com_StoreValue(SignalDataPtr, sig->dataSize, value);

    return E_OK;
}

Std_ReturnType Com_InvalidateSignal(Com_SignalIdType SignalId)
{
    const Com_SignalConfigType* sig;
    uint8 txIdx;

    sig = Com_FindSignal(SignalId);

    if ((sig == NULL_PTR) || (sig->isTx == COM_FALSE))
    {
        return E_NOT_OK;
    }

    txIdx = Com_FindTxIpdu(sig->pduId);

    if (txIdx == 0xFFu)
    {
        return E_NOT_OK;
    }

    Com_WriteBits(Com_TxRt[txIdx].buffer, sig->bitPosition, sig->bitSize, sig->invalidValue);
    Com_TxRt[txIdx].dirty = COM_TRUE;

    if (sig->hasUpdateBit != COM_FALSE)
    {
        Com_WriteBits(Com_TxRt[txIdx].buffer, sig->updateBitPosition, 1u, 1u);
    }

    return Com_TriggerTransmit(txIdx);
}

void Com_RxIndication(PduIdType RxPduId, const uint8* data, PduLengthType len)
{
    uint8 rxIdx;

    if ((data == NULL_PTR) || (len == 0u))
    {
        return;
    }

    rxIdx = Com_FindRxIpdu(RxPduId);

    if (rxIdx == 0xFFu)
    {
        return;
    }

    if (Com_RxRt[rxIdx].active == COM_FALSE)
    {
        return;
    }

    if (len > Com_RxIpduCfg[rxIdx].len)
    {
        len = Com_RxIpduCfg[rxIdx].len;
    }

    memcpy(Com_RxRt[rxIdx].buffer, data, len);
    Com_RxRt[rxIdx].deadlineTimer = Com_RxIpduCfg[rxIdx].deadlineTicks;
    Com_RxRt[rxIdx].timeout = COM_FALSE;
}

void Com_TxConfirmation(PduIdType TxPduId)
{
    uint8 txIdx;

    txIdx = Com_FindTxIpdu(TxPduId);

    if (txIdx == 0xFFu)
    {
        return;
    }

    Com_TxRt[txIdx].txInProgress = COM_FALSE;
    Com_TxRt[txIdx].dirty = COM_FALSE;
}

void Com_MainFunctionTx(void)
{
    uint8 i;

    for (i = 0u; i < (uint8)(sizeof(Com_TxIpduCfg) / sizeof(Com_TxIpduCfg[0])); i++)
    {
        if (Com_TxRt[i].active == COM_FALSE)
        {
            continue;
        }

        if (Com_TxRt[i].mdtTimer > 0u)
        {
            Com_TxRt[i].mdtTimer--;
        }

        if (Com_TxRt[i].deadlineTimer > 0u)
        {
            Com_TxRt[i].deadlineTimer--;
        }
        else if (Com_TxRt[i].txInProgress != COM_FALSE)
        {
            Com_TxRt[i].txInProgress = COM_FALSE;
        }

        if (Com_TxRt[i].repetitionTimer > 0u)
        {
            Com_TxRt[i].repetitionTimer--;
        }

        if ((Com_TxRt[i].repetitionsLeft > 0u) &&
            (Com_TxRt[i].repetitionTimer == 0u) &&
            (Com_TxRt[i].txInProgress == COM_FALSE))
        {
            if (Com_TriggerTransmit(i) == E_OK)
            {
                Com_TxRt[i].repetitionsLeft--;
                Com_TxRt[i].repetitionTimer = Com_TxIpduCfg[i].repetitionPeriodTicks;
            }
        }

        if ((Com_TxIpduCfg[i].txMode == COM_TX_MODE_PERIODIC) ||
            (Com_TxIpduCfg[i].txMode == COM_TX_MODE_MIXED))
        {
            if (Com_TxRt[i].periodTimer > 0u)
            {
                Com_TxRt[i].periodTimer--;
            }
            else
            {
                (void)Com_TriggerTransmit(i);
                Com_TxRt[i].periodTimer = Com_TxIpduCfg[i].periodTicks;
            }
        }

        if ((Com_TxRt[i].dirty != COM_FALSE) &&
            (Com_TxRt[i].txInProgress == COM_FALSE) &&
            (Com_TxRt[i].mdtTimer == 0u))
        {
            (void)Com_TriggerTransmit(i);
        }
    }
}

void Com_MainFunctionRx(void)
{
    uint8 i;

    for (i = 0u; i < (uint8)(sizeof(Com_RxIpduCfg) / sizeof(Com_RxIpduCfg[0])); i++)
    {
        if (Com_RxRt[i].active == COM_FALSE)
        {
            continue;
        }

        if (Com_RxIpduCfg[i].deadlineTicks == 0u)
        {
            continue;
        }

        if (Com_RxRt[i].deadlineTimer > 0u)
        {
            Com_RxRt[i].deadlineTimer--;
        }
        else
        {
            Com_RxRt[i].timeout = COM_TRUE;
            memcpy(Com_RxRt[i].buffer, Com_RxIpduCfg[i].init, Com_RxIpduCfg[i].len);
        }
    }
}
