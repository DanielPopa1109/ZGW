#ifndef CODINGAPP_H
#define CODINGAPP_H

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Dcm.h"
#include "GatewaySwc.h"

#define CODINGAPP_VERSION_MAJOR                 1u
#define CODINGAPP_VERSION_MINOR                 0u

#ifndef CODINGAPP_DEBUG_INSTRUMENTATION
#define CODINGAPP_DEBUG_INSTRUMENTATION         0
#endif

#define CODINGAPP_DID_STATUS                    0xF1C0u
#define CODINGAPP_DID_IMAGE                     0xF1C1u
#define CODINGAPP_DID_RX_MESSAGE_EXPECTED       0xF1C2u
#define CODINGAPP_DID_CODING_VERSION            0xF1C3u

#define CODINGAPP_ROUTINE_VALIDATE              0x0201u
#define CODINGAPP_ROUTINE_WRITE_ALL             0x0202u
#define CODINGAPP_ROUTINE_READ_NVM              0x0203u
#define CODINGAPP_ROUTINE_LOAD_DEFAULTS         0x0204u

#define CODINGAPP_ROUTINE_START                 0x01u
#define CODINGAPP_ROUTINE_REQUEST_RESULTS       0x03u

#define CODINGAPP_IMAGE_MAGIC                   0x434F4447u
#define CODINGAPP_IMAGE_VERSION                 0x0004u

#define CODINGAPP_RX_MESSAGE_EXPECTED_BYTES     (((uint16)GATEWAYSWC_RX_MESSAGE_DIAG_COUNT + 7u) / 8u)
#define CODINGAPP_TX_PDU_COUNT                  17u
#define CODINGAPP_TX_PDU_ENABLED_BYTES          (((uint16)CODINGAPP_TX_PDU_COUNT + 7u) / 8u)
#define CODINGAPP_MASK_BYTES                    (CODINGAPP_RX_MESSAGE_EXPECTED_BYTES + CODINGAPP_TX_PDU_ENABLED_BYTES)

/* Logical PDU IDs used to apply the common TX coding mask to Ethernet
 * application frames that do not pass through Com. */
#define CODINGAPP_TX_PDU_ETH_SUMMARY            ((PduIdType)0x8001u)
#define CODINGAPP_TX_PDU_ETH_DTC_TRANSITION     ((PduIdType)0x8002u)
#define CODINGAPP_TX_PDU_ETH_MCU_STATUS         ((PduIdType)0x8003u)
#define CODINGAPP_TX_PDU_ETH_AI_MODEL           ((PduIdType)0x8004u)
#define CODINGAPP_TX_PDU_ETH_CPU_PERF           ((PduIdType)0x8005u)
#define CODINGAPP_TX_PDU_ETH_VEHICLESTATE       ((PduIdType)0x8006u)

#define CODINGAPP_STATE_NOT_CODED               0u
#define CODINGAPP_STATE_CODED                   1u
#define CODINGAPP_STATE_INVALID                 2u

#define CODINGAPP_VALIDATION_OK                 0u
#define CODINGAPP_VALIDATION_NOT_CODED          1u
#define CODINGAPP_VALIDATION_BAD_MAGIC          2u
#define CODINGAPP_VALIDATION_BAD_VERSION        3u
#define CODINGAPP_VALIDATION_BAD_LENGTH         4u
#define CODINGAPP_VALIDATION_BAD_MESSAGE_COUNT  5u
#define CODINGAPP_VALIDATION_RESERVED_6         6u
#define CODINGAPP_VALIDATION_NVM_ERROR          7u

#define CODINGAPP_ROUTINE_STATUS_OK             0u
#define CODINGAPP_ROUTINE_STATUS_PENDING        1u
#define CODINGAPP_ROUTINE_STATUS_FAILED         2u
#define CODINGAPP_ROUTINE_STATUS_NOT_CHANGED    3u

typedef struct
{
    uint32 magic;
    uint16 version;
    uint16 length;
    uint32 generation;
    uint16 rxMessageCount;
    uint16 txPduCount;
    uint8 rxMessageExpected[CODINGAPP_RX_MESSAGE_EXPECTED_BYTES];
    uint8 txPduEnabled[CODINGAPP_TX_PDU_ENABLED_BYTES];
    uint32 reserved;
} CodingApp_NvImageType;

#define CODINGAPP_NVM_IMAGE_SIZE                ((uint32)sizeof(CodingApp_NvImageType))

void CodingApp_Init(void);
void CodingApp_MainFunction(void);
void CodingApp_OnNvMReadAllComplete(void);
void CodingApp_OnDemEventCleared(Dem_EventIdType eventId);

boolean CodingApp_IsCoded(void);
boolean CodingApp_IsRxMessageExpected(uint16 diagIndex);
boolean CodingApp_IsTxPduEnabled(PduIdType txPduId);

Std_ReturnType CodingApp_ReadDid(uint16 did, uint8 *data, Dcm_PduLengthType *dataLen);
Dcm_ReturnType CodingApp_WriteDid(uint16 did, const uint8 *data, Dcm_PduLengthType dataLen);
Dcm_ReturnType CodingApp_RoutineControl(
    Dcm_OpStatusType opStatus,
    uint8 routineControlType,
    uint16 routineId,
    const uint8 *reqData,
    Dcm_PduLengthType reqLen,
    uint8 *respData,
    Dcm_PduLengthType *respLen
);

#if CODINGAPP_DEBUG_INSTRUMENTATION
extern volatile uint8 CodingApp_DebugState;
extern volatile uint8 CodingApp_DebugValidationStatus;
extern volatile uint8 CodingApp_DebugDirty;
extern volatile uint16 CodingApp_DebugRxMessageExpectedCount;
extern volatile uint32 CodingApp_DebugGeneration;
extern volatile uint32 CodingApp_DebugInvalidCodingCounter;
extern volatile uint32 CodingApp_DebugWriteAllCounter;
extern volatile uint8 CodingApp_DebugLastNvMResult;
#endif

#endif /* CODINGAPP_H */
