/* Dcm.h */
#ifndef DCM_H
#define DCM_H

#include "Ifx_Types.h"
#include "ComStack_Types.h"
#include "Std_Types.h"
#include <stddef.h>

typedef uint16 Dcm_PduIdType;
typedef uint16 Dcm_PduLengthType;
typedef uint8  Dcm_ReturnType;
typedef NotifResultType Dcm_NotifResultType;

#define DCM_E_OK      0x00u
#define DCM_E_NOT_OK  0x01u
#define DCM_E_PENDING 0xFFu

#define DCM_INITIAL   0x00u
#define DCM_PENDING   0x01u
#define DCM_CANCEL    0x02u

#define DCM_S3_SERVER_TICKS 1000u

#define DCM_SESSION_MASK_DEFAULT     (1u << DCM_SESSION_DEFAULT)
#define DCM_SESSION_MASK_PROGRAMMING (1u << DCM_SESSION_PROGRAMMING)
#define DCM_SESSION_MASK_EXTENDED    (1u << DCM_SESSION_EXTENDED)
#define DCM_SESSION_MASK_CODING      (1u << 4u)

typedef uint8 Dcm_OpStatusType;

/* ===================== Limits ===================== */

#define DCM_MAX_CONNECTIONS          16u
#define DCM_CLASSIC_ISOTP_MAX_LEN    4095u
#define DCM_MAX_PDU_LEN              8192u
#define DCM_MAX_RESPONSE_LEN         8192u
#define DCM_RX_QUEUE_DEPTH           8u
#define DCM_RX_QUEUE_BUFFER_LEN      320u
#define DCM_MAX_SERVICES             32u
#define DCM_MAX_DID_RESPONSE_LEN     512u
#define DCM_MAX_ROUTINE_RESPONSE_LEN 512u
#define DCM_MAX_SERVICE_RESPONSE_LEN (DCM_MAX_RESPONSE_LEN - 1u)
#define DCM_MAX_DTC_RESPONSE_LEN     (DCM_MAX_SERVICE_RESPONSE_LEN - 1u)
#define DCM_TRANSFER_BLOCK_LEN       1024u

/* ===================== Timing ===================== */

#define DCM_P2_SERVER_TICKS          50u
#define DCM_P2STAR_SERVER_TICKS      5000u
#define DCM_RESPONSE_PENDING_MAX     6u

/* ===================== AUTOSAR-like TP ===================== */


typedef struct
{
    uint8* SduDataPtr;
    Dcm_PduLengthType SduLength;
} Dcm_PduInfoType;

/* ===================== UDS NRC ===================== */

#define DCM_NRC_GENERAL_REJECT                          0x10u
#define DCM_NRC_SERVICE_NOT_SUPPORTED                   0x11u
#define DCM_NRC_SUBFUNCTION_NOT_SUPPORTED               0x12u
#define DCM_NRC_INCORRECT_LENGTH                        0x13u
#define DCM_NRC_RESPONSE_TOO_LONG                       0x14u
#define DCM_NRC_BUSY_REPEAT_REQUEST                     0x21u
#define DCM_NRC_CONDITIONS_NOT_CORRECT                  0x22u
#define DCM_NRC_REQUEST_SEQUENCE_ERROR                  0x24u
#define DCM_NRC_REQUEST_OUT_OF_RANGE                    0x31u
#define DCM_NRC_AUTHENTICATION_REQUIRED                 0x34u
#define DCM_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED            0x70u
#define DCM_NRC_TRANSFER_DATA_SUSPENDED                 0x71u
#define DCM_NRC_GENERAL_PROGRAMMING_FAILURE             0x72u
#define DCM_NRC_WRONG_BLOCK_SEQUENCE_COUNTER            0x73u
#define DCM_NRC_RESPONSE_PENDING                        0x78u
#define DCM_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_SESSION    0x7Eu
#define DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION        0x7Fu


/* ===================== UDS SID ===================== */

#define DCM_SID_DIAGNOSTIC_SESSION_CONTROL              0x10u
#define DCM_SID_ECU_RESET                               0x11u
#define DCM_SID_CLEAR_DIAGNOSTIC_INFORMATION            0x14u
#define DCM_SID_READ_DTC_INFORMATION                    0x19u
#define DCM_SID_READ_DATA_BY_IDENTIFIER                 0x22u
#define DCM_SID_READ_MEMORY_BY_ADDRESS                  0x23u
#define DCM_SID_READ_SCALING_DATA_BY_IDENTIFIER         0x24u
#define DCM_SID_COMMUNICATION_CONTROL                   0x28u
#define DCM_SID_AUTHENTICATION                          0x29u
#define DCM_SID_READ_DATA_BY_PERIODIC_IDENTIFIER        0x2Au
#define DCM_SID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER      0x2Cu
#define DCM_SID_WRITE_DATA_BY_IDENTIFIER                0x2Eu
#define DCM_SID_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER      0x2Fu
#define DCM_SID_ROUTINE_CONTROL                         0x31u
#define DCM_SID_REQUEST_DOWNLOAD                        0x34u
#define DCM_SID_REQUEST_UPLOAD                          0x35u
#define DCM_SID_TRANSFER_DATA                           0x36u
#define DCM_SID_REQUEST_TRANSFER_EXIT                   0x37u
#define DCM_SID_REQUEST_FILE_TRANSFER                   0x38u
#define DCM_SID_WRITE_MEMORY_BY_ADDRESS                 0x3Du
#define DCM_SID_TESTER_PRESENT                          0x3Eu
#define DCM_SID_ACCESS_TIMING_PARAMETER                 0x83u
#define DCM_SID_SECURED_DATA_TRANSMISSION               0x84u
#define DCM_SID_CONTROL_DTC_SETTING                     0x85u
#define DCM_SID_RESPONSE_ON_EVENT                       0x86u
#define DCM_SID_LINK_CONTROL                            0x87u

#define DCM_SUPPRESS_POS_RSP_MSG_INDICATION_BIT         0x80u

/* ===================== Standard DIDs ===================== */

#define DCM_DID_ACTIVE_SOFTWARE_BLOCK                 0xF100u
#define DCM_DID_APPLICATION_SOFTWARE_VERSION          0xF101u
#define DCM_DID_ACTIVE_DIAGNOSTIC_SESSION               0xF186u

#define DCM_ACTIVE_SOFTWARE_BLOCK_APP                   0x00u

#define DCM_APP_SW_VERSION_MAJOR                        1u
#define DCM_APP_SW_VERSION_MINOR                        0u
#define DCM_APP_SW_VERSION_PATCH                        0u
#define DCM_FBL_SW_VERSION_MAJOR                        1u
#define DCM_FBL_SW_VERSION_MINOR                        0u
#define DCM_FBL_SW_VERSION_PATCH                        0u

/* ===================== Sessions ===================== */

#define DCM_SESSION_DEFAULT                             0x01u
#define DCM_SESSION_PROGRAMMING                         0x02u
#define DCM_SESSION_EXTENDED                            0x03u
#define DCM_SESSION_CODING                              0x41u

/* ===================== Connection ===================== */

typedef enum
{
    DCM_ADDR_PHYSICAL   = 0u,
    DCM_ADDR_FUNCTIONAL = 1u
} Dcm_AddressingType;

typedef enum
{
    DCM_BUS_CAN_CLASSIC = 0u,
    DCM_BUS_CAN_FD      = 1u,
    DCM_BUS_LIN         = 2u,
    DCM_BUS_ETHERNET    = 3u
} Dcm_BusType;

typedef struct
{
    Dcm_PduIdType rxPduId;
    Dcm_PduIdType txPduId;
    uint32 testerId;
    uint32 ecuId;
    Dcm_AddressingType addressing;
    Dcm_BusType busType;
} Dcm_ConnectionConfigType;

/* ===================== Service callback ===================== */

typedef Dcm_ReturnType (*Dcm_ServiceHandlerType)(
    uint8 connIdx,
    Dcm_OpStatusType opStatus,
    const uint8* reqData,
    Dcm_PduLengthType reqLen,
    uint8* respData,
    Dcm_PduLengthType* respLen
);

typedef struct
{
    uint8 sid;
    Dcm_ServiceHandlerType handler;
} Dcm_ServiceType;

typedef struct
{
    const Dcm_ConnectionConfigType* connections;
    uint8 numConnections;
    const Dcm_ServiceType* services;
    uint8 numServices;
} Dcm_ConfigType;

/* ===================== API ===================== */

void Dcm_Init(const Dcm_ConfigType* ConfigPtr);
void Dcm_ResetDoIPSession(void);
void Dcm_MainFunction(void);
Std_ReturnType Dcm_RxIndication(PduIdType rxPduId, const uint8* data, PduLengthType len);
void Dcm_TxConfirmation(PduIdType txPduId, Std_ReturnType result);
uint8 Dcm_GetActiveSession(uint8 connIdx);
uint8 *Dcm_ProvideRxBufferFromDoIP(uint16 len);

BufReq_ReturnType Dcm_StartOfReception(
    Dcm_PduIdType id,
    const Dcm_PduInfoType* info,
    Dcm_PduLengthType TpSduLength,
    Dcm_PduLengthType* bufferSizePtr
);

BufReq_ReturnType Dcm_CopyRxData(
    Dcm_PduIdType id,
    const Dcm_PduInfoType* info,
    Dcm_PduLengthType* bufferSizePtr
);

void Dcm_TpRxIndication(
    Dcm_PduIdType id,
    Dcm_NotifResultType result
);

BufReq_ReturnType Dcm_CopyTxData(
    Dcm_PduIdType id,
    const Dcm_PduInfoType* info,
    Dcm_PduLengthType* availableDataPtr
);

void Dcm_TpTxConfirmation(
    Dcm_PduIdType id,
    Dcm_NotifResultType result
);

/* Lower-layer transmit trigger.
 * Implement this in PduR/CanTp/LinTp/DoIP later.
 */
Std_ReturnType Dcm_LoTransmit(
    Dcm_PduIdType txPduId,
    Dcm_PduLengthType len
);

extern volatile uint32 Dcm_DebugExtForwardRequests;
extern volatile uint8 Dcm_DebugExtForwardLastExt;
extern volatile uint8 Dcm_DebugExtForwardLastSid;
extern volatile uint16 Dcm_DebugExtForwardLastLen;
extern volatile uint8 Dcm_DebugExtForwardLastResult;

/* ===================== Weak application hooks ===================== */

void DcmAppl_DiagnosticSessionChanged(
    uint8 connIdx,
    uint8 session
);

Dcm_ReturnType DcmAppl_ReadDataByIdentifier(
    uint8 connIdx,
    Dcm_OpStatusType opStatus,
    uint16 did,
    uint8* data,
    Dcm_PduLengthType* dataLen
);

Dcm_ReturnType DcmAppl_WriteDataByIdentifier(
    uint8 connIdx,
    Dcm_OpStatusType opStatus,
    uint16 did,
    const uint8* data,
    Dcm_PduLengthType dataLen
);

Dcm_ReturnType DcmAppl_RoutineControl(
    uint8 connIdx,
    Dcm_OpStatusType opStatus,
    uint8 routineControlType,
    uint16 routineId,
    const uint8* reqData,
    Dcm_PduLengthType reqLen,
    uint8* respData,
    Dcm_PduLengthType* respLen
);

Dcm_ReturnType DcmAppl_CommunicationControl(
    uint8 connIdx,
    Dcm_OpStatusType opStatus,
    uint8 controlType,
    uint8 communicationType
);

Dcm_ReturnType DcmAppl_ControlDtcSetting(
    uint8 connIdx,
    Dcm_OpStatusType opStatus,
    uint8 settingType
);

Dcm_ReturnType DcmAppl_RequestDownload(
    uint8 connIdx,
    Dcm_OpStatusType opStatus,
    const uint8* reqData,
    Dcm_PduLengthType reqLen,
    uint8* respData,
    Dcm_PduLengthType* respLen
);

Dcm_ReturnType DcmAppl_TransferData(
    uint8 connIdx,
    Dcm_OpStatusType opStatus,
    uint8 blockSequenceCounter,
    const uint8* reqData,
    Dcm_PduLengthType reqLen,
    uint8* respData,
    Dcm_PduLengthType* respLen
);

Dcm_ReturnType DcmAppl_RequestTransferExit(
    uint8 connIdx,
    Dcm_OpStatusType opStatus,
    const uint8* reqData,
    Dcm_PduLengthType reqLen,
    uint8* respData,
    Dcm_PduLengthType* respLen
);

Dcm_ReturnType DcmAppl_StandardService(
    uint8 connIdx,
    Dcm_OpStatusType opStatus,
    uint8 sid,
    const uint8* reqData,
    Dcm_PduLengthType reqLen,
    uint8* respData,
    Dcm_PduLengthType* respLen
);

extern const Dcm_ConfigType Dcm_Config;
extern const Dcm_ConnectionConfigType Dcm_DefaultConnections[];
extern const Dcm_ServiceType Dcm_DefaultServices[];

#endif
