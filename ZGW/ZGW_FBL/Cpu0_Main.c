#include <string.h>
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "IfxScuRcu.h"
#include "IfxFlash.h"
#include "IfxCan_Can.h"
#include "IfxPort.h"
#include "IfxPort_reg.h"
#include "aurix_pin_mappings.h"

#define FBL_TRANSPORT_ETH                1u
#define FBL_TRANSPORT_CANFD              2u
#define FBL_TRANSPORT_CAN_CLASSIC        3u

#define FBL_RESET_INFO_ENTER_DOIP        0xFCD0u

/* Mirrors the APP NCR group ordering in Lcf_Tasking_Tricore_Tc.lsl. */
#define FBL_NCR_DIAG_SESSION_ADDR        0x900000DCu
#define FBL_NCR_RESET_COUNTER_ADDR       0x900000E4u
#define FBL_NCR_PROGRAMMING_REQ_ADDR     0x900000E5u
#define FBL_NCR_COMM_INTERFACE_ADDR      0x900000E6u
#define FBL_PROGRAMMING_REQUEST_ACTIVE   1u
#define FBL_DIAG_SESSION_PROGRAMMING     0x02u
#define FBL_RESET_COUNTER_FORCE_MIN      49u
#define FBL_RESET_COUNTER_FORCE_MAX      52u

#define FBL_START_NCACHED                0xA0000000u
#define FBL_END_NCACHED                  0xA002FFFFu
#define FBL_SIZE_BYTES                   0x00030000u

#define APP_START_NCACHED                0xA0030000u
#define APP_END_NCACHED                  0xA07FFFFFu

#define PFLASH_NC_ADDRESS_PREFIX         0xA0000000u
#define FLASH_ADDRESS_PREFIX_MASK        0xFF000000u

/* PFLASH alias bases.
 * The same physical PFLASH is visible through a cached segment-8 alias (used for
 * normal code execution) and a non-cached segment-A alias (required by the iLLD
 * IfxFlash erase/program/verify primitives). Application code is linked/executed
 * from the cached 0x8 alias; the FBL converts every download/erase/verify target
 * to the non-cached 0xA alias before touching the flash controller. */
#define PFLASH_CACHED_BASE               0x80000000u
#define PFLASH_NONCACHED_BASE            0xA0000000u
#define PFLASH_ALIAS_MASK                0x1FFFFFFFu

/* Explicit FBL / APP PFLASH ranges, expressed in both aliases.
 * Values mirror the linker memory objects (FBL BootManager_PFLASH = 192K,
 * APP pfls0 starts at offset 0x30000). */
#define FBL_PFLASH_START_CACHED          0x80000000u
#define FBL_PFLASH_END_CACHED            0x8002FFFFu

#define APP_PFLASH_START_CACHED          0x80030000u
#define APP_PFLASH_END_CACHED            0x807FFFFFu

#define APP_PFLASH_START_NC              0xA0030000u
#define APP_PFLASH_END_NC                0xA07FFFFFu

/* APP reset/start execution address (cached alias). The FBL jumps here. */
#define APP_START_CACHED                 APP_PFLASH_START_CACHED

/* DFLASH occupies segment-A too; reject it explicitly to catch PFLASH/DFLASH mixups. */
#define DFLASH_START_NC                  0xAF000000u
#define DFLASH_END_NC                    0xAFFFFFFFu

typedef enum
{
    FBL_ADDR_OK = 0,
    FBL_ADDR_ERR_OVERFLOW,
    FBL_ADDR_ERR_ALIGN,
    FBL_ADDR_ERR_NOT_APP,
    FBL_ADDR_ERR_PROTECTED
} Fbl_AddressStatus;

static inline uint32 Fbl_ToCachedPflash(uint32 addr)
{
    return (addr & PFLASH_ALIAS_MASK) | PFLASH_CACHED_BASE;
}

static inline uint32 Fbl_ToNonCachedPflash(uint32 addr)
{
    return (addr & PFLASH_ALIAS_MASK) | PFLASH_NONCACHED_BASE;
}

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
#define FBL_CAN_RES_ID                   0x711u
#define FBL_CAN_RX_PRIO                  1u
#define FBL_CAN_MAX_DL                   64u
#define FBL_CAN_CLASSIC_MAX_DL           8u
#define FBL_CAN_EXT_ADDR_ZGW             0x41u
#define FBL_CAN_EXT_ADDR_TESTER          0x41u
#define FBL_CAN_CLASSIC_TRCV_STB_PORT    (&MODULE_P20)
#define FBL_CAN_CLASSIC_TRCV_STB_PIN     6u

#define FBL_ISOTP_MAX_PAYLOAD            4095u
#define FBL_ISOTP_FC_TIMEOUT_LOOPS       1000000u
#define FBL_ISOTP_FC_WFT_MAX             8u
#define FBL_CAN_TX_BUSY_RETRY_MAX        100000u

#define UDS_SID_SESSION                  0x10u
#define UDS_SID_RESET                    0x11u
#define UDS_SID_RDBI                     0x22u
#define UDS_SID_SECURITY_ACCESS          0x27u
#define UDS_SID_COMM_CONTROL             0x28u
#define UDS_SID_ROUTINE                  0x31u
#define UDS_SID_REQ_DOWNLOAD             0x34u
#define UDS_SID_TRANSFER_DATA            0x36u
#define UDS_SID_TRANSFER_EXIT            0x37u
#define UDS_SID_TESTER_PRESENT           0x3Eu
#define UDS_SID_CONTROL_DTC_SETTING      0x85u
#define UDS_NEG_RESP                     0x7Fu

#define UDS_NRC_SUBFUNC_NOT_SUPPORTED    0x12u
#define UDS_NRC_INCORRECT_LEN            0x13u
#define UDS_NRC_COND_NOT_CORRECT         0x22u
#define UDS_NRC_SEQUENCE_ERROR           0x24u
#define UDS_NRC_OUT_OF_RANGE             0x31u
#define UDS_NRC_INVALID_KEY              0x35u
#define UDS_NRC_TRANSFER_FAIL            0x70u
#define UDS_NRC_WRONG_BLOCK_SEQUENCE     0x73u

#define UDS_RID_ERASE_APP                0x0001u
#define UDS_RID_CRC_CHECK                0x0002u
#define UDS_RID_START_FBL_RAM_UPDATER    0x0155u
#define UDS_DID_ACTIVE_SOFTWARE_BLOCK    0xF100u
#define UDS_DID_SOFTWARE_VERSION         0xF101u
#define UDS_DID_ACTIVE_SESSION           0xF186u
#define FBL_ACTIVE_SOFTWARE_BLOCK        0x01u
#define APP_SW_VERSION_MAJOR             1u
#define APP_SW_VERSION_MINOR             0u
#define APP_SW_VERSION_PATCH             0u
#define FBL_SW_VERSION_MAJOR             1u
#define FBL_SW_VERSION_MINOR             0u
#define FBL_SW_VERSION_PATCH             0u

#define DOIP_TCP_PORT                    13400u
#define DOIP_UDP_PORT                    13400u
#define DOIP_PROTO_VER                   0x02u
#define DOIP_INV_PROTO_VER               0xFDu
#define DOIP_PT_VID_REQ                  0x0001u
#define DOIP_PT_VID_RES                  0x0004u
#define DOIP_PT_ROUTING_ACT_REQ          0x0005u
#define DOIP_PT_ROUTING_ACT_RES          0x0006u
#define DOIP_PT_ALIVE_CHECK_REQ          0x0007u
#define DOIP_PT_ALIVE_CHECK_RES          0x0008u
#define DOIP_PT_DIAG_MSG                 0x8001u
#define DOIP_PT_DIAG_ACK                 0x8002u
#define DOIP_PT_DIAG_NACK                0x8003u
#define DOIP_TESTER_ADDR                 0x0710u
#define DOIP_ECU_ADDR                    0x1001u
#define DOIP_HEADER_LEN                  8u
#define DOIP_VID_RES_LEN                 33u
#define DOIP_RA_RES_LEN                  13u
#define DOIP_ALIVE_RES_LEN               2u
#define DOIP_DIAG_ACK_LEN                5u
#define DOIP_TCP_RX_STREAM_SIZE          8192u
#define DOIP_RA_RES_DENIED_UNKNOWN_SRC   0x00u
#define DOIP_RA_RES_UNSUPPORTED_ACT      0x05u
#define DOIP_RA_RES_OK                   0x10u
#define DOIP_NACK_INVALID_SOURCE_ADDR    0x02u
#define DOIP_NACK_UNKNOWN_TARGET_ADDR    0x03u
#define DOIP_NACK_DIAG_MSG_TOO_LARGE     0x04u
#define DOIP_NACK_TRANSPORT_ERROR        0x08u

#define FBL_FLASH_FUNC_BASE              0x70100000u
#define FBL_FLASH_FUNC_LEN               192u
#define FBL_FLASH_ERASE_ADDR             (FBL_FLASH_FUNC_BASE)
#define FBL_FLASH_WAIT_ADDR              (FBL_FLASH_ERASE_ADDR + FBL_FLASH_FUNC_LEN)
#define FBL_FLASH_ENTER_ADDR             (FBL_FLASH_WAIT_ADDR + FBL_FLASH_FUNC_LEN)
#define FBL_FLASH_LOAD_ADDR              (FBL_FLASH_ENTER_ADDR + FBL_FLASH_FUNC_LEN)
#define FBL_FLASH_WRITE_ADDR             (FBL_FLASH_LOAD_ADDR + FBL_FLASH_FUNC_LEN)

extern void FblRamUpdater_Entry(void); // @suppress("Unused variable declaration in file scope")

extern void FblEth_Init(void);
extern void FblEth_MainFunction(void);
extern uint8 FblEth_TcpReceive(uint8 *buf, uint16 *len);
extern uint8 FblEth_UdpReceive(uint8 *buf, uint16 *len);
extern void FblEth_TcpSend(const uint8 *buf, uint16 len);
extern void FblEth_UdpSend(const uint8 *buf, uint16 len);

volatile uint32 g_FblTransportSelect = FBL_TRANSPORT_ETH;
volatile uint32 g_FblStayInBoot = 1u;
volatile uint32 g_FblStartRamUpdater = 0u;
volatile uint16 g_FblLastResetReason = 0u;
volatile uint8 g_FblDiagBootRequestSeen = 0u;

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
    uint8 addrOffset;
    uint8 txAddrOffset;
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
    uint32 logicalAddr;   /* cached 0x8 address as requested by the tester (for diagnostics) */
    uint32 startAddr;     /* internal non-cached 0xA flash address */
    uint32 curAddr;       /* internal non-cached 0xA running flash address */
    uint32 length;
    uint32 received;
    uint8 nextBlock;
} FblDownload_Type;

static FblCan_Type g_can;
static FblIsoTp_Type g_iso;
static FblFlashCmd_Type g_flash;
static FblDownload_Type g_dl;
static uint32 g_secSeed;
static uint8 g_secSeedValid;
static uint8 g_pageBuf[PFLASH_PAGE_SIZE];
static uint32 g_pageAddr = 0xFFFFFFFFu;
static uint32 g_pageFill = 0u;
static uint8 g_doipTcpStream[DOIP_TCP_RX_STREAM_SIZE];
static uint16 g_doipTcpStreamLen;
static uint16 g_doipTesterAddr;
static uint8 g_doipRoutingActive;

static const uint8 Fbl_DoIpVin[17] = "LABTC375DOIP0001";
static const uint8 Fbl_DoIpEid[6] = {0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u};
static const uint8 Fbl_DoIpGid[6] = {0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u};

IFX_INTERRUPT(Fbl_CanRxIsr, 0u, FBL_CAN_RX_PRIO);

static uint8 Fbl_ReadNcrU8(uint32 addr);
static void Fbl_WriteNcrU8(uint32 addr, uint8 value);
static uint8 Fbl_NormalizeTransport(uint8 transport);
static uint8 Fbl_ResetCounterForcesProgramming(uint8 resetCounter);
static uint8 Fbl_IsAppValid(void);
static void Fbl_PlatformInit(void);
static void Fbl_CanReleaseFdRxPinFromScr(void);
static void Fbl_CanClassicTrcvSetNormalMode(void);
static void Fbl_CanInit(uint8 transport);
static void Fbl_CanSend(const uint8 *data, uint8 len);
static uint8 Fbl_CanDlcFromLen(uint8 len);
static uint8 Fbl_CanLenFromDlc(uint8 dlc);
static uint8 Fbl_CanPayloadLen(void);
static void Fbl_IsoTpInit(void);
static void Fbl_IsoTpRx(const uint8 *data, uint8 len);
static void Fbl_IsoTpSend(const uint8 *data, uint16 len);
static void Fbl_IsoTpSendFc(void);
static uint8 Fbl_IsoTpDetectAddrOffset(const uint8 *data, uint8 len);
static uint8 Fbl_IsoTpWaitFc(void);
static void Fbl_IsoTpDelayStMin(uint8 stMin);
static void Fbl_UdsHandle(const uint8 *req, uint16 len, uint8 transport);
static void Fbl_UdsSend(const uint8 *res, uint16 len, uint8 transport);
static void Fbl_UdsNeg(uint8 sid, uint8 nrc, uint8 transport);
static void Fbl_UdsSecurityAccess(const uint8 *req, uint16 len, uint8 transport);
static uint32 Fbl_SecCalcKey(uint32 seed, uint8 level);
static void Fbl_DoIpMain(void);
static void Fbl_DoIpHandleUdp(const uint8 *buf, uint16 len);
static void Fbl_DoIpHandleTcp(const uint8 *buf, uint16 len);
static void Fbl_DoIpHandleTcpFrame(const uint8 *buf, uint16 len);
static void Fbl_DoIpSendVehicleId(void);
static void Fbl_DoIpSendRoutingActivationRes(uint16 testerAddr, uint8 code);
static void Fbl_DoIpSendAliveRes(void);
static void Fbl_DoIpSendDiagAck(uint16 testerAddr, uint16 ecuAddr);
static void Fbl_DoIpSendDiagNack(uint16 testerAddr, uint16 ecuAddr, uint8 nack);
static void Fbl_DoIpSendDiag(const uint8 *uds, uint16 udsLen);
static void Fbl_FlashInit(void);
static uint32 Fbl_FlashEraseRange(uint32 addr, uint32 len);
static uint32 Fbl_FlashProgram(uint32 addr, const uint8 *data, uint32 len);
static uint32 Fbl_FlashFlush(void);
static uint32 Fbl_FlashProgramPage(uint32 addr, const uint8 *data);
static IfxFlash_FlashType Fbl_FlashBank(uint32 addr);
static uint32 Fbl_Crc32(uint32 addr, uint32 len);
static Fbl_AddressStatus Fbl_ValidateDownloadRange(uint32 logicalAddr, uint32 length);
static uint32 Fbl_Rd32(const uint8 *p);
static uint16 Fbl_Rd16(const uint8 *p);
static void Fbl_Wr16(uint8 *p, uint16 v);
static void Fbl_Wr32(uint8 *p, uint32 v);
static void Fbl_JumpToApp(void);
static void Fbl_CopyAndJumpRamUpdater(void);

static uint8 Fbl_ReadNcrU8(uint32 addr)
{
    return *((volatile uint8 *)addr);
}

static void Fbl_WriteNcrU8(uint32 addr, uint8 value)
{
    *((volatile uint8 *)addr) = value;
}

static uint8 Fbl_NormalizeTransport(uint8 transport)
{
    if((transport == FBL_TRANSPORT_CANFD) || (transport == FBL_TRANSPORT_CAN_CLASSIC))
    {
        return transport;
    }

    return FBL_TRANSPORT_ETH;
}

static uint8 Fbl_ResetCounterForcesProgramming(uint8 resetCounter)
{
    return ((resetCounter >= FBL_RESET_COUNTER_FORCE_MIN) &&
            (resetCounter <= FBL_RESET_COUNTER_FORCE_MAX)) ? 1u : 0u;
}

static uint8 Fbl_IsAppValid(void)
{
    const uint32 *appStart = (const uint32 *)APP_START_NCACHED;
    uint8 i;

    for(i = 0u; i < 8u; i++)
    {
        if((appStart[i] != 0x00000000u) &&
           (appStart[i] != 0xFFFFFFFFu) &&
           (appStart[i] != 0x36363636u))
        {
            return 1u;
        }
    }

    return 0u;
}

void core0_main(void)
{
    IfxScuRcu_ResetCode resetCode;
    uint16 resetInfo;
    uint8 programmingRequest;
    uint8 requestedTransport;
    uint8 resetCounter;

    resetCode = IfxScuRcu_evaluateReset();
    resetInfo = MODULE_SCU.RSTCON2.B.USRINFO;
    g_FblLastResetReason = resetInfo;

    programmingRequest = Fbl_ReadNcrU8(FBL_NCR_PROGRAMMING_REQ_ADDR);
    requestedTransport = Fbl_NormalizeTransport(Fbl_ReadNcrU8(FBL_NCR_COMM_INTERFACE_ADDR));
    resetCounter = Fbl_ReadNcrU8(FBL_NCR_RESET_COUNTER_ADDR);

    if(Fbl_ResetCounterForcesProgramming(resetCounter) != 0u)
    {
        Fbl_WriteNcrU8(FBL_NCR_COMM_INTERFACE_ADDR, FBL_TRANSPORT_ETH);
        Fbl_WriteNcrU8(FBL_NCR_DIAG_SESSION_ADDR, FBL_DIAG_SESSION_PROGRAMMING);
        g_FblTransportSelect = FBL_TRANSPORT_ETH;
        g_FblStayInBoot = 0u;
        g_FblDiagBootRequestSeen = 1u;
    }
    else if((programmingRequest == FBL_PROGRAMMING_REQUEST_ACTIVE) ||
            (resetCode.resetReason == FBL_RESET_INFO_ENTER_DOIP) ||
            (resetInfo == FBL_RESET_INFO_ENTER_DOIP))
    {
        g_FblTransportSelect = requestedTransport;
        g_FblStayInBoot = 0u;
        g_FblDiagBootRequestSeen = 1u;
        Fbl_WriteNcrU8(FBL_NCR_DIAG_SESSION_ADDR, FBL_DIAG_SESSION_PROGRAMMING);
    }
    else
    {
        g_FblDiagBootRequestSeen = 0u;
    }

    if(programmingRequest == FBL_PROGRAMMING_REQUEST_ACTIVE)
    {
        Fbl_WriteNcrU8(FBL_NCR_PROGRAMMING_REQ_ADDR, 0u);
    }

    Fbl_PlatformInit();
    Fbl_FlashInit();
    Fbl_IsoTpInit();

    if(g_FblStayInBoot == 1u)
    {
        if(Fbl_IsAppValid() != 0u)
        {
            Fbl_JumpToApp();
        }

        g_FblStayInBoot = 0u;
        g_FblTransportSelect = FBL_TRANSPORT_ETH;
        Fbl_WriteNcrU8(FBL_NCR_DIAG_SESSION_ADDR, FBL_DIAG_SESSION_PROGRAMMING);
    }

    g_FblTransportSelect = Fbl_NormalizeTransport((uint8)g_FblTransportSelect);

    if(g_FblTransportSelect == FBL_TRANSPORT_ETH)
    {
        FblEth_Init();
    }
    else
    {
        Fbl_CanInit((uint8)g_FblTransportSelect);
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
            Fbl_UdsHandle(g_iso.rxBuf, g_iso.rxLen, (uint8)g_FblTransportSelect);
        }
    }
}

static void Fbl_PlatformInit(void)
{
    IfxCpu_disableInterrupts();
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());
    gpio_init_pins();
    IfxCpu_enableInterrupts();
}

static void Fbl_CanReleaseFdRxPinFromScr(void)
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

static void Fbl_CanClassicTrcvSetNormalMode(void)
{
    IfxPort_setPinModeOutput(FBL_CAN_CLASSIC_TRCV_STB_PORT,
            FBL_CAN_CLASSIC_TRCV_STB_PIN,
            IfxPort_OutputMode_pushPull,
            IfxPort_OutputIdx_general);
    IfxPort_setPinLow(FBL_CAN_CLASSIC_TRCV_STB_PORT, FBL_CAN_CLASSIC_TRCV_STB_PIN);
}

static void Fbl_CanInit(uint8 transport)
{
    uint8 isClassic = (transport == FBL_TRANSPORT_CAN_CLASSIC) ? 1u : 0u;

    if(isClassic != 0u)
    {
        can0_node0_init_pins();
    }
    else
    {
        Fbl_CanReleaseFdRxPinFromScr();
        can1_node3_init_pins();
    }

    IfxScuWdt_clearCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_clearSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCan_Can_initModuleConfig(&g_can.canConfig, (isClassic != 0u) ? &MODULE_CAN0 : &MODULE_CAN1);
    IfxCan_Can_initModule(&g_can.canModule, &g_can.canConfig);
    IfxCan_Can_initNodeConfig(&g_can.nodeConfig, &g_can.canModule);

    IfxScuCcu_setMcanFrequency(40000000.0f);

    g_can.nodeConfig.nodeId = (isClassic != 0u) ? IfxCan_NodeId_0 : IfxCan_NodeId_3;
    g_can.nodeConfig.frame.mode = (isClassic != 0u) ? IfxCan_FrameMode_standard : IfxCan_FrameMode_fdLong;
    g_can.nodeConfig.frame.type = IfxCan_FrameType_transmitAndReceive;
    g_can.nodeConfig.baudRate.baudrate = 500000u;

    if(isClassic == 0u)
    {
        g_can.nodeConfig.baudRate.prescaler = 3u;
        g_can.nodeConfig.baudRate.timeSegment1 = 14u;
        g_can.nodeConfig.baudRate.timeSegment2 = 3u;
        g_can.nodeConfig.baudRate.syncJumpWidth = 1u;
        g_can.nodeConfig.fastBaudRate.baudrate = 2000000u;
        g_can.nodeConfig.fastBaudRate.prescaler = 0u;
        g_can.nodeConfig.fastBaudRate.timeSegment1 = 14u;
        g_can.nodeConfig.fastBaudRate.timeSegment2 = 3u;
        g_can.nodeConfig.fastBaudRate.syncJumpWidth = 1u;
    }

    g_can.nodeConfig.calculateBitTimingValues = TRUE;

    g_can.nodeConfig.txConfig.txMode = IfxCan_TxMode_dedicatedBuffers;
    g_can.nodeConfig.txConfig.dedicatedTxBuffersNumber = 1u;
    g_can.nodeConfig.txConfig.txBufferDataFieldSize =
            (isClassic != 0u) ? IfxCan_DataFieldSize_8 : IfxCan_DataFieldSize_64;

    g_can.nodeConfig.rxConfig.rxMode = IfxCan_RxMode_fifo0;
    g_can.nodeConfig.rxConfig.rxFifo0DataFieldSize =
            (isClassic != 0u) ? IfxCan_DataFieldSize_8 : IfxCan_DataFieldSize_64;
    g_can.nodeConfig.rxConfig.rxBufferDataFieldSize =
            (isClassic != 0u) ? IfxCan_DataFieldSize_8 : IfxCan_DataFieldSize_64;
    g_can.nodeConfig.rxConfig.rxFifo0Size = (isClassic != 0u) ? 32u : 12u;

    g_can.nodeConfig.filterConfig.messageIdLength = IfxCan_MessageIdLength_both;
    g_can.nodeConfig.filterConfig.standardListSize = 1u;
    g_can.nodeConfig.filterConfig.extendedListSize = 0u;
    g_can.nodeConfig.filterConfig.standardFilterForNonMatchingFrames = IfxCan_NonMatchingFrame_reject;
    g_can.nodeConfig.filterConfig.extendedFilterForNonMatchingFrames = IfxCan_NonMatchingFrame_reject;
    g_can.nodeConfig.filterConfig.rejectRemoteFramesWithStandardId = TRUE;
    g_can.nodeConfig.filterConfig.rejectRemoteFramesWithExtendedId = TRUE;

    g_can.nodeConfig.interruptConfig.rxFifo0NewMessageEnabled = TRUE;
    g_can.nodeConfig.interruptConfig.rxf0n.priority = FBL_CAN_RX_PRIO;
    g_can.nodeConfig.interruptConfig.rxf0n.interruptLine = IfxCan_InterruptLine_0;

    g_can.nodeConfig.messageRAM.baseAddress = (uint32)g_can.nodeConfig.can;
    g_can.nodeConfig.messageRAM.standardFilterListStartAddress = (isClassic != 0u) ? 0x000u : 0x800u;
    g_can.nodeConfig.messageRAM.extendedFilterListStartAddress = (isClassic != 0u) ? 0x080u : 0x880u;
    g_can.nodeConfig.messageRAM.rxFifo0StartAddress = (isClassic != 0u) ? 0x180u : 0x980u;
    g_can.nodeConfig.messageRAM.txBuffersStartAddress = (isClassic != 0u) ? 0x400u : 0xD00u;

    IfxCan_Can_initNode(&g_can.canNode, &g_can.nodeConfig);

    g_can.filter.number = 0u;
    g_can.filter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxFifo0;
    g_can.filter.type = IfxCan_FilterType_classic;
    g_can.filter.id1 = FBL_CAN_REQ_ID;
    g_can.filter.id2 = FBL_CAN_REQ_ID;
    IfxCan_Can_setStandardFilter(&g_can.canNode, &g_can.filter);

    if(isClassic != 0u)
    {
        IfxCan_Node_initRxPin(g_can.canNode.node, &IfxCan_RXD00B_P20_7_IN, IfxPort_Mode_inputPullUp, IfxPort_PadDriver_cmosAutomotiveSpeed1);
        IfxCan_Node_initTxPin(&IfxCan_TXD00_P20_8_OUT, IfxPort_OutputMode_pushPull, IfxPort_PadDriver_cmosAutomotiveSpeed4);
        Fbl_CanClassicTrcvSetNormalMode();
    }
    else
    {
        IfxCan_Node_initRxPin(g_can.canNode.node, &IfxCan_RXD13B_P33_5_IN, IfxPort_Mode_inputPullUp, IfxPort_PadDriver_cmosAutomotiveSpeed1);
        IfxCan_Node_initTxPin(&IfxCan_TXD13_P33_4_OUT, IfxPort_OutputMode_pushPull, IfxPort_PadDriver_cmosAutomotiveSpeed4);
    }

    IfxScuWdt_setCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_setSafetyEndinit(IfxScuWdt_getSafetyWatchdogPassword());
}

void Fbl_CanRxIsr(void)
{
    uint8 len;

    IfxCan_Node_clearInterruptFlag(g_can.canNode.node, IfxCan_Interrupt_rxFifo0NewMessage);

    while(IfxCan_Can_getRxFifo0FillLevel(&g_can.canNode) > 0u)
    {
        IfxCan_Can_initMessage(&g_can.rxMsg);
        g_can.rxMsg.readFromRxFifo0 = TRUE;
        memset(g_can.rxData, 0u, sizeof(g_can.rxData));
        IfxCan_Can_readMessage(&g_can.canNode, &g_can.rxMsg, (uint32 *)g_can.rxData);

        if(g_can.rxMsg.messageId == FBL_CAN_REQ_ID)
        {
            len = Fbl_CanLenFromDlc((uint8)g_can.rxMsg.dataLengthCode);
            if(len > Fbl_CanPayloadLen())
            {
                len = Fbl_CanPayloadLen();
            }
            Fbl_IsoTpRx(g_can.rxData, len);
        }
    }
}

static void Fbl_CanSend(const uint8 *data, uint8 len)
{
    uint32 retry = FBL_CAN_TX_BUSY_RETRY_MAX;
    uint8 maxLen = Fbl_CanPayloadLen();

    if(len > maxLen)
    {
        len = maxLen;
    }

    memset(g_can.txData, 0u, sizeof(g_can.txData));
    memcpy(g_can.txData, data, len);

    IfxCan_Can_initMessage(&g_can.txMsg);
    g_can.txMsg.messageId = FBL_CAN_RES_ID;
    g_can.txMsg.frameMode = (g_FblTransportSelect == FBL_TRANSPORT_CAN_CLASSIC) ?
            IfxCan_FrameMode_standard : IfxCan_FrameMode_fdLong;
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

static uint8 Fbl_CanPayloadLen(void)
{
    return (g_FblTransportSelect == FBL_TRANSPORT_CAN_CLASSIC) ?
            FBL_CAN_CLASSIC_MAX_DL : FBL_CAN_MAX_DL;
}

static void Fbl_IsoTpInit(void)
{
    memset(&g_iso, 0u, sizeof(g_iso));
}

static void Fbl_IsoTpSendFc(void)
{
    uint8 fc[64];
    uint8 addrOffset = g_iso.txAddrOffset;

    memset(fc, 0u, sizeof(fc));
    if(addrOffset != 0u)
    {
        fc[0u] = FBL_CAN_EXT_ADDR_TESTER;
    }

    fc[addrOffset] = 0x30u;
    fc[addrOffset + 1u] = 0x00u;
    fc[addrOffset + 2u] = 0x00u;
    Fbl_CanSend(fc, Fbl_CanPayloadLen());
}

static uint8 Fbl_IsoTpDetectAddrOffset(const uint8 *data, uint8 len)
{
    if((len > 1u) && (data[0u] == FBL_CAN_EXT_ADDR_ZGW))
    {
        return 1u;
    }

    return 0u;
}

static void Fbl_IsoTpRx(const uint8 *data, uint8 len)
{
    uint8 pci;
    uint8 pciIdx;
    uint16 ffLen;
    uint8 copy;
    uint8 sn;

    if(len == 0u) { return; }

    g_iso.addrOffset = Fbl_IsoTpDetectAddrOffset(data, len);
    if(len <= g_iso.addrOffset) { return; }

    pciIdx = g_iso.addrOffset;
    pci = data[pciIdx] & 0xF0u;

    if(pci == 0x00u)
    {
        uint8 sfLen = data[pciIdx] & 0x0Fu;
        uint8 offset = (uint8)(pciIdx + 1u);

        if(sfLen == 0u)
        {
            if(len < (uint8)(pciIdx + 2u)) { return; }
            sfLen = data[pciIdx + 1u];
            offset = (uint8)(pciIdx + 2u);
        }

        if(sfLen == 0u) { return; }

        if((sfLen <= (len - offset)) && (sfLen <= FBL_ISOTP_MAX_PAYLOAD))
        {
            memcpy(g_iso.rxBuf, &data[offset], sfLen);
            g_iso.rxLen = sfLen;
            g_iso.rxIdx = 0u;
            g_iso.txAddrOffset = g_iso.addrOffset;
            g_iso.rxReady = 1u;
        }
    }
    else if(pci == 0x10u)
    {
        if(len < (uint8)(pciIdx + 3u)) { return; }

        ffLen = (((uint16)(data[pciIdx] & 0x0Fu)) << 8u) | data[pciIdx + 1u];
        if((ffLen == 0u) || (ffLen > FBL_ISOTP_MAX_PAYLOAD)) { return; }

        copy = (uint8)(len - (uint8)(pciIdx + 2u));
        if(copy > ffLen) { copy = (uint8)ffLen; }

        memcpy(g_iso.rxBuf, &data[pciIdx + 2u], copy);
        g_iso.rxLen = ffLen;
        g_iso.rxIdx = copy;
        g_iso.nextSn = 1u;
        g_iso.txAddrOffset = g_iso.addrOffset;
        Fbl_IsoTpSendFc();
    }
    else if(pci == 0x20u)
    {
        if((len < (uint8)(pciIdx + 2u)) || (g_iso.rxIdx == 0u) || (g_iso.rxIdx >= g_iso.rxLen)) { return; }

        sn = data[pciIdx] & 0x0Fu;
        if(sn != g_iso.nextSn) { return; }

        copy = (uint8)(len - (uint8)(pciIdx + 1u));
        if((g_iso.rxIdx + copy) > g_iso.rxLen)
        {
            copy = (uint8)(g_iso.rxLen - g_iso.rxIdx);
        }

        memcpy(&g_iso.rxBuf[g_iso.rxIdx], &data[pciIdx + 1u], copy);
        g_iso.rxIdx += copy;
        g_iso.nextSn = (uint8)((g_iso.nextSn + 1u) & 0x0Fu);

        if(g_iso.rxIdx >= g_iso.rxLen)
        {
            g_iso.txAddrOffset = g_iso.addrOffset;
            g_iso.rxReady = 1u;
        }
    }
    else if(pci == 0x30u)
    {
        if(len < (uint8)(pciIdx + 3u)) { return; }

        g_iso.fcStatus = data[pciIdx] & 0x0Fu;
        g_iso.fcBlockSize = data[pciIdx + 1u];
        g_iso.fcStMin = data[pciIdx + 2u];
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
    uint8 payloadLen;
    uint8 addrOffset;
    uint8 pciIdx;
    uint8 sfMax;
    uint8 ffPayload;
    uint8 cfPayload;

    if(len > FBL_ISOTP_MAX_PAYLOAD)
    {
        return;
    }

    payloadLen = Fbl_CanPayloadLen();
    addrOffset = g_iso.txAddrOffset;
    pciIdx = addrOffset;

    if(addrOffset != 0u)
    {
        if(payloadLen <= 2u)
        {
            return;
        }
    }

    sfMax = (uint8)(payloadLen - addrOffset - 1u);
    if((payloadLen > 8u) && (sfMax > 7u))
    {
        sfMax = (uint8)(payloadLen - addrOffset - 2u);
    }

    if(len <= sfMax)
    {
        memset(frame, 0u, sizeof(frame));
        if(addrOffset != 0u)
        {
            frame[0u] = FBL_CAN_EXT_ADDR_TESTER;
        }

        if((payloadLen > 8u) && (len > 7u))
        {
            frame[pciIdx] = 0x00u;
            frame[pciIdx + 1u] = (uint8)len;
            memcpy(&frame[pciIdx + 2u], data, len);
        }
        else
        {
            frame[pciIdx] = (uint8)len;
            memcpy(&frame[pciIdx + 1u], data, len);
        }

        Fbl_CanSend(frame, payloadLen);
        return;
    }

    memset(frame, 0u, sizeof(frame));
    if(addrOffset != 0u)
    {
        frame[0u] = FBL_CAN_EXT_ADDR_TESTER;
    }

    frame[pciIdx] = (uint8)(0x10u | ((len >> 8u) & 0x0Fu));
    frame[pciIdx + 1u] = (uint8)(len & 0xFFu);
    ffPayload = (uint8)(payloadLen - addrOffset - 2u);
    chunk = ffPayload;
    if(chunk > len) { chunk = (uint8)len; }
    memcpy(&frame[pciIdx + 2u], data, chunk);
    Fbl_CanSend(frame, payloadLen);

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
        if(addrOffset != 0u)
        {
            frame[0u] = FBL_CAN_EXT_ADDR_TESTER;
        }

        frame[pciIdx] = (uint8)(0x20u | (g_iso.txSn & 0x0Fu));

        cfPayload = (uint8)(payloadLen - addrOffset - 1u);
        chunk = cfPayload;
        if((g_iso.txIdx + chunk) > g_iso.txLen)
        {
            chunk = (uint8)(g_iso.txLen - g_iso.txIdx);
        }

        memcpy(&frame[pciIdx + 1u], &data[g_iso.txIdx], chunk);
        Fbl_CanSend(frame, payloadLen);
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
    uint16 did;
    uint32 addr;
    uint32 size;
    uint32 crcExpected;
    uint32 crcCalc;

    if(len == 0u) { return; }
    sid = req[0u];

    if(sid == UDS_SID_SESSION)
    {
        if(len < 2u) { Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport); return; }
        if((req[1u] & 0x7Fu) == 0x01u)
        {
            g_secSeedValid = 0u;
        }

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
        if(len != 3u)
        {
            Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport);
            return;
        }

        did = Fbl_Rd16(&req[1u]);

        if(did == UDS_DID_ACTIVE_SESSION)
        {
            res[0u] = 0x62u;
            Fbl_Wr16(&res[1u], did);
            res[3u] = FBL_DIAG_SESSION_PROGRAMMING;
            Fbl_UdsSend(res, 4u, transport);
        }
        else if(did == UDS_DID_ACTIVE_SOFTWARE_BLOCK)
        {
            res[0u] = 0x62u;
            Fbl_Wr16(&res[1u], did);
            res[3u] = FBL_ACTIVE_SOFTWARE_BLOCK;
            Fbl_UdsSend(res, 4u, transport);
        }
        else if(did == UDS_DID_SOFTWARE_VERSION)
        {
            res[0u] = 0x62u;
            Fbl_Wr16(&res[1u], did);
            res[3u] = APP_SW_VERSION_MAJOR;
            res[4u] = APP_SW_VERSION_MINOR;
            res[5u] = APP_SW_VERSION_PATCH;
            res[6u] = FBL_SW_VERSION_MAJOR;
            res[7u] = FBL_SW_VERSION_MINOR;
            res[8u] = FBL_SW_VERSION_PATCH;
            Fbl_UdsSend(res, 9u, transport);
        }
        else
        {
            Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
        }
    }
    else if(sid == UDS_SID_SECURITY_ACCESS)
    {
        Fbl_UdsSecurityAccess(req, len, transport);
    }
    else if(sid == UDS_SID_COMM_CONTROL)
    {
        if(len != 3u) { Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport); return; }

        res[0u] = 0x68u;
        res[1u] = (uint8)(req[1u] & 0x7Fu);
        Fbl_UdsSend(res, 2u, transport);
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

            addr = Fbl_Rd32(&req[4u]);   /* logical address from tester, cached 0x8 alias */
            size = Fbl_Rd32(&req[8u]);
            crcExpected = Fbl_Rd32(&req[12u]);

            if(Fbl_ValidateDownloadRange(addr, size) != FBL_ADDR_OK)
            {
                Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
                return;
            }

            (void)Fbl_FlashFlush();
            /* Verification reads use the non-cached 0xA alias so they never hit stale cache. */
            crcCalc = Fbl_Crc32(Fbl_ToNonCachedPflash(addr), size);

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

        addr = Fbl_Rd32(&req[4u]);   /* logical address from tester, cached 0x8 alias */
        size = Fbl_Rd32(&req[8u]);

        if(Fbl_ValidateDownloadRange(addr, size) != FBL_ADDR_OK)
        {
            Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
            return;
        }

        g_dl.active = 1u;
        g_dl.logicalAddr = addr;                        /* keep cached address for diagnostics */
        g_dl.startAddr = Fbl_ToNonCachedPflash(addr);   /* program/verify via non-cached 0xA alias */
        g_dl.curAddr = g_dl.startAddr;
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

        /* Programming of this block is complete: drop any stale instructions for
         * the freshly written PFLASH before it can be executed. */
        IfxCpu_invalidateProgramCache();

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
    else if(sid == UDS_SID_CONTROL_DTC_SETTING)
    {
        uint8 settingType;

        if(len != 2u) { Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport); return; }

        settingType = (uint8)(req[1u] & 0x7Fu);
        if((settingType != 0x01u) && (settingType != 0x02u))
        {
            Fbl_UdsNeg(sid, UDS_NRC_SUBFUNC_NOT_SUPPORTED, transport);
            return;
        }

        res[0u] = 0xC5u;
        res[1u] = settingType;
        Fbl_UdsSend(res, 2u, transport);
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

static void Fbl_UdsSecurityAccess(const uint8 *req, uint16 len, uint8 transport)
{
    uint8 res[6];
    uint8 sub;
    uint8 level;
    uint32 key;
    uint32 expected;

    if(len < 2u)
    {
        Fbl_UdsNeg(UDS_SID_SECURITY_ACCESS, UDS_NRC_INCORRECT_LEN, transport);
        return;
    }

    sub = req[1u];

    if((sub == 0u) || (sub > 0x08u))
    {
        Fbl_UdsNeg(UDS_SID_SECURITY_ACCESS, UDS_NRC_OUT_OF_RANGE, transport);
        return;
    }

    level = (uint8)((sub - 1u) / 2u);

    if((sub & 0x01u) != 0u)
    {
        if(len != 2u)
        {
            Fbl_UdsNeg(UDS_SID_SECURITY_ACCESS, UDS_NRC_INCORRECT_LEN, transport);
            return;
        }

        g_secSeed = 0x5A5A0000u ^ ((uint32)level << 8u) ^ g_dl.received ^ g_dl.curAddr;
        g_secSeedValid = 1u;

        res[0u] = 0x67u;
        res[1u] = sub;
        Fbl_Wr32(&res[2u], g_secSeed);
        Fbl_UdsSend(res, 6u, transport);
        return;
    }

    if(len != 6u)
    {
        Fbl_UdsNeg(UDS_SID_SECURITY_ACCESS, UDS_NRC_INCORRECT_LEN, transport);
        return;
    }

    if(g_secSeedValid == 0u)
    {
        Fbl_UdsNeg(UDS_SID_SECURITY_ACCESS, UDS_NRC_SEQUENCE_ERROR, transport);
        return;
    }

    key = Fbl_Rd32(&req[2u]);
    expected = Fbl_SecCalcKey(g_secSeed, level);

    if(key != expected)
    {
        g_secSeedValid = 0u;
        Fbl_UdsNeg(UDS_SID_SECURITY_ACCESS, UDS_NRC_INVALID_KEY, transport);
        return;
    }

    g_secSeedValid = 0u;
    res[0u] = 0x67u;
    res[1u] = sub;
    Fbl_UdsSend(res, 2u, transport);
}

static uint32 Fbl_SecCalcKey(uint32 seed, uint8 level)
{
    uint32 key;

    key = seed ^ 0x6A09E667u;
    key += 0x13572468u + ((uint32)level * 0x1F3D5B79u);
    key = (key << 3u) | (key >> 29u);
    key ^= ((seed << 16u) | (seed >> 16u));

    return key;
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
    uint16 payloadType;

    if(len < DOIP_HEADER_LEN) { return; }
    if((buf[0u] != DOIP_PROTO_VER) || (buf[1u] != DOIP_INV_PROTO_VER)) { return; }

    payloadType = Fbl_Rd16(&buf[2u]);

    if(payloadType == DOIP_PT_VID_REQ)
    {
        Fbl_DoIpSendVehicleId();
    }
}

static void Fbl_DoIpHandleTcp(const uint8 *buf, uint16 len)
{
    uint32 payloadLen;
    uint16 frameLen;
    uint16 remaining;

    if((buf == NULL) || (len == 0u)) { return; }

    if(len > (uint16)(sizeof(g_doipTcpStream) - g_doipTcpStreamLen))
    {
        g_doipTcpStreamLen = 0u;
        return;
    }

    memcpy(&g_doipTcpStream[g_doipTcpStreamLen], buf, len);
    g_doipTcpStreamLen = (uint16)(g_doipTcpStreamLen + len);

    while(g_doipTcpStreamLen >= DOIP_HEADER_LEN)
    {
        if((g_doipTcpStream[0u] != DOIP_PROTO_VER) ||
           (g_doipTcpStream[1u] != DOIP_INV_PROTO_VER))
        {
            g_doipTcpStreamLen = 0u;
            return;
        }

        payloadLen = Fbl_Rd32(&g_doipTcpStream[4u]);

        if(payloadLen > (uint32)(sizeof(g_doipTcpStream) - DOIP_HEADER_LEN))
        {
            g_doipTcpStreamLen = 0u;
            return;
        }

        frameLen = (uint16)(payloadLen + DOIP_HEADER_LEN);

        if(g_doipTcpStreamLen < frameLen)
        {
            return;
        }

        Fbl_DoIpHandleTcpFrame(g_doipTcpStream, frameLen);

        remaining = (uint16)(g_doipTcpStreamLen - frameLen);
        if(remaining > 0u)
        {
            memmove(g_doipTcpStream, &g_doipTcpStream[frameLen], remaining);
        }
        g_doipTcpStreamLen = remaining;
    }
}

static void Fbl_DoIpHandleTcpFrame(const uint8 *buf, uint16 len)
{
    uint16 payloadType;
    uint16 testerAddr;
    uint16 ecuAddr;
    uint16 udsLen;
    uint32 payloadLen;

    if(len < DOIP_HEADER_LEN) { return; }
    if((buf[0u] != DOIP_PROTO_VER) || (buf[1u] != DOIP_INV_PROTO_VER)) { return; }

    payloadType = Fbl_Rd16(&buf[2u]);
    payloadLen = Fbl_Rd32(&buf[4u]);

    if((payloadLen + DOIP_HEADER_LEN) != len) { return; }

    if(payloadType == DOIP_PT_ROUTING_ACT_REQ)
    {
        if(payloadLen < 7u) { return; }

        testerAddr = Fbl_Rd16(&buf[8u]);

        if((testerAddr == 0u) || (testerAddr != DOIP_TESTER_ADDR))
        {
            Fbl_DoIpSendRoutingActivationRes(testerAddr, DOIP_RA_RES_DENIED_UNKNOWN_SRC);
            return;
        }

        if((buf[10u] != 0x00u) && (buf[10u] != 0x01u))
        {
            Fbl_DoIpSendRoutingActivationRes(testerAddr, DOIP_RA_RES_UNSUPPORTED_ACT);
            return;
        }

        g_doipTesterAddr = testerAddr;
        g_doipRoutingActive = 1u;

        Fbl_DoIpSendRoutingActivationRes(testerAddr, DOIP_RA_RES_OK);
    }
    else if(payloadType == DOIP_PT_ALIVE_CHECK_REQ)
    {
        if(payloadLen == 0u)
        {
            Fbl_DoIpSendAliveRes();
        }
    }
    else if(payloadType == DOIP_PT_DIAG_MSG)
    {
        if(payloadLen < 5u) { return; }

        testerAddr = Fbl_Rd16(&buf[8u]);
        ecuAddr = Fbl_Rd16(&buf[10u]);

        if(g_doipRoutingActive == 0u)
        {
            Fbl_DoIpSendDiagNack(testerAddr, ecuAddr, DOIP_NACK_TRANSPORT_ERROR);
            return;
        }

        if((testerAddr != g_doipTesterAddr) || (testerAddr != DOIP_TESTER_ADDR))
        {
            Fbl_DoIpSendDiagNack(testerAddr, ecuAddr, DOIP_NACK_INVALID_SOURCE_ADDR);
            return;
        }

        if(ecuAddr != DOIP_ECU_ADDR)
        {
            Fbl_DoIpSendDiagNack(testerAddr, ecuAddr, DOIP_NACK_UNKNOWN_TARGET_ADDR);
            return;
        }

        if((payloadLen - 4u) > FBL_ISOTP_MAX_PAYLOAD)
        {
            Fbl_DoIpSendDiagNack(testerAddr, ecuAddr, DOIP_NACK_DIAG_MSG_TOO_LARGE);
            return;
        }

        udsLen = (uint16)(payloadLen - 4u);
        Fbl_DoIpSendDiagAck(testerAddr, ecuAddr);
        Fbl_UdsHandle(&buf[12u], udsLen, FBL_TRANSPORT_ETH);
    }
}

static void Fbl_DoIpSendVehicleId(void)
{
    uint8 res[DOIP_HEADER_LEN + DOIP_VID_RES_LEN];
    uint16 idx;

    res[0u] = DOIP_PROTO_VER;
    res[1u] = DOIP_INV_PROTO_VER;
    Fbl_Wr16(&res[2u], DOIP_PT_VID_RES);
    Fbl_Wr32(&res[4u], DOIP_VID_RES_LEN);

    idx = DOIP_HEADER_LEN;
    memcpy(&res[idx], Fbl_DoIpVin, sizeof(Fbl_DoIpVin));
    idx = (uint16)(idx + sizeof(Fbl_DoIpVin));

    Fbl_Wr16(&res[idx], DOIP_ECU_ADDR);
    idx = (uint16)(idx + 2u);

    memcpy(&res[idx], Fbl_DoIpEid, sizeof(Fbl_DoIpEid));
    idx = (uint16)(idx + sizeof(Fbl_DoIpEid));

    memcpy(&res[idx], Fbl_DoIpGid, sizeof(Fbl_DoIpGid));
    idx = (uint16)(idx + sizeof(Fbl_DoIpGid));

    res[idx++] = 0x00u;
    res[idx++] = 0x00u;

    FblEth_UdpSend(res, idx);
}

static void Fbl_DoIpSendRoutingActivationRes(uint16 testerAddr, uint8 code)
{
    uint8 res[DOIP_HEADER_LEN + DOIP_RA_RES_LEN];
    uint16 idx;

    res[0u] = DOIP_PROTO_VER;
    res[1u] = DOIP_INV_PROTO_VER;
    Fbl_Wr16(&res[2u], DOIP_PT_ROUTING_ACT_RES);
    Fbl_Wr32(&res[4u], DOIP_RA_RES_LEN);

    idx = DOIP_HEADER_LEN;
    Fbl_Wr16(&res[idx], testerAddr);
    idx = (uint16)(idx + 2u);
    Fbl_Wr16(&res[idx], DOIP_ECU_ADDR);
    idx = (uint16)(idx + 2u);
    res[idx++] = code;

    res[idx++] = 0x00u;
    res[idx++] = 0x00u;
    res[idx++] = 0x00u;
    res[idx++] = 0x00u;
    res[idx++] = 0x00u;
    res[idx++] = 0x00u;
    res[idx++] = 0x00u;
    res[idx++] = 0x00u;

    FblEth_TcpSend(res, idx);
}

static void Fbl_DoIpSendAliveRes(void)
{
    uint8 res[DOIP_HEADER_LEN + DOIP_ALIVE_RES_LEN];
    uint16 idx;

    res[0u] = DOIP_PROTO_VER;
    res[1u] = DOIP_INV_PROTO_VER;
    Fbl_Wr16(&res[2u], DOIP_PT_ALIVE_CHECK_RES);
    Fbl_Wr32(&res[4u], DOIP_ALIVE_RES_LEN);

    idx = DOIP_HEADER_LEN;
    Fbl_Wr16(&res[idx], DOIP_ECU_ADDR);
    idx = (uint16)(idx + 2u);

    FblEth_TcpSend(res, idx);
}

static void Fbl_DoIpSendDiagAck(uint16 testerAddr, uint16 ecuAddr)
{
    uint8 res[DOIP_HEADER_LEN + DOIP_DIAG_ACK_LEN];
    uint16 idx;

    res[0u] = DOIP_PROTO_VER;
    res[1u] = DOIP_INV_PROTO_VER;
    Fbl_Wr16(&res[2u], DOIP_PT_DIAG_ACK);
    Fbl_Wr32(&res[4u], DOIP_DIAG_ACK_LEN);

    idx = DOIP_HEADER_LEN;
    Fbl_Wr16(&res[idx], testerAddr);
    idx = (uint16)(idx + 2u);
    Fbl_Wr16(&res[idx], ecuAddr);
    idx = (uint16)(idx + 2u);
    res[idx++] = 0x00u;

    FblEth_TcpSend(res, idx);
}

static void Fbl_DoIpSendDiagNack(uint16 testerAddr, uint16 ecuAddr, uint8 nack)
{
    uint8 res[DOIP_HEADER_LEN + DOIP_DIAG_ACK_LEN];
    uint16 idx;

    res[0u] = DOIP_PROTO_VER;
    res[1u] = DOIP_INV_PROTO_VER;
    Fbl_Wr16(&res[2u], DOIP_PT_DIAG_NACK);
    Fbl_Wr32(&res[4u], DOIP_DIAG_ACK_LEN);

    idx = DOIP_HEADER_LEN;
    Fbl_Wr16(&res[idx], testerAddr);
    idx = (uint16)(idx + 2u);
    Fbl_Wr16(&res[idx], ecuAddr);
    idx = (uint16)(idx + 2u);
    res[idx++] = nack;

    FblEth_TcpSend(res, idx);
}

static void Fbl_DoIpSendDiag(const uint8 *uds, uint16 udsLen)
{
    uint8 buf[512];
    uint16 targetAddr;

    if((udsLen + 12u) > sizeof(buf)) { return; }
    if(g_doipRoutingActive == 0u) { return; }

    targetAddr = (g_doipTesterAddr != 0u) ? g_doipTesterAddr : DOIP_TESTER_ADDR;

    buf[0u] = DOIP_PROTO_VER;
    buf[1u] = DOIP_INV_PROTO_VER;
    Fbl_Wr16(&buf[2u], DOIP_PT_DIAG_MSG);
    Fbl_Wr32(&buf[4u], (uint32)udsLen + 4u);
    Fbl_Wr16(&buf[8u], DOIP_ECU_ADDR);
    Fbl_Wr16(&buf[10u], targetAddr);
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

    /* TriCore has no range-granular program-cache invalidate; do a full program
     * cache invalidate so no stale instructions remain for the just-erased range. */
    IfxCpu_invalidateProgramCache();

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
        return PROGRAM_FLASH_BANK_B; // @suppress("Symbol is not resolved")
    }

    return PROGRAM_FLASH_BANK_A; // @suppress("Symbol is not resolved")
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

/* Validate a download/verify target supplied by the tester.
 * The address is a logical address and may be given in either the cached 0x8 or
 * the non-cached 0xA PFLASH alias; validation is performed on the physical offset
 * so both aliases are treated identically. Conversion to the non-cached alias is
 * done by the caller (via Fbl_ToNonCachedPflash) only after this returns OK. */
static Fbl_AddressStatus Fbl_ValidateDownloadRange(uint32 logicalAddr, uint32 length)
{
    uint32 seg;
    uint32 offset;
    uint32 endOffset;
    const uint32 appStartOff = (APP_PFLASH_START_NC & PFLASH_ALIAS_MASK);
    const uint32 appEndOff   = (APP_PFLASH_END_NC   & PFLASH_ALIAS_MASK);
    const uint32 fblStartOff = (FBL_PFLASH_START_CACHED & PFLASH_ALIAS_MASK);
    const uint32 fblEndOff   = (FBL_PFLASH_END_CACHED   & PFLASH_ALIAS_MASK);

    if(length == 0u)                            { return FBL_ADDR_ERR_OVERFLOW; }
    if((logicalAddr + length) < logicalAddr)    { return FBL_ADDR_ERR_OVERFLOW; }

    /* Reject DFLASH and anything that is not a PFLASH cached/non-cached alias. */
    if((logicalAddr >= DFLASH_START_NC) && (logicalAddr <= DFLASH_END_NC))
    {
        return FBL_ADDR_ERR_PROTECTED;
    }

    seg = logicalAddr & FLASH_ADDRESS_PREFIX_MASK;
    if((seg != (PFLASH_CACHED_BASE    & FLASH_ADDRESS_PREFIX_MASK)) &&
       (seg != (PFLASH_NONCACHED_BASE & FLASH_ADDRESS_PREFIX_MASK)))
    {
        return FBL_ADDR_ERR_NOT_APP;
    }

    offset    = logicalAddr & PFLASH_ALIAS_MASK;
    endOffset = offset + length - 1u;

    /* Reject writes into the FBL / BMHD / UCB / protected areas. */
    if((offset <= fblEndOff) && (endOffset >= fblStartOff))
    {
        return FBL_ADDR_ERR_PROTECTED;
    }

    /* Must lie fully inside the configured APP PFLASH window. */
    if((offset < appStartOff) || (endOffset > appEndOff))
    {
        return FBL_ADDR_ERR_NOT_APP;
    }

    /* TC3xx programs in 32-byte pages: the start must be page aligned. */
    if((offset & (PFLASH_PAGE_SIZE - 1u)) != 0u)
    {
        return FBL_ADDR_ERR_ALIGN;
    }

    return FBL_ADDR_OK;
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

    /* The updated application image was programmed through the non-cached alias.
     * Invalidate the program cache so the core fetches the fresh image, then jump
     * to the cached execution alias (0x8...). Never jump to the 0xA... alias. */
    IfxCpu_invalidateProgramCache();
    __asm("isync");
    __asm("ji %0" : : "a"(APP_START_CACHED));
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
