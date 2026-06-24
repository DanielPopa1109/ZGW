#include "LinTp.h"
#include "LinIf.h"
#include "PduR.h"
#include <string.h>

#define LINTP_TIMER_N_CR 200u
#define LINTP_TIMER_N_AS 200u
#define LINTP_CONFIGURED_NODE_COUNT 4u
#define LINTP_PENDING_TX_DEPTH 4u

typedef enum
{
    LINTP_IDLE = 0u,
    LINTP_TX_WAIT_FRAME_CONFIRM,
    LINTP_TX_WAIT_NEXT_SLOT,
    LINTP_RX_IN_PROGRESS
} LinTp_InternalStateType;

typedef struct
{
    uint8 activeTargetNad;
    uint8 txNad;

    uint8 txBuf[LINTP_MAX_PAYLOAD];
    PduLengthType txLen;
    PduLengthType txOffset;
    uint8 txSn;
    PduIdType txPduId;
    uint8 txFrame[8];

    PduLengthType rxLen;
    PduLengthType expectedRxLen;
    uint8 rxSn;
    PduIdType rxPduId;

    LinTp_InternalStateType state;
    uint16 timer;
} LinTp_StateType;

typedef struct
{
    uint8 initialNad;
    uint8 currentNad;
    uint16 supplierId;
    uint16 functionId;
} LinTp_NodeType;

typedef struct
{
    uint8 valid;
    PduIdType txPduId;
    uint8 targetNad;
    PduLengthType len;
    uint8 data[LINTP_MAX_PAYLOAD];
} LinTp_PendingTxType;

static LinTp_StateType LinTp_State;
static LinTp_NodeType LinTp_ConfiguredNodes[LINTP_CONFIGURED_NODE_COUNT];
static LinTp_PendingTxType LinTp_PendingTx[LINTP_PENDING_TX_DEPTH];

long long LinTp_MainFunction_Counter = 0;

static Std_ReturnType LinTp_StartTransmitToNad(PduIdType TxPduId, uint8 targetNad, const uint8* data, PduLengthType len);
static void LinTp_StartNextPendingTx(void);

static void LinTp_InitConfiguredNodes(uint8 configuredNad)
{
    if ((configuredNad == 0u) ||
        (configuredNad >= LINTP_NAD_FUNCTIONAL) ||
        (configuredNad == LINIF_NAD_ALT) ||
        (configuredNad == LINIF_NAD_PCU48) ||
        (configuredNad == LINIF_NAD_HVDCDC))
    {
        configuredNad = LINTP_NAD_ZGW_DEFAULT;
    }

    LinTp_ConfiguredNodes[0u].initialNad = configuredNad;
    LinTp_ConfiguredNodes[0u].currentNad = configuredNad;
    LinTp_ConfiguredNodes[0u].supplierId = LINTP_SUPPLIER_ID;
    LinTp_ConfiguredNodes[0u].functionId = LINTP_FUNCTION_ID;

    LinTp_ConfiguredNodes[1u].initialNad = LINIF_NAD_ALT;
    LinTp_ConfiguredNodes[1u].currentNad = LINIF_NAD_ALT;
    LinTp_ConfiguredNodes[1u].supplierId = LINTP_SUPPLIER_ID;
    LinTp_ConfiguredNodes[1u].functionId = LINTP_FUNCTION_ID;

    LinTp_ConfiguredNodes[2u].initialNad = LINIF_NAD_PCU48;
    LinTp_ConfiguredNodes[2u].currentNad = LINIF_NAD_PCU48;
    LinTp_ConfiguredNodes[2u].supplierId = LINTP_SUPPLIER_ID;
    LinTp_ConfiguredNodes[2u].functionId = LINTP_FUNCTION_ID;

    LinTp_ConfiguredNodes[3u].initialNad = LINIF_NAD_HVDCDC;
    LinTp_ConfiguredNodes[3u].currentNad = LINIF_NAD_HVDCDC;
    LinTp_ConfiguredNodes[3u].supplierId = LINTP_SUPPLIER_ID;
    LinTp_ConfiguredNodes[3u].functionId = LINTP_FUNCTION_ID;
}

static sint16 LinTp_FindConfiguredNad(uint8 nad)
{
    uint8 idx;

    for (idx = 0u; idx < LINTP_CONFIGURED_NODE_COUNT; idx++)
    {
        if (LinTp_ConfiguredNodes[idx].currentNad == nad)
        {
            return (sint16)idx;
        }
    }

    return -1;
}

uint8 LinTp_IsConfiguredNad(uint8 nad)
{
    return (LinTp_FindConfiguredNad(nad) >= 0) ? TRUE : FALSE;
}

void LinTp_Init(uint8 configuredNad)
{
    memset(&LinTp_State, 0, sizeof(LinTp_State));
    memset(LinTp_PendingTx, 0, sizeof(LinTp_PendingTx));
    LinTp_InitConfiguredNodes(configuredNad);
    LinTp_State.activeTargetNad = LinTp_ConfiguredNodes[0u].currentNad;
    LinTp_State.txNad = LinTp_State.activeTargetNad;
    LinTp_State.state = LINTP_IDLE;
}

uint8 LinTp_GetNad(void)
{
    return LinTp_State.activeTargetNad;
}

uint8 LinTp_GetTargetNad(void)
{
    return LinTp_State.activeTargetNad;
}

Std_ReturnType LinTp_SetTargetNad(uint8 targetNad)
{
    if ((LinTp_State.state != LINTP_IDLE) || (LinTp_IsConfiguredNad(targetNad) == FALSE))
    {
        return E_NOT_OK;
    }

    LinTp_State.activeTargetNad = targetNad;
    return E_OK;
}

Std_ReturnType LinTp_AssignNad(uint8 initialNad, uint16 supplierId, uint16 functionId, uint8 newNad)
{
    uint8 idx;
    uint8 conflictIdx;

    if ((newNad == 0u) || (newNad >= LINTP_NAD_FUNCTIONAL))
    {
        return E_NOT_OK;
    }

    for (idx = 0u; idx < LINTP_CONFIGURED_NODE_COUNT; idx++)
    {
        if ((LinTp_ConfiguredNodes[idx].initialNad == initialNad) &&
            (LinTp_ConfiguredNodes[idx].supplierId == supplierId) &&
            (LinTp_ConfiguredNodes[idx].functionId == functionId))
        {
            for (conflictIdx = 0u; conflictIdx < LINTP_CONFIGURED_NODE_COUNT; conflictIdx++)
            {
                if ((conflictIdx != idx) && (LinTp_ConfiguredNodes[conflictIdx].currentNad == newNad))
                {
                    return E_NOT_OK;
                }
            }

            if (LinTp_State.activeTargetNad == LinTp_ConfiguredNodes[idx].currentNad)
            {
                LinTp_State.activeTargetNad = newNad;
            }

            LinTp_ConfiguredNodes[idx].currentNad = newNad;
            return E_OK;
        }
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

static uint8 LinTp_CanStartTransmitNow(void)
{
    LinIf_DiagStateType diagState;

    if (LinTp_State.state != LINTP_IDLE)
    {
        return FALSE;
    }

    diagState = LinIf_GetDiagState();
    if ((diagState != LINIF_DIAG_IDLE) &&
        (diagState != LINIF_DIAG_DONE) &&
        (diagState != LINIF_DIAG_ERROR))
    {
        return FALSE;
    }

    return TRUE;
}

uint8 LinTp_CanAcceptTransmitToNad(uint8 targetNad)
{
    uint8 idx;

    if (LinTp_IsConfiguredNad(targetNad) == FALSE)
    {
        return FALSE;
    }

    if (LinTp_CanStartTransmitNow() != FALSE)
    {
        return TRUE;
    }

    for (idx = 0u; idx < LINTP_PENDING_TX_DEPTH; idx++)
    {
        if (LinTp_PendingTx[idx].valid == FALSE)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static Std_ReturnType LinTp_StorePendingTx(PduIdType TxPduId, uint8 targetNad, const uint8* data, PduLengthType len)
{
    uint8 idx;

    for (idx = 0u; idx < LINTP_PENDING_TX_DEPTH; idx++)
    {
        if (LinTp_PendingTx[idx].valid == FALSE)
        {
            LinTp_PendingTx[idx].txPduId = TxPduId;
            LinTp_PendingTx[idx].targetNad = targetNad;
            LinTp_PendingTx[idx].len = len;
            memcpy(LinTp_PendingTx[idx].data, data, len);
            LinTp_PendingTx[idx].valid = TRUE;
            return E_OK;
        }
    }

    return E_NOT_OK;
}

static void LinTp_StartNextPendingTx(void)
{
    uint8 idx;
    PduIdType txPduId;
    uint8 targetNad;
    PduLengthType len;

    if (LinTp_CanStartTransmitNow() == FALSE)
    {
        return;
    }

    for (idx = 0u; idx < LINTP_PENDING_TX_DEPTH; idx++)
    {
        if (LinTp_PendingTx[idx].valid != FALSE)
        {
            txPduId = LinTp_PendingTx[idx].txPduId;
            targetNad = LinTp_PendingTx[idx].targetNad;
            len = LinTp_PendingTx[idx].len;

            if (LinTp_StartTransmitToNad(txPduId, targetNad, LinTp_PendingTx[idx].data, len) == E_OK)
            {
                LinTp_PendingTx[idx].valid = FALSE;
            }
            else
            {
                LinTp_PendingTx[idx].valid = FALSE;
                PduR_LinTpTxConfirmation(txPduId, E_NOT_OK);
            }

            return;
        }
    }
}

static void LinTp_BuildNextCf(uint8 frame[8])
{
    PduLengthType rem;
    uint8 copy;

    memset(frame, 0xFF, 8u);

    frame[0] = LinTp_State.txNad;
    frame[1] = (uint8)(LINTP_PCI_CF | (LinTp_State.txSn & 0x0Fu));

    rem = (PduLengthType)(LinTp_State.txLen - LinTp_State.txOffset);
    copy = (rem > 6u) ? 6u : (uint8)rem;

    memcpy(&frame[2], &LinTp_State.txBuf[LinTp_State.txOffset], copy);

    LinTp_State.txOffset = (PduLengthType)(LinTp_State.txOffset + copy);
    LinTp_State.txSn = (uint8)((LinTp_State.txSn + 1u) & 0x0Fu);
}

static Std_ReturnType LinTp_StartTransmitToNad(PduIdType TxPduId, uint8 targetNad, const uint8* data, PduLengthType len)
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

    if (LinTp_IsConfiguredNad(targetNad) == FALSE)
    {
        return E_NOT_OK;
    }

    memcpy(LinTp_State.txBuf, data, len);

    LinTp_State.txLen = len;
    LinTp_State.txOffset = 0u;
    LinTp_State.txSn = 1u;
    LinTp_State.txPduId = TxPduId;
    LinTp_State.activeTargetNad = targetNad;
    LinTp_State.txNad = targetNad;

    memset(frame, 0xFF, sizeof(frame));
    frame[0] = LinTp_State.txNad;

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

Std_ReturnType LinTp_TransmitToNad(PduIdType TxPduId, uint8 targetNad, const uint8* data, PduLengthType len)
{
    if ((data == NULL_PTR) || (len == 0u) || (len > LINTP_MAX_PAYLOAD))
    {
        return E_NOT_OK;
    }

    if (LinTp_IsConfiguredNad(targetNad) == FALSE)
    {
        return E_NOT_OK;
    }

    if (LinTp_CanStartTransmitNow() == FALSE)
    {
        return LinTp_StorePendingTx(TxPduId, targetNad, data, len);
    }

    return LinTp_StartTransmitToNad(TxPduId, targetNad, data, len);
}

Std_ReturnType LinTp_Transmit(PduIdType TxPduId, const uint8* data, PduLengthType len)
{
    return LinTp_TransmitToNad(TxPduId, LinTp_State.activeTargetNad, data, len);
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
        LinTp_StartNextPendingTx();
        return;
    }

    if (LinTp_State.txOffset >= LinTp_State.txLen)
    {
        PduR_LinTpTxConfirmation(LinTp_State.txPduId, E_OK);
        LinTp_State.state = LINTP_IDLE;
        LinTp_StartNextPendingTx();
        return;
    }

    LinTp_BuildNextCf(frame);

    if (LinTp_QueueFrame(frame) != E_OK)
    {
        PduR_LinTpTxConfirmation(LinTp_State.txPduId, E_NOT_OK);
        LinTp_State.state = LINTP_IDLE;
        LinTp_StartNextPendingTx();
    }
}

void LinTp_MainFunction(void)
{
    if (LinTp_State.state == LINTP_IDLE)
    {
        LinTp_StartNextPendingTx();
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
        LinTp_StartNextPendingTx();
    }
    else if (LinTp_State.state == LINTP_RX_IN_PROGRESS)
    {
        PduR_LinTpRxIndication(LinTp_State.rxPduId, E_NOT_OK);
        LinTp_State.state = LINTP_IDLE;
        LinTp_StartNextPendingTx();
    }

    LinTp_MainFunction_Counter++;
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
    uint8 diagnosticAddress;

    nad = frame[0];

    if ((LinTp_IsConfiguredNad(nad) == FALSE) &&
        (nad != LINTP_NAD_FUNCTIONAL) &&
        (nad != LINTP_NAD_BROADCAST))
    {
        return;
    }

    pci = frame[1] & 0xF0u;
    diagnosticAddress = ((nad == LINTP_NAD_FUNCTIONAL) || (nad == LINTP_NAD_BROADCAST)) ? TRUE : FALSE;

    if ((diagnosticAddress != FALSE) && (pci != LINTP_PCI_SF))
    {
        return;
    }

    if (pci == LINTP_PCI_SF)
    {
        if (LinTp_State.state != LINTP_IDLE)
        {
            return;
        }

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
        LinTp_StartNextPendingTx();
        return;
    }

    if (pci == LINTP_PCI_FF)
    {
        if (LinTp_State.state != LINTP_IDLE)
        {
            return;
        }

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
        LinTp_State.rxPduId = pduRId;
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
            PduR_LinTpRxIndication(LinTp_State.rxPduId, E_NOT_OK);
            LinTp_State.state = LINTP_IDLE;
            LinTp_StartNextPendingTx();
            return;
        }

        rem = (PduLengthType)(LinTp_State.expectedRxLen - LinTp_State.rxLen);
        copy = (rem > 6u) ? 6u : (uint8)rem;

        if (PduR_LinTpCopyRxData(LinTp_State.rxPduId, &frame[2], copy) != E_OK)
        {
            PduR_LinTpRxIndication(LinTp_State.rxPduId, E_NOT_OK);
            LinTp_State.state = LINTP_IDLE;
            LinTp_StartNextPendingTx();
            return;
        }

        LinTp_State.rxLen = (PduLengthType)(LinTp_State.rxLen + copy);
        LinTp_State.rxSn = (uint8)((LinTp_State.rxSn + 1u) & 0x0Fu);
        LinTp_State.timer = LINTP_TIMER_N_CR;

        if (LinTp_State.rxLen >= LinTp_State.expectedRxLen)
        {
            PduR_LinTpRxIndication(LinTp_State.rxPduId, E_OK);
            LinTp_State.state = LINTP_IDLE;
            LinTp_StartNextPendingTx();
        }
    }
}

void LinTp_RxMasterRequest(const uint8 frame[8])
{
    LinTp_HandleRxFrame(frame, LINTP_PDUR_MASTER_REQ_ID);
}

void LinTp_RxSlaveResponse(const uint8 frame[8])
{
    LinTp_HandleRxFrame(frame, LINTP_PDUR_SLAVE_RESP_ID);
}
