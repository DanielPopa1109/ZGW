#include <string.h>
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "IfxScuRcu.h"
#include "IfxFlash.h"
#include "IfxCan_Can.h"
#include "aurix_pin_mappings.h"

#define FBL_TRANSPORT_CANFD              1u
#define FBL_TRANSPORT_ETH                2u

#define FBL_START_NCACHED                0xA0000000u
#define FBL_END_NCACHED                  0xA002FFFFu
#define FBL_SIZE_BYTES                   0x00030000u

#define APP_START_NCACHED                0xA0030000u
#define APP_END_NCACHED                  0xA07FFFFFu

#define PFLASH_BANK_A_START              0xA0000000u
#define PFLASH_BANK_A_END                0xA03FFFFFu
#define PFLASH_BANK_B_START              0xA0400000u
#define PFLASH_BANK_B_END                0xA07FFFFFu

#define PFLASH_PAGE_SIZE                 32u
#define PFLASH_SECTOR_SIZE               0x4000u

#define FLASH_MODULE                     0u
#define PROGRAM_FLASH_BANK_A             IfxFlash_FlashType_P0
#define PROGRAM_FLASH_BANK_B             IfxFlash_FlashType_P1

#define FBL_CAN_REQ_ID                   0x710u
#define FBL_CAN_RES_ID                   0x709u
#define FBL_CAN_RX_PRIO                  1u
#define FBL_CAN_MAX_DL                   64u

#define FBL_ISOTP_MAX_PAYLOAD            4095u
#define FBL_ISOTP_FC_TIMEOUT_LOOPS       1000000u
#define FBL_ISOTP_FC_WFT_MAX             8u
#define FBL_CAN_TX_BUSY_RETRY_MAX        100000u

#define UDS_SID_SESSION                  0x10u
#define UDS_SID_RESET                    0x11u
#define UDS_SID_RDBI                     0x22u
#define UDS_SID_ROUTINE                  0x31u
#define UDS_SID_REQ_DOWNLOAD             0x34u
#define UDS_SID_TRANSFER_DATA            0x36u
#define UDS_SID_TRANSFER_EXIT            0x37u
#define UDS_SID_TESTER_PRESENT           0x3Eu
#define UDS_NEG_RESP                     0x7Fu

#define UDS_NRC_INCORRECT_LEN            0x13u
#define UDS_NRC_COND_NOT_CORRECT         0x22u
#define UDS_NRC_OUT_OF_RANGE             0x31u
#define UDS_NRC_TRANSFER_FAIL            0x70u
#define UDS_NRC_WRONG_BLOCK_SEQUENCE     0x73u

#define UDS_RID_ERASE_APP                0x0001u
#define UDS_RID_CRC_CHECK                0x0002u
#define UDS_RID_START_FBL_RAM_UPDATER    0x0155u

#define DOIP_TCP_PORT                    13400u
#define DOIP_UDP_PORT                    13400u
#define DOIP_PROTO_VER                   0x02u
#define DOIP_INV_PROTO_VER               0xFDu
#define DOIP_PT_VID_REQ                  0x0001u
#define DOIP_PT_VID_RES                  0x0004u
#define DOIP_PT_ROUTING_ACT_REQ          0x0005u
#define DOIP_PT_ROUTING_ACT_RES          0x0006u
#define DOIP_PT_DIAG_MSG                 0x8001u
#define DOIP_PT_DIAG_ACK                 0x8002u
#define DOIP_TESTER_ADDR                 0x0E00u
#define DOIP_ECU_ADDR                    0x1000u

#define FBL_FLASH_FUNC_BASE              0x70100000u
#define FBL_FLASH_FUNC_LEN               192u
#define FBL_FLASH_ERASE_ADDR             (FBL_FLASH_FUNC_BASE)
#define FBL_FLASH_WAIT_ADDR              (FBL_FLASH_ERASE_ADDR + FBL_FLASH_FUNC_LEN)
#define FBL_FLASH_ENTER_ADDR             (FBL_FLASH_WAIT_ADDR + FBL_FLASH_FUNC_LEN)
#define FBL_FLASH_LOAD_ADDR              (FBL_FLASH_ENTER_ADDR + FBL_FLASH_FUNC_LEN)
#define FBL_FLASH_WRITE_ADDR             (FBL_FLASH_LOAD_ADDR + FBL_FLASH_FUNC_LEN)

extern uint8 __ram_code_start[];
extern uint8 __ram_code_end[];
extern uint8 __ram_code_load[];
extern void FblRamUpdater_Entry(void);

extern void FblEth_Init(void);
extern void FblEth_MainFunction(void);
extern uint8 FblEth_TcpReceive(uint8 *buf, uint16 *len);
extern uint8 FblEth_UdpReceive(uint8 *buf, uint16 *len);
extern void FblEth_TcpSend(const uint8 *buf, uint16 len);
extern void FblEth_UdpSend(const uint8 *buf, uint16 len);

volatile uint32 g_FblTransportSelect = FBL_TRANSPORT_CANFD;
volatile uint32 g_FblStayInBoot = 1u;
volatile uint32 g_FblStartRamUpdater = 0u;

typedef struct
{
    IfxCan_Can_Config canConfig;
    IfxCan_Can canModule;
    IfxCan_Can_Node canNode;
    IfxCan_Can_NodeConfig nodeConfig;
    IfxCan_Filter filter;
    IfxCan_Message rxMsg;
    IfxCan_Message txMsg;
    uint8 rxData[FBL_CAN_MAX_DL];
    uint8 txData[FBL_CAN_MAX_DL];
} FblCan_Type;

typedef struct
{
    uint8 rxBuf[FBL_ISOTP_MAX_PAYLOAD];
    uint8 txBuf[FBL_ISOTP_MAX_PAYLOAD];
    uint16 rxLen;
    uint16 rxIdx;
    uint16 txLen;
    uint16 txIdx;
    uint8 nextSn;
    uint8 txSn;
    uint8 rxReady;
    uint8 txActive;
    volatile uint8 waitFc;
    volatile uint8 fcStatus;
    volatile uint8 fcBlockSize;
    volatile uint8 fcStMin;
} FblIsoTp_Type;

typedef struct
{
    void (*eraseSectors)(uint32 sectorAddr, uint32 numSector);
    uint8 (*waitUnbusy)(uint32 flash, IfxFlash_FlashType flashType);
    uint8 (*enterPageMode)(uint32 pageAddr);
    void (*load2X32)(uint32 pageAddr, uint32 wordL, uint32 wordU);
    void (*writePage)(uint32 pageAddr);
} FblFlashCmd_Type;

typedef struct
{
    uint8 active;
    uint32 startAddr;
    uint32 curAddr;
    uint32 length;
    uint32 received;
    uint8 nextBlock;
} FblDownload_Type;

static FblCan_Type g_can;
static FblIsoTp_Type g_iso;
static FblFlashCmd_Type g_flash;
static FblDownload_Type g_dl;
static uint8 g_pageBuf[PFLASH_PAGE_SIZE];
static uint32 g_pageAddr = 0xFFFFFFFFu;
static uint32 g_pageFill = 0u;

IFX_INTERRUPT(Fbl_CanRxIsr, 0u, FBL_CAN_RX_PRIO);

static void Fbl_PlatformInit(void);
static void Fbl_CanInit(void);
static void Fbl_CanSend(const uint8 *data, uint8 len);
static uint8 Fbl_CanDlcFromLen(uint8 len);
static uint8 Fbl_CanLenFromDlc(uint8 dlc);
static void Fbl_IsoTpInit(void);
static void Fbl_IsoTpRx(const uint8 *data, uint8 len);
static void Fbl_IsoTpSend(const uint8 *data, uint16 len);
static void Fbl_IsoTpSendFc(void);
static uint8 Fbl_IsoTpWaitFc(void);
static void Fbl_IsoTpDelayStMin(uint8 stMin);
static void Fbl_UdsHandle(const uint8 *req, uint16 len, uint8 transport);
static void Fbl_UdsSend(const uint8 *res, uint16 len, uint8 transport);
static void Fbl_UdsNeg(uint8 sid, uint8 nrc, uint8 transport);
static void Fbl_DoIpMain(void);
static void Fbl_DoIpHandleUdp(const uint8 *buf, uint16 len);
static void Fbl_DoIpHandleTcp(const uint8 *buf, uint16 len);
static void Fbl_DoIpSendDiag(const uint8 *uds, uint16 udsLen);
static void Fbl_FlashInit(void);
static uint32 Fbl_FlashEraseRange(uint32 addr, uint32 len);
static uint32 Fbl_FlashProgram(uint32 addr, const uint8 *data, uint32 len);
static uint32 Fbl_FlashFlush(void);
static uint32 Fbl_FlashProgramPage(uint32 addr, const uint8 *data);
static IfxFlash_FlashType Fbl_FlashBank(uint32 addr);
static uint32 Fbl_Crc32(uint32 addr, uint32 len);
static uint8 Fbl_AppRangeValid(uint32 addr, uint32 len);
static uint32 Fbl_Rd32(const uint8 *p);
static uint16 Fbl_Rd16(const uint8 *p);
static void Fbl_Wr16(uint8 *p, uint16 v);
static void Fbl_Wr32(uint8 *p, uint32 v);
static void Fbl_JumpToApp(void);
static void Fbl_CopyAndJumpRamUpdater(void);

void core0_main(void)
{
    Fbl_PlatformInit();
    Fbl_FlashInit();
    Fbl_IsoTpInit();

    if(g_FblTransportSelect == FBL_TRANSPORT_ETH)
    {
        FblEth_Init();
    }
    else
    {
        g_FblTransportSelect = FBL_TRANSPORT_CANFD;
        Fbl_CanInit();
    }

    if(g_FblStayInBoot == 1u)
    {
        Fbl_JumpToApp();
    }

    while(1)
    {
        if(g_FblStartRamUpdater != 0u)
        {
            Fbl_CopyAndJumpRamUpdater();
        }

        if(g_FblTransportSelect == FBL_TRANSPORT_ETH)
        {
            Fbl_DoIpMain();
        }

        if(g_iso.rxReady != 0u)
        {
            g_iso.rxReady = 0u;
            Fbl_UdsHandle(g_iso.rxBuf, g_iso.rxLen, FBL_TRANSPORT_CANFD);
        }
    }
}

static void Fbl_PlatformInit(void)
{
    IfxCpu_disableInterrupts();
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());
    gpio_init_pins();
    can0_node0_init_pins();
    IfxCpu_enableInterrupts();
}

static void Fbl_CanInit(void)
{
    IfxScuWdt_clearCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_clearSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCan_Can_initModuleConfig(&g_can.canConfig, &MODULE_CAN0);
    IfxCan_Can_initModule(&g_can.canModule, &g_can.canConfig);
    IfxCan_Can_initNodeConfig(&g_can.nodeConfig, &g_can.canModule);

    IfxScuCcu_setMcanFrequency(40000000.0f);

    g_can.nodeConfig.frame.mode = IfxCan_FrameMode_fdLongAndFast;
    g_can.nodeConfig.frame.type = IfxCan_FrameType_transmitAndReceive;
    g_can.nodeConfig.baudRate.baudrate = 500000u;
    g_can.nodeConfig.baudRate.prescaler = 3u;
    g_can.nodeConfig.baudRate.timeSegment1 = 14u;
    g_can.nodeConfig.baudRate.timeSegment2 = 3u;
    g_can.nodeConfig.baudRate.syncJumpWidth = 1u;
    g_can.nodeConfig.fastBaudRate.baudrate = 2000000u;
    g_can.nodeConfig.fastBaudRate.prescaler = 0u;
    g_can.nodeConfig.fastBaudRate.timeSegment1 = 14u;
    g_can.nodeConfig.fastBaudRate.timeSegment2 = 3u;
    g_can.nodeConfig.fastBaudRate.syncJumpWidth = 1u;
    g_can.nodeConfig.calculateBitTimingValues = TRUE;

    g_can.nodeConfig.txConfig.txMode = IfxCan_TxMode_dedicatedBuffers;
    g_can.nodeConfig.txConfig.dedicatedTxBuffersNumber = 1u;
    g_can.nodeConfig.txConfig.txBufferDataFieldSize = IfxCan_DataFieldSize_64;

    g_can.nodeConfig.rxConfig.rxMode = IfxCan_RxMode_dedicatedBuffers;
    g_can.nodeConfig.rxConfig.rxBufferDataFieldSize = IfxCan_DataFieldSize_64;

    g_can.nodeConfig.filterConfig.messageIdLength = IfxCan_MessageIdLength_standard;
    g_can.nodeConfig.filterConfig.standardListSize = 1u;
    g_can.nodeConfig.filterConfig.extendedListSize = 0u;
    g_can.nodeConfig.filterConfig.standardFilterForNonMatchingFrames = IfxCan_NonMatchingFrame_reject;
    g_can.nodeConfig.filterConfig.extendedFilterForNonMatchingFrames = IfxCan_NonMatchingFrame_reject;
    g_can.nodeConfig.filterConfig.rejectRemoteFramesWithStandardId = TRUE;
    g_can.nodeConfig.filterConfig.rejectRemoteFramesWithExtendedId = TRUE;

    g_can.nodeConfig.interruptConfig.messageStoredToDedicatedRxBufferEnabled = TRUE;
    g_can.nodeConfig.interruptConfig.reint.priority = FBL_CAN_RX_PRIO;
    g_can.nodeConfig.interruptConfig.reint.interruptLine = IfxCan_InterruptLine_0;
    g_can.nodeConfig.interruptConfig.reint.typeOfService = IfxSrc_Tos_cpu0;

    g_can.nodeConfig.messageRAM.baseAddress = (uint32)g_can.nodeConfig.can;
    g_can.nodeConfig.messageRAM.standardFilterListStartAddress = 0x000u;
    g_can.nodeConfig.messageRAM.extendedFilterListStartAddress = 0x080u;
    g_can.nodeConfig.messageRAM.rxBuffersStartAddress = 0x100u;
    g_can.nodeConfig.messageRAM.txBuffersStartAddress = 0x500u;

    IfxCan_Can_initNode(&g_can.canNode, &g_can.nodeConfig);

    g_can.filter.number = 0u;
    g_can.filter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
    g_can.filter.id1 = FBL_CAN_REQ_ID;
    g_can.filter.rxBufferOffset = IfxCan_RxBufferId_0;
    IfxCan_Can_setStandardFilter(&g_can.canNode, &g_can.filter);

    IfxCan_Node_initRxPin(g_can.canNode.node, &IfxCan_RXD00B_P20_7_IN, IfxPort_Mode_inputPullUp, IfxPort_PadDriver_cmosAutomotiveSpeed1);
    IfxCan_Node_initTxPin(&IfxCan_TXD00_P20_8_OUT, IfxPort_OutputMode_pushPull, IfxPort_PadDriver_cmosAutomotiveSpeed4);

    IfxScuWdt_setCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_setSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());
}

void Fbl_CanRxIsr(void)
{
    uint8 len;

    IfxCan_Node_clearInterruptFlag(g_can.canNode.node, IfxCan_Interrupt_messageStoredToDedicatedRxBuffer);
    g_can.rxMsg.bufferNumber = 0u;

    if(IfxCan_Can_isNewDataReceived(&g_can.canNode, 0u) != 0u)
    {
        IfxCan_Can_readMessage(&g_can.canNode, &g_can.rxMsg, (uint32 *)g_can.rxData);

        if(g_can.rxMsg.messageId == FBL_CAN_REQ_ID)
        {
            len = Fbl_CanLenFromDlc((uint8)g_can.rxMsg.dataLengthCode);
            Fbl_IsoTpRx(g_can.rxData, len);
        }

        IfxCan_Node_clearRxBufferNewDataFlag(g_can.canNode.node, 0u);
    }
}

static void Fbl_CanSend(const uint8 *data, uint8 len)
{
    uint32 retry = FBL_CAN_TX_BUSY_RETRY_MAX;

    memset(g_can.txData, 0u, sizeof(g_can.txData));
    memcpy(g_can.txData, data, len);

    IfxCan_Can_initMessage(&g_can.txMsg);
    g_can.txMsg.messageId = FBL_CAN_RES_ID;
    g_can.txMsg.frameMode = IfxCan_FrameMode_fdLongAndFast;
    g_can.txMsg.messageIdLength = IfxCan_MessageIdLength_standard;
    g_can.txMsg.dataLengthCode = Fbl_CanDlcFromLen(len);
    g_can.txMsg.bufferNumber = 0u;

    while((retry > 0u) &&
          (IfxCan_Can_sendMessage(&g_can.canNode, &g_can.txMsg, (uint32 *)g_can.txData) == IfxCan_Status_notSentBusy))
    {
        retry--;
    }
}

static uint8 Fbl_CanDlcFromLen(uint8 len)
{
    if(len <= 8u) { return len; }
    if(len <= 12u) { return 9u; }
    if(len <= 16u) { return 10u; }
    if(len <= 20u) { return 11u; }
    if(len <= 24u) { return 12u; }
    if(len <= 32u) { return 13u; }
    if(len <= 48u) { return 14u; }
    return 15u;
}

static uint8 Fbl_CanLenFromDlc(uint8 dlc)
{
    static const uint8 map[16] = {0u,1u,2u,3u,4u,5u,6u,7u,8u,12u,16u,20u,24u,32u,48u,64u};
    return map[dlc & 0x0Fu];
}

static void Fbl_IsoTpInit(void)
{
    memset(&g_iso, 0u, sizeof(g_iso));
}

static void Fbl_IsoTpSendFc(void)
{
    uint8 fc[3] = {0x30u, 0x00u, 0x00u};
    Fbl_CanSend(fc, 3u);
}

static void Fbl_IsoTpRx(const uint8 *data, uint8 len)
{
    uint8 pci;
    uint16 ffLen;
    uint8 copy;
    uint8 sn;

    if(len == 0u) { return; }

    pci = data[0u] & 0xF0u;

    if(pci == 0x00u)
    {
        uint8 sfLen = data[0u] & 0x0Fu;
        uint8 offset = 1u;

        if(sfLen == 0u)
        {
            if(len < 2u) { return; }
            sfLen = data[1u];
            offset = 2u;
        }

        if(sfLen == 0u) { return; }

        if((sfLen <= (len - offset)) && (sfLen <= FBL_ISOTP_MAX_PAYLOAD))
        {
            memcpy(g_iso.rxBuf, &data[offset], sfLen);
            g_iso.rxLen = sfLen;
            g_iso.rxReady = 1u;
        }
    }
    else if(pci == 0x10u)
    {
        if(len < 3u) { return; }

        ffLen = (((uint16)(data[0u] & 0x0Fu)) << 8u) | data[1u];
        if((ffLen == 0u) || (ffLen > FBL_ISOTP_MAX_PAYLOAD)) { return; }

        copy = (uint8)(len - 2u);
        if(copy > ffLen) { copy = (uint8)ffLen; }

        memcpy(g_iso.rxBuf, &data[2u], copy);
        g_iso.rxLen = ffLen;
        g_iso.rxIdx = copy;
        g_iso.nextSn = 1u;
        Fbl_IsoTpSendFc();
    }
    else if(pci == 0x20u)
    {
        if((len < 2u) || (g_iso.rxIdx == 0u) || (g_iso.rxIdx >= g_iso.rxLen)) { return; }

        sn = data[0u] & 0x0Fu;
        if(sn != g_iso.nextSn) { return; }

        copy = (uint8)(len - 1u);
        if((g_iso.rxIdx + copy) > g_iso.rxLen)
        {
            copy = (uint8)(g_iso.rxLen - g_iso.rxIdx);
        }

        memcpy(&g_iso.rxBuf[g_iso.rxIdx], &data[1u], copy);
        g_iso.rxIdx += copy;
        g_iso.nextSn = (uint8)((g_iso.nextSn + 1u) & 0x0Fu);

        if(g_iso.rxIdx >= g_iso.rxLen)
        {
            g_iso.rxReady = 1u;
        }
    }
    else if(pci == 0x30u)
    {
        if(len < 3u) { return; }

        g_iso.fcStatus = data[0u] & 0x0Fu;
        g_iso.fcBlockSize = data[1u];
        g_iso.fcStMin = data[2u];
        g_iso.waitFc = 0u;
    }
    else
    {
    }
}

static uint8 Fbl_IsoTpWaitFc(void)
{
    uint32 timeout;
    uint8 waitFrames = 0u;

    do
    {
        timeout = FBL_ISOTP_FC_TIMEOUT_LOOPS;

        while((g_iso.waitFc != 0u) && (timeout > 0u))
        {
            timeout--;
        }

        if(timeout == 0u)
        {
            return 0u;
        }

        if(g_iso.fcStatus == 0u)
        {
            return 1u;
        }

        if(g_iso.fcStatus == 1u)
        {
            waitFrames++;
            if(waitFrames > FBL_ISOTP_FC_WFT_MAX)
            {
                return 0u;
            }
            g_iso.waitFc = 1u;
        }
        else
        {
            return 0u;
        }
    } while(waitFrames <= FBL_ISOTP_FC_WFT_MAX);

    return 0u;
}

static void Fbl_IsoTpDelayStMin(uint8 stMin)
{
    volatile uint32 loops;

    if(stMin <= 0x7Fu)
    {
        loops = (uint32)stMin * 1000u;
    }
    else if((stMin >= 0xF1u) && (stMin <= 0xF9u))
    {
        loops = (uint32)(stMin - 0xF0u) * 100u;
    }
    else
    {
        loops = 0u;
    }

    while(loops > 0u)
    {
        loops--;
    }
}

static void Fbl_IsoTpSend(const uint8 *data, uint16 len)
{
    uint8 frame[64];
    uint8 chunk;
    uint8 blockCounter;

    if(len <= 7u)
    {
        memset(frame, 0u, sizeof(frame));
        frame[0u] = (uint8)len;
        memcpy(&frame[1u], data, len);
        Fbl_CanSend(frame, (uint8)(len + 1u));
        return;
    }

    memset(frame, 0u, sizeof(frame));
    frame[0u] = (uint8)(0x10u | ((len >> 8u) & 0x0Fu));
    frame[1u] = (uint8)(len & 0xFFu);
    chunk = 62u;
    if(chunk > len) { chunk = (uint8)len; }
    memcpy(&frame[2u], data, chunk);
    Fbl_CanSend(frame, 64u);

    g_iso.txLen = len;
    g_iso.txIdx = chunk;
    g_iso.txSn = 1u;
    g_iso.waitFc = 1u;
    g_iso.fcStatus = 0xFFu;

    if(Fbl_IsoTpWaitFc() == 0u)
    {
        g_iso.txLen = 0u;
        g_iso.txIdx = 0u;
        return;
    }

    blockCounter = 0u;

    while(g_iso.txIdx < g_iso.txLen)
    {
        if((g_iso.fcBlockSize != 0u) && (blockCounter >= g_iso.fcBlockSize))
        {
            g_iso.waitFc = 1u;
            g_iso.fcStatus = 0xFFu;

            if(Fbl_IsoTpWaitFc() == 0u)
            {
                g_iso.txLen = 0u;
                g_iso.txIdx = 0u;
                return;
            }

            blockCounter = 0u;
        }

        Fbl_IsoTpDelayStMin(g_iso.fcStMin);

        memset(frame, 0u, sizeof(frame));
        frame[0u] = (uint8)(0x20u | (g_iso.txSn & 0x0Fu));

        chunk = 63u;
        if((g_iso.txIdx + chunk) > g_iso.txLen)
        {
            chunk = (uint8)(g_iso.txLen - g_iso.txIdx);
        }

        memcpy(&frame[1u], &data[g_iso.txIdx], chunk);
        Fbl_CanSend(frame, (uint8)(chunk + 1u));
        g_iso.txIdx += chunk;
        g_iso.txSn = (uint8)((g_iso.txSn + 1u) & 0x0Fu);
        blockCounter++;
    }
}

static void Fbl_UdsHandle(const uint8 *req, uint16 len, uint8 transport)
{
    uint8 res[64];
    uint8 sid;
    uint16 rid;
    uint32 addr;
    uint32 size;
    uint32 crcExpected;
    uint32 crcCalc;

    if(len == 0u) { return; }
    sid = req[0u];

    if(sid == UDS_SID_SESSION)
    {
        if(len < 2u) { Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport); return; }
        res[0u] = 0x50u;
        res[1u] = req[1u];
        res[2u] = 0x00u;
        res[3u] = 0x32u;
        res[4u] = 0x01u;
        res[5u] = 0xF4u;
        Fbl_UdsSend(res, 6u, transport);
    }
    else if(sid == UDS_SID_TESTER_PRESENT)
    {
        res[0u] = 0x7Eu;
        res[1u] = 0x00u;
        Fbl_UdsSend(res, 2u, transport);
    }
    else if(sid == UDS_SID_RDBI)
    {
        if((len >= 3u) && (req[1u] == 0xF1u) && (req[2u] == 0x86u))
        {
            res[0u] = 0x62u;
            res[1u] = 0xF1u;
            res[2u] = 0x86u;
            res[3u] = 0x02u;
            Fbl_UdsSend(res, 4u, transport);
        }
        else
        {
            Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
        }
    }
    else if(sid == UDS_SID_ROUTINE)
    {
        if(len < 4u) { Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport); return; }

        rid = Fbl_Rd16(&req[2u]);

        if((req[1u] == 0x01u) && (rid == UDS_RID_START_FBL_RAM_UPDATER))
        {
            g_FblStartRamUpdater = 1u;
            res[0u] = 0x71u;
            res[1u] = 0x01u;
            res[2u] = 0x01u;
            res[3u] = 0x55u;
            res[4u] = 0x00u;
            Fbl_UdsSend(res, 5u, transport);
        }
        else if((req[1u] == 0x01u) && (rid == UDS_RID_ERASE_APP))
        {
            if(Fbl_FlashEraseRange(APP_START_NCACHED, (APP_END_NCACHED - APP_START_NCACHED) + 1u) != 0u)
            {
                Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
                return;
            }

            res[0u] = 0x71u;
            res[1u] = 0x01u;
            res[2u] = 0x00u;
            res[3u] = 0x01u;
            res[4u] = 0x00u;
            Fbl_UdsSend(res, 5u, transport);
        }
        else if((req[1u] == 0x01u) && (rid == UDS_RID_CRC_CHECK))
        {
            if(len < 16u) { Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport); return; }

            addr = Fbl_Rd32(&req[4u]);
            size = Fbl_Rd32(&req[8u]);
            crcExpected = Fbl_Rd32(&req[12u]);

            if(Fbl_AppRangeValid(addr, size) == 0u)
            {
                Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
                return;
            }

            (void)Fbl_FlashFlush();
            crcCalc = Fbl_Crc32(addr, size);

            res[0u] = 0x71u;
            res[1u] = 0x01u;
            res[2u] = 0x00u;
            res[3u] = 0x02u;
            Fbl_Wr32(&res[4u], crcCalc);
            res[8u] = (crcCalc == crcExpected) ? 0x00u : 0x01u;
            Fbl_UdsSend(res, 9u, transport);
        }
        else
        {
            Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
        }
    }
    else if(sid == UDS_SID_REQ_DOWNLOAD)
    {
        if((len < 12u) || (req[3u] != 0x44u))
        {
            Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport);
            return;
        }

        addr = Fbl_Rd32(&req[4u]);
        size = Fbl_Rd32(&req[8u]);

        if(Fbl_AppRangeValid(addr, size) == 0u)
        {
            Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
            return;
        }

        g_dl.active = 1u;
        g_dl.startAddr = addr;
        g_dl.curAddr = addr;
        g_dl.length = size;
        g_dl.received = 0u;
        g_dl.nextBlock = 1u;
        g_pageAddr = 0xFFFFFFFFu;
        g_pageFill = 0u;
        memset(g_pageBuf, 0x36, sizeof(g_pageBuf));

        res[0u] = 0x74u;
        res[1u] = 0x20u;
        res[2u] = 0x0Fu;
        res[3u] = 0xFFu;
        Fbl_UdsSend(res, 4u, transport);
    }
    else if(sid == UDS_SID_TRANSFER_DATA)
    {
        uint16 dataLen;

        if((g_dl.active == 0u) || (len < 2u))
        {
            Fbl_UdsNeg(sid, UDS_NRC_COND_NOT_CORRECT, transport);
            return;
        }

        if(req[1u] != g_dl.nextBlock)
        {
            Fbl_UdsNeg(sid, UDS_NRC_WRONG_BLOCK_SEQUENCE, transport);
            return;
        }

        dataLen = (uint16)(len - 2u);

        if((g_dl.received + dataLen) > g_dl.length)
        {
            Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
            return;
        }

        if(Fbl_FlashProgram(g_dl.curAddr, &req[2u], dataLen) != 0u)
        {
            Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
            return;
        }

        g_dl.curAddr += dataLen;
        g_dl.received += dataLen;
        g_dl.nextBlock++;

        res[0u] = 0x76u;
        res[1u] = req[1u];
        Fbl_UdsSend(res, 2u, transport);
    }
    else if(sid == UDS_SID_TRANSFER_EXIT)
    {
        if(g_dl.active == 0u)
        {
            Fbl_UdsNeg(sid, UDS_NRC_COND_NOT_CORRECT, transport);
            return;
        }

        if(Fbl_FlashFlush() != 0u)
        {
            Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
            return;
        }

        if(g_dl.received != g_dl.length)
        {
            Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
            return;
        }

        if(len >= 5u)
        {
            crcExpected = Fbl_Rd32(&req[1u]);
            crcCalc = Fbl_Crc32(g_dl.startAddr, g_dl.length);

            if(crcCalc != crcExpected)
            {
                Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
                return;
            }
        }

        g_dl.active = 0u;
        res[0u] = 0x77u;
        Fbl_UdsSend(res, 1u, transport);
    }
    else if(sid == UDS_SID_RESET)
    {
        if(len < 2u) { Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport); return; }

        res[0u] = 0x51u;
        res[1u] = req[1u];
        Fbl_UdsSend(res, 2u, transport);

        for(volatile uint32 i = 0u; i < 200000u; i++) {}
        IfxCpu_disableInterrupts();
        IfxScuRcu_performReset(2u, 0u);
    }
    else
    {
        Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
    }
}

static void Fbl_UdsSend(const uint8 *res, uint16 len, uint8 transport)
{
    if(transport == FBL_TRANSPORT_ETH)
    {
        Fbl_DoIpSendDiag(res, len);
    }
    else
    {
        Fbl_IsoTpSend(res, len);
    }
}

static void Fbl_UdsNeg(uint8 sid, uint8 nrc, uint8 transport)
{
    uint8 res[3];

    res[0u] = UDS_NEG_RESP;
    res[1u] = sid;
    res[2u] = nrc;

    Fbl_UdsSend(res, 3u, transport);
}

static void Fbl_DoIpMain(void)
{
    uint8 buf[1600];
    uint16 len;

    FblEth_MainFunction();

    while(FblEth_UdpReceive(buf, &len) != 0u)
    {
        Fbl_DoIpHandleUdp(buf, len);
    }

    while(FblEth_TcpReceive(buf, &len) != 0u)
    {
        Fbl_DoIpHandleTcp(buf, len);
    }
}

static void Fbl_DoIpHandleUdp(const uint8 *buf, uint16 len)
{
    uint8 res[64];
    uint16 payloadType;

    if(len < 8u) { return; }
    if((buf[0u] != DOIP_PROTO_VER) || (buf[1u] != DOIP_INV_PROTO_VER)) { return; }

    payloadType = Fbl_Rd16(&buf[2u]);

    if(payloadType == DOIP_PT_VID_REQ)
    {
        res[0u] = DOIP_PROTO_VER;
        res[1u] = DOIP_INV_PROTO_VER;
        Fbl_Wr16(&res[2u], DOIP_PT_VID_RES);
        Fbl_Wr32(&res[4u], 17u);
        memcpy(&res[8u], "AURIXTC375FBL0001", 17u);
        FblEth_UdpSend(res, 25u);
    }
}

static void Fbl_DoIpHandleTcp(const uint8 *buf, uint16 len)
{
    uint16 payloadType;
    uint32 payloadLen;
    uint8 res[32];

    if(len < 8u) { return; }
    if((buf[0u] != DOIP_PROTO_VER) || (buf[1u] != DOIP_INV_PROTO_VER)) { return; }

    payloadType = Fbl_Rd16(&buf[2u]);
    payloadLen = Fbl_Rd32(&buf[4u]);

    if((payloadLen + 8u) > len) { return; }

    if(payloadType == DOIP_PT_ROUTING_ACT_REQ)
    {
        res[0u] = DOIP_PROTO_VER;
        res[1u] = DOIP_INV_PROTO_VER;
        Fbl_Wr16(&res[2u], DOIP_PT_ROUTING_ACT_RES);
        Fbl_Wr32(&res[4u], 9u);
        res[8u] = buf[8u];
        res[9u] = buf[9u];
        Fbl_Wr16(&res[10u], DOIP_ECU_ADDR);
        res[12u] = 0x10u;
        res[13u] = 0x00u;
        res[14u] = 0x00u;
        res[15u] = 0x00u;
        res[16u] = 0x00u;
        FblEth_TcpSend(res, 17u);
    }
    else if(payloadType == DOIP_PT_DIAG_MSG)
    {
        if(payloadLen < 4u) { return; }

        res[0u] = DOIP_PROTO_VER;
        res[1u] = DOIP_INV_PROTO_VER;
        Fbl_Wr16(&res[2u], DOIP_PT_DIAG_ACK);
        Fbl_Wr32(&res[4u], 5u);
        res[8u] = buf[8u];
        res[9u] = buf[9u];
        res[10u] = buf[10u];
        res[11u] = buf[11u];
        res[12u] = 0x00u;
        FblEth_TcpSend(res, 13u);

        Fbl_UdsHandle(&buf[12u], (uint16)(payloadLen - 4u), FBL_TRANSPORT_ETH);
    }
}

static void Fbl_DoIpSendDiag(const uint8 *uds, uint16 udsLen)
{
    uint8 buf[512];

    if((udsLen + 12u) > sizeof(buf)) { return; }

    buf[0u] = DOIP_PROTO_VER;
    buf[1u] = DOIP_INV_PROTO_VER;
    Fbl_Wr16(&buf[2u], DOIP_PT_DIAG_MSG);
    Fbl_Wr32(&buf[4u], (uint32)udsLen + 4u);
    Fbl_Wr16(&buf[8u], DOIP_ECU_ADDR);
    Fbl_Wr16(&buf[10u], DOIP_TESTER_ADDR);
    memcpy(&buf[12u], uds, udsLen);

    FblEth_TcpSend(buf, (uint16)(udsLen + 12u));
}

static void Fbl_FlashInit(void)
{
    memcpy((void *)FBL_FLASH_ERASE_ADDR, (const void *)IfxFlash_eraseMultipleSectors, FBL_FLASH_FUNC_LEN);
    memcpy((void *)FBL_FLASH_WAIT_ADDR, (const void *)IfxFlash_waitUnbusy, FBL_FLASH_FUNC_LEN);
    memcpy((void *)FBL_FLASH_ENTER_ADDR, (const void *)IfxFlash_enterPageMode, FBL_FLASH_FUNC_LEN);
    memcpy((void *)FBL_FLASH_LOAD_ADDR, (const void *)IfxFlash_loadPage2X32, FBL_FLASH_FUNC_LEN);
    memcpy((void *)FBL_FLASH_WRITE_ADDR, (const void *)IfxFlash_writePage, FBL_FLASH_FUNC_LEN);

    g_flash.eraseSectors = (void *)FBL_FLASH_ERASE_ADDR;
    g_flash.waitUnbusy = (void *)FBL_FLASH_WAIT_ADDR;
    g_flash.enterPageMode = (void *)FBL_FLASH_ENTER_ADDR;
    g_flash.load2X32 = (void *)FBL_FLASH_LOAD_ADDR;
    g_flash.writePage = (void *)FBL_FLASH_WRITE_ADDR;

    memset(g_pageBuf, 0x36, sizeof(g_pageBuf));
    g_pageAddr = 0xFFFFFFFFu;
    g_pageFill = 0u;
}

static uint32 Fbl_FlashEraseRange(uint32 addr, uint32 len)
{
    uint32 start;
    uint32 end;
    uint32 count;
    uint16 pw;
    boolean irq;
    IfxFlash_FlashType bank;

    if(len == 0u) { return 1u; }

    start = addr & ~(PFLASH_SECTOR_SIZE - 1u);
    end = (addr + len + PFLASH_SECTOR_SIZE - 1u) & ~(PFLASH_SECTOR_SIZE - 1u);

    while(start < end)
    {
        uint32 subEnd = end;

        if((start <= PFLASH_BANK_A_END) && (subEnd > (PFLASH_BANK_A_END + 1u)))
        {
            subEnd = PFLASH_BANK_A_END + 1u;
        }

        count = (subEnd - start) / PFLASH_SECTOR_SIZE;
        bank = Fbl_FlashBank(start);

        irq = IfxCpu_disableInterrupts();
        pw = IfxScuWdt_getSafetyWatchdogPasswordInline();
        IfxScuWdt_clearSafetyEndinitInline(pw);
        g_flash.eraseSectors(start, count);
        IfxScuWdt_setSafetyEndinitInline(pw);
        g_flash.waitUnbusy(FLASH_MODULE, bank);
        IfxCpu_restoreInterrupts(irq);

        start = subEnd;
    }

    return 0u;
}

static uint32 Fbl_FlashProgram(uint32 addr, const uint8 *data, uint32 len)
{
    uint32 i;
    uint32 page;
    uint32 off;

    for(i = 0u; i < len; i++)
    {
        page = (addr + i) & ~(PFLASH_PAGE_SIZE - 1u);
        off = (addr + i) - page;

        if(g_pageAddr == 0xFFFFFFFFu)
        {
            g_pageAddr = page;
            memset(g_pageBuf, 0x36, sizeof(g_pageBuf));
            g_pageFill = 0u;
        }
        else if(g_pageAddr != page)
        {
            if(Fbl_FlashFlush() != 0u) { return 1u; }

            g_pageAddr = page;
            memset(g_pageBuf, 0x36, sizeof(g_pageBuf));
            g_pageFill = 0u;
        }

        g_pageBuf[off] = data[i];

        if((off + 1u) > g_pageFill)
        {
            g_pageFill = off + 1u;
        }

        if(g_pageFill >= PFLASH_PAGE_SIZE)
        {
            if(Fbl_FlashFlush() != 0u) { return 1u; }
        }
    }

    return 0u;
}

static uint32 Fbl_FlashFlush(void)
{
    uint32 ret = 0u;

    if(g_pageAddr != 0xFFFFFFFFu)
    {
        ret = Fbl_FlashProgramPage(g_pageAddr, g_pageBuf);
        g_pageAddr = 0xFFFFFFFFu;
        g_pageFill = 0u;
        memset(g_pageBuf, 0x36, sizeof(g_pageBuf));
    }

    return ret;
}

static uint32 Fbl_FlashProgramPage(uint32 addr, const uint8 *data)
{
    uint32 w[8];
    uint16 pw;
    boolean irq;
    IfxFlash_FlashType bank;

    for(uint32 i = 0u; i < 8u; i++)
    {
        w[i] = Fbl_Rd32(&data[i * 4u]);
    }

    bank = Fbl_FlashBank(addr);

    irq = IfxCpu_disableInterrupts();
    pw = IfxScuWdt_getSafetyWatchdogPasswordInline();
    IfxScuWdt_clearSafetyEndinitInline(pw);

    g_flash.enterPageMode(addr);
    g_flash.waitUnbusy(FLASH_MODULE, bank);
    g_flash.load2X32(addr, w[0u], w[1u]);
    g_flash.load2X32(addr, w[2u], w[3u]);
    g_flash.load2X32(addr, w[4u], w[5u]);
    g_flash.load2X32(addr, w[6u], w[7u]);
    g_flash.writePage(addr);
    g_flash.waitUnbusy(FLASH_MODULE, bank);

    IfxScuWdt_setSafetyEndinitInline(pw);
    IfxCpu_restoreInterrupts(irq);

    return 0u;
}

static IfxFlash_FlashType Fbl_FlashBank(uint32 addr)
{
    if((addr >= PFLASH_BANK_B_START) && (addr <= PFLASH_BANK_B_END))
    {
        return PROGRAM_FLASH_BANK_B;
    }

    return PROGRAM_FLASH_BANK_A;
}

static uint32 Fbl_Crc32(uint32 addr, uint32 len)
{
    volatile const uint8 *p = (volatile const uint8 *)addr;
    uint32 crc = 0xFFFFFFFFu;

    for(uint32 i = 0u; i < len; i++)
    {
        crc ^= p[i];

        for(uint8 b = 0u; b < 8u; b++)
        {
            if((crc & 1u) != 0u)
            {
                crc = (crc >> 1u) ^ 0xEDB88320u;
            }
            else
            {
                crc >>= 1u;
            }
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

static uint8 Fbl_AppRangeValid(uint32 addr, uint32 len)
{
    if(len == 0u) { return 0u; }
    if(addr < APP_START_NCACHED) { return 0u; }
    if((addr + len - 1u) > APP_END_NCACHED) { return 0u; }
    if((addr + len) < addr) { return 0u; }
    return 1u;
}

static uint32 Fbl_Rd32(const uint8 *p)
{
    return (((uint32)p[0u]) << 24u) | (((uint32)p[1u]) << 16u) | (((uint32)p[2u]) << 8u) | ((uint32)p[3u]);
}

static uint16 Fbl_Rd16(const uint8 *p)
{
    return (uint16)((((uint16)p[0u]) << 8u) | ((uint16)p[1u]));
}

static void Fbl_Wr16(uint8 *p, uint16 v)
{
    p[0u] = (uint8)(v >> 8u);
    p[1u] = (uint8)(v);
}

static void Fbl_Wr32(uint8 *p, uint32 v)
{
    p[0u] = (uint8)(v >> 24u);
    p[1u] = (uint8)(v >> 16u);
    p[2u] = (uint8)(v >> 8u);
    p[3u] = (uint8)(v);
}

static void Fbl_JumpToApp(void)
{
    IfxCpu_disableInterrupts();
    __asm("isync");
    __asm("ji %0" : : "a"(APP_START_NCACHED));
}

static void Fbl_CopyAndJumpRamUpdater(void)
{
    void (*entry)(void);


    IfxCpu_disableInterrupts();

    entry = FblRamUpdater_Entry;

    __asm("isync");

    entry();

    while(1)
    {
    }
}
