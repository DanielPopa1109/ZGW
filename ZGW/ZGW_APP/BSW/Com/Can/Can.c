#include "Can.h"
#include "CanIf.h"
#include <string.h>
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "IfxPort_reg.h"
#include "SysMgr.h"

#define CAN_IRQ_PRIO_RX_CLASSIC       40u
#define CAN_IRQ_PRIO_RX_FD            41u
#define CAN_IRQ_PRIO_BUSOFF_CLASSIC   42u
#define CAN_IRQ_PRIO_BUSOFF_FD        43u

#define CAN_CLASSIC_TRCV_STB_PORT     (&MODULE_P20)
#define CAN_CLASSIC_TRCV_STB_PIN      6u
#define CAN_TX_CANCEL_TIMEOUT         1000u
#define CAN_RX_FIFO_DRAIN_BUDGET      2u
#define CAN_TX_HW_PENDING_TIMEOUT_MS  64u
#define CAN_TX_HW_PENDING_TIMEOUT_TICKS \
        ((CAN_TX_HW_PENDING_TIMEOUT_MS + CAN_MAINFUNCTION_PERIOD_MS - 1u) / CAN_MAINFUNCTION_PERIOD_MS)
#define CAN_TX_ATTEMPTS_PER_MAIN \
        (CAN_TX_BUDGET_PER_MAIN + CAN_NUM_CONTROLLERS)
#define CAN_TX_HW_BUFFER_COUNT_CLASSIC 4u
#define CAN_TX_HW_BUFFER_COUNT_FD      8u
#define CAN_TX_HW_BUFFER_COUNT_MAX     CAN_TX_HW_BUFFER_COUNT_FD

typedef struct
{
        Can_ErrorStateType errorState;
        uint8 busOffPending;
        uint16 busOffRecoveryTicks;
        uint32 busOffCounter;
        uint32 recoveredCounter;
        uint32 rxOverflowCounter;
        uint32 txOverflowCounter;
        uint32 txFailCounter;
        uint8 rxErrorCounter;
        uint8 txErrorCounter;
} Can_ControllerRuntimeType;

typedef struct
{
    uint8 active;
    PduIdType swPduHandle;
    Can_PduType pdu;
    uint8 data[CAN_MAX_DLC];
    uint16 ageTicks;
} Can_TxPendingType;


typedef struct
{
    uint16 id;
    const char* name;
} Can_StdIdFilterConfigType;

#define CAN_FD_STD_FILTER_COUNT ((uint8)(sizeof(Can_FdStdIdFilters) / sizeof(Can_FdStdIdFilters[0])))

static const Can_StdIdFilterConfigType Can_FdStdIdFilters[] =
{
    { 0x093u, "PDM4_LoadStatus" }, /* PDM4_LoadStatus, sender PDM4 */
    { 0x092u, "PDM3_LoadStatus" }, /* PDM3_LoadStatus, sender PDM3 */
    { 0x091u, "PDM2_LoadStatus" }, /* PDM2_LoadStatus, sender PDM2 */
    { 0x090u, "PDM1_LoadStatus" }, /* PDM1_LoadStatus, sender PDM1 */
    { 0x300u, "PDM1_VoltageFeedback_1" }, /* PDM1_VoltageFeedback_1, sender PDM1 */
    { 0x301u, "PDM1_VoltageFeedback_2" }, /* PDM1_VoltageFeedback_2, sender PDM1 */
    { 0x302u, "PDM1_VoltageFeedback_3" }, /* PDM1_VoltageFeedback_3, sender PDM1 */
    { 0x303u, "PDM1_VoltageFeedback_4" }, /* PDM1_VoltageFeedback_4, sender PDM1 */
    { 0x304u, "PDM1_VoltageFeedback_5" }, /* PDM1_VoltageFeedback_5, sender PDM1 */
    { 0x305u, "PDM1_CurrentFeedback_1" }, /* PDM1_CurrentFeedback_1, sender PDM1 */
    { 0x306u, "PDM1_CurrentFeedback_2" }, /* PDM1_CurrentFeedback_2, sender PDM1 */
    { 0x307u, "PDM1_CurrentFeedback_3" }, /* PDM1_CurrentFeedback_3, sender PDM1 */
    { 0x308u, "PDM1_CurrentFeedback_4" }, /* PDM1_CurrentFeedback_4, sender PDM1 */
    { 0x309u, "PDM1_CurrentFeedback_5" }, /* PDM1_CurrentFeedback_5, sender PDM1 */
    { 0x30Au, "PDM1_StuckAtOnEvent" }, /* PDM1_StuckAtOnEvent, sender PDM1 */
    { 0x30Bu, "PDM1_StuckAtOffEvent" }, /* PDM1_StuckAtOffEvent, sender PDM1 */
    { 0x30Cu, "PDM1_TemperatureFeedback_1" }, /* PDM1_TemperatureFeedback_1, sender PDM1 */
    { 0x30Du, "PDM1_TemperatureFeedback_2" }, /* PDM1_TemperatureFeedback_2, sender PDM1 */
    { 0x30Eu, "PDM1_TemperatureFeedback_3" }, /* PDM1_TemperatureFeedback_3, sender PDM1 */
    { 0x30Fu, "PDM1_TemperatureFeedback_4" }, /* PDM1_TemperatureFeedback_4, sender PDM1 */
    { 0x310u, "PDM1_TemperatureFeedback_5" }, /* PDM1_TemperatureFeedback_5, sender PDM1 */
    { 0x311u, "PDM2_VoltageFeedback_1" }, /* PDM2_VoltageFeedback_1, sender PDM2 */
    { 0x312u, "PDM2_VoltageFeedback_2" }, /* PDM2_VoltageFeedback_2, sender PDM2 */
    { 0x313u, "PDM2_VoltageFeedback_3" }, /* PDM2_VoltageFeedback_3, sender PDM2 */
    { 0x314u, "PDM2_VoltageFeedback_4" }, /* PDM2_VoltageFeedback_4, sender PDM2 */
    { 0x315u, "PDM2_VoltageFeedback_5" }, /* PDM2_VoltageFeedback_5, sender PDM2 */
    { 0x316u, "PDM2_CurrentFeedback_1" }, /* PDM2_CurrentFeedback_1, sender PDM2 */
    { 0x317u, "PDM2_CurrentFeedback_2" }, /* PDM2_CurrentFeedback_2, sender PDM2 */
    { 0x318u, "PDM2_CurrentFeedback_3" }, /* PDM2_CurrentFeedback_3, sender PDM2 */
    { 0x319u, "PDM2_CurrentFeedback_4" }, /* PDM2_CurrentFeedback_4, sender PDM2 */
    { 0x31Au, "PDM2_CurrentFeedback_5" }, /* PDM2_CurrentFeedback_5, sender PDM2 */
    { 0x31Bu, "PDM2_StuckAtOnEvent" }, /* PDM2_StuckAtOnEvent, sender PDM2 */
    { 0x31Cu, "PDM2_StuckAtOffEvent" }, /* PDM2_StuckAtOffEvent, sender PDM2 */
    { 0x31Du, "PDM2_TemperatureFeedback_1" }, /* PDM2_TemperatureFeedback_1, sender PDM2 */
    { 0x31Eu, "PDM2_TemperatureFeedback_2" }, /* PDM2_TemperatureFeedback_2, sender PDM2 */
    { 0x31Fu, "PDM2_TemperatureFeedback_3" }, /* PDM2_TemperatureFeedback_3, sender PDM2 */
    { 0x320u, "PDM2_TemperatureFeedback_4" }, /* PDM2_TemperatureFeedback_4, sender PDM2 */
    { 0x321u, "PDM2_TemperatureFeedback_5" }, /* PDM2_TemperatureFeedback_5, sender PDM2 */
    { 0x322u, "PDM3_VoltageFeedback_1" }, /* PDM3_VoltageFeedback_1, sender PDM3 */
    { 0x323u, "PDM3_VoltageFeedback_2" }, /* PDM3_VoltageFeedback_2, sender PDM3 */
    { 0x324u, "PDM3_VoltageFeedback_3" }, /* PDM3_VoltageFeedback_3, sender PDM3 */
    { 0x325u, "PDM3_VoltageFeedback_4" }, /* PDM3_VoltageFeedback_4, sender PDM3 */
    { 0x326u, "PDM3_VoltageFeedback_5" }, /* PDM3_VoltageFeedback_5, sender PDM3 */
    { 0x327u, "PDM3_CurrentFeedback_1" }, /* PDM3_CurrentFeedback_1, sender PDM3 */
    { 0x328u, "PDM3_CurrentFeedback_2" }, /* PDM3_CurrentFeedback_2, sender PDM3 */
    { 0x329u, "PDM3_CurrentFeedback_3" }, /* PDM3_CurrentFeedback_3, sender PDM3 */
    { 0x32Au, "PDM3_CurrentFeedback_4" }, /* PDM3_CurrentFeedback_4, sender PDM3 */
    { 0x32Bu, "PDM3_CurrentFeedback_5" }, /* PDM3_CurrentFeedback_5, sender PDM3 */
    { 0x32Cu, "PDM3_StuckAtOnEvent" }, /* PDM3_StuckAtOnEvent, sender PDM3 */
    { 0x32Du, "PDM3_StuckAtOffEvent" }, /* PDM3_StuckAtOffEvent, sender PDM3 */
    { 0x32Eu, "PDM3_TemperatureFeedback_1" }, /* PDM3_TemperatureFeedback_1, sender PDM3 */
    { 0x32Fu, "PDM3_TemperatureFeedback_2" }, /* PDM3_TemperatureFeedback_2, sender PDM3 */
    { 0x330u, "PDM3_TemperatureFeedback_3" }, /* PDM3_TemperatureFeedback_3, sender PDM3 */
    { 0x331u, "PDM3_TemperatureFeedback_4" }, /* PDM3_TemperatureFeedback_4, sender PDM3 */
    { 0x332u, "PDM3_TemperatureFeedback_5" }, /* PDM3_TemperatureFeedback_5, sender PDM3 */
    { 0x333u, "PDM4_VoltageFeedback_1" }, /* PDM4_VoltageFeedback_1, sender PDM4 */
    { 0x334u, "PDM4_VoltageFeedback_2" }, /* PDM4_VoltageFeedback_2, sender PDM4 */
    { 0x335u, "PDM4_VoltageFeedback_3" }, /* PDM4_VoltageFeedback_3, sender PDM4 */
    { 0x336u, "PDM4_VoltageFeedback_4" }, /* PDM4_VoltageFeedback_4, sender PDM4 */
    { 0x337u, "PDM4_VoltageFeedback_5" }, /* PDM4_VoltageFeedback_5, sender PDM4 */
    { 0x338u, "PDM4_CurrentFeedback_1" }, /* PDM4_CurrentFeedback_1, sender PDM4 */
    { 0x339u, "PDM4_CurrentFeedback_2" }, /* PDM4_CurrentFeedback_2, sender PDM4 */
    { 0x33Au, "PDM4_CurrentFeedback_3" }, /* PDM4_CurrentFeedback_3, sender PDM4 */
    { 0x33Bu, "PDM4_CurrentFeedback_4" }, /* PDM4_CurrentFeedback_4, sender PDM4 */
    { 0x33Cu, "PDM4_CurrentFeedback_5" }, /* PDM4_CurrentFeedback_5, sender PDM4 */
    { 0x33Du, "PDM4_StuckAtOnEvent" }, /* PDM4_StuckAtOnEvent, sender PDM4 */
    { 0x33Eu, "PDM4_StuckAtOffEvent" }, /* PDM4_StuckAtOffEvent, sender PDM4 */
    { 0x33Fu, "PDM4_TemperatureFeedback_1" }, /* PDM4_TemperatureFeedback_1, sender PDM4 */
    { 0x340u, "PDM4_TemperatureFeedback_2" }, /* PDM4_TemperatureFeedback_2, sender PDM4 */
    { 0x341u, "PDM4_TemperatureFeedback_3" }, /* PDM4_TemperatureFeedback_3, sender PDM4 */
    { 0x342u, "PDM4_TemperatureFeedback_4" }, /* PDM4_TemperatureFeedback_4, sender PDM4 */
    { 0x343u, "PDM4_TemperatureFeedback_5" }, /* PDM4_TemperatureFeedback_5, sender PDM4 */
    { 0x721u, "PDM1_DiagResponse" }, /* PDM1_DiagResponse, sender PDM1 */
    { 0x723u, "PDM2_DiagResponse" }, /* PDM2_DiagResponse, sender PDM2 */
    { 0x725u, "PDM3_DiagResponse" }, /* PDM3_DiagResponse, sender PDM3 */
    { 0x727u, "PDM4_DiagResponse" }, /* PDM4_DiagResponse, sender PDM4 */
    { 0x710u, "DiagRequest_710" } /* DiagRequest_710, sender Tester */
};

#define CAN_CLASSIC_STD_FILTER_COUNT ((uint8)(sizeof(Can_ClassicStdIdFilters) / sizeof(Can_ClassicStdIdFilters[0])))

static const Can_StdIdFilterConfigType Can_ClassicStdIdFilters[] =
{
    { 0x3A0u, "VehicleState" }, /* VehicleState, sender ZGW */
    { 0x253u, "CentralLockData" }, /* CentralLockData, sender FRBE */
    { 0x252u, "LightData1" }, /* LightData1, sender FRBE */
    { 0x221u, "DisplayOutTemp" }, /* DisplayOutTemp, sender ZGW */
    { 0x240u, "StatusActuator" }, /* StatusActuator, sender CBM */
    { 0x040u, "StatusBodyData1" }, /* StatusBodyData1, sender ZGW */
    { 0x251u, "OutsideTemperatureStatus" }, /* OutsideTemperatureStatus, sender CBM */
    { 0x250u, "CentralCommand1" }, /* CentralCommand1, sender CBM */
    { 0x201u, "DmuStatus" }, /* DmuStatus, sender DMU */
    { 0x220u, "CommandDisplayStatus" }, /* CommandDisplayStatus, sender ZGW */
    { 0x086u, "EngineData7" }, /* EngineData7, sender DME */
    { 0x6F6u, "BattFullStat" }, /* BattFullStat, sender ELC */
    { 0x085u, "EngineData6" }, /* EngineData6, sender DME */
    { 0x084u, "EngineData5" }, /* EngineData5, sender DME */
    { 0x083u, "EngineData4" }, /* EngineData4, sender DME */
    { 0x082u, "EngineData3" }, /* EngineData3, sender DME */
    { 0x081u, "EngineData2" }, /* EngineData2, sender DME */
    { 0x092u, "DSCData3" }, /* DSCData3, sender DSC */
    { 0x091u, "DSCData2" }, /* DSCData2, sender DSC */
    { 0x090u, "DSCData1" }, /* DSCData1, sender DSC */
    { 0x080u, "EngineData1" }, /* EngineData1, sender DME */
    { 0x100u, "ASGData1" }, /* ASGData1, sender AGS */
    { 0x10Du, "PdcStat" }, /* PdcStat, sender FRBE */
    { 0x299u, "Mileage" }, /* Mileage, sender DMU */
    { 0x200u, "Dmu_Alive" }, /* Dmu_Alive, sender DMU */
    { 0x7C7u, "XcpResponse_7C7" }, /* XcpResponse_7C7, sender ELC */
    { 0x7C5u, "XcpResponse_7C5" }, /* XcpResponse_7C5, sender DMU */
    { 0x7C3u, "XcpResponse_7C3" }, /* XcpResponse_7C3, sender FRBE */
    { 0x7C1u, "XcpResponse_7C1" }, /* XcpResponse_7C1, sender CBM */
    { 0x7C8u, "XcpRequest_7C8" }, /* XcpRequest_7C8, sender ZGW */
    { 0x7C6u, "XcpRequest_7C6" }, /* XcpRequest_7C6, sender ZGW */
    { 0x7C4u, "XcpRequest_7C4" }, /* XcpRequest_7C4, sender ZGW */
    { 0x7C2u, "XcpRequest_7C2" }, /* XcpRequest_7C2, sender ZGW */
    { 0x7C0u, "XcpRequest_7C0" }, /* XcpRequest_7C0, sender ZGW */
    { 0x6EFu, "VoltageCurrent" }, /* VoltageCurrent, sender ELC */
    { 0x6EEu, "TempMeas" }, /* TempMeas, sender ELC */
    { 0x202u, "SDAT" }, /* SDAT, sender ZGW */
    { 0x3FFu, "NM3" }, /* NM3, sender ZGW */
    { 0x051u, "LoadStatus" }, /* LoadStatus, sender ELC */
    { 0x050u, "LoadRequest" }, /* LoadRequest, sender ZGW */
    { 0x6F0u, "L1_I2T_Counter" }, /* L1_I2T_Counter, sender ELC */
    { 0x753u, "Error_706" }, /* Error_706, sender ELC */
    { 0x752u, "Error_704" }, /* Error_704, sender DMU */
    { 0x751u, "Error_702" }, /* Error_702, sender FRBE */
    { 0x750u, "Error_700" }, /* Error_700, sender CBM */
    { 0x710u, "DiagRequest_710" }, /* DiagRequest_710, sender Tester */
    { 0x707u, "DiagResponse_707" }, /* DiagResponse_707, sender ELC */
    { 0x705u, "DiagResponse_705" }, /* DiagResponse_705, sender DMU */
    { 0x703u, "DiagResponse_703" }, /* DiagResponse_703, sender FRBE */
    { 0x701u, "DiagResponse_701" }, /* DiagResponse_701, sender CBM */
    { 0x706u, "DiagRequest_706" }, /* DiagRequest_706, sender ZGW */
    { 0x704u, "DiagRequest_704" }, /* DiagRequest_704, sender ZGW */
    { 0x702u, "DiagRequest_702" }, /* DiagRequest_702, sender ZGW */
    { 0x700u, "DiagRequest_700" }, /* DiagRequest_700, sender ZGW */
    { 0x6F3u, "BattSoCSoH" }, /* BattSoCSoH, sender ELC */
    { 0x6F2u, "BattSoC" }, /* BattSoC, sender ELC */
    { 0x6F7u, "BattDiagnosis" }, /* BattDiagnosis, sender ELC */
    { 0x6F5u, "BattCurrent" }, /* BattCurrent, sender ELC */
    { 0x6F1u, "BattCapDischarge" }, /* BattCapDischarge, sender ELC */
    { 0x6F4u, "BattCapRes" }, /* BattCapRes, sender ELC */
    { 0x500u, "LegacyWakeOrLab_500" }, /* LegacyWakeOrLab_500, sender LOCAL */
};

typedef struct
{
        IfxCan_Can_Config moduleConfigClassic;
        IfxCan_Can_Config moduleConfigFd;
        IfxCan_Can moduleClassic;
        IfxCan_Can moduleFd;

        IfxCan_Can_NodeConfig nodeConfigClassic;
        IfxCan_Can_NodeConfig nodeConfigFd;

        IfxCan_Can_Node nodeClassic;
        IfxCan_Can_Node nodeFd;

        IfxCan_Filter filter;
        IfxCan_Message txMsg;

        uint32 txWords[16];
} Can_HwType;

typedef struct
{
        Can_PduType pdu;
        uint8 data[CAN_MAX_DLC];
        uint8 used;
} Can_TxQueueElementType;

typedef struct
{
        Can_FrameType frame;
        uint8 used;
} Can_RxQueueElementType;

static Can_HwType Can_Hw;
static Can_ControllerStateType Can_ControllerState[CAN_NUM_CONTROLLERS];

static Can_TxQueueElementType Can_TxQueue[CAN_TX_QUEUE_SIZE];
static uint8 Can_TxHead;
static uint8 Can_TxTail;
static uint8 Can_TxCount;
static uint8 Can_TxSyncPolling;
static Can_TxPendingType Can_TxPending[CAN_NUM_CONTROLLERS][CAN_TX_HW_BUFFER_COUNT_MAX];

static Can_RxQueueElementType Can_RxQueue[CAN_RX_QUEUE_SIZE];
static uint8 Can_RxHead;
static uint8 Can_RxTail;
static uint8 Can_RxCount;

static uint8 Can_InterruptsEnabled[CAN_NUM_CONTROLLERS];

static Can_ControllerRuntimeType Can_Runtime[CAN_NUM_CONTROLLERS];

long long Can_MainFunction_Counter = 0;
volatile uint32 Can_TxQueueRotateCounter = 0u;
volatile uint32 Can_TxControllerNotReadyCounter = 0u;
volatile uint32 Can_TxPendingBusyCounter = 0u;
volatile uint32 Can_TxSendRetryCounter = 0u;
volatile uint32 Can_TxProcessAttemptLimitCounter = 0u;

static void Can_ReadRxFifo(uint8 controllerId);
static void Can_ProcessRx(void);
static void Can_RequeuePendingTx(uint8 controllerId);

static void CanClassic_TrcvSetNormalMode(void)
{
    IfxPort_setPinModeOutput(CAN_CLASSIC_TRCV_STB_PORT,
            CAN_CLASSIC_TRCV_STB_PIN,
            IfxPort_OutputMode_pushPull,
            IfxPort_OutputIdx_general);

    /* KIT on-board CAN transceiver STB is active high; low keeps normal mode. */
    IfxPort_setPinLow(CAN_CLASSIC_TRCV_STB_PORT, CAN_CLASSIC_TRCV_STB_PIN);
}

static void Can_ReleaseFdRxPinFromScr(void)
{
    uint16 safetyWdtPw;

    safetyWdtPw = IfxScuWdt_getSafetyWatchdogPassword();
    IfxScuWdt_clearSafetyEndinit(safetyWdtPw);
    while(P33_PCSR.B.LCK)
    {
    }
    P33_PCSR.B.SEL5 = 0u;
    IfxScuWdt_setSafetyEndinit(safetyWdtPw);
}

static uint8 Can_GetTxBufferCount(uint8 controllerId)
{
    if (controllerId == CAN_CONTROLLER_FD)
    {
        return CAN_TX_HW_BUFFER_COUNT_FD;
    }

    if (controllerId == CAN_CONTROLLER_CLASSIC)
    {
        return CAN_TX_HW_BUFFER_COUNT_CLASSIC;
    }

    return 0u;
}

static boolean Can_CancelTxBuffer(IfxCan_Can_Node* node, IfxCan_TxBufferId txBufferId)
{
    uint16 timeout;

    if ((node == NULL_PTR) || (node->node == NULL_PTR))
    {
        return FALSE;
    }

    if (IfxCan_Can_isTxBufferRequestPending(node, txBufferId) != FALSE)
    {
        IfxCan_Node_setTxBufferCancellationRequest(node->node, txBufferId);

        timeout = CAN_TX_CANCEL_TIMEOUT;
        while ((IfxCan_Can_isTxBufferRequestPending(node, txBufferId) != FALSE) &&
               (timeout > 0u))
        {
            timeout--;
        }
    }

    return (IfxCan_Can_isTxBufferRequestPending(node, txBufferId) == FALSE) ? TRUE : FALSE;
}

static boolean Can_CancelPendingTxBuffers(IfxCan_Can_Node* node, uint8 txBufferCount)
{
    uint8 bufferIdx;

    for (bufferIdx = 0u; bufferIdx < txBufferCount; bufferIdx++)
    {
        if (Can_CancelTxBuffer(node, (IfxCan_TxBufferId)bufferIdx) == FALSE)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static uint8 Can_DlcToBytes(uint8 dlc)
{
    static const uint8 map[16] =
    {
            0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u,
            8u, 12u, 16u, 20u, 24u, 32u, 48u, 64u
    };

    return map[dlc & 0x0Fu];
}

static boolean Can_EnterCritical(void)
{
    return IfxCpu_disableInterrupts();
}

static void Can_ExitCritical(boolean state)
{
    IfxCpu_restoreInterrupts(state);
}

static uint8 Can_BytesToDlc(uint8 len)
{
    if (len <= 8u)  { return len;  }
    if (len <= 12u) { return 9u;   }
    if (len <= 16u) { return 10u;  }
    if (len <= 20u) { return 11u;  }
    if (len <= 24u) { return 12u;  }
    if (len <= 32u) { return 13u;  }
    if (len <= 48u) { return 14u;  }
    return 15u;
}

static Std_ReturnType Can_ValidatePdu(PduIdType Hth, const Can_PduType* pdu)
{
    if ((pdu == NULL_PTR) || (pdu->sdu == NULL_PTR) || (pdu->dlc == 0u))
    {
        return E_NOT_OK;
    }

    if (pdu->controllerId >= CAN_NUM_CONTROLLERS)
    {
        return E_NOT_OK;
    }

    if (Can_ControllerState[pdu->controllerId] != CAN_READY)
    {
        return E_NOT_OK;
    }

    if ((pdu->idType != CAN_ID_STANDARD) && (pdu->idType != CAN_ID_EXTENDED))
    {
        return E_NOT_OK;
    }

    if ((pdu->frameType != CAN_FRAME_CLASSIC) && (pdu->frameType != CAN_FRAME_FD))
    {
        return E_NOT_OK;
    }

    if ((pdu->idType == CAN_ID_STANDARD) && (pdu->id > CAN_STD_ID_MAX))
    {
        return E_NOT_OK;
    }

    if ((pdu->idType == CAN_ID_EXTENDED) && (pdu->id > CAN_EXT_ID_MAX))
    {
        return E_NOT_OK;
    }

    if (pdu->frameType == CAN_FRAME_CLASSIC)
    {
        if (pdu->dlc > CAN_CLASSIC_MAX_DLC)
        {
            return E_NOT_OK;
        }
    }
    else
    {
        if (pdu->dlc > CANFD_MAX_DLC)
        {
            return E_NOT_OK;
        }
    }

    if (pdu->controllerId == CAN_CONTROLLER_CLASSIC)
    {
        if ((Hth != CAN_HTH_CLASSIC) || (pdu->frameType != CAN_FRAME_CLASSIC))
        {
            return E_NOT_OK;
        }
    }

    if (pdu->controllerId == CAN_CONTROLLER_FD)
    {
        if (Hth != CAN_HTH_FD)
        {
            return E_NOT_OK;
        }
    }

    return E_OK;
}

static void Can_ResetQueues(void)
{
    boolean irqState;

    irqState = Can_EnterCritical();

    memset(Can_TxQueue, 0, sizeof(Can_TxQueue));
    memset(Can_RxQueue, 0, sizeof(Can_RxQueue));
    memset(Can_TxPending, 0, sizeof(Can_TxPending));

    Can_TxHead = 0u;
    Can_TxTail = 0u;
    Can_TxCount = 0u;
    Can_TxSyncPolling = FALSE;

    Can_RxHead = 0u;
    Can_RxTail = 0u;
    Can_RxCount = 0u;

    Can_ExitCritical(irqState);
}

static Std_ReturnType Can_TxQueuePushInternal(const Can_PduType* pdu, boolean replaceDuplicate)
{
    Can_TxQueueElementType* e;
    boolean irqState;
    uint8 idx;
    uint8 count;
    uint8 bufferIdx;

    if ((pdu == NULL_PTR) || (pdu->sdu == NULL_PTR) || (pdu->dlc > CAN_MAX_DLC))
    {
        return E_NOT_OK;
    }

    irqState = Can_EnterCritical();

    if (pdu->controllerId < CAN_NUM_CONTROLLERS)
    {
        for (bufferIdx = 0u;
                (replaceDuplicate != FALSE) && (bufferIdx < Can_GetTxBufferCount(pdu->controllerId));
                bufferIdx++)
        {
            if ((Can_TxPending[pdu->controllerId][bufferIdx].active != FALSE) &&
                    (Can_TxPending[pdu->controllerId][bufferIdx].swPduHandle == pdu->swPduHandle))
            {
                Can_ExitCritical(irqState);
                return E_NOT_OK;
            }
        }
    }

    idx = Can_TxTail;
    for (count = 0u; count < Can_TxCount; count++)
    {
        e = &Can_TxQueue[idx];

        if ((e->used != FALSE) &&
                (e->pdu.swPduHandle == pdu->swPduHandle) &&
                (e->pdu.controllerId == pdu->controllerId))
        {
            if (replaceDuplicate != FALSE)
            {
                e->pdu = *pdu;
                memcpy(e->data, pdu->sdu, pdu->dlc);
                e->pdu.sdu = e->data;

                Can_ExitCritical(irqState);
                return E_OK;
            }
        }

        idx++;
        if (idx >= CAN_TX_QUEUE_SIZE)
        {
            idx = 0u;
        }
    }

    if (Can_TxCount >= CAN_TX_QUEUE_SIZE)
    {
        if (pdu->controllerId < CAN_NUM_CONTROLLERS)
        {
            Can_Runtime[pdu->controllerId].txOverflowCounter++;
        }

        Can_ExitCritical(irqState);
        return E_NOT_OK;
    }

    e = &Can_TxQueue[Can_TxHead];

    e->pdu = *pdu;
    memcpy(e->data, pdu->sdu, pdu->dlc);
    e->pdu.sdu = e->data;
    e->used = TRUE;

    Can_TxHead++;
    if (Can_TxHead >= CAN_TX_QUEUE_SIZE)
    {
        Can_TxHead = 0u;
    }

    Can_TxCount++;

    Can_ExitCritical(irqState);

    return E_OK;
}

static Std_ReturnType Can_TxQueuePush(const Can_PduType* pdu)
{
    return Can_TxQueuePushInternal(pdu, TRUE);
}

static Std_ReturnType Can_TxQueuePushNoReplace(const Can_PduType* pdu)
{
    return Can_TxQueuePushInternal(pdu, FALSE);
}

static Std_ReturnType Can_TxQueuePeek(Can_PduType* pdu)
{
    Can_TxQueueElementType* e;
    boolean irqState;

    if (pdu == NULL_PTR)
    {
        return E_NOT_OK;
    }

    irqState = Can_EnterCritical();

    if (Can_TxCount == 0u)
    {
        Can_ExitCritical(irqState);
        return E_NOT_OK;
    }

    e = &Can_TxQueue[Can_TxTail];

    if (e->used == FALSE)
    {
        Can_ExitCritical(irqState);
        return E_NOT_OK;
    }

    *pdu = e->pdu;

    Can_ExitCritical(irqState);

    return E_OK;
}

static Std_ReturnType Can_TxQueueDrop(void)
{
    Can_TxQueueElementType* e;
    boolean irqState;

    irqState = Can_EnterCritical();

    if (Can_TxCount == 0u)
    {
        Can_ExitCritical(irqState);
        return E_NOT_OK;
    }

    e = &Can_TxQueue[Can_TxTail];
    e->used = FALSE;

    Can_TxTail++;
    if (Can_TxTail >= CAN_TX_QUEUE_SIZE)
    {
        Can_TxTail = 0u;
    }

    Can_TxCount--;

    Can_ExitCritical(irqState);

    return E_OK;
}

static Std_ReturnType Can_TxQueueRotate(void)
{
    Can_TxQueueElementType temp;
    Can_TxQueueElementType* e;
    boolean irqState;

    irqState = Can_EnterCritical();

    if (Can_TxCount <= 1u)
    {
        Can_ExitCritical(irqState);
        return E_NOT_OK;
    }

    temp = Can_TxQueue[Can_TxTail];
    Can_TxQueue[Can_TxTail].used = FALSE;

    Can_TxTail++;
    if (Can_TxTail >= CAN_TX_QUEUE_SIZE)
    {
        Can_TxTail = 0u;
    }

    e = &Can_TxQueue[Can_TxHead];
    *e = temp;
    memcpy(e->data, temp.data, temp.pdu.dlc);
    e->pdu.sdu = e->data;
    e->used = TRUE;

    Can_TxHead++;
    if (Can_TxHead >= CAN_TX_QUEUE_SIZE)
    {
        Can_TxHead = 0u;
    }

    Can_TxQueueRotateCounter++;

    Can_ExitCritical(irqState);

    return E_OK;
}

static boolean Can_TxPduIsQueuedOrPending(PduIdType swPduHandle, uint8 controllerId)
{
    boolean irqState;
    boolean found = FALSE;
    uint8 idx;
    uint8 count;
    uint8 bufferIdx;
    const Can_TxQueueElementType* e;

    if (controllerId >= CAN_NUM_CONTROLLERS)
    {
        return FALSE;
    }

    irqState = Can_EnterCritical();

    for (bufferIdx = 0u; (found == FALSE) && (bufferIdx < Can_GetTxBufferCount(controllerId)); bufferIdx++)
    {
        if ((Can_TxPending[controllerId][bufferIdx].active != FALSE) &&
            (Can_TxPending[controllerId][bufferIdx].swPduHandle == swPduHandle))
        {
            found = TRUE;
        }
    }

    idx = Can_TxTail;
    for (count = 0u; (found == FALSE) && (count < Can_TxCount); count++)
    {
        e = &Can_TxQueue[idx];

        if ((e->used != FALSE) &&
            (e->pdu.controllerId == controllerId) &&
            (e->pdu.swPduHandle == swPduHandle))
        {
            found = TRUE;
        }

        idx++;
        if (idx >= CAN_TX_QUEUE_SIZE)
        {
            idx = 0u;
        }
    }

    Can_ExitCritical(irqState);

    return found;
}

static void Can_RequeuePendingTxBuffer(uint8 controllerId, uint8 bufferIdx)
{
    Can_TxPendingType* pending;
    Can_PduType pdu;
    uint8 data[CAN_MAX_DLC];

    if ((controllerId >= CAN_NUM_CONTROLLERS) ||
            (bufferIdx >= Can_GetTxBufferCount(controllerId)))
    {
        return;
    }

    pending = &Can_TxPending[controllerId][bufferIdx];

    if (pending->active == FALSE)
    {
        return;
    }

    pdu = pending->pdu;
    memcpy(data, pending->data, pdu.dlc);
    pdu.sdu = data;

    pending->active = FALSE;

    if (Can_TxQueuePush(&pdu) != E_OK)
    {
        pending->pdu = pdu;
        memcpy(pending->data, data, pdu.dlc);
        pending->pdu.sdu = pending->data;
        pending->swPduHandle = pdu.swPduHandle;
        pending->active = TRUE;
        pending->ageTicks = 0u;
    }
}

static void Can_RequeuePendingTx(uint8 controllerId)
{
    uint8 bufferIdx;

    if (controllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    for (bufferIdx = 0u; bufferIdx < Can_GetTxBufferCount(controllerId); bufferIdx++)
    {
        Can_RequeuePendingTxBuffer(controllerId, bufferIdx);
    }
}

static Std_ReturnType Can_RxQueuePush(const Can_FrameType* frame)
{
    Can_RxQueueElementType* e;
    boolean irqState;

    if (frame == NULL_PTR)
    {
        return E_NOT_OK;
    }

    irqState = Can_EnterCritical();

    if (Can_RxCount >= CAN_RX_QUEUE_SIZE)
    {
        if (frame->controllerId < CAN_NUM_CONTROLLERS)
        {
            Can_Runtime[frame->controllerId].rxOverflowCounter++;
        }

        Can_ExitCritical(irqState);
        return E_NOT_OK;
    }

    e = &Can_RxQueue[Can_RxHead];
    e->frame = *frame;
    e->used = TRUE;

    Can_RxHead++;
    if (Can_RxHead >= CAN_RX_QUEUE_SIZE)
    {
        Can_RxHead = 0u;
    }

    Can_RxCount++;

    Can_ExitCritical(irqState);

    return E_OK;
}

static Std_ReturnType Can_RxQueuePop(Can_FrameType* frame)
{
    Can_RxQueueElementType* e;
    boolean irqState;

    if (frame == NULL_PTR)
    {
        return E_NOT_OK;
    }

    irqState = Can_EnterCritical();

    if (Can_RxCount == 0u)
    {
        Can_ExitCritical(irqState);
        return E_NOT_OK;
    }

    e = &Can_RxQueue[Can_RxTail];

    if (e->used == FALSE)
    {
        Can_ExitCritical(irqState);
        return E_NOT_OK;
    }

    *frame = e->frame;
    e->used = FALSE;

    Can_RxTail++;
    if (Can_RxTail >= CAN_RX_QUEUE_SIZE)
    {
        Can_RxTail = 0u;
    }

    Can_RxCount--;

    Can_ExitCritical(irqState);

    return E_OK;
}

static void Can_InitClassicNode(void)
{
    IfxCan_Can_initNodeConfig(&Can_Hw.nodeConfigClassic, &Can_Hw.moduleClassic);

    Can_Hw.nodeConfigClassic.nodeId = IfxCan_NodeId_0;
    Can_Hw.nodeConfigClassic.baudRate.baudrate = 500000u;
    Can_Hw.nodeConfigClassic.calculateBitTimingValues = TRUE;
    Can_Hw.nodeConfigClassic.frame.mode = IfxCan_FrameMode_standard;
    Can_Hw.nodeConfigClassic.frame.type = IfxCan_FrameType_transmitAndReceive;

    Can_Hw.nodeConfigClassic.txConfig.txMode = IfxCan_TxMode_dedicatedBuffers;
    Can_Hw.nodeConfigClassic.txConfig.dedicatedTxBuffersNumber = CAN_TX_HW_BUFFER_COUNT_CLASSIC;
    Can_Hw.nodeConfigClassic.txConfig.txBufferDataFieldSize = IfxCan_DataFieldSize_8;

    Can_Hw.nodeConfigClassic.rxConfig.rxMode = IfxCan_RxMode_fifo0;
    Can_Hw.nodeConfigClassic.rxConfig.rxFifo0DataFieldSize = IfxCan_DataFieldSize_8;
    Can_Hw.nodeConfigClassic.rxConfig.rxBufferDataFieldSize = IfxCan_DataFieldSize_8;
    Can_Hw.nodeConfigClassic.rxConfig.rxFifo0Size = 32u;

    Can_Hw.nodeConfigClassic.filterConfig.messageIdLength = IfxCan_MessageIdLength_both;
    Can_Hw.nodeConfigClassic.filterConfig.standardListSize = CAN_CLASSIC_STD_FILTER_COUNT;
    Can_Hw.nodeConfigClassic.filterConfig.extendedListSize = 0u;
    Can_Hw.nodeConfigClassic.filterConfig.standardFilterForNonMatchingFrames = IfxCan_NonMatchingFrame_reject;
    Can_Hw.nodeConfigClassic.filterConfig.extendedFilterForNonMatchingFrames = IfxCan_NonMatchingFrame_reject;
    Can_Hw.nodeConfigClassic.filterConfig.rejectRemoteFramesWithStandardId = TRUE;
    Can_Hw.nodeConfigClassic.filterConfig.rejectRemoteFramesWithExtendedId = TRUE;

    Can_Hw.nodeConfigClassic.interruptConfig.rxFifo0NewMessageEnabled = TRUE;
    Can_Hw.nodeConfigClassic.interruptConfig.rxf0n.priority = CAN_IRQ_PRIO_RX_CLASSIC       ;
    Can_Hw.nodeConfigClassic.interruptConfig.rxf0n.interruptLine = IfxCan_InterruptLine_0;

    Can_Hw.nodeConfigClassic.interruptConfig.busOffStatusEnabled = TRUE;
    Can_Hw.nodeConfigClassic.interruptConfig.boff.priority = CAN_IRQ_PRIO_BUSOFF_CLASSIC   ;
    Can_Hw.nodeConfigClassic.interruptConfig.boff.interruptLine = IfxCan_InterruptLine_3;

    Can_Hw.nodeConfigClassic.messageRAM.baseAddress = (uint32)Can_Hw.nodeConfigClassic.can;
    Can_Hw.nodeConfigClassic.messageRAM.standardFilterListStartAddress = 0x000u;
    Can_Hw.nodeConfigClassic.messageRAM.extendedFilterListStartAddress = 0x080u;
    Can_Hw.nodeConfigClassic.messageRAM.rxFifo0StartAddress = 0x180u;
    Can_Hw.nodeConfigClassic.messageRAM.txBuffersStartAddress = 0x400u;

    IfxCan_Can_initNode(&Can_Hw.nodeClassic, &Can_Hw.nodeConfigClassic);
    Can_CancelPendingTxBuffers(&Can_Hw.nodeClassic, CAN_TX_HW_BUFFER_COUNT_CLASSIC);

    IfxCan_Node_initRxPin(Can_Hw.nodeClassic.node,
            &IfxCan_RXD00B_P20_7_IN,
            IfxPort_Mode_inputPullUp,
            IfxPort_PadDriver_cmosAutomotiveSpeed1);

    IfxCan_Node_initTxPin(&IfxCan_TXD00_P20_8_OUT,
            IfxPort_OutputMode_pushPull,
            IfxPort_PadDriver_cmosAutomotiveSpeed4);

    CanClassic_TrcvSetNormalMode();

    {
        uint8 i;

        Can_Hw.filter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxFifo0;
        Can_Hw.filter.type = IfxCan_FilterType_classic;

        for (i = 0u; i < CAN_CLASSIC_STD_FILTER_COUNT; i++)
        {
            Can_Hw.filter.number = i;
            Can_Hw.filter.id1 = Can_ClassicStdIdFilters[i].id;
            Can_Hw.filter.id2 = Can_ClassicStdIdFilters[i].id;
            IfxCan_Can_setStandardFilter(&Can_Hw.nodeClassic, &Can_Hw.filter);
        }
    }
}

static void Can_InitFdNode(void)
{
    Can_ReleaseFdRxPinFromScr();

    IfxCan_Can_initNodeConfig(&Can_Hw.nodeConfigFd, &Can_Hw.moduleFd);

    Can_Hw.nodeConfigFd.nodeId = IfxCan_NodeId_3;
    Can_Hw.nodeConfigFd.baudRate.baudrate = 500000u;
    Can_Hw.nodeConfigFd.baudRate.prescaler = 3u;
    Can_Hw.nodeConfigFd.baudRate.timeSegment1 = 14u;
    Can_Hw.nodeConfigFd.baudRate.timeSegment2 = 3u;
    Can_Hw.nodeConfigFd.baudRate.syncJumpWidth = 1u;
    Can_Hw.nodeConfigFd.fastBaudRate.baudrate = 2000000u;
    Can_Hw.nodeConfigFd.fastBaudRate.prescaler = 0u;
    Can_Hw.nodeConfigFd.fastBaudRate.timeSegment1 = 14u;
    Can_Hw.nodeConfigFd.fastBaudRate.timeSegment2 = 3u;
    Can_Hw.nodeConfigFd.fastBaudRate.syncJumpWidth = 1u;
    /* The iLLD wrapper applies baudrate fields only when this is TRUE. */
    Can_Hw.nodeConfigFd.calculateBitTimingValues = TRUE;
    Can_Hw.nodeConfigFd.frame.mode = IfxCan_FrameMode_fdLong;
    Can_Hw.nodeConfigFd.frame.type = IfxCan_FrameType_transmitAndReceive;

    Can_Hw.nodeConfigFd.txConfig.txMode = IfxCan_TxMode_dedicatedBuffers;
    Can_Hw.nodeConfigFd.txConfig.dedicatedTxBuffersNumber = CAN_TX_HW_BUFFER_COUNT_FD;
    Can_Hw.nodeConfigFd.txConfig.txBufferDataFieldSize = IfxCan_DataFieldSize_64;

    Can_Hw.nodeConfigFd.rxConfig.rxMode = IfxCan_RxMode_fifo0;
    Can_Hw.nodeConfigFd.rxConfig.rxFifo0DataFieldSize = IfxCan_DataFieldSize_64;
    Can_Hw.nodeConfigFd.rxConfig.rxBufferDataFieldSize = IfxCan_DataFieldSize_64;
    Can_Hw.nodeConfigFd.rxConfig.rxFifo0Size = 12u;

    Can_Hw.nodeConfigFd.filterConfig.messageIdLength = IfxCan_MessageIdLength_both;
    Can_Hw.nodeConfigFd.filterConfig.standardListSize = CAN_FD_STD_FILTER_COUNT;
    Can_Hw.nodeConfigFd.filterConfig.extendedListSize = 0u;
    Can_Hw.nodeConfigFd.filterConfig.standardFilterForNonMatchingFrames = IfxCan_NonMatchingFrame_reject;
    Can_Hw.nodeConfigFd.filterConfig.extendedFilterForNonMatchingFrames = IfxCan_NonMatchingFrame_reject;
    Can_Hw.nodeConfigFd.filterConfig.rejectRemoteFramesWithStandardId = TRUE;
    Can_Hw.nodeConfigFd.filterConfig.rejectRemoteFramesWithExtendedId = TRUE;

    Can_Hw.nodeConfigFd.interruptConfig.rxFifo0NewMessageEnabled = TRUE;
    Can_Hw.nodeConfigFd.interruptConfig.rxf0n.priority = CAN_IRQ_PRIO_RX_FD             ;
    Can_Hw.nodeConfigFd.interruptConfig.rxf0n.interruptLine = IfxCan_InterruptLine_1;

    Can_Hw.nodeConfigFd.interruptConfig.busOffStatusEnabled = TRUE;
    Can_Hw.nodeConfigFd.interruptConfig.boff.priority = CAN_IRQ_PRIO_BUSOFF_FD        ;
    Can_Hw.nodeConfigFd.interruptConfig.boff.interruptLine = IfxCan_InterruptLine_4;

    Can_Hw.nodeConfigFd.messageRAM.baseAddress = (uint32)Can_Hw.nodeConfigFd.can;
    Can_Hw.nodeConfigFd.messageRAM.standardFilterListStartAddress = 0x800u;
    Can_Hw.nodeConfigFd.messageRAM.extendedFilterListStartAddress = 0x880u;
    Can_Hw.nodeConfigFd.messageRAM.rxFifo0StartAddress = 0x980u;
    Can_Hw.nodeConfigFd.messageRAM.txBuffersStartAddress = 0xD00u;

    IfxCan_Can_initNode(&Can_Hw.nodeFd, &Can_Hw.nodeConfigFd);
    Can_CancelPendingTxBuffers(&Can_Hw.nodeFd, CAN_TX_HW_BUFFER_COUNT_FD);

    IfxCan_Node_initRxPin(Can_Hw.nodeFd.node,
            &IfxCan_RXD13B_P33_5_IN,
            IfxPort_Mode_inputPullUp,
            IfxPort_PadDriver_cmosAutomotiveSpeed1);

    IfxCan_Node_initTxPin(&IfxCan_TXD13_P33_4_OUT,
            IfxPort_OutputMode_pushPull,
            IfxPort_PadDriver_cmosAutomotiveSpeed4);

    {
        uint8 i;

        Can_Hw.filter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxFifo0;
        Can_Hw.filter.type = IfxCan_FilterType_classic;

        for (i = 0u; i < CAN_FD_STD_FILTER_COUNT; i++)
        {
            Can_Hw.filter.number = i;
            Can_Hw.filter.id1 = Can_FdStdIdFilters[i].id;
            Can_Hw.filter.id2 = Can_FdStdIdFilters[i].id;
            IfxCan_Can_setStandardFilter(&Can_Hw.nodeFd, &Can_Hw.filter);
        }
    }
}

static Ifx_CAN_N* Can_GetNodeSfr(uint8 controllerId)
{
    if (controllerId == CAN_CONTROLLER_CLASSIC)
    {
        return Can_Hw.nodeClassic.node;
    }

    if (controllerId == CAN_CONTROLLER_FD)
    {
        return Can_Hw.nodeFd.node;
    }

    return NULL_PTR;
}

static void Can_UpdateErrorCounters(uint8 controllerId)
{
    Ifx_CAN_N* node;
    uint32 ecr;
    uint8 rxErr;
    uint8 txErr;
    Can_ErrorStateType newState;

    node = Can_GetNodeSfr(controllerId);

    if (node == NULL_PTR)
    {
        return;
    }

    ecr = node->ECR.U;

    txErr = (uint8)(ecr & 0xFFu);
    rxErr = (uint8)((ecr >> 8u) & 0x7Fu);

    Can_Runtime[controllerId].txErrorCounter = txErr;
    Can_Runtime[controllerId].rxErrorCounter = rxErr;

    if (Can_ControllerState[controllerId] == CAN_BUS_OFF)
    {
        newState = CAN_ERROR_BUS_OFF;
    }
    else if ((txErr >= CAN_ERROR_PASSIVE_LIMIT) || (rxErr >= CAN_ERROR_PASSIVE_LIMIT))
    {
        newState = CAN_ERROR_PASSIVE;
    }
    else if ((txErr >= CAN_ERROR_WARNING_LIMIT) || (rxErr >= CAN_ERROR_WARNING_LIMIT))
    {
        newState = CAN_ERROR_WARNING;
    }
    else
    {
        newState = CAN_ERROR_ACTIVE;
    }

    if (newState != Can_Runtime[controllerId].errorState)
    {
        Can_Runtime[controllerId].errorState = newState;

        if (newState == CAN_ERROR_PASSIVE)
        {
            CanIf_ControllerErrorPassive(controllerId);
        }
        else if (newState == CAN_ERROR_WARNING)
        {
            CanIf_ControllerErrorWarning(controllerId);
        }
    }
}

static void Can_EnterBusOff(uint8 controllerId)
{
    if (controllerId >= CAN_NUM_CONTROLLERS)
    {
        return;
    }

    if (Can_ControllerState[controllerId] == CAN_BUS_OFF)
    {
        return;
    }

    Can_ControllerState[controllerId] = CAN_BUS_OFF;
    Can_RequeuePendingTx(controllerId);
    Can_Runtime[controllerId].errorState = CAN_ERROR_BUS_OFF;
    Can_Runtime[controllerId].busOffPending = TRUE;
    Can_Runtime[controllerId].busOffRecoveryTicks = CAN_BUSOFF_RECOVERY_TICKS;
    Can_Runtime[controllerId].busOffCounter++;

    CanIf_ControllerBusOff(controllerId);
}

void Can_Init(void)
{
    memset(&Can_Hw, 0, sizeof(Can_Hw));
    Can_ResetQueues();
    memset(Can_Runtime, 0, sizeof(Can_Runtime));

    Can_Runtime[CAN_CONTROLLER_CLASSIC].errorState = CAN_ERROR_ACTIVE;
    Can_Runtime[CAN_CONTROLLER_FD].errorState = CAN_ERROR_ACTIVE;

    Can_ControllerState[CAN_CONTROLLER_CLASSIC] = CAN_UNINIT;
    Can_ControllerState[CAN_CONTROLLER_FD] = CAN_UNINIT;

    Can_InterruptsEnabled[CAN_CONTROLLER_CLASSIC] = TRUE;
    Can_InterruptsEnabled[CAN_CONTROLLER_FD] = TRUE;

    IfxScuCcu_setMcanFrequency(40000000.0f);
    Can_ReleaseFdRxPinFromScr();

    IfxCan_Can_initModuleConfig(&Can_Hw.moduleConfigClassic, &MODULE_CAN0);
    IfxCan_Can_initModule(&Can_Hw.moduleClassic, &Can_Hw.moduleConfigClassic);

    IfxCan_Can_initModuleConfig(&Can_Hw.moduleConfigFd, &MODULE_CAN1);
    IfxCan_Can_initModule(&Can_Hw.moduleFd, &Can_Hw.moduleConfigFd);

    Can_InitClassicNode();
    Can_InitFdNode();

    Can_ControllerState[CAN_CONTROLLER_CLASSIC] = CAN_READY;
    Can_ControllerState[CAN_CONTROLLER_FD] = CAN_READY;
}

Std_ReturnType Can_Write(PduIdType Hth, const Can_PduType* PduInfo)
{
    if (Can_ValidatePdu(Hth, PduInfo) != E_OK)
    {
        return E_NOT_OK;
    }

    return Can_TxQueuePush(PduInfo);
}

Std_ReturnType Can_WriteNoReplace(PduIdType Hth, const Can_PduType* PduInfo)
{
    if (Can_ValidatePdu(Hth, PduInfo) != E_OK)
    {
        return E_NOT_OK;
    }

    return Can_TxQueuePushNoReplace(PduInfo);
}

static IfxCan_Can_Node* Can_GetNode(uint8 controllerId)
{
    if (controllerId == CAN_CONTROLLER_CLASSIC)
    {
        return &Can_Hw.nodeClassic;
    }

    if (controllerId == CAN_CONTROLLER_FD)
    {
        return &Can_Hw.nodeFd;
    }

    return NULL_PTR;
}

static boolean Can_FindFreeTxBuffer(uint8 controllerId, IfxCan_Can_Node* node, uint8* bufferIdx)
{
    uint8 idx;

    if ((controllerId >= CAN_NUM_CONTROLLERS) ||
            (node == NULL_PTR) ||
            (bufferIdx == NULL_PTR))
    {
        return FALSE;
    }

    for (idx = 0u; idx < Can_GetTxBufferCount(controllerId); idx++)
    {
        if ((Can_TxPending[controllerId][idx].active == FALSE) &&
            (IfxCan_Can_isTxBufferRequestPending(node, (IfxCan_TxBufferId)idx) == FALSE))
        {
            *bufferIdx = idx;
            return TRUE;
        }
    }

    return FALSE;
}

static boolean Can_TxQueueHasReadyOtherController(uint8 controllerId)
{
    boolean irqState;
    boolean found = FALSE;
    uint8 idx;
    uint8 count;
    uint8 txBufferIdx;
    Can_PduType pdu;
    const Can_TxQueueElementType* e;
    IfxCan_Can_Node* node;

    irqState = Can_EnterCritical();

    idx = Can_TxTail;
    for (count = 0u; count < Can_TxCount; count++)
    {
        e = &Can_TxQueue[idx];

        if (e->used != FALSE)
        {
            if ((e->pdu.controllerId != controllerId) &&
                    (e->pdu.controllerId < CAN_NUM_CONTROLLERS))
            {
                pdu = e->pdu;
                found = TRUE;
                break;
            }
        }

        idx++;
        if (idx >= CAN_TX_QUEUE_SIZE)
        {
            idx = 0u;
        }
    }

    Can_ExitCritical(irqState);

    if (found == FALSE)
    {
        return FALSE;
    }

    if (Can_ControllerState[pdu.controllerId] != CAN_READY)
    {
        return FALSE;
    }

    node = Can_GetNode(pdu.controllerId);

    if (node == NULL_PTR)
    {
        return FALSE;
    }

    return Can_FindFreeTxBuffer(pdu.controllerId, node, &txBufferIdx);
}

static void Can_ProcessTxConfirmations(void)
{
    IfxCan_Can_Node* node;
    PduIdType swPduHandle;
    uint8 controller;
    uint8 bufferIdx;
    uint8 bufferCount;
    IfxCan_TxBufferId txBufferId;

    for (controller = 0u; controller < CAN_NUM_CONTROLLERS; controller++)
    {
        node = Can_GetNode(controller);
        bufferCount = Can_GetTxBufferCount(controller);

        for (bufferIdx = 0u; bufferIdx < bufferCount; bufferIdx++)
        {
            txBufferId = (IfxCan_TxBufferId)bufferIdx;

            if (Can_TxPending[controller][bufferIdx].active == FALSE)
            {
                continue;
            }

            if ((node == NULL_PTR) || (Can_ControllerState[controller] != CAN_READY))
            {
                Can_RequeuePendingTxBuffer(controller, bufferIdx);
                continue;
            }

            if (IfxCan_Node_isTxBufferTransmissionOccured(node->node, txBufferId) != FALSE)
            {
                swPduHandle = Can_TxPending[controller][bufferIdx].swPduHandle;
                Can_TxPending[controller][bufferIdx].active = FALSE;
                CanIf_TxConfirmation(swPduHandle);
            }
            else if (IfxCan_Can_isTxBufferRequestPending(node, txBufferId) == FALSE)
            {
                if (Can_TxSyncPolling == FALSE)
                {
                    Can_TxPending[controller][bufferIdx].ageTicks++;

                    if (Can_TxPending[controller][bufferIdx].ageTicks >= CAN_TX_HW_PENDING_TIMEOUT_TICKS)
                    {
                        Can_RequeuePendingTxBuffer(controller, bufferIdx);
                        Can_Runtime[controller].txFailCounter++;
                    }
                }
            }
            else
            {
                if (Can_TxSyncPolling == FALSE)
                {
                    Can_TxPending[controller][bufferIdx].ageTicks++;

                    if (Can_TxPending[controller][bufferIdx].ageTicks >= CAN_TX_HW_PENDING_TIMEOUT_TICKS)
                    {
                        if (Can_CancelTxBuffer(node, txBufferId) != FALSE)
                        {
                            if (IfxCan_Node_isTxBufferTransmissionOccured(node->node, txBufferId) != FALSE)
                            {
                                swPduHandle = Can_TxPending[controller][bufferIdx].swPduHandle;
                                Can_TxPending[controller][bufferIdx].active = FALSE;
                                CanIf_TxConfirmation(swPduHandle);
                            }
                            else
                            {
                                Can_RequeuePendingTxBuffer(controller, bufferIdx);
                                Can_Runtime[controller].txFailCounter++;
                            }
                        }
                        else
                        {
                            Can_TxPending[controller][bufferIdx].ageTicks = 0u;
                            Can_Runtime[controller].txFailCounter++;
                        }
                    }
                }
            }
        }
    }
}

static void Can_CopyBytesToWords(uint32* words, const uint8* bytes, uint8 len)
{
    uint8 i;
    uint8* dst;

    memset(words, 0, 16u * sizeof(uint32));

    dst = (uint8*)words;

    for (i = 0u; i < len; i++)
    {
        dst[i] = bytes[i];
    }
}

static void Can_CopyWordsToBytes(uint8* bytes, const uint32* words, uint8 len)
{
    uint8 i;
    const uint8* src;

    src = (const uint8*)words;

    for (i = 0u; i < len; i++)
    {
        bytes[i] = src[i];
    }
}

static void Can_ProcessTx(void)
{
    Can_PduType pdu;
    IfxCan_Can_Node* node;
    uint8 budget = CAN_TX_BUDGET_PER_MAIN;
    uint8 attempts = CAN_TX_ATTEMPTS_PER_MAIN;
    uint8 txBufferIdx;
    Can_TxPendingType* pending;

    Can_ProcessTxConfirmations();

    while ((budget > 0u) && (attempts > 0u))
    {
        attempts--;

        if (Can_TxQueuePeek(&pdu) != E_OK)
        {
            return;
        }

        if (pdu.controllerId >= CAN_NUM_CONTROLLERS)
        {
            (void)Can_TxQueueDrop();
            continue;
        }

        if (Can_ControllerState[pdu.controllerId] != CAN_READY)
        {
            Can_TxControllerNotReadyCounter++;

            if ((Can_TxQueueHasReadyOtherController(pdu.controllerId) != FALSE) &&
                    (Can_TxQueueRotate() == E_OK))
            {
                continue;
            }

            return;
        }

        node = Can_GetNode(pdu.controllerId);

        if (node == NULL_PTR)
        {
            (void)Can_TxQueueDrop();
            continue;
        }

        if (Can_FindFreeTxBuffer(pdu.controllerId, node, &txBufferIdx) == FALSE)
        {
            Can_TxPendingBusyCounter++;

            if ((Can_TxQueueHasReadyOtherController(pdu.controllerId) != FALSE) &&
                    (Can_TxQueueRotate() == E_OK))
            {
                continue;
            }

            return;
        }

        Can_CopyBytesToWords(Can_Hw.txWords, pdu.sdu, pdu.dlc);

        IfxCan_Can_initMessage(&Can_Hw.txMsg);

        Can_Hw.txMsg.messageId = pdu.id;
        Can_Hw.txMsg.dataLengthCode = Can_BytesToDlc(pdu.dlc);

        Can_Hw.txMsg.messageIdLength =
                (pdu.idType == CAN_ID_EXTENDED) ? IfxCan_MessageIdLength_extended : IfxCan_MessageIdLength_standard;

        Can_Hw.txMsg.frameMode =
                (pdu.frameType == CAN_FRAME_FD) ? IfxCan_FrameMode_fdLong : IfxCan_FrameMode_standard;
        Can_Hw.txMsg.bufferNumber = txBufferIdx;

        if (IfxCan_Can_sendMessage(node, &Can_Hw.txMsg, Can_Hw.txWords) == IfxCan_Status_ok)
        {
            pending = &Can_TxPending[pdu.controllerId][txBufferIdx];
            pending->active = TRUE;
            pending->swPduHandle = pdu.swPduHandle;
            pending->pdu = pdu;
            memcpy(pending->data, pdu.sdu, pdu.dlc);
            pending->pdu.sdu = pending->data;
            pending->ageTicks = 0u;
            (void)Can_TxQueueDrop();
            budget--;
        }
        else
        {
            if (pdu.controllerId < CAN_NUM_CONTROLLERS)
            {
                Can_Runtime[pdu.controllerId].txFailCounter++;
            }

            Can_TxSendRetryCounter++;

            if ((Can_TxQueueHasReadyOtherController(pdu.controllerId) != FALSE) &&
                    (Can_TxQueueRotate() == E_OK))
            {
                continue;
            }

            return;
        }
    }

    if ((budget > 0u) && (attempts == 0u))
    {
        Can_TxProcessAttemptLimitCounter++;
    }
}

Std_ReturnType Can_DrainTxPdu(PduIdType SwPduHandle, uint8 ControllerId, uint32 PollLimit)
{
    uint32 poll;

    if ((ControllerId >= CAN_NUM_CONTROLLERS) || (PollLimit == 0u))
    {
        return E_NOT_OK;
    }

    for (poll = 0u; poll < PollLimit; poll++)
    {
        Can_TxSyncPolling = TRUE;
        Can_ProcessTx();
        Can_TxSyncPolling = FALSE;

        if (Can_TxPduIsQueuedOrPending(SwPduHandle, ControllerId) == FALSE)
        {
            return E_OK;
        }

        if (Can_ControllerState[ControllerId] != CAN_READY)
        {
            return E_NOT_OK;
        }
    }

    return E_NOT_OK;
}

static void Can_ProcessRx(void)
{
    Can_FrameType frame;

    while (Can_RxQueuePop(&frame) == E_OK)
    {
        CanIf_RxIndication(&frame);
    }
}

void Can_MainFunction(void)
{
    Can_MainFunction_BusOff();
    Can_MainFunction_Mode();
    Can_MainFunction_Read();
    Can_MainFunction_Write();
    Can_MainFunction_Counter++;
}

void Can_MainFunction_BusOff(void)
{
    if ((Can_ControllerState[CAN_CONTROLLER_CLASSIC] == CAN_READY) &&
            (IfxCan_Node_getBusOffStatus(Can_Hw.nodeClassic.node) != 0u))
    {
        Can_EnterBusOff(CAN_CONTROLLER_CLASSIC);
    }

    if ((Can_ControllerState[CAN_CONTROLLER_FD] == CAN_READY) &&
            (IfxCan_Node_getBusOffStatus(Can_Hw.nodeFd.node) != 0u))
    {
        Can_EnterBusOff(CAN_CONTROLLER_FD);
    }

    Can_UpdateErrorCounters(CAN_CONTROLLER_CLASSIC);
    Can_UpdateErrorCounters(CAN_CONTROLLER_FD);
}

void Can_MainFunction_Mode(void)
{
    uint8 controller;

    for (controller = 0u; controller < CAN_NUM_CONTROLLERS; controller++)
    {
        if ((Can_ControllerState[controller] == CAN_BUS_OFF) &&
                (Can_Runtime[controller].busOffPending != FALSE))
        {
            if (Can_Runtime[controller].busOffRecoveryTicks > 0u)
            {
                Can_Runtime[controller].busOffRecoveryTicks--;
            }
            else
            {
                if (Can_SetControllerMode(controller, CAN_READY) == E_OK)
                {
                    Can_Runtime[controller].busOffPending = FALSE;
                    Can_Runtime[controller].errorState = CAN_ERROR_ACTIVE;
                    Can_Runtime[controller].recoveredCounter++;
                    CanIf_ControllerRecovered(controller);
                }
            }
        }
    }
}

void Can_MainFunction_Read(void)
{
    Can_ReadRxFifo(CAN_CONTROLLER_CLASSIC);
    Can_ReadRxFifo(CAN_CONTROLLER_FD);
    Can_ProcessRx();
}

void Can_MainFunction_Write(void)
{
    Can_ProcessTx();
}

static void Can_ReadRxFifo(uint8 controllerId)
{
    Can_FrameType frame;
    IfxCan_Can_Node* node;
    IfxCan_Message rxMsg;
    uint32 rxWords[16];
    uint8 budget = CAN_RX_FIFO_DRAIN_BUDGET;
    uint8 acceptedActivity = FALSE;

    node = Can_GetNode(controllerId);

    if (node == NULL_PTR)
    {
        return;
    }

    while ((IfxCan_Can_getRxFifo0FillLevel(node) > 0u) && (budget > 0u))
    {
        budget--;
        memset(&frame, 0, sizeof(frame));
        memset(rxWords, 0, sizeof(rxWords));
        IfxCan_Can_initMessage(&rxMsg);
        rxMsg.readFromRxFifo0 = TRUE;

        IfxCan_Can_readMessage(node, &rxMsg, rxWords);

        frame.controllerId = controllerId;
        frame.id = rxMsg.messageId;
        frame.idType = (rxMsg.messageIdLength == IfxCan_MessageIdLength_extended) ? CAN_ID_EXTENDED : CAN_ID_STANDARD;
        frame.frameType = (rxMsg.frameMode == IfxCan_FrameMode_standard) ? CAN_FRAME_CLASSIC : CAN_FRAME_FD;
        frame.dlc = Can_DlcToBytes((uint8)rxMsg.dataLengthCode);

        if ((controllerId == CAN_CONTROLLER_CLASSIC) && (frame.frameType != CAN_FRAME_CLASSIC))
        {
            continue;
        }

        if ((frame.frameType == CAN_FRAME_CLASSIC) && (frame.dlc > CAN_CLASSIC_MAX_DLC))
        {
            continue;
        }

        if (frame.dlc > CAN_MAX_DLC)
        {
            frame.dlc = CAN_MAX_DLC;
        }

        Can_CopyWordsToBytes(frame.data, rxWords, frame.dlc);
        acceptedActivity = TRUE;
        (void)Can_RxQueuePush(&frame);
    }

    if (acceptedActivity != FALSE)
    {
        SysMgr_NotifyBusActivity();
    }
}

void Can_IsrRxClassic(void)
{
    if (Can_InterruptsEnabled[CAN_CONTROLLER_CLASSIC] != FALSE)
    {
        Can_ReadRxFifo(CAN_CONTROLLER_CLASSIC);
    }

    IfxCan_Node_clearInterruptFlag(Can_Hw.nodeClassic.node, IfxCan_Interrupt_rxFifo0NewMessage);
}

void Can_IsrRxFd(void)
{
    if (Can_InterruptsEnabled[CAN_CONTROLLER_FD] != FALSE)
    {
        Can_ReadRxFifo(CAN_CONTROLLER_FD);
    }

    IfxCan_Node_clearInterruptFlag(Can_Hw.nodeFd.node, IfxCan_Interrupt_rxFifo0NewMessage);
}

void Can_IsrBusOffClassic(void)
{
    Can_EnterBusOff(CAN_CONTROLLER_CLASSIC);
}

void Can_IsrBusOffFd(void)
{
    Can_EnterBusOff(CAN_CONTROLLER_FD);
}

Can_ControllerStateType Can_GetControllerState(uint8 controllerId)
{
    if (controllerId >= CAN_NUM_CONTROLLERS)
    {
        return CAN_UNINIT;
    }

    return Can_ControllerState[controllerId];
}

Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_ControllerStateType Transition)
{
    if (Controller >= CAN_NUM_CONTROLLERS)
    {
        return E_NOT_OK;
    }

    switch (Transition)
    {
        case CAN_READY:
            if (Can_ControllerState[Controller] != CAN_READY)
            {
                if (Controller == CAN_CONTROLLER_CLASSIC)
                {
                    Can_InitClassicNode();
                }
                else
                {
                    Can_InitFdNode();
                }
            }
            else
            {
                /* Already initialized. */
            }

            Can_RequeuePendingTx(Controller);
            Can_ControllerState[Controller] = CAN_READY;
            return E_OK;

        case CAN_SLEEP:
            Can_RequeuePendingTx(Controller);
            Can_ControllerState[Controller] = CAN_SLEEP;
            return E_OK;

        case CAN_BUS_OFF:
            Can_RequeuePendingTx(Controller);
            Can_ControllerState[Controller] = CAN_BUS_OFF;
            return E_OK;

        case CAN_UNINIT:
            Can_RequeuePendingTx(Controller);
            Can_ControllerState[Controller] = CAN_UNINIT;
            return E_OK;

        default:
            return E_NOT_OK;
    }
}

Can_ControllerStateType Can_GetControllerMode(uint8 Controller)
{
    return Can_GetControllerState(Controller);
}

Std_ReturnType Can_GetControllerRxErrorCounter(uint8 Controller, uint8* RxErrorCounterPtr)
{
    if ((Controller >= CAN_NUM_CONTROLLERS) || (RxErrorCounterPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    Can_UpdateErrorCounters(Controller);
    *RxErrorCounterPtr = Can_Runtime[Controller].rxErrorCounter;

    return E_OK;
}

Std_ReturnType Can_GetControllerTxErrorCounter(uint8 Controller, uint8* TxErrorCounterPtr)
{
    if ((Controller >= CAN_NUM_CONTROLLERS) || (TxErrorCounterPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    Can_UpdateErrorCounters(Controller);
    *TxErrorCounterPtr = Can_Runtime[Controller].txErrorCounter;

    return E_OK;
}

Can_ErrorStateType Can_GetControllerErrorState(uint8 Controller)
{
    if (Controller >= CAN_NUM_CONTROLLERS)
    {
        return CAN_ERROR_BUS_OFF;
    }

    Can_UpdateErrorCounters(Controller);

    return Can_Runtime[Controller].errorState;
}

void Can_EnableControllerInterrupts(uint8 Controller)
{
    if (Controller < CAN_NUM_CONTROLLERS)
    {
        Can_InterruptsEnabled[Controller] = TRUE;
    }
}

void Can_DisableControllerInterrupts(uint8 Controller)
{
    if (Controller < CAN_NUM_CONTROLLERS)
    {
        Can_InterruptsEnabled[Controller] = FALSE;
    }
}

IFX_INTERRUPT(Can_IrqRxClassic, 0, CAN_IRQ_PRIO_RX_CLASSIC);
void Can_IrqRxClassic(void)
{
    Can_IsrRxClassic();
}

IFX_INTERRUPT(Can_IrqRxFd, 0, CAN_IRQ_PRIO_RX_FD);
void Can_IrqRxFd(void)
{
    Can_IsrRxFd();
}

IFX_INTERRUPT(Can_IrqBusOffClassic, 0, CAN_IRQ_PRIO_BUSOFF_CLASSIC);
void Can_IrqBusOffClassic(void)
{
    Can_IsrBusOffClassic();
}

IFX_INTERRUPT(Can_IrqBusOffFd, 0, CAN_IRQ_PRIO_BUSOFF_FD);
void Can_IrqBusOffFd(void)
{
    Can_IsrBusOffFd();
}
