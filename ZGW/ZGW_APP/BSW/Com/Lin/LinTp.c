#include "LinTp.h"
#include "LinIf.h"
#include "PduR.h"
#include <string.h>

#define LINTP_TIMER_N_CR 200u
#define LINTP_TIMER_N_AS 200u

typedef enum
{
    LINTP_IDLE = 0u,
    LINTP_TX_WAIT_FRAME_CONFIRM,
    LINTP_TX_WAIT_NEXT_SLOT,
    LINTP_RX_IN_PROGRESS
} LinTp_InternalStateType;

typedef struct
{
    uint8 nad;

    uint8 txBuf[LINTP_MAX_PAYLOAD];
    PduLengthType txLen;
    PduLengthType txOffset;
    uint8 txSn;
    PduIdType txPduId;
    uint8 txFrame[8];

    PduLengthType rxLen;
    PduLengthType expectedRxLen;
    uint8 rxSn;

    LinTp_InternalStateType state;
    uint16 timer;
} LinTp_StateType;

static LinTp_StateType LinTp_State;

void LinTp_Init(uint8 configuredNad)
{
    memset(&LinTp_State, 0, sizeof(LinTp_State));
    LinTp_State.nad = configuredNad;
    LinTp_State.state = LINTP_IDLE;
}

uint8 LinTp_GetNad(void)
{
    return LinTp_State.nad;
}

Std_ReturnType LinTp_AssignNad(uint8 initialNad, uint16 supplierId, uint16 functionId, uint8 newNad)
{
    (void)supplierId;
    (void)functionId;

    if ((initialNad == LinTp_State.nad) || (initialNad == LINTP_NAD_BROADCAST))
    {
        LinTp_State.nad = newNad;
        return E_OK;
    }

    return E_NOT_OK;
}

static Std_ReturnType LinTp_QueueFrame(const uint8 frame[8])
{
    if (LinIf_SetDiagRequest(frame) != E_OK)
    {
        return E_NOT_OK;
    }

    LinTp_State.state = LINTP_TX_WAIT_FRAME_CONFIRM;
    LinTp_State.timer = LINTP_TIMER_N_AS;

    return E_OK;
}

static void LinTp_BuildNextCf(uint8 frame[8])
{
    PduLengthType rem;
    uint8 copy;

    memset(frame, 0xFF, 8u);

    frame[0] = LinTp_State.nad;
    frame[1] = (uint8)(LINTP_PCI_CF | (LinTp_State.txSn & 0x0Fu));

    rem = (PduLengthType)(LinTp_State.txLen - LinTp_State.txOffset);
    copy = (rem > 6u) ? 6u : (uint8)rem;

    memcpy(&frame[2], &LinTp_State.txBuf[LinTp_State.txOffset], copy);

    LinTp_State.txOffset = (PduLengthType)(LinTp_State.txOffset + copy);
    LinTp_State.txSn = (uint8)((LinTp_State.txSn + 1u) & 0x0Fu);
}

Std_ReturnType LinTp_Transmit(PduIdType TxPduId, const uint8* data, PduLengthType len)
{
    uint8 frame[8];

    if ((data == NULL_PTR) || (len == 0u) || (len > LINTP_MAX_PAYLOAD))
    {
        return E_NOT_OK;
    }

    if (LinTp_State.state != LINTP_IDLE)
    {
        return E_NOT_OK;
    }

    memcpy(LinTp_State.txBuf, data, len);

    LinTp_State.txLen = len;
    LinTp_State.txOffset = 0u;
    LinTp_State.txSn = 1u;
    LinTp_State.txPduId = TxPduId;

    memset(frame, 0xFF, sizeof(frame));
    frame[0] = LinTp_State.nad;

    if (len <= 6u)
    {
        frame[1] = (uint8)(LINTP_PCI_SF | len);
        memcpy(&frame[2], data, len);
        LinTp_State.txOffset = len;
    }
    else
    {
        frame[1] = (uint8)(LINTP_PCI_FF | ((len >> 8u) & 0x0Fu));
        frame[2] = (uint8)(len & 0xFFu);
        memcpy(&frame[3], data, 5u);
        LinTp_State.txOffset = 5u;
    }

    memcpy(LinTp_State.txFrame, frame, 8u);

    return LinTp_QueueFrame(frame);
}

void LinTp_TxFrameConfirmation(uint8 success)
{
    uint8 frame[8];

    if (LinTp_State.state != LINTP_TX_WAIT_FRAME_CONFIRM)
    {
        return;
    }

    if (success == FALSE)
    {
        PduR_LinTpTxConfirmation(LinTp_State.txPduId, E_NOT_OK);
        LinTp_State.state = LINTP_IDLE;
        return;
    }

    if (LinTp_State.txOffset >= LinTp_State.txLen)
    {
        PduR_LinTpTxConfirmation(LinTp_State.txPduId, E_OK);
        LinTp_State.state = LINTP_IDLE;
        return;
    }

    LinTp_BuildNextCf(frame);

    if (LinTp_QueueFrame(frame) != E_OK)
    {
        PduR_LinTpTxConfirmation(LinTp_State.txPduId, E_NOT_OK);
        LinTp_State.state = LINTP_IDLE;
    }
}

void LinTp_MainFunction(void)
{
    if (LinTp_State.state == LINTP_IDLE)
    {
        return;
    }

    if (LinTp_State.timer > 0u)
    {
        LinTp_State.timer--;
        return;
    }

    if (LinTp_State.state == LINTP_TX_WAIT_FRAME_CONFIRM)
    {
        PduR_LinTpTxConfirmation(LinTp_State.txPduId, E_NOT_OK);
        LinTp_State.state = LINTP_IDLE;
    }
    else if (LinTp_State.state == LINTP_RX_IN_PROGRESS)
    {
        PduR_LinTpRxIndication(LINTP_PDUR_ID, E_NOT_OK);
        LinTp_State.state = LINTP_IDLE;
    }
}

static void LinTp_HandleRxFrame(const uint8 frame[8], PduIdType pduRId)
{
    uint8 nad;
    uint8 pci;
    uint8 len;
    uint16 totalLen;
    uint8 sn;
    uint8 copy;
    PduLengthType rem;

    nad = frame[0];

    if ((nad != LinTp_State.nad) &&
        (nad != LINTP_NAD_FUNCTIONAL) &&
        (nad != LINTP_NAD_BROADCAST))
    {
        return;
    }

    pci = frame[1] & 0xF0u;

    if (pci == LINTP_PCI_SF)
    {
        len = frame[1] & 0x0Fu;

        if ((len == 0u) || (len > 6u))
        {
            return;
        }

        if (PduR_LinTpStartOfReception(pduRId, len) != E_OK)
        {
            return;
        }

        if (PduR_LinTpCopyRxData(pduRId, &frame[2], len) != E_OK)
        {
            PduR_LinTpRxIndication(pduRId, E_NOT_OK);
            return;
        }

        PduR_LinTpRxIndication(pduRId, E_OK);
        return;
    }

    if (pci == LINTP_PCI_FF)
    {
        totalLen = (uint16)((((uint16)frame[1] & 0x0Fu) << 8u) | frame[2]);

        if ((totalLen == 0u) || (totalLen > LINTP_MAX_PAYLOAD))
        {
            return;
        }

        if (PduR_LinTpStartOfReception(pduRId, totalLen) != E_OK)
        {
            return;
        }

        copy = (totalLen > 5u) ? 5u : (uint8)totalLen;

        if (PduR_LinTpCopyRxData(pduRId, &frame[3], copy) != E_OK)
        {
            PduR_LinTpRxIndication(pduRId, E_NOT_OK);
            return;
        }

        LinTp_State.expectedRxLen = totalLen;
        LinTp_State.rxLen = copy;
        LinTp_State.rxSn = 1u;
        LinTp_State.state = LINTP_RX_IN_PROGRESS;
        LinTp_State.timer = LINTP_TIMER_N_CR;
        return;
    }

    if (pci == LINTP_PCI_CF)
    {
        if (LinTp_State.state != LINTP_RX_IN_PROGRESS)
        {
            return;
        }

        sn = frame[1] & 0x0Fu;

        if (sn != LinTp_State.rxSn)
        {
            PduR_LinTpRxIndication(pduRId, E_NOT_OK);
            LinTp_State.state = LINTP_IDLE;
            return;
        }

        rem = (PduLengthType)(LinTp_State.expectedRxLen - LinTp_State.rxLen);
        copy = (rem > 6u) ? 6u : (uint8)rem;

        if (PduR_LinTpCopyRxData(pduRId, &frame[2], copy) != E_OK)
        {
            PduR_LinTpRxIndication(pduRId, E_NOT_OK);
            LinTp_State.state = LINTP_IDLE;
            return;
        }

        LinTp_State.rxLen = (PduLengthType)(LinTp_State.rxLen + copy);
        LinTp_State.rxSn = (uint8)((LinTp_State.rxSn + 1u) & 0x0Fu);
        LinTp_State.timer = LINTP_TIMER_N_CR;

        if (LinTp_State.rxLen >= LinTp_State.expectedRxLen)
        {
            PduR_LinTpRxIndication(pduRId, E_OK);
            LinTp_State.state = LINTP_IDLE;
        }
    }
}

void LinTp_RxMasterRequest(const uint8 frame[8])
{
    LinTp_HandleRxFrame(frame, LINTP_PDUR_ID);
}

void LinTp_RxSlaveResponse(const uint8 frame[8])
{
    LinTp_HandleRxFrame(frame, LINTP_PDUR_ID);
}
