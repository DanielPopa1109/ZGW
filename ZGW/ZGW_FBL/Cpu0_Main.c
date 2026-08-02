#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "IfxScuRcu.h"
#include "IfxStm_reg.h"
#include "IfxPort.h"
#include "IfxPort_reg.h"
#include "aurix_pin_mappings.h"
#include "FblBlu.h"
#include "FblTransport_Cfg.h"

#define FBL_TRANSPORT_ETH                1u

#define FBL_RESET_INFO_ENTER_DOIP        0xFCD0u

/* Application -> FBL programming handoff.
 *
 * The handoff used to live in the cached LMU "NCR" region, but LBIST
 * clears that on every reset, so it was dropped.  The application now publishes
 * the request in the SCR XRAM mailbox (PMS standby RAM at 0xF0240000), which
 * survives the application reset that enters the FBL.  Layout mirrors the
 * SCR_FBL_* record in ZGW_APP/SCR/scr_time_shared.h and the writer
 * McuSm_ArmFblProgrammingRequest() in ZGW_APP/BSW/Sys/McuSm/McuSm.c. */
#define FBL_SCR_FBL_BASE                 0x17E0u
#define FBL_SCR_FBL_MAGIC                0x46424C31u   /* "FBL1" */
#define FBL_SCR_FBL_VERSION              1u
#define FBL_SCR_FBL_VALID                1u
#define FBL_SCR_OFF_MAGIC                0u
#define FBL_SCR_OFF_VERSION              4u
#define FBL_SCR_OFF_VALID                5u
#define FBL_SCR_OFF_PROG_REQUEST         6u
#define FBL_SCR_OFF_COMM_INTERFACE       7u
#define FBL_SCR_OFF_RESET_COUNTER        8u
#define FBL_SCR_OFF_CHECKSUM             12u

/* Retained NCR diag-session marker in non-cached CPU0 DLMU (write-only breadcrumb; nothing reads it). */
#define FBL_NCR_DIAG_SESSION_ADDR        0x900000F0u
#define FBL_PROGRAMMING_REQUEST_ACTIVE   1u
#define FBL_DIAG_SESSION_PROGRAMMING     0x02u
#define FBL_RESET_COUNTER_FORCE_MIN      49u
#define FBL_RESET_COUNTER_FORCE_MAX      52u

#define FBL_START_NCACHED                0xA0000000u
#define FBL_END_NCACHED                  0xA002FFFFu
#define FBL_SIZE_BYTES                   0x00030000u

#define APP_START_NCACHED                0xA0030000u
#define APP_END_NCACHED                  0xA05CFFFFu
#define APP_SIZE_BYTES                   ((APP_END_NCACHED - APP_START_NCACHED) + 1u)

#define PFLASH_NC_ADDRESS_PREFIX         0xA0000000u
#define FLASH_ADDRESS_PREFIX_MASK        0xFF000000u

/* PFLASH alias bases.
 * The same physical PFLASH is visible through cached segment 8 and non-cached
 * segment A. Direct erase/program/verify operations always use segment A. */
#define PFLASH_CACHED_BASE               0x80000000u
#define PFLASH_NONCACHED_BASE            0xA0000000u
#define PFLASH_ALIAS_MASK                0x1FFFFFFFu

/* Explicit FBL / APP PFLASH ranges, expressed in both aliases.
 * Values mirror the linker memory objects (FBL BootManager_PFLASH = 192K,
 * APP pfls0 starts at offset 0x30000). */
#define FBL_PFLASH_START_CACHED          0x80000000u
#define FBL_PFLASH_END_CACHED            0x8002FFFFu

#define APP_PFLASH_START_CACHED          0x80030000u
#define APP_PFLASH_END_CACHED            0x805CFFFFu

#define APP_PFLASH_START_NC              0xA0030000u
#define APP_PFLASH_END_NC                0xA05CFFFFu // @suppress("Unused static function")
#define APP_PFLASH_SIZE_BYTES            ((APP_PFLASH_END_NC - APP_PFLASH_START_NC) + 1u)

/* APP reset/start execution address (cached alias). The FBL jumps here. */
#define APP_START_CACHED                 APP_PFLASH_START_CACHED

/* DFLASH occupies segment-A too; reject it explicitly to catch PFLASH/DFLASH mixups. */
#define DFLASH_START_NC                  0xAF000000u
#define DFLASH_END_NC                    0xAFFFFFFFu
#define DFLASH_REJECT_PREFIX_MASK        0xFFFF0000u
#define DFLASH_REJECT_PREFIX_AF40        0xAF400000u
#define DFLASH_REJECT_PREFIX_AF01        0xAF010000u
#define DFLASH_REJECT_PREFIX_AF02        0xAF020000u

typedef enum
{
    FBL_ADDR_OK = 0,
    FBL_ADDR_ERR_OVERFLOW,
    FBL_ADDR_ERR_ALIGN,
    FBL_ADDR_ERR_NOT_APP,
    FBL_ADDR_ERR_PROTECTED
} Fbl_AddressStatus;

static inline uint32 Fbl_ToNonCachedPflash(uint32 addr)
{
    return (addr & PFLASH_ALIAS_MASK) | PFLASH_NONCACHED_BASE;
}

#define PFLASH_PAGE_SIZE                 32u
#define PFLASH_SECTOR_SIZE               0x4000u
#define FBL_BOOT_CRITICAL_SIZE           PFLASH_PAGE_SIZE
#define FBL_FLASH_PAD_BYTE               0xFFu
#define FBL_FLASH_NO_PAGE_ADDR           0xFFFFFFFFu
#define FBL_TX_DRAIN_POLLS               2000u
#define FBL_FINAL_TX_DRAIN_POLLS         10000u

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
#define UDS_NRC_RESPONSE_PENDING         0x78u

/* The whole-application erase is issued as one logical operation. The RAM flash
 * primitive splits only where the request crosses a physical PFLASH bank. */
#define FBL_BLU_REJECT_NONE                         0u
#define FBL_BLU_REJECT_SELECT_TARGET_CONFLICT       1u
#define FBL_BLU_REJECT_FBL_ERASE_STATE              2u
#define FBL_BLU_REJECT_APP_ERASE_FBL_NOT_STARTED    3u
#define FBL_BLU_REJECT_FBL_CRC_STATE                4u
#define FBL_BLU_REJECT_DOWNLOAD_FBL_TARGET          5u
#define FBL_BLU_REJECT_DOWNLOAD_NO_TARGET           6u
#define FBL_BLU_REJECT_DOWNLOAD_APP_SWITCH          7u
#define FBL_BLU_REJECT_DOWNLOAD_FBL_STATE           8u
#define FBL_BLU_REJECT_RESET_FBL_ACTIVE             9u

#define UDS_RID_ERASE_APP                0x0001u
#define UDS_RID_CRC_CHECK                0x0002u
#define UDS_RID_START_FBL_RAM_UPDATER    0x0155u
#define UDS_RID_SELECT_SW_BLOCK          0x0200u
#define UDS_DID_ACTIVE_SOFTWARE_BLOCK    0xF100u
#define UDS_DID_SOFTWARE_VERSION         0xF101u
#define UDS_DID_ACTIVE_SESSION           0xF186u
#define FBL_ACTIVE_SOFTWARE_BLOCK_APP    0x01u
#define FBL_ACTIVE_SOFTWARE_BLOCK_FBL    0x02u
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
#define DOIP_PT_GENERIC_NACK             0x0000u
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
#define DOIP_HEADER_LEN                  FBL_DOIP_HEADER_SIZE
#define DOIP_VID_RES_LEN                 33u
#define DOIP_RA_RES_LEN                  13u
#define DOIP_ALIVE_RES_LEN               2u
#define DOIP_DIAG_ACK_LEN                5u
/* The Ethernet layer owns the TCP reassembly stream.
 * Cpu0_Main owns only one same-sized complete-frame buffer.
 * A full transfer frame is 8-byte DoIP header + 4-byte diagnostic addresses +
 * 4098-byte UDS TransferData request = 4110 bytes. */
#define DOIP_DIAG_ADDRESS_BYTES          FBL_DOIP_DIAG_ADDRESS_SIZE
#define DOIP_TCP_RX_STREAM_SIZE          FBL_DOIP_TCP_RX_STREAM_SIZE

#if DOIP_TCP_RX_STREAM_SIZE < (DOIP_HEADER_LEN + DOIP_DIAG_ADDRESS_BYTES + FBL_UDS_MAX_BLOCK_LENGTH)
#error "DOIP_TCP_RX_STREAM_SIZE is smaller than the advertised maximum diagnostic frame"
#endif
#define DOIP_RA_RES_DENIED_UNKNOWN_SRC   0x00u
#define DOIP_RA_RES_UNSUPPORTED_ACT      0x05u
#define DOIP_RA_RES_OK                   0x10u
#define DOIP_GEN_NACK_INCORRECT_PATTERN  0x00u
#define DOIP_GEN_NACK_UNKNOWN_PAYLOAD    0x01u
#define DOIP_GEN_NACK_MESSAGE_TOO_LARGE  0x02u
#define DOIP_GEN_NACK_INVALID_LENGTH     0x04u
#define DOIP_NACK_INVALID_SOURCE_ADDR    0x02u
#define DOIP_NACK_UNKNOWN_TARGET_ADDR    0x03u
#define DOIP_NACK_DIAG_MSG_TOO_LARGE     0x04u
#define DOIP_NACK_TRANSPORT_ERROR        0x08u
#define DOIP_SEND_UDP                    0u
#define DOIP_SEND_TCP                    1u

#define FBL_RAM_CODE                     FBL_RAM_BLU_CODE

extern void FblEth_Init(void);
extern void FblEth_MainFunction(void);
extern void FblEth_PollReceiveOnly(void);
extern void FblEth_PollTimerOnly(void);
extern uint8 FblEth_TcpReceive(uint8 *buf, uint16 *len);
extern uint8 FblEth_UdpReceive(uint8 *buf, uint16 *len);
extern uint8 FblEth_HasPendingTcpData(void);
extern void FblEth_TcpSend(const uint8 *buf, uint16 len);
extern void FblEth_UdpSend(const uint8 *buf, uint16 len);
extern uint8 FblEth_TcpDrain(uint32 pollBudget);
extern void FblEth_SetDoIpCallbacks(void (*connectedCb)(void),
                                    void (*disconnectedCb)(void),
                                    void (*rxOverflowCb)(void));
extern uint8 FblEth_RuntimeClosureOk(void);

volatile uint32 g_FblTransportSelect = FBL_TRANSPORT_ETH;
volatile uint32 g_FblStayInBoot = 1u;
volatile uint16 g_FblLastResetReason = 0u;
volatile uint32 g_FblLastRawRstStat = 0u;
volatile uint32 g_FblLastResetType = 0u;
volatile uint32 g_FblLastResetTrigger = 0u;
volatile uint32 g_FblLastResetSafeState = 0u;
volatile uint16 g_FblLastResetUserInfoAfterConsume = 0u;
volatile uint8 g_FblDiagBootRequestSeen = 0u;

typedef struct
{
    uint8 active;
    uint8 imageKind;
    uint32 startAddr;     /* internal non-cached 0xA flash address */
    uint32 curAddr;       /* internal non-cached 0xA running flash address */
    uint32 length;
    uint32 received;
    uint8 nextBlock;
    uint8 lastBlock;
} FblDownload_Type;

typedef union
{
    uint8 bytes[PFLASH_PAGE_SIZE];
    uint32 words[PFLASH_PAGE_SIZE / sizeof(uint32)];
} FblPageBuffer_Type;

static FblDownload_Type g_dl;
static FblBlu_RecordType g_blu;
static FblPageBuffer_Type IFX_ALIGN(32) g_pageBuf;
static FblPageBuffer_Type IFX_ALIGN(32) g_fblBootCriticalPage;
static uint32 g_pageAddr = FBL_FLASH_NO_PAGE_ADDR;
static uint32 g_pageFill = 0u;
static uint32 g_fblBootCriticalMask = 0u;
/* One receive buffer only. FblEth owns TCP stream reassembly and returns a
 * complete DoIP frame, so Cpu0_Main does not maintain a second TCP stream
 * accumulation stream. The same buffer is reused for UDP datagrams. */
static uint8 IFX_ALIGN(32) g_doipRxScratch[DOIP_TCP_RX_STREAM_SIZE];
static uint16 g_doipRxLenScratch;
static uint8 g_doipTcpOverflowPending;
volatile uint32 g_FblDoIpLastTcpReceiveBufPtr;
volatile uint32 g_FblDoIpLastTcpReceiveLenPtr;
volatile uint32 g_FblDoIpLastUdpReceiveBufPtr;
volatile uint32 g_FblDoIpLastUdpReceiveLenPtr;
volatile uint32 g_FblTransferAdvertisedBlockLength;
volatile uint32 g_FblTransferLastUdsLength;
volatile uint32 g_FblTransferLastDataLength;
volatile uint32 g_FblTransferLastDoipPayloadLength;
volatile uint32 g_FblTransferLastFrameLength;
volatile uint32 g_FblTransferTcpStreamHighWatermark;
volatile uint32 g_FblTransferTcpCallbackCount;
volatile uint32 g_FblTransferOverflowCount;
volatile uint32 g_FblTransferProgrammedPageCount;
volatile uint32 g_FblBluLastRejectReason;
volatile uint32 g_FblBluLastRejectState;
volatile uint32 g_FblBluLastRejectFailure;
volatile uint32 g_FblBluLastRejectImageKind;
volatile uint32 g_FblBluLastRejectFlags;
volatile uint32 g_FblBluLastRejectImageLen;
volatile uint32 g_FblBluLastRejectImageCrc;
volatile uint32 g_FblBluLastTransition;
volatile uint32 g_FblBluLastTcpDrainResult;
volatile uint32 g_FblBluLastResetStage;
volatile uint32 g_FblBluScrReadValid;
volatile uint32 g_FblBluLastEraseFailAddr;
volatile uint32 g_FblBluLastEraseFailLen;
volatile uint32 g_FblBluLastEraseFailDmu;
volatile uint32 g_FblEraseLogicalCallCount;
volatile uint32 g_FblEraseLogicalLastStart;
volatile uint32 g_FblEraseLogicalLastLength;
volatile uint32 g_FblEraseLogicalLastResult;
volatile uint32 g_FblEraseLogicalStartTick;
volatile uint32 g_FblEraseLogicalEndTick;
volatile uint32 g_FblEraseLogicalTicks;
volatile uint32 g_FblEraseDoIpDrainStartTick;
volatile uint32 g_FblEraseDoIpDrainEndTick;
volatile uint32 g_FblEraseDoIpDrainTicks;
static uint16 g_doipTesterAddr;
static uint8 g_doipRoutingActive;

static void Fbl_WriteNcrU8(uint32 addr, uint8 value);
static uint32 Fbl_ScrFblChecksum(uint8 progRequest, uint8 commInterface, uint8 resetCounter);
static uint8 Fbl_ScrReadFblHandoff(uint8 *progRequest, uint8 *commInterface, uint8 *resetCounter);
static void Fbl_ScrInvalidateFblHandoff(void);
static uint8 Fbl_ScrReadBluState(FblBlu_RecordType *record);
static void Fbl_ScrWriteBluState(const FblBlu_RecordType *record);
static void Fbl_ScrClearBluState(void);
static void Fbl_BluResetRuntime(void);
static uint8 Fbl_BluSelectTarget(uint8 imageKind);
static uint8 Fbl_BluCanSwitchFromBootloaderToApplication(void);
static void Fbl_BluSetFailure(uint8 failure);
static uint8 Fbl_BluActiveSoftwareBlock(void);
static uint8 Fbl_NormalizeTransport(uint8 transport);
static uint8 Fbl_ResetCounterForcesProgramming(uint8 resetCounter);
static void Fbl_ConsumeResetUserInfo(void);
static uint8 Fbl_IsAppValid(void);
static void Fbl_PlatformInit(void);
static void Fbl_UdsHandle(const uint8 *req, uint16 len, uint8 transport);
static void Fbl_UdsSend(const uint8 *res, uint16 len, uint8 transport);
static void Fbl_UdsNeg(uint8 sid, uint8 nrc, uint8 transport);
static void Fbl_UdsNegSequence(uint8 sid, uint8 transport, uint32 reason);
static void Fbl_UdsKeepAlive(uint8 sid, uint8 transport);
static void Fbl_ServiceCommsDuringLongOp(void);
static uint8 Fbl_SendAndDrainFinalPositive(const uint8 *res, uint16 len, uint8 transport);
static uint8 Fbl_SendAndDrainPositive(const uint8 *res, uint16 len, uint8 transport);
static void Fbl_UdsSecurityAccess(const uint8 *req, uint16 len, uint8 transport);
static void Fbl_DoIpMain(void);
static void Fbl_DoIpHandleUdp(const uint8 *buf, uint16 len);
static void Fbl_DoIpHandleTcpFrame(const uint8 *buf, uint16 len);
static void Fbl_DoIpResetTcpState(void);
static void Fbl_DoIpSendGenericNack(uint8 viaTcp, uint8 code);
static void Fbl_DoIpSendVehicleId(void);
static void Fbl_DoIpSendRoutingActivationRes(uint16 testerAddr, uint8 code);
static void Fbl_DoIpSendAliveRes(void);
static void Fbl_DoIpSendDiagAck(uint16 testerAddr, uint16 ecuAddr);
static void Fbl_DoIpSendDiagNack(uint16 testerAddr, uint16 ecuAddr, uint8 nack);
static void Fbl_DoIpSendDiag(const uint8 *uds, uint16 udsLen);
void Fbl_DoIpTcpConnected(void);
void Fbl_DoIpTcpDisconnected(void);
void Fbl_DoIpTcpRxOverflow(void);
static void Fbl_FlashInit(void);
FBL_RAM_CODE static uint32 Fbl_FlashEraseRange(uint32 addr, uint32 len);
FBL_RAM_CODE static uint32 Fbl_FlashProgram(uint32 addr, const uint8 *data, uint32 len, uint8 sid, uint8 transport);
FBL_RAM_CODE static uint32 Fbl_FlashProgramFblPayload(uint32 addr, const uint8 *data, uint32 len, uint8 sid, uint8 transport);
FBL_RAM_CODE static uint32 Fbl_FlashFlush(uint8 sid, uint8 transport);
FBL_RAM_CODE static uint32 Fbl_FlashProgramPage(uint32 addr, const uint8 *data);
FBL_RAM_CODE static uint8 Fbl_FlashVerifyPage(uint32 addr, const uint8 *data);
FBL_RAM_CODE static uint32 Fbl_BluEraseFblRange(uint32 addr, uint32 len);
FBL_RAM_CODE static uint8 Fbl_BootCriticalPageComplete(void);
FBL_RAM_CODE static uint32 Fbl_CommitBootCriticalPage(uint8 sid, uint8 transport);
FBL_RAM_CODE static uint32 Fbl_Crc32(uint32 addr, uint32 len, uint8 sid, uint8 transport);
FBL_RAM_CODE static uint32 Fbl_Crc32FblWithDelayedBootPage(uint32 addr, uint32 len, uint8 sid, uint8 transport);
FBL_RAM_CODE static uint32 Fbl_Crc32UpdateByte(uint32 crc, uint8 value);
static uint8 Fbl_IsExplicitlyRejectedProgrammingAddress(uint32 addr);
static uint8 Fbl_IsFullFblLogicalRange(uint32 logicalAddr, uint32 length);
static Fbl_AddressStatus Fbl_ValidateDownloadRange(uint32 logicalAddr, uint32 length);
static uint32 Fbl_Rd32(const uint8 *p);
static uint16 Fbl_Rd16(const uint8 *p);
static void Fbl_Wr16(uint8 *p, uint16 v);
static void Fbl_Wr32(uint8 *p, uint32 v);
static void Fbl_JumpToApp(void);
static uint32 Fbl_ActiveDownloadPhysicalStart(uint32 logicalAddr);
static uint8 Fbl_RuntimeClosureOk(void);
static uint8 Fbl_EnterDirectUpdateRuntime(void);
static uint8 Fbl_BluHasVerifiedFblEvidence(void);
static void Fbl_BluCaptureReject(uint32 reason);

static void Fbl_WriteNcrU8(uint32 addr, uint8 value)
{
    *((volatile uint8 *)addr) = value;
}

/* FNV-1a over the three payload bytes. Must match McuSm_ArmFblProgrammingRequest
 * (McuSm_CalculateChecksum) on the application side. */
static uint32 Fbl_ScrFblChecksum(uint8 progRequest, uint8 commInterface, uint8 resetCounter)
{
    uint32 checksum = 0x811C9DC5u;

    checksum = (checksum ^ (uint32)progRequest) * 16777619u;
    checksum = (checksum ^ (uint32)commInterface) * 16777619u;
    checksum = (checksum ^ (uint32)resetCounter) * 16777619u;

    return checksum;
}

/* Read the application's programming handoff from the SCR XRAM mailbox.
 * Returns 1 and fills the outputs only for a valid, checksum-matched record. */
static uint8 Fbl_ScrReadFblHandoff(uint8 *progRequest, uint8 *commInterface, uint8 *resetCounter)
{
    volatile const uint8 *rec = &((volatile const uint8 *)FBL_SCR_XRAM_ADDR)[FBL_SCR_FBL_BASE];
    uint32 magic;
    uint32 stored;
    uint8 prog;
    uint8 comm;
    uint8 rstc;

    magic = ((uint32)rec[FBL_SCR_OFF_MAGIC] << 24u) |
            ((uint32)rec[FBL_SCR_OFF_MAGIC + 1u] << 16u) |
            ((uint32)rec[FBL_SCR_OFF_MAGIC + 2u] << 8u) |
            ((uint32)rec[FBL_SCR_OFF_MAGIC + 3u]);

    if((magic != FBL_SCR_FBL_MAGIC) ||
       (rec[FBL_SCR_OFF_VERSION] != FBL_SCR_FBL_VERSION) ||
       (rec[FBL_SCR_OFF_VALID] != FBL_SCR_FBL_VALID))
    {
        return 0u;
    }

    prog = rec[FBL_SCR_OFF_PROG_REQUEST];
    comm = rec[FBL_SCR_OFF_COMM_INTERFACE];
    rstc = rec[FBL_SCR_OFF_RESET_COUNTER];

    stored = ((uint32)rec[FBL_SCR_OFF_CHECKSUM] << 24u) |
             ((uint32)rec[FBL_SCR_OFF_CHECKSUM + 1u] << 16u) |
             ((uint32)rec[FBL_SCR_OFF_CHECKSUM + 2u] << 8u) |
             ((uint32)rec[FBL_SCR_OFF_CHECKSUM + 3u]);

    if(stored != Fbl_ScrFblChecksum(prog, comm, rstc))
    {
        return 0u;
    }

    *progRequest = prog;
    *commInterface = comm;
    *resetCounter = rstc;

    return 1u;
}

/* Consume the mailbox: clearing VALID makes the programming request single-shot
 * so the FBL cannot re-enter programming on the next (non-diagnostic) reset. */
static void Fbl_ScrInvalidateFblHandoff(void)
{
    volatile uint8 *rec = &((volatile uint8 *)FBL_SCR_XRAM_ADDR)[FBL_SCR_FBL_BASE];

    rec[FBL_SCR_OFF_VALID] = 0u;
}

static uint8 Fbl_ScrReadBluState(FblBlu_RecordType *record)
{
    volatile const uint8 *rec = &((volatile const uint8 *)FBL_SCR_XRAM_ADDR)[FBL_SCR_BLU_BASE];
    uint8 framed[18];
    uint32 magic;
    uint32 stored;
    uint32 checksum = 0x811C9DC5u;
    uint8 i;

    magic = Fbl_Rd32((const uint8 *)&rec[FBL_SCR_BLU_OFF_MAGIC]);
    if((magic != FBL_SCR_BLU_MAGIC) ||
       (rec[FBL_SCR_BLU_OFF_VERSION] != FBL_SCR_BLU_VERSION) ||
       (rec[FBL_SCR_BLU_OFF_VALID] != FBL_SCR_BLU_VALID))
    {
        return 0u;
    }

    FblRam_CopyBytes(&framed[0u], (const uint8 *)&rec[FBL_SCR_BLU_OFF_STATE], sizeof(framed));
    stored = Fbl_Rd32((const uint8 *)&rec[FBL_SCR_BLU_OFF_CHECKSUM]);

    for(i = 0u; i < sizeof(framed); i++)
    {
        checksum ^= (uint32)framed[i];
        checksum *= 16777619u;
    }

    if(stored != checksum)
    {
        return 0u;
    }

    record->state = rec[FBL_SCR_BLU_OFF_STATE];
    record->failure = rec[FBL_SCR_BLU_OFF_FAILURE];
    record->imageKind = rec[FBL_SCR_BLU_OFF_IMAGE_KIND];
    record->imageLen = Fbl_Rd32((const uint8 *)&rec[FBL_SCR_BLU_OFF_IMAGE_LEN]);
    record->imageCrc = Fbl_Rd32((const uint8 *)&rec[FBL_SCR_BLU_OFF_IMAGE_CRC]);
    record->flags = Fbl_Rd32((const uint8 *)&rec[FBL_SCR_BLU_OFF_FLAGS]);

    return 1u;
}

static void Fbl_ScrWriteBluState(const FblBlu_RecordType *record)
{
    volatile uint8 *rec = &((volatile uint8 *)FBL_SCR_XRAM_ADDR)[FBL_SCR_BLU_BASE];
    uint8 framed[18];
    uint32 checksum = 0x811C9DC5u;
    uint8 i;

    rec[FBL_SCR_BLU_OFF_VALID] = 0u;
    rec[FBL_SCR_BLU_OFF_MAGIC + 0u] = (uint8)(FBL_SCR_BLU_MAGIC >> 24u);
    rec[FBL_SCR_BLU_OFF_MAGIC + 1u] = (uint8)(FBL_SCR_BLU_MAGIC >> 16u);
    rec[FBL_SCR_BLU_OFF_MAGIC + 2u] = (uint8)(FBL_SCR_BLU_MAGIC >> 8u);
    rec[FBL_SCR_BLU_OFF_MAGIC + 3u] = (uint8)FBL_SCR_BLU_MAGIC;
    rec[FBL_SCR_BLU_OFF_VERSION] = FBL_SCR_BLU_VERSION;
    rec[FBL_SCR_BLU_OFF_STATE] = record->state;
    rec[FBL_SCR_BLU_OFF_FAILURE] = record->failure;
    rec[FBL_SCR_BLU_OFF_IMAGE_KIND] = record->imageKind;
    rec[FBL_SCR_BLU_OFF_RESERVED] = 0u;
    Fbl_Wr32((uint8 *)&rec[FBL_SCR_BLU_OFF_IMAGE_LEN], record->imageLen);
    Fbl_Wr32((uint8 *)&rec[FBL_SCR_BLU_OFF_IMAGE_CRC], record->imageCrc);
    Fbl_Wr32((uint8 *)&rec[FBL_SCR_BLU_OFF_FLAGS], record->flags);

    FblRam_CopyBytes(&framed[0u], (const uint8 *)&rec[FBL_SCR_BLU_OFF_STATE], sizeof(framed));
    for(i = 0u; i < sizeof(framed); i++)
    {
        checksum ^= (uint32)framed[i];
        checksum *= 16777619u;
    }

    Fbl_Wr32((uint8 *)&rec[FBL_SCR_BLU_OFF_CHECKSUM], checksum);
    __dsync();
    rec[FBL_SCR_BLU_OFF_VALID] = FBL_SCR_BLU_VALID;
    __dsync();
}

static void Fbl_ScrClearBluState(void)
{
    volatile uint8 *rec = &((volatile uint8 *)FBL_SCR_XRAM_ADDR)[FBL_SCR_BLU_BASE];

    rec[FBL_SCR_BLU_OFF_VALID] = 0u;
    __dsync();
}

static void Fbl_BluResetRuntime(void)
{
    g_blu.state = FBL_BLU_STATE_IDLE;
    g_blu.failure = FBL_BLU_FAILURE_NONE;
    g_blu.imageKind = FBL_BLU_IMAGE_KIND_NONE;
    g_blu.imageLen = 0u;
    g_blu.imageCrc = 0u;
    g_blu.flags = 0u;
}

static uint8 Fbl_BluHasVerifiedFblEvidence(void)
{
    if((g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) &&
       (g_blu.imageLen == FBL_SIZE_BYTES) &&
       (g_blu.imageCrc != 0u) &&
       (g_blu.failure == FBL_BLU_FAILURE_NONE) &&
       ((g_blu.flags & FBL_BLU_FLAG_DIRECT_UPDATE) != 0u))
    {
        return 1u;
    }

    return 0u;
}

static void Fbl_BluCaptureReject(uint32 reason)
{
    g_FblBluLastRejectReason = reason;
    g_FblBluLastRejectState = g_blu.state;
    g_FblBluLastRejectFailure = g_blu.failure;
    g_FblBluLastRejectImageKind = g_blu.imageKind;
    g_FblBluLastRejectFlags = g_blu.flags;
    g_FblBluLastRejectImageLen = g_blu.imageLen;
    g_FblBluLastRejectImageCrc = g_blu.imageCrc;
}

static uint8 Fbl_BluSelectTarget(uint8 imageKind)
{
    if((imageKind != FBL_BLU_IMAGE_KIND_APPLICATION) &&
       (imageKind != FBL_BLU_IMAGE_KIND_BOOTLOADER))
    {
        return 0u;
    }

    if((g_blu.imageKind != FBL_BLU_IMAGE_KIND_NONE) &&
       (g_blu.imageKind != imageKind) &&
       !((imageKind == FBL_BLU_IMAGE_KIND_APPLICATION) &&
         (Fbl_BluCanSwitchFromBootloaderToApplication() != 0u)))
    {
        return 0u;
    }

    g_blu.imageKind = imageKind;
    if(imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER)
    {
        g_blu.state = FBL_BLU_STATE_ENTER_REQUESTED;
    }
    else
    {
        g_blu.state = FBL_BLU_STATE_PREPARE;
    }
    g_blu.failure = FBL_BLU_FAILURE_NONE;
    g_blu.flags |= FBL_BLU_FLAG_TARGET_LOCKED;
    Fbl_ScrWriteBluState(&g_blu);
    return 1u;
}

static uint8 Fbl_BluCanSwitchFromBootloaderToApplication(void)
{
    if(g_blu.imageKind != FBL_BLU_IMAGE_KIND_BOOTLOADER)
    {
        return 0u;
    }

    if((g_blu.state == FBL_BLU_STATE_FBL_STARTED) &&
       (Fbl_BluHasVerifiedFblEvidence() != 0u))
    {
        return 1u;
    }

    if((g_blu.state == FBL_BLU_STATE_ERASING) ||
       (g_blu.state == FBL_BLU_STATE_ERASE_COMPLETE) ||
       (g_blu.state == FBL_BLU_STATE_DOWNLOAD_ACTIVE) ||
       (g_blu.state == FBL_BLU_STATE_PROGRAMMING) ||
       (g_blu.state == FBL_BLU_STATE_VERIFYING) ||
       (g_blu.state == FBL_BLU_STATE_TRANSFER_COMPLETE) ||
       (g_blu.state == FBL_BLU_STATE_FINAL_VERIFY) ||
       (g_blu.state == FBL_BLU_STATE_COMMITTING) ||
       (g_blu.state == FBL_BLU_STATE_FBL_VERIFIED) ||
       (g_blu.state == FBL_BLU_STATE_REBOOT_PENDING) ||
       (g_blu.state == FBL_BLU_STATE_FAILED) ||
       (g_blu.state == FBL_BLU_STATE_RECOVERY_WAIT))
    {
        return 0u;
    }

    return 0u;
}

static void Fbl_BluSetFailure(uint8 failure)
{
    g_blu.state = FBL_BLU_STATE_FAILED;
    g_blu.failure = failure;
    Fbl_ScrWriteBluState(&g_blu);
}

FBL_RAM_CODE void Fbl_BluEnterRecoveryWaitFromTrap(void)
{
    g_blu.state = FBL_BLU_STATE_RECOVERY_WAIT;
    g_blu.failure = FBL_BLU_FAILURE_FLASH;
    Fbl_ScrWriteBluState(&g_blu);
}

static uint8 Fbl_BluActiveSoftwareBlock(void)
{
    if(g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER)
    {
        return FBL_ACTIVE_SOFTWARE_BLOCK_FBL;
    }

    return FBL_ACTIVE_SOFTWARE_BLOCK_APP;
}

static uint32 Fbl_ActiveDownloadPhysicalStart(uint32 logicalAddr)
{
    return Fbl_ToNonCachedPflash(logicalAddr);
}

static uint8 Fbl_NormalizeTransport(uint8 transport)
{
    (void)transport;
    return FBL_TRANSPORT_ETH;
}

static uint8 Fbl_ResetCounterForcesProgramming(uint8 resetCounter)
{
    return ((resetCounter >= FBL_RESET_COUNTER_FORCE_MIN) &&
            (resetCounter <= FBL_RESET_COUNTER_FORCE_MAX)) ? 1u : 0u;
}

static void Fbl_ConsumeResetUserInfo(void)
{
    uint16 password = IfxScuWdt_getCpuWatchdogPassword();

    IfxScuWdt_clearCpuEndinit(password);
    MODULE_SCU.RSTCON2.B.USRINFO = 0u;
    IfxScuWdt_setCpuEndinit(password);

    g_FblLastResetUserInfoAfterConsume = MODULE_SCU.RSTCON2.B.USRINFO;
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

volatile uint32 loopCounter = 0u;

void core0_main(void)
{
    IfxScuRcu_ResetCode resetCode;
    uint16 resetInfo;
    uint8 programmingRequest;
    uint8 requestedTransport;
    uint8 resetCounter;
    Fbl_BluResetRuntime();

    resetCode = IfxScuRcu_evaluateReset();
    resetInfo = MODULE_SCU.RSTCON2.B.USRINFO;
    g_FblLastResetReason = resetInfo;
    g_FblLastRawRstStat = MODULE_SCU.RSTSTAT.U;
    g_FblLastResetType = (uint32)resetCode.resetType;
    g_FblLastResetTrigger = (uint32)resetCode.resetTrigger;
    g_FblLastResetSafeState = (uint32)resetCode.cpuSafeState;

    {
        uint8 scrProg = 0u;
        uint8 scrComm = 0u;
        uint8 scrResetCounter = 0u;

        if(Fbl_ScrReadFblHandoff(&scrProg, &scrComm, &scrResetCounter) != 0u)
        {
            programmingRequest = scrProg;
            (void)scrComm;
            requestedTransport = FBL_TRANSPORT_ETH;
            resetCounter = scrResetCounter;
        }
        else
        {
            programmingRequest = 0u;
            requestedTransport = FBL_TRANSPORT_ETH;
            resetCounter = 0u;
        }
    }

    g_FblBluScrReadValid = (uint32)Fbl_ScrReadBluState(&g_blu);

    /* A direct FBL update reaches REBOOT_PENDING only after the complete live
     * image was programmed and CRC-verified. Reaching this code proves the new
     * FBL has started; normalize the durable record instead of clearing it. */
    if((g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) &&
       (g_blu.state == FBL_BLU_STATE_REBOOT_PENDING) &&
       (Fbl_BluHasVerifiedFblEvidence() != 0u))
    {
        g_blu.state = FBL_BLU_STATE_FBL_STARTED;
        g_blu.failure = FBL_BLU_FAILURE_NONE;
        Fbl_ScrWriteBluState(&g_blu);
        g_FblBluLastTransition = 0x00060001u;
    }

    if(Fbl_ResetCounterForcesProgramming(resetCounter) != 0u)
    {
        /* Consume the mailbox so a freshly recovered application can boot: without
         * this the same in-window counter would force the loader again after the
         * recovery flash + reset. The application republishes a higher counter on
         * its next error reset, keeping the [49,52] window alive while it loops. */
        Fbl_ScrInvalidateFblHandoff();
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

    if(resetInfo == FBL_RESET_INFO_ENTER_DOIP)
    {
        Fbl_ConsumeResetUserInfo();
    }
    else
    {
        g_FblLastResetUserInfoAfterConsume = MODULE_SCU.RSTCON2.B.USRINFO;
    }

    if((g_blu.state != FBL_BLU_STATE_IDLE) &&
       (g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER))
    {
        g_FblTransportSelect = FBL_TRANSPORT_ETH;
        g_FblStayInBoot = 0u;
        g_FblDiagBootRequestSeen = 1u;
    }

    if(programmingRequest == FBL_PROGRAMMING_REQUEST_ACTIVE)
    {
        Fbl_ScrInvalidateFblHandoff();
    }

    Fbl_PlatformInit();
    Fbl_FlashInit();
    if(g_FblStayInBoot == 1u)
    {
        if(Fbl_IsAppValid() != 0u)
        {
            if((g_blu.state == FBL_BLU_STATE_IDLE) && (g_blu.failure == FBL_BLU_FAILURE_NONE))
            {
                Fbl_ScrClearBluState();
            }
            Fbl_JumpToApp();
        }

        g_FblStayInBoot = 0u;
        g_FblTransportSelect = FBL_TRANSPORT_ETH;
        Fbl_WriteNcrU8(FBL_NCR_DIAG_SESSION_ADDR, FBL_DIAG_SESSION_PROGRAMMING);
    }

    g_FblTransportSelect = Fbl_NormalizeTransport((uint8)g_FblTransportSelect);

    g_FblTransportSelect = FBL_TRANSPORT_ETH;
    FblEth_SetDoIpCallbacks(Fbl_DoIpTcpConnected,
                            Fbl_DoIpTcpDisconnected,
                            Fbl_DoIpTcpRxOverflow);
    FblEth_Init();

    while(1)
    {
      loopCounter++;

        Fbl_DoIpMain();

    }
}

static void Fbl_PlatformInit(void)
{
    IfxCpu_disableInterrupts();
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());
    gpio_init_pins();
    rmii0_init_pins();
    IfxCpu_enableInterrupts();
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
        if(len != 2u) { Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport); return; }

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
        if(len != 2u) { Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport); return; }

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
            res[3u] = Fbl_BluActiveSoftwareBlock();
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
        if((req[1u] == 0x01u) && (rid == UDS_RID_SELECT_SW_BLOCK))
        {
            uint8 imageKind;

            if(len != 5u)
            {
                Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport);
                return;
            }

            imageKind = req[4u];
            if(Fbl_BluSelectTarget(imageKind) == 0u)
            {
                Fbl_BluSetFailure(FBL_BLU_FAILURE_SEQUENCE);
                Fbl_UdsNegSequence(sid, transport, FBL_BLU_REJECT_SELECT_TARGET_CONFLICT);
                return;
            }

            res[0u] = 0x71u;
            res[1u] = 0x01u;
            res[2u] = 0x02u;
            res[3u] = 0x00u;
            res[4u] = imageKind;
            Fbl_UdsSend(res, 5u, transport);
        }
        else if((req[1u] == 0x01u) && (rid == UDS_RID_START_FBL_RAM_UPDATER))
        {
            if(len != 4u)
            {
                Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport);
                return;
            }

            if(g_blu.imageKind != FBL_BLU_IMAGE_KIND_BOOTLOADER)
            {
                Fbl_BluSetFailure(FBL_BLU_FAILURE_SEQUENCE);
                Fbl_UdsNegSequence(sid, transport, FBL_BLU_REJECT_DOWNLOAD_FBL_TARGET);
                return;
            }

            g_blu.state = FBL_BLU_STATE_ENTER_REQUESTED;
            g_blu.failure = FBL_BLU_FAILURE_NONE;
            Fbl_ScrWriteBluState(&g_blu);

            if(Fbl_EnterDirectUpdateRuntime() == 0u)
            {
                Fbl_BluSetFailure(FBL_BLU_FAILURE_RAM_CLOSURE);
                Fbl_UdsNeg(sid, UDS_NRC_COND_NOT_CORRECT, transport);
                return;
            }

            g_blu.state = FBL_BLU_STATE_RAM_READY;
            g_blu.failure = FBL_BLU_FAILURE_NONE;
            Fbl_ScrWriteBluState(&g_blu);

            res[0u] = 0x71u;
            res[1u] = 0x01u;
            res[2u] = 0x01u;
            res[3u] = 0x55u;
            Fbl_UdsSend(res, 4u, transport);
            return;
        }
        else if((req[1u] == 0x01u) && (rid == UDS_RID_ERASE_APP))
        {
            uint32 eraseLogicalAddr = APP_START_NCACHED;
            uint32 eraseLen = APP_SIZE_BYTES;
            uint32 eraseAddr;
            uint8 eraseOk;
            uint8 previousImageKind;

            if(len == 12u)
            {
                eraseLogicalAddr = Fbl_Rd32(&req[4u]);
                eraseLen = Fbl_Rd32(&req[8u]);
            }
            else if(len != 4u)
            {
                Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport);
                return;
            }

            if(Fbl_IsFullFblLogicalRange(eraseLogicalAddr, eraseLen) != 0u)
            {
                uint32 erasePhysicalAddr;

                g_blu.imageKind = FBL_BLU_IMAGE_KIND_BOOTLOADER;
                if((g_blu.state != FBL_BLU_STATE_RAM_READY) &&
                   (g_blu.state != FBL_BLU_STATE_ERASE_COMPLETE))
                {
                    Fbl_UdsNegSequence(sid, transport, FBL_BLU_REJECT_FBL_ERASE_STATE);
                    return;
                }

                if(Fbl_RuntimeClosureOk() == 0u)
                {
                    Fbl_BluSetFailure(FBL_BLU_FAILURE_RAM_CLOSURE);
                    Fbl_UdsNeg(sid, UDS_NRC_COND_NOT_CORRECT, transport);
                    return;
                }

                erasePhysicalAddr = Fbl_ActiveDownloadPhysicalStart(eraseLogicalAddr);
                Fbl_UdsKeepAlive(sid, transport);
                (void)FblEth_TcpDrain(FBL_TX_DRAIN_POLLS);

                g_blu.state = FBL_BLU_STATE_ERASING;
                g_blu.failure = FBL_BLU_FAILURE_NONE;
                Fbl_ScrWriteBluState(&g_blu);
                FblRamRuntime_SetDestructivePhase(1u);

                if(Fbl_BluEraseFblRange(erasePhysicalAddr, eraseLen) != 0u)
                {
                    Fbl_BluSetFailure(FBL_BLU_FAILURE_FLASH);
                    Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
                    return;
                }

                g_blu.state = FBL_BLU_STATE_ERASE_COMPLETE;
                g_blu.failure = FBL_BLU_FAILURE_NONE;
                g_blu.imageLen = eraseLen;
                g_blu.imageCrc = 0u;
                Fbl_ScrWriteBluState(&g_blu);

                res[0u] = 0x71u;
                res[1u] = 0x01u;
                res[2u] = 0x00u;
                res[3u] = 0x01u;
                res[4u] = 0x00u;
                Fbl_UdsSend(res, 5u, transport);
                return;
            }

            previousImageKind = g_blu.imageKind;
            if((previousImageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) &&
               (Fbl_BluCanSwitchFromBootloaderToApplication() == 0u))
            {
                Fbl_UdsNegSequence(sid, transport, FBL_BLU_REJECT_APP_ERASE_FBL_NOT_STARTED);
                return;
            }

            g_blu.imageKind = FBL_BLU_IMAGE_KIND_APPLICATION;
            if(Fbl_ValidateDownloadRange(eraseLogicalAddr, eraseLen) != FBL_ADDR_OK)
            {
                g_blu.imageKind = previousImageKind;
                Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
                return;
            }

            g_blu.state = FBL_BLU_STATE_PREPARE;
            g_blu.failure = FBL_BLU_FAILURE_NONE;
            g_blu.flags |= FBL_BLU_FLAG_TARGET_LOCKED;
            Fbl_ScrWriteBluState(&g_blu);

            eraseAddr = Fbl_ActiveDownloadPhysicalStart(eraseLogicalAddr);

            /* Tell the tester to wait before the complete blocking erase command. */
            Fbl_UdsKeepAlive(sid, transport);
            g_FblEraseDoIpDrainStartTick = STM0_TIM0.U;
            (void)FblEth_TcpDrain(FBL_TX_DRAIN_POLLS);
            g_FblEraseDoIpDrainEndTick = STM0_TIM0.U;
            g_FblEraseDoIpDrainTicks = g_FblEraseDoIpDrainEndTick - g_FblEraseDoIpDrainStartTick;

            g_FblEraseLogicalCallCount = 0u;
            g_FblEraseCommandCount = 0u;
            g_FblEraseDmuCommandCount = 0u;
            g_FblEraseBank0CommandCount = 0u;
            g_FblEraseBank1CommandCount = 0u;
            g_FblEraseLastError = 0u;
            g_FblEraseLastChunkLength = 0u;
            g_FblEraseLastSectorCount = 0u;
            g_FblEraseLastPhysicalBoundary = 0u;
            g_FblEraseErrorBeforeCommand = 0u;
            g_FblEraseErrorAfterCommand = 0u;
            g_FblEraseFailedCommandIndex = 0u;
            g_FblEraseDmuStatusBeforeClear = 0u;
            g_FblEraseDmuErrorBeforeClear = 0u;
            g_FblEraseDmuStatusAfterCommand = 0u;
            g_FblEraseDmuErrorAfterCommand = 0u;
            g_FblEraseDmuStatusAfterWait = 0u;
            g_FblEraseDmuErrorAfterWait = 0u;
            g_FblEraseLogicalStartTick = STM0_TIM0.U;
            eraseOk = (Fbl_FlashEraseRange(eraseAddr, eraseLen) == 0u) ? 1u : 0u;
            g_FblEraseLogicalEndTick = STM0_TIM0.U;
            g_FblEraseLogicalTicks = g_FblEraseLogicalEndTick - g_FblEraseLogicalStartTick;
            g_FblEraseLogicalLastResult = (uint32)eraseOk;

            if(eraseOk == 0u)
            {
                g_FblBluLastEraseFailAddr = eraseAddr;
                g_FblBluLastEraseFailLen = eraseLen;
                g_FblBluLastEraseFailDmu = g_FblRamRuntimeLastDmuError;
                Fbl_BluSetFailure(FBL_BLU_FAILURE_DOWNLOAD);
                Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
                return;
            }

            g_blu.state = FBL_BLU_STATE_PREPARE;
            Fbl_ScrWriteBluState(&g_blu);
            res[0u] = 0x71u;
            res[1u] = 0x01u;
            res[2u] = 0x00u;
            res[3u] = 0x01u;
            res[4u] = 0x00u;
            Fbl_UdsSend(res, 5u, transport);
        }
        else if((req[1u] == 0x01u) && (rid == UDS_RID_CRC_CHECK))
        {
            if(len != 16u) { Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport); return; }

            addr = Fbl_Rd32(&req[4u]);   /* logical address from tester, cached 0x8 alias */
            size = Fbl_Rd32(&req[8u]);
            crcExpected = Fbl_Rd32(&req[12u]);

            if((g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) ||
               (g_dl.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER))
            {
                if(Fbl_IsFullFblLogicalRange(addr, size) == 0u)
                {
                    Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
                    return;
                }

                if((g_dl.active != 0u) ||
                   ((g_blu.state != FBL_BLU_STATE_TRANSFER_COMPLETE) &&
                    (g_blu.state != FBL_BLU_STATE_FBL_VERIFIED)))
                {
                    Fbl_UdsNegSequence(sid, transport, FBL_BLU_REJECT_FBL_CRC_STATE);
                    return;
                }
            }
            else if(Fbl_ValidateDownloadRange(addr, size) != FBL_ADDR_OK)
            {
                Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
                return;
            }

            (void)Fbl_FlashFlush(sid, transport);
            FblRam_InvalidateProgramCache();
            crcCalc = Fbl_Crc32(Fbl_ActiveDownloadPhysicalStart(addr), size, sid, transport);

            res[0u] = 0x71u;
            res[1u] = 0x01u;
            res[2u] = 0x00u;
            res[3u] = 0x02u;
            Fbl_Wr32(&res[4u], crcCalc);
            res[8u] = (crcCalc == crcExpected) ? 0x00u : 0x01u;

            if((g_dl.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) &&
               (crcCalc == crcExpected) &&
               (g_dl.received == FBL_SIZE_BYTES) &&
               (g_dl.length == FBL_SIZE_BYTES))
            {
                g_blu.state = FBL_BLU_STATE_FBL_VERIFIED;
                g_blu.failure = FBL_BLU_FAILURE_NONE;
                g_blu.imageLen = FBL_SIZE_BYTES;
                g_blu.imageCrc = crcExpected;
                Fbl_ScrWriteBluState(&g_blu);
            }
            else if((g_dl.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) ||
                    (g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER))
            {
                Fbl_BluSetFailure(FBL_BLU_FAILURE_VERIFY);
            }

            Fbl_UdsSend(res, 9u, transport);
        }
        else
        {
            Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
        }
    }
    else if(sid == UDS_SID_REQ_DOWNLOAD)
    {
        /* UDS RequestDownload here is:
         *   0x34 | dataFormatIdentifier | addressAndLengthFormatIdentifier | 4-byte addr | 4-byte size */
        if((len != 11u) || (req[2u] != 0x44u))
        {
            Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport);
            return;
        }

        addr = Fbl_Rd32(&req[3u]);   /* logical address from tester, cached 0x8 alias */
        size = Fbl_Rd32(&req[7u]);

        if(Fbl_IsFullFblLogicalRange(addr, size) != 0u)
        {
            if(g_blu.imageKind != FBL_BLU_IMAGE_KIND_BOOTLOADER)
            {
                Fbl_BluSetFailure(FBL_BLU_FAILURE_SEQUENCE);
                Fbl_UdsNegSequence(sid, transport, FBL_BLU_REJECT_DOWNLOAD_FBL_TARGET);
                return;
            }
        }
        else if((g_blu.imageKind == FBL_BLU_IMAGE_KIND_NONE) &&
                (Fbl_BluSelectTarget(FBL_BLU_IMAGE_KIND_APPLICATION) == 0u))
        {
            Fbl_BluSetFailure(FBL_BLU_FAILURE_SEQUENCE);
            Fbl_UdsNegSequence(sid, transport, FBL_BLU_REJECT_DOWNLOAD_NO_TARGET);
            return;
        }
        else if(g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER)
        {
            if(Fbl_BluCanSwitchFromBootloaderToApplication() == 0u)
            {
                Fbl_UdsNegSequence(sid, transport, FBL_BLU_REJECT_DOWNLOAD_APP_SWITCH);
                return;
            }

            g_blu.imageKind = FBL_BLU_IMAGE_KIND_APPLICATION;
            g_blu.state = FBL_BLU_STATE_PREPARE;
            g_blu.failure = FBL_BLU_FAILURE_NONE;
            g_blu.flags |= FBL_BLU_FLAG_TARGET_LOCKED;
            Fbl_ScrWriteBluState(&g_blu);
        }

        if(Fbl_ValidateDownloadRange(addr, size) != FBL_ADDR_OK)
        {
            Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
            return;
        }

        if(g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER)
        {
            if((FblRamRuntime_IsActive() == 0u) ||
               (g_blu.state != FBL_BLU_STATE_ERASE_COMPLETE))
            {
                Fbl_BluSetFailure(FBL_BLU_FAILURE_SEQUENCE);
                Fbl_UdsNegSequence(sid, transport, FBL_BLU_REJECT_DOWNLOAD_FBL_STATE);
                return;
            }
        }

        g_dl.active = 1u;
        g_dl.imageKind = g_blu.imageKind;
        g_dl.startAddr = Fbl_ActiveDownloadPhysicalStart(addr);
        g_dl.curAddr = g_dl.startAddr;
        g_dl.length = size;
        g_dl.received = 0u;
        g_dl.nextBlock = 1u;
        g_dl.lastBlock = 0u;
        g_blu.state = FBL_BLU_STATE_DOWNLOAD_ACTIVE;
        g_blu.failure = FBL_BLU_FAILURE_NONE;
        g_blu.imageLen = size;
        g_blu.imageCrc = 0u;
        Fbl_ScrWriteBluState(&g_blu);
        g_pageAddr = FBL_FLASH_NO_PAGE_ADDR;
        g_pageFill = 0u;
        FblRam_SetBytes(g_pageBuf.bytes, FBL_FLASH_PAD_BYTE, sizeof(g_pageBuf.bytes));
        FblRam_SetBytes(g_fblBootCriticalPage.bytes, FBL_FLASH_PAD_BYTE, sizeof(g_fblBootCriticalPage.bytes));
        g_fblBootCriticalMask = 0u;
        g_FblTransferAdvertisedBlockLength = FBL_UDS_MAX_BLOCK_LENGTH;
        g_FblTransferLastUdsLength = 0u;
        g_FblTransferLastDataLength = 0u;
        g_FblTransferLastDoipPayloadLength = 0u;
        g_FblTransferLastFrameLength = 0u;
        g_FblTransferTcpStreamHighWatermark = 0u;
        g_FblTransferTcpCallbackCount = 0u;
        g_FblTransferOverflowCount = 0u;
        g_FblTransferProgrammedPageCount = 0u;

        res[0u] = 0x74u;
        res[1u] = 0x20u;
        res[2u] = (uint8)(FBL_UDS_MAX_BLOCK_LENGTH >> 8u);
        res[3u] = (uint8)FBL_UDS_MAX_BLOCK_LENGTH;
        Fbl_UdsSend(res, 4u, transport);
    }
    else if(sid == UDS_SID_TRANSFER_DATA)
    {
        uint32 dataLen;

        if((req == NULL_PTR) || (len < 2u))
        {
            Fbl_UdsNeg(UDS_SID_TRANSFER_DATA, UDS_NRC_INCORRECT_LEN, transport);
            return;
        }

        g_FblTransferLastUdsLength = len;

        if(len > FBL_UDS_MAX_BLOCK_LENGTH)
        {
            Fbl_UdsNeg(UDS_SID_TRANSFER_DATA, UDS_NRC_INCORRECT_LEN, transport);
            return;
        }

        if(g_dl.active == 0u)
        {
            Fbl_UdsNeg(sid, UDS_NRC_COND_NOT_CORRECT, transport);
            return;
        }

        if(req[1u] != g_dl.nextBlock)
        {
            if((req[1u] == g_dl.lastBlock) && (g_dl.received != 0u))
            {
                res[0u] = 0x76u;
                res[1u] = req[1u];
                Fbl_UdsSend(res, 2u, transport);
                return;
            }

            Fbl_UdsNeg(sid, UDS_NRC_WRONG_BLOCK_SEQUENCE, transport);
            return;
        }

        dataLen = (uint32)len - FBL_UDS_TRANSFER_OVERHEAD;
        g_FblTransferLastDataLength = dataLen;

        if((dataLen == 0u) || (dataLen > FBL_TRANSFER_DATA_BYTES))
        {
            Fbl_UdsNeg(UDS_SID_TRANSFER_DATA, UDS_NRC_INCORRECT_LEN, transport);
            return;
        }

        if(dataLen > (g_dl.length - g_dl.received))
        {
            Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
            return;
        }

        g_blu.state = FBL_BLU_STATE_PROGRAMMING;
        Fbl_ScrWriteBluState(&g_blu);
        g_FblTransferProgrammedPageCount = 0u;
        Fbl_ServiceCommsDuringLongOp();

        if(((g_dl.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) ?
                Fbl_FlashProgramFblPayload(g_dl.curAddr, &req[2u], dataLen, sid, transport) :
                Fbl_FlashProgram(g_dl.curAddr, &req[2u], dataLen, sid, transport)) != 0u)
        {
            Fbl_BluSetFailure(FBL_BLU_FAILURE_DOWNLOAD);
            Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
            return;
        }

        g_dl.curAddr += dataLen;
        g_dl.received += dataLen;
        g_dl.lastBlock = g_dl.nextBlock;
        g_dl.nextBlock++;
        g_blu.state = FBL_BLU_STATE_DOWNLOAD_ACTIVE;
        Fbl_ScrWriteBluState(&g_blu);

        res[0u] = 0x76u;
        res[1u] = req[1u];
        (void)Fbl_SendAndDrainPositive(res, 2u, transport);
    }
    else if(sid == UDS_SID_TRANSFER_EXIT)
    {
        if(g_dl.active == 0u)
        {
            Fbl_UdsNeg(sid, UDS_NRC_COND_NOT_CORRECT, transport);
            return;
        }

        if((g_dl.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) && (len != 5u))
        {
            Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport);
            return;
        }

        if((g_dl.imageKind != FBL_BLU_IMAGE_KIND_BOOTLOADER) &&
           ((len != 1u) && (len != 5u)))
        {
            Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport);
            return;
        }

        if(Fbl_FlashFlush(sid, transport) != 0u)
        {
            Fbl_BluSetFailure(FBL_BLU_FAILURE_DOWNLOAD);
            Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
            return;
        }

        /* Programming of this block is complete: drop any stale instructions for
         * the freshly written PFLASH before it can be executed. */
        FblRam_InvalidateProgramCache();

        if(g_dl.received != g_dl.length)
        {
            Fbl_BluSetFailure(FBL_BLU_FAILURE_DOWNLOAD);
            Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
            return;
        }

        g_blu.state = FBL_BLU_STATE_TRANSFER_COMPLETE;
        Fbl_ScrWriteBluState(&g_blu);

        if(g_dl.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER)
        {
            if(Fbl_BootCriticalPageComplete() == 0u)
            {
                Fbl_BluSetFailure(FBL_BLU_FAILURE_DOWNLOAD);
                Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
                return;
            }

            if(len == 5u)
            {
                crcExpected = Fbl_Rd32(&req[1u]);
                crcCalc = Fbl_Crc32FblWithDelayedBootPage(g_dl.startAddr, g_dl.length, sid, transport);
                if(crcCalc != crcExpected)
                {
                    g_blu.imageCrc = crcExpected;
                    Fbl_ScrWriteBluState(&g_blu);
                    Fbl_BluSetFailure(FBL_BLU_FAILURE_VERIFY);
                    Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
                    return;
                }
            }

            g_blu.state = FBL_BLU_STATE_COMMITTING;
            Fbl_ScrWriteBluState(&g_blu);
            if(Fbl_CommitBootCriticalPage(sid, transport) != 0u)
            {
                Fbl_BluSetFailure(FBL_BLU_FAILURE_VERIFY);
                Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
                return;
            }

            FblRam_InvalidateProgramCache();
        }

        g_blu.state = FBL_BLU_STATE_FINAL_VERIFY;
        Fbl_ScrWriteBluState(&g_blu);

        if(len == 5u)
        {
            crcExpected = Fbl_Rd32(&req[1u]);
            crcCalc = Fbl_Crc32(g_dl.startAddr, g_dl.length, sid, transport);
            g_blu.imageCrc = crcExpected;
            Fbl_ScrWriteBluState(&g_blu);

            if(crcCalc != crcExpected)
            {
                Fbl_BluSetFailure(FBL_BLU_FAILURE_VERIFY);
                Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
                return;
            }

        }
        else if(g_dl.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER)
        {
            Fbl_BluSetFailure(FBL_BLU_FAILURE_VERIFY);
            Fbl_UdsNeg(sid, UDS_NRC_TRANSFER_FAIL, transport);
            return;
        }

        g_dl.active = 0u;
        g_blu.failure = FBL_BLU_FAILURE_NONE;
        if(g_dl.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER)
        {
            g_blu.state = FBL_BLU_STATE_FBL_VERIFIED;
            g_blu.imageLen = FBL_SIZE_BYTES;
        }
        else
        {
            g_blu.state = FBL_BLU_STATE_COMPLETE;
        }
        Fbl_ScrWriteBluState(&g_blu);
        FblRamRuntime_SetDestructivePhase(0u);
        res[0u] = 0x77u;
        if(g_dl.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER)
        {
            if(Fbl_SendAndDrainPositive(res, 1u, transport) == 0u)
            {
                Fbl_BluSetFailure(FBL_BLU_FAILURE_RESET);
            }
        }
        else
        {
            Fbl_UdsSend(res, 1u, transport);
        }
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
        if(len != 2u) { Fbl_UdsNeg(sid, UDS_NRC_INCORRECT_LEN, transport); return; }

        if((g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) &&
           ((g_blu.state == FBL_BLU_STATE_ERASING) ||
            (g_blu.state == FBL_BLU_STATE_ERASE_COMPLETE) ||
            (g_blu.state == FBL_BLU_STATE_DOWNLOAD_ACTIVE) ||
            (g_blu.state == FBL_BLU_STATE_PROGRAMMING) ||
            (g_blu.state == FBL_BLU_STATE_TRANSFER_COMPLETE) ||
            (g_blu.state == FBL_BLU_STATE_FINAL_VERIFY) ||
            (g_blu.state == FBL_BLU_STATE_COMMITTING) ||
            (g_blu.state == FBL_BLU_STATE_FAILED) ||
            (g_blu.state == FBL_BLU_STATE_RECOVERY_WAIT)))
        {
            Fbl_UdsNegSequence(sid, transport, FBL_BLU_REJECT_RESET_FBL_ACTIVE);
            return;
        }

        res[0u] = 0x51u;
        res[1u] = req[1u];

        if((g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) &&
           (g_blu.state == FBL_BLU_STATE_FBL_VERIFIED) &&
           (Fbl_BluHasVerifiedFblEvidence() != 0u))
        {
            g_blu.state = FBL_BLU_STATE_REBOOT_PENDING;
            g_blu.failure = FBL_BLU_FAILURE_NONE;
            Fbl_ScrWriteBluState(&g_blu);
        }

        g_FblBluLastResetStage = 1u;
        (void)Fbl_SendAndDrainFinalPositive(res, 2u, transport);
        g_FblBluLastResetStage = 2u;
        FblRamRuntime_RequestReset();
        g_FblBluLastResetStage = 3u;

        Fbl_BluSetFailure(FBL_BLU_FAILURE_RESET);
    }
    else
    {
        Fbl_UdsNeg(sid, UDS_NRC_OUT_OF_RANGE, transport);
    }
}

static void Fbl_UdsSend(const uint8 *res, uint16 len, uint8 transport)
{
    (void)transport;
    Fbl_DoIpSendDiag(res, len);
}

static void Fbl_UdsNeg(uint8 sid, uint8 nrc, uint8 transport)
{
    uint8 res[3];

    res[0u] = UDS_NEG_RESP;
    res[1u] = sid;
    res[2u] = nrc;

    Fbl_UdsSend(res, 3u, transport);
}

static void Fbl_UdsNegSequence(uint8 sid, uint8 transport, uint32 reason)
{
    Fbl_BluCaptureReject(reason);
    Fbl_UdsNeg(sid, UDS_NRC_SEQUENCE_ERROR, transport);
}

static void Fbl_ServiceCommsDuringLongOp(void)
{
    (void)FblEth_TcpDrain(FBL_TX_DRAIN_POLLS);
    FblEth_PollReceiveOnly();
    FblEth_PollTimerOnly();
}

static uint8 Fbl_SendAndDrainFinalPositive(const uint8 *res, uint16 len, uint8 transport)
{
    uint8 drainResult;

    drainResult = Fbl_SendAndDrainPositive(res, len, transport);
    g_FblBluLastTcpDrainResult = drainResult;
    return drainResult;
}

static uint8 Fbl_SendAndDrainPositive(const uint8 *res, uint16 len, uint8 transport)
{
    Fbl_UdsSend(res, len, transport);
    return FblEth_TcpDrain(FBL_FINAL_TX_DRAIN_POLLS);
}

/* Emit a responsePending (7F sid 78) during a long-running service. */
static void Fbl_UdsKeepAlive(uint8 sid, uint8 transport)
{
    Fbl_UdsNeg(sid, UDS_NRC_RESPONSE_PENDING, transport);
}

static void Fbl_UdsSecurityAccess(const uint8 *req, uint16 len, uint8 transport)
{
    uint8 res[6];
    uint8 sub;

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

    if((sub & 0x01u) != 0u)
    {
        if(len != 2u)
        {
            Fbl_UdsNeg(UDS_SID_SECURITY_ACCESS, UDS_NRC_INCORRECT_LEN, transport);
            return;
        }

        res[0u] = 0x67u;
        res[1u] = sub;
        Fbl_Wr32(&res[2u], 0u);
        Fbl_UdsSend(res, 6u, transport);
        return;
    }

    if(len != 6u)
    {
        Fbl_UdsNeg(UDS_SID_SECURITY_ACCESS, UDS_NRC_INCORRECT_LEN, transport);
        return;
    }

    res[0u] = 0x67u;
    res[1u] = sub;
    Fbl_UdsSend(res, 2u, transport);
}

static void Fbl_DoIpResetTcpState(void)
{
    g_doipTcpOverflowPending = 0u;
    g_doipTesterAddr = 0u;
    g_doipRoutingActive = 0u;

    g_dl.active = 0u;
    g_dl.imageKind = FBL_BLU_IMAGE_KIND_NONE;
    g_dl.startAddr = 0u;
    g_dl.curAddr = 0u;
    g_dl.length = 0u;
    g_dl.received = 0u;
    g_dl.nextBlock = 1u;
    g_dl.lastBlock = 0u;

    g_pageAddr = FBL_FLASH_NO_PAGE_ADDR;
    g_pageFill = 0u;
    FblRam_SetBytes(g_pageBuf.bytes, FBL_FLASH_PAD_BYTE, sizeof(g_pageBuf.bytes));
    FblRam_SetBytes(g_fblBootCriticalPage.bytes, FBL_FLASH_PAD_BYTE, sizeof(g_fblBootCriticalPage.bytes));
    g_fblBootCriticalMask = 0u;
}

void Fbl_DoIpTcpConnected(void)
{
    if((g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) &&
       (g_blu.state == FBL_BLU_STATE_RECOVERY_WAIT))
    {
        g_doipTcpOverflowPending = 0u;
        return;
    }

    Fbl_DoIpResetTcpState();
}

void Fbl_DoIpTcpDisconnected(void)
{
    if((g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER) &&
       ((g_blu.state == FBL_BLU_STATE_ERASING) ||
        (g_blu.state == FBL_BLU_STATE_ERASE_COMPLETE) ||
        (g_blu.state == FBL_BLU_STATE_DOWNLOAD_ACTIVE) ||
        (g_blu.state == FBL_BLU_STATE_PROGRAMMING) ||
        (g_blu.state == FBL_BLU_STATE_TRANSFER_COMPLETE) ||
        (g_blu.state == FBL_BLU_STATE_FINAL_VERIFY) ||
        (g_blu.state == FBL_BLU_STATE_COMMITTING)))
    {
        g_dl.active = 0u;
        g_pageAddr = FBL_FLASH_NO_PAGE_ADDR;
        g_pageFill = 0u;
        FblRam_SetBytes(g_pageBuf.bytes, FBL_FLASH_PAD_BYTE, sizeof(g_pageBuf.bytes));
        FblRam_SetBytes(g_fblBootCriticalPage.bytes, FBL_FLASH_PAD_BYTE, sizeof(g_fblBootCriticalPage.bytes));
        g_fblBootCriticalMask = 0u;
        g_blu.state = FBL_BLU_STATE_RECOVERY_WAIT;
        Fbl_ScrWriteBluState(&g_blu);
        return;
    }

    Fbl_DoIpResetTcpState();
}

void Fbl_DoIpTcpRxOverflow(void)
{
    g_FblTransferOverflowCount++;
    g_doipTcpOverflowPending = 1u;
}

static void Fbl_DoIpMain(void)
{
    FblEth_PollReceiveOnly();

    if((g_doipRoutingActive == 0u) && (g_dl.active == 0u))
    {
        g_FblDoIpLastUdpReceiveBufPtr = (uint32)g_doipRxScratch;
        g_FblDoIpLastUdpReceiveLenPtr = (uint32)&g_doipRxLenScratch;
        while(FblEth_UdpReceive(g_doipRxScratch, &g_doipRxLenScratch) != 0u)
        {
            Fbl_DoIpHandleUdp(g_doipRxScratch, g_doipRxLenScratch);
        }
    }

    g_FblDoIpLastTcpReceiveBufPtr = (uint32)g_doipRxScratch;
    g_FblDoIpLastTcpReceiveLenPtr = (uint32)&g_doipRxLenScratch;
    while((FblEth_HasPendingTcpData() != 0u) && (FblEth_TcpReceive(g_doipRxScratch, &g_doipRxLenScratch) != 0u))
    {
        Fbl_DoIpHandleTcpFrame(g_doipRxScratch, g_doipRxLenScratch);
    }

    if(g_doipTcpOverflowPending != 0u)
    {
        g_doipTcpOverflowPending = 0u;
        Fbl_DoIpSendGenericNack(DOIP_SEND_TCP, DOIP_GEN_NACK_MESSAGE_TOO_LARGE);
    }

    FblEth_PollTimerOnly();
}

static void Fbl_DoIpHandleUdp(const uint8 *buf, uint16 len)
{
    uint16 payloadType;
    uint32 payloadLen;

    if(len < DOIP_HEADER_LEN)
    {
        Fbl_DoIpSendGenericNack(DOIP_SEND_UDP, DOIP_GEN_NACK_INCORRECT_PATTERN);
        return;
    }
    if((buf[0u] != DOIP_PROTO_VER) || (buf[1u] != DOIP_INV_PROTO_VER))
    {
        Fbl_DoIpSendGenericNack(DOIP_SEND_UDP, DOIP_GEN_NACK_INCORRECT_PATTERN);
        return;
    }

    payloadType = Fbl_Rd16(&buf[2u]);
    payloadLen = Fbl_Rd32(&buf[4u]);

    if(payloadLen != ((uint32)len - DOIP_HEADER_LEN))
    {
        Fbl_DoIpSendGenericNack(DOIP_SEND_UDP, DOIP_GEN_NACK_INVALID_LENGTH);
        return;
    }

    if(payloadType == DOIP_PT_VID_REQ)
    {
        if(payloadLen == 0u)
        {
            Fbl_DoIpSendVehicleId();
        }
    }
    else
    {
        Fbl_DoIpSendGenericNack(DOIP_SEND_UDP, DOIP_GEN_NACK_UNKNOWN_PAYLOAD);
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

    if(payloadLen != ((uint32)len - DOIP_HEADER_LEN))
    {
        Fbl_DoIpSendGenericNack(DOIP_SEND_TCP, DOIP_GEN_NACK_INVALID_LENGTH);
        return;
    }

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

        g_FblTransferLastDoipPayloadLength = payloadLen;
        g_FblTransferLastFrameLength = payloadLen + DOIP_HEADER_LEN;

        if((payloadLen < DOIP_DIAG_ADDRESS_BYTES) ||
           ((payloadLen - DOIP_DIAG_ADDRESS_BYTES) > FBL_UDS_MAX_BLOCK_LENGTH))
        {
            Fbl_DoIpSendDiagNack(testerAddr, ecuAddr, DOIP_NACK_DIAG_MSG_TOO_LARGE);
            return;
        }

        Fbl_DoIpSendDiagAck(testerAddr, ecuAddr);
        (void)FblEth_TcpDrain(FBL_TX_DRAIN_POLLS);

        udsLen = (uint16)(payloadLen - DOIP_DIAG_ADDRESS_BYTES);
        Fbl_UdsHandle(&buf[12u], udsLen, FBL_TRANSPORT_ETH);
    }
    else
    {
        Fbl_DoIpSendGenericNack(DOIP_SEND_TCP, DOIP_GEN_NACK_UNKNOWN_PAYLOAD);
    }
}

static void Fbl_DoIpSendGenericNack(uint8 viaTcp, uint8 code)
{
    uint8 res[DOIP_HEADER_LEN + 1u];
    uint16 idx;

    res[0u] = DOIP_PROTO_VER;
    res[1u] = DOIP_INV_PROTO_VER;
    Fbl_Wr16(&res[2u], DOIP_PT_GENERIC_NACK);
    Fbl_Wr32(&res[4u], 1u);

    idx = DOIP_HEADER_LEN;
    res[idx++] = code;

    if(viaTcp != 0u)
    {
        FblEth_TcpSend(res, idx);
    }
    else
    {
        FblEth_UdpSend(res, idx);
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

    /* Encode the fixed 17-byte VIN directly into the response. This avoids
     * separate .ram_rodata input sections competing for the same fixed DLMU
     * address. Preserve the previous payload exactly: 16 ASCII bytes followed
     * by one zero byte. */
    Fbl_Wr32(&res[idx], 0x4C414254u); /* LABT */
    idx = (uint16)(idx + 4u);
    Fbl_Wr32(&res[idx], 0x43333735u); /* C375 */
    idx = (uint16)(idx + 4u);
    Fbl_Wr32(&res[idx], 0x444F4950u); /* DOIP */
    idx = (uint16)(idx + 4u);
    Fbl_Wr32(&res[idx], 0x30303031u); /* 0001 */
    idx = (uint16)(idx + 4u);
    res[idx++] = 0x00u;

    Fbl_Wr16(&res[idx], DOIP_ECU_ADDR);
    idx = (uint16)(idx + 2u);

    /* EID = 02:00:00:00:00:01. */
    FblRam_SetBytes(&res[idx], 0u, 6u);
    res[idx + 0u] = 0x02u;
    res[idx + 5u] = 0x01u;
    idx = (uint16)(idx + 6u);

    /* GID = 03:00:00:00:00:01. */
    FblRam_SetBytes(&res[idx], 0u, 6u);
    res[idx + 0u] = 0x03u;
    res[idx + 5u] = 0x01u;
    idx = (uint16)(idx + 6u);

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
    FblRam_CopyBytes(&buf[12u], uds, udsLen);

    FblEth_TcpSend(buf, (uint16)(udsLen + 12u));
}

static void Fbl_FlashInit(void)
{
    FblRamFlash_ClearStatus();
    FblRam_SetBytes(g_pageBuf.bytes, FBL_FLASH_PAD_BYTE, sizeof(g_pageBuf.bytes));
    g_pageAddr = FBL_FLASH_NO_PAGE_ADDR;
    g_pageFill = 0u;
}

FBL_RAM_CODE static uint32 Fbl_FlashEraseRange(uint32 addr, uint32 len)
{
    uint32 current = addr;
    uint32 remaining = len;

    g_FblEraseLogicalCallCount++;
    g_FblEraseLogicalLastStart = addr;
    g_FblEraseLogicalLastLength = len;
    g_FblEraseLogicalStartTick = STM0_TIM0.U;

    while(remaining != 0u)
    {
        uint32 erasedLength;

        if(FblRamFlash_EraseNextChunk(current, remaining, &erasedLength) != 0u)
        {
            g_FblEraseLogicalEndTick = STM0_TIM0.U;
            g_FblEraseLogicalTicks = g_FblEraseLogicalEndTick - g_FblEraseLogicalStartTick;
            g_FblEraseLogicalLastResult = 0u;
            return 1u;
        }

        if((erasedLength == 0u) || (erasedLength > remaining))
        {
            g_FblEraseLogicalEndTick = STM0_TIM0.U;
            g_FblEraseLogicalTicks = g_FblEraseLogicalEndTick - g_FblEraseLogicalStartTick;
            g_FblEraseLogicalLastResult = 0u;
            return 1u;
        }

        current += erasedLength;
        remaining -= erasedLength;

        if(remaining != 0u)
        {
            Fbl_UdsKeepAlive(UDS_SID_ROUTINE, FBL_TRANSPORT_ETH);
            Fbl_ServiceCommsDuringLongOp();
        }
    }

    FblRam_InvalidateProgramCache();
    g_FblEraseLogicalEndTick = STM0_TIM0.U;
    g_FblEraseLogicalTicks = g_FblEraseLogicalEndTick - g_FblEraseLogicalStartTick;
    g_FblEraseLogicalLastResult = 1u;
    return 0u;
}

FBL_RAM_CODE static uint32 Fbl_FlashProgram(uint32 addr, const uint8 *data, uint32 len, uint8 sid, uint8 transport)
{
    uint32 i;
    uint32 page;
    uint32 off;

    for(i = 0u; i < len; i++)
    {
        page = (addr + i) & ~(PFLASH_PAGE_SIZE - 1u);
        off = (addr + i) - page;

        if(g_pageAddr == FBL_FLASH_NO_PAGE_ADDR)
        {
            g_pageAddr = page;
            FblRam_SetBytes(g_pageBuf.bytes, FBL_FLASH_PAD_BYTE, sizeof(g_pageBuf.bytes));
            g_pageFill = 0u;
        }
        else if(g_pageAddr != page)
        {
            if(Fbl_FlashFlush(sid, transport) != 0u) { return 1u; }

            g_pageAddr = page;
            FblRam_SetBytes(g_pageBuf.bytes, FBL_FLASH_PAD_BYTE, sizeof(g_pageBuf.bytes));
            g_pageFill = 0u;
        }

        g_pageBuf.bytes[off] = data[i];
        if((off + 1u) > g_pageFill) { g_pageFill = off + 1u; }

        if(g_pageFill >= PFLASH_PAGE_SIZE)
        {
            if(Fbl_FlashFlush(sid, transport) != 0u) { return 1u; }
        }
    }

    return 0u;
}

FBL_RAM_CODE static uint32 Fbl_FlashProgramFblPayload(uint32 addr, const uint8 *data, uint32 len, uint8 sid, uint8 transport)
{
    uint32 bootStart = FBL_START_NCACHED;
    uint32 bootEnd = FBL_START_NCACHED + FBL_BOOT_CRITICAL_SIZE;
    uint32 end = addr + len;
    uint32 pos = addr;
    uint32 dataOffset = 0u;

    if((data == NULL_PTR) || (len == 0u) || (end < addr))
    {
        return 1u;
    }

    while(pos < end)
    {
        if((pos >= bootStart) && (pos < bootEnd))
        {
            uint32 bootOffset = pos - bootStart;
            g_fblBootCriticalPage.bytes[bootOffset] = data[dataOffset];
            g_fblBootCriticalMask |= (1u << bootOffset);
            pos++;
            dataOffset++;
        }
        else
        {
            uint32 chunk = end - pos;
            if((pos < bootStart) && ((pos + chunk) > bootStart))
            {
                chunk = bootStart - pos;
            }
            else if((pos < bootEnd) && ((pos + chunk) > bootEnd))
            {
                chunk = bootEnd - pos;
            }

            if(Fbl_FlashProgram(pos, &data[dataOffset], chunk, sid, transport) != 0u)
            {
                return 1u;
            }

            pos += chunk;
            dataOffset += chunk;
        }
    }

    return 0u;
}

FBL_RAM_CODE static uint32 Fbl_BluEraseFblRange(uint32 addr, uint32 len)
{
    uint32 eraseStart;
    uint32 eraseEnd;

    if((len == 0u) ||
       (addr < FBL_START_NCACHED) ||
       (addr > FBL_END_NCACHED) ||
       ((addr + len - 1u) < addr) ||
       ((addr + len - 1u) > FBL_END_NCACHED))
    {
        return 1u;
    }

    eraseStart = addr & ~(PFLASH_SECTOR_SIZE - 1u);
    eraseEnd = (addr + len + (PFLASH_SECTOR_SIZE - 1u)) & ~(PFLASH_SECTOR_SIZE - 1u);
    if((eraseEnd < (addr + len)) || (eraseEnd > (FBL_END_NCACHED + 1u)))
    {
        eraseEnd = FBL_END_NCACHED + 1u;
    }

    if((eraseStart < FBL_START_NCACHED) ||
       ((eraseStart & (PFLASH_SECTOR_SIZE - 1u)) != 0u) ||
       (eraseStart >= eraseEnd))
    {
        return 1u;
    }

    g_blu.state = FBL_BLU_STATE_ERASE;
    Fbl_ScrWriteBluState(&g_blu);

    while(eraseStart < eraseEnd)
    {
        Fbl_UdsKeepAlive(UDS_SID_ROUTINE, FBL_TRANSPORT_ETH);
        if(Fbl_FlashEraseRange(eraseStart, PFLASH_SECTOR_SIZE) != 0u)
        {
            return 1u;
        }

        eraseStart += PFLASH_SECTOR_SIZE;
        Fbl_ServiceCommsDuringLongOp();
    }

    return 0u;
}

FBL_RAM_CODE static uint8 Fbl_BootCriticalPageComplete(void)
{
    return (g_fblBootCriticalMask == 0xFFFFFFFFu) ? 1u : 0u;
}

FBL_RAM_CODE static uint32 Fbl_CommitBootCriticalPage(uint8 sid, uint8 transport)
{
    if(Fbl_BootCriticalPageComplete() == 0u)
    {
        return 1u;
    }

    if(Fbl_FlashFlush(sid, transport) != 0u)
    {
        return 1u;
    }

    return Fbl_FlashProgramPage(FBL_START_NCACHED, g_fblBootCriticalPage.bytes);
}

FBL_RAM_CODE static uint32 Fbl_FlashFlush(uint8 sid, uint8 transport)
{
    uint32 result = 0u;

    if(g_pageAddr != FBL_FLASH_NO_PAGE_ADDR)
    {
        result = Fbl_FlashProgramPage(g_pageAddr, g_pageBuf.bytes);
        if(result == 0u)
        {
            g_FblTransferProgrammedPageCount++;
            if((g_FblTransferProgrammedPageCount & 0x0Fu) == 0u)
            {
                Fbl_ServiceCommsDuringLongOp();
            }
        }
        g_pageAddr = FBL_FLASH_NO_PAGE_ADDR;
        g_pageFill = 0u;
        FblRam_SetBytes(g_pageBuf.bytes, FBL_FLASH_PAD_BYTE, sizeof(g_pageBuf.bytes));
    }

    return result;
}

FBL_RAM_CODE static uint32 Fbl_FlashProgramPage(uint32 addr, const uint8 *data)
{
    if(FblRamFlash_ProgramPage(addr, data) != 0u)
    {
        return 1u;
    }

    return (Fbl_FlashVerifyPage(addr, data) != 0u) ? 0u : 1u;
}

FBL_RAM_CODE static uint8 Fbl_FlashVerifyPage(uint32 addr, const uint8 *data)
{
    volatile const uint8 *p = (volatile const uint8 *)addr;
    uint32 i;

    for(i = 0u; i < PFLASH_PAGE_SIZE; i++)
    {
        if(p[i] != data[i]) { return 0u; }
    }

    return 1u;
}

FBL_RAM_CODE static uint32 Fbl_Crc32UpdateByte(uint32 crc, uint8 value)
{
    uint8 b;

    crc ^= value;
    for(b = 0u; b < 8u; b++)
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

    return crc;
}

FBL_RAM_CODE static uint32 Fbl_Crc32(uint32 addr, uint32 len, uint8 sid, uint8 transport)
{
    volatile const uint8 *p = (volatile const uint8 *)addr;
    uint32 crc = 0xFFFFFFFFu;
    uint32 i;

    for(i = 0u; i < len; i++)
    {
        if((i != 0u) && ((i & 0xFFFFu) == 0u))
        {
            Fbl_UdsKeepAlive(sid, transport);
            Fbl_ServiceCommsDuringLongOp();
        }

        crc = Fbl_Crc32UpdateByte(crc, p[i]);
    }

    return crc ^ 0xFFFFFFFFu;
}

FBL_RAM_CODE static uint32 Fbl_Crc32FblWithDelayedBootPage(uint32 addr, uint32 len, uint8 sid, uint8 transport)
{
    volatile const uint8 *p = (volatile const uint8 *)addr;
    uint32 crc = 0xFFFFFFFFu;
    uint32 i;

    for(i = 0u; i < len; i++)
    {
        uint32 absolute = addr + i;
        uint8 value;

        if((i != 0u) && ((i & 0xFFFFu) == 0u))
        {
            Fbl_UdsKeepAlive(sid, transport);
            Fbl_ServiceCommsDuringLongOp();
        }

        if((absolute >= FBL_START_NCACHED) &&
           (absolute < (FBL_START_NCACHED + FBL_BOOT_CRITICAL_SIZE)))
        {
            value = g_fblBootCriticalPage.bytes[absolute - FBL_START_NCACHED];
        }
        else
        {
            value = p[i];
        }

        crc = Fbl_Crc32UpdateByte(crc, value);
    }

    return crc ^ 0xFFFFFFFFu;
}

static uint8 Fbl_IsExplicitlyRejectedProgrammingAddress(uint32 addr)
{
    uint32 prefix = addr & DFLASH_REJECT_PREFIX_MASK;

    if((prefix == DFLASH_REJECT_PREFIX_AF40) ||
       (prefix == DFLASH_REJECT_PREFIX_AF01) ||
       (prefix == DFLASH_REJECT_PREFIX_AF02))
    {
        return 1u;
    }

    return 0u;
}

static uint8 Fbl_IsFullFblLogicalRange(uint32 logicalAddr, uint32 length)
{
    uint32 seg;
    uint32 offset;

    if(length != FBL_SIZE_BYTES)
    {
        return 0u;
    }

    if((logicalAddr + length) < logicalAddr)
    {
        return 0u;
    }

    seg = logicalAddr & FLASH_ADDRESS_PREFIX_MASK;
    if((seg != (PFLASH_CACHED_BASE & FLASH_ADDRESS_PREFIX_MASK)) &&
       (seg != (PFLASH_NONCACHED_BASE & FLASH_ADDRESS_PREFIX_MASK)))
    {
        return 0u;
    }

    offset = logicalAddr & PFLASH_ALIAS_MASK;
    if(offset != (FBL_START_NCACHED & PFLASH_ALIAS_MASK))
    {
        return 0u;
    }

    return 1u;
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

    if((Fbl_IsExplicitlyRejectedProgrammingAddress(logicalAddr) != 0u) ||
       (Fbl_IsExplicitlyRejectedProgrammingAddress(logicalAddr + length - 1u) != 0u))
    {
        return FBL_ADDR_ERR_PROTECTED;
    }

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

    if(g_blu.imageKind == FBL_BLU_IMAGE_KIND_BOOTLOADER)
    {
        if((offset != fblStartOff) ||
           (endOffset != fblEndOff) ||
           (length != FBL_SIZE_BYTES))
        {
            return FBL_ADDR_ERR_NOT_APP;
        }
    }
    else
    {
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

static uint8 Fbl_RuntimeClosureOk(void)
{
    g_FblRuntimeClosureFailStep = 0u;

#define FBL_CLOSURE_CHECK(step, symbol) \
    do { if(FblRamRuntime_IsExecutableAddress((uint32)(symbol)) == 0u) { g_FblRuntimeClosureFailStep = (step); return 0u; } } while(0)

    FBL_CLOSURE_CHECK(1u, core0_main);
    FBL_CLOSURE_CHECK(2u, Fbl_UdsHandle);
    FBL_CLOSURE_CHECK(3u, Fbl_DoIpMain);
    FBL_CLOSURE_CHECK(4u, Fbl_DoIpHandleTcpFrame);
    FBL_CLOSURE_CHECK(5u, Fbl_FlashEraseRange);
    FBL_CLOSURE_CHECK(6u, Fbl_FlashProgram);
    FBL_CLOSURE_CHECK(7u, Fbl_FlashProgramFblPayload);
    FBL_CLOSURE_CHECK(8u, Fbl_FlashFlush);
    FBL_CLOSURE_CHECK(9u, Fbl_CommitBootCriticalPage);
    FBL_CLOSURE_CHECK(10u, Fbl_Crc32);
    FBL_CLOSURE_CHECK(11u, Fbl_Crc32FblWithDelayedBootPage);
    FBL_CLOSURE_CHECK(12u, Fbl_Crc32UpdateByte);
    FBL_CLOSURE_CHECK(13u, FblRam_DisableInterrupts);
    FBL_CLOSURE_CHECK(14u, FblRam_RestoreInterrupts);
    FBL_CLOSURE_CHECK(15u, FblRam_InvalidateProgramCache);
    FBL_CLOSURE_CHECK(16u, FblRam_RequestSystemReset);
    FBL_CLOSURE_CHECK(17u, FblRam_CopyBytes);
    FBL_CLOSURE_CHECK(18u, FblRam_SetBytes);
    FBL_CLOSURE_CHECK(19u, FblRam_MoveBytes);
    FBL_CLOSURE_CHECK(20u, FblRam_CompareBytes);
    FBL_CLOSURE_CHECK(21u, Fbl_ScrWriteBluState);
    FBL_CLOSURE_CHECK(22u, FblRamRuntime_SetDestructivePhase);
    FBL_CLOSURE_CHECK(23u, Fbl_BluEnterRecoveryWaitFromTrap);

#undef FBL_CLOSURE_CHECK

    if(FblEth_RuntimeClosureOk() == 0u)
    {
        g_FblRuntimeClosureFailStep = 1000u + g_FblEthRuntimeClosureFailStep;
        return 0u;
    }
    return 1u;
}

static uint8 Fbl_EnterDirectUpdateRuntime(void)
{
    if(FblRamRuntime_IsActive() != 0u)
    {
        return 1u;
    }

    if(Fbl_RuntimeClosureOk() == 0u)
    {
        return 0u;
    }

    if(FblRamRuntime_EnterCritical() == 0u)
    {
        return 0u;
    }

    g_blu.flags |= FBL_BLU_FLAG_DIRECT_UPDATE;
    Fbl_ScrWriteBluState(&g_blu);
    return 1u;
}

static void Fbl_JumpToApp(void)
{
    (void)FblRam_DisableInterrupts();

    /* The updated application image was programmed through the non-cached alias.
     * Invalidate the program cache so the core fetches the fresh image, then jump
     * to the cached execution alias (0x8...). Never jump to the 0xA... alias. */
    FblRam_InvalidateProgramCache();
    __asm("isync");
    __asm("ji %0" : : "a"(APP_START_CACHED));
}
