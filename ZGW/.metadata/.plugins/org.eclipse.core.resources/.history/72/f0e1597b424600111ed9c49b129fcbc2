#include "CanIf.h"
#include "CanTp.h"
#include "CanSM.h"
#include "Com.h"
#include <string.h>

static uint8 CanIf_BusOffState[CAN_NUM_CONTROLLERS];
static uint8 CanIf_ErrorPassiveState[CAN_NUM_CONTROLLERS];
static uint8 CanIf_ErrorWarningState[CAN_NUM_CONTROLLERS];

static const CanIf_RxPduConfigType CanIf_RxPduConfig[] =
{
    { CANIF_PDU_CLASSIC_PHYS_RX, CAN_CONTROLLER_CLASSIC, 0x710u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 1u, 8u,  CANIF_RX_TARGET_CANTP },
    { CANIF_PDU_CLASSIC_FUNC_RX, CAN_CONTROLLER_CLASSIC, 0x7DFu, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 1u, 8u,  CANIF_RX_TARGET_CANTP },
    { CANIF_PDU_FD_PHYS_RX,      CAN_CONTROLLER_FD,      0x711u, CAN_ID_STANDARD, CAN_FRAME_FD,      1u, 64u, CANIF_RX_TARGET_CANTP },
    { CANIF_PDU_FD_FUNC_RX,      CAN_CONTROLLER_FD,      0x7E0u, CAN_ID_STANDARD, CAN_FRAME_FD,      1u, 64u, CANIF_RX_TARGET_CANTP },

    { CANIF_PDU_APP_RX,          CAN_CONTROLLER_CLASSIC, 0x500u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u, 8u,  CANIF_RX_TARGET_COM }
};

static const CanIf_TxPduConfigType CanIf_TxPduConfig[] =
{
    { CANIF_PDU_CLASSIC_PHYS_TX, CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x718u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u,  CANIF_TX_TARGET_CANTP },
    { CANIF_PDU_FD_PHYS_TX,      CAN_HTH_FD,      CAN_CONTROLLER_FD,      0x719u, CAN_ID_STANDARD, CAN_FRAME_FD,      64u, CANIF_TX_TARGET_CANTP },
    { CANIF_PDU_APP_TX,          CAN_HTH_CLASSIC, CAN_CONTROLLER_CLASSIC, 0x500u, CAN_ID_STANDARD, CAN_FRAME_CLASSIC, 8u,  CANIF_TX_TARGET_COM   }
};

const CanIf_ConfigType CanIf_Config =
{
    CanIf_RxPduConfig,
    (uint8)(sizeof(CanIf_RxPduConfig) / sizeof(CanIf_RxPduConfig[0])),
    CanIf_TxPduConfig,
    (uint8)(sizeof(CanIf_TxPduConfig) / sizeof(CanIf_TxPduConfig[0]))
};

static const CanIf_ConfigType* CanIf_ConfigPtr;
static uint8 CanIf_PduMode[CAN_NUM_CONTROLLERS];

static const CanIf_TxPduConfigType* CanIf_FindTxPdu(PduIdType CanIfTxSduId)
{
    uint8 i;

    if (CanIf_ConfigPtr == NULL_PTR)
    {
        return NULL_PTR;
    }

    for (i = 0u; i < CanIf_ConfigPtr->numTxPdus; i++)
    {
        if (CanIf_ConfigPtr->txPdus[i].txPduId == CanIfTxSduId)
        {
            return &CanIf_ConfigPtr->txPdus[i];
        }
    }

    return NULL_PTR;
}

void CanIf_Init(const CanIf_ConfigType* ConfigPtr)
{
    uint8 i;

    CanIf_ConfigPtr = (ConfigPtr == NULL_PTR) ? &CanIf_Config : ConfigPtr;

    for (i = 0u; i < CAN_NUM_CONTROLLERS; i++)
    {
        CanIf_PduMode[i] = CANIF_PDU_MODE_ONLINE;
        CanIf_BusOffState[i] = FALSE;
        CanIf_ErrorPassiveState[i] = FALSE;
        CanIf_ErrorWarningState[i] = FALSE;
    }
}

Std_ReturnType CanIf_SetPduMode(uint8 ControllerId, uint8 PduMode)
{
    if ((ControllerId >= CAN_NUM_CONTROLLERS) || (PduMode > CANIF_PDU_MODE_ONLINE))
    {
        return E_NOT_OK;
    }

    CanIf_PduMode[ControllerId] = PduMode;
    return E_OK;
}

uint8 CanIf_GetPduMode(uint8 ControllerId)
{
    if (ControllerId >= CAN_NUM_CONTROLLERS)
    {
        return CANIF_PDU_MODE_OFFLINE;
    }

    return CanIf_PduMode[ControllerId];
}

Std_ReturnType CanIf_Transmit(PduIdType CanIfTxSduId, const uint8* data, PduLengthType len)
{
    const CanIf_TxPduConfigType* cfg;
    Can_PduType pdu;

    if ((data == NULL_PTR) || (len == 0u))
    {
        return E_NOT_OK;
    }

    cfg = CanIf_FindTxPdu(CanIfTxSduId);

    if (cfg == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (cfg->controllerId >= CAN_NUM_CONTROLLERS)
    {
        return E_NOT_OK;
    }

    if (CanIf_BusOffState[cfg->controllerId] != FALSE)
    {
        return E_NOT_OK;
    }

    if ((CanIf_PduMode[cfg->controllerId] & CANIF_PDU_MODE_TX_ONLINE) == 0u)
    {
        return E_NOT_OK;
    }

    if (len != cfg->fixedDlc)
    {
        return E_NOT_OK;
    }

    if ((cfg->frameType == CAN_FRAME_CLASSIC) && (len > CAN_CLASSIC_MAX_DLC))
    {
        return E_NOT_OK;
    }

    if ((cfg->frameType == CAN_FRAME_FD) && (len > CANFD_MAX_DLC))
    {
        return E_NOT_OK;
    }

    pdu.swPduHandle = CanIfTxSduId;
    pdu.id = cfg->canId;
    pdu.idType = cfg->idType;
    pdu.controllerId = cfg->controllerId;
    pdu.frameType = cfg->frameType;
    pdu.dlc = (uint8)len;
    pdu.sdu = data;

    return Can_Write(cfg->hth, &pdu);
}

void CanIf_RxIndication(const Can_FrameType* frame)
{
    uint8 i;

    if ((CanIf_ConfigPtr == NULL_PTR) || (frame == NULL_PTR))
    {
        return;
    }

    if (frame->controllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    if ((CanIf_PduMode[frame->controllerId] & CANIF_PDU_MODE_RX_ONLINE) == 0u)
    {
        return;
    }

    for (i = 0u; i < CanIf_ConfigPtr->numRxPdus; i++)
    {
        const CanIf_RxPduConfigType* cfg = &CanIf_ConfigPtr->rxPdus[i];

        if ((cfg->controllerId == frame->controllerId) &&
            (cfg->canId == frame->id) &&
            (cfg->idType == frame->idType) &&
            (cfg->frameType == frame->frameType) &&
            (frame->dlc >= cfg->minDlc) &&
            (frame->dlc <= cfg->maxDlc))
        {
            if (cfg->target == CANIF_RX_TARGET_CANTP)
            {
                CanTp_RxIndication(cfg->rxPduId, frame->data, frame->dlc);
            }
            else
            {
                Com_RxIndication(cfg->rxPduId, frame->data, frame->dlc);
            }

            return;
        }
    }
}

void CanIf_TxConfirmation(PduIdType CanIfTxSduId)
{
    const CanIf_TxPduConfigType* cfg;

    cfg = CanIf_FindTxPdu(CanIfTxSduId);

    if (cfg == NULL_PTR)
    {
        return;
    }

    if (cfg->target == CANIF_TX_TARGET_CANTP)
    {
        CanTp_TxConfirmation(CanIfTxSduId);
    }
    else
    {
        Com_TxConfirmation(CanIfTxSduId);
    }
}

void CanIf_ControllerBusOff(uint8 ControllerId)
{
    if (ControllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    CanIf_BusOffState[ControllerId] = TRUE;
    CanIf_ErrorPassiveState[ControllerId] = FALSE;
    CanIf_ErrorWarningState[ControllerId] = FALSE;

    (void)CanIf_SetPduMode(ControllerId, CANIF_PDU_MODE_OFFLINE);

    CanSM_ControllerBusOff(ControllerId);
    CanIf_AppBusOff(ControllerId);
}

void CanIf_ControllerRecovered(uint8 ControllerId)
{
    if (ControllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    CanIf_BusOffState[ControllerId] = FALSE;
    CanIf_ErrorPassiveState[ControllerId] = FALSE;
    CanIf_ErrorWarningState[ControllerId] = FALSE;

    (void)CanIf_SetPduMode(ControllerId, CANIF_PDU_MODE_ONLINE);

    CanIf_AppControllerRecovered(ControllerId);
}

void CanIf_ControllerErrorPassive(uint8 ControllerId)
{
    if (ControllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    CanIf_ErrorPassiveState[ControllerId] = TRUE;
    CanIf_ErrorWarningState[ControllerId] = FALSE;

    CanIf_AppErrorPassive(ControllerId);
}

void CanIf_ControllerErrorWarning(uint8 ControllerId)
{
    if (ControllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    if (CanIf_ErrorPassiveState[ControllerId] == FALSE)
    {
        CanIf_ErrorWarningState[ControllerId] = TRUE;
    }

    CanIf_AppErrorWarning(ControllerId);
}

__attribute__((weak)) void CanIf_AppBusOff(uint8 ControllerId)
{
    (void)ControllerId;
}

__attribute__((weak)) void CanIf_AppControllerRecovered(uint8 ControllerId)
{
    (void)ControllerId;
}

__attribute__((weak)) void CanIf_AppErrorPassive(uint8 ControllerId)
{
    (void)ControllerId;
}

__attribute__((weak)) void CanIf_AppErrorWarning(uint8 ControllerId)
{
    (void)ControllerId;
}
