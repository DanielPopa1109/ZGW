#ifndef PARALLELFLASHSWC_H
#define PARALLELFLASHSWC_H

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Dcm.h"
#include "GatewaySwc.h"

#define PARALLELFLASHSWC_VERSION_MAJOR              1u
#define PARALLELFLASHSWC_VERSION_MINOR              0u

#define PARALLELFLASHSWC_MAX_TARGETS                16u
#define PARALLELFLASHSWC_MAX_BLOCKS_PER_TARGET       8u
#define PARALLELFLASHSWC_MAX_ACTIVE_PER_BUS          2u
#define PARALLELFLASHSWC_NODE_NAME_LEN              24u
#define PARALLELFLASHSWC_EXT_ADDR_LEN                8u

#ifndef PARALLELFLASHSWC_DEBUG_INSTRUMENTATION
#define PARALLELFLASHSWC_DEBUG_INSTRUMENTATION       0
#endif

void ParallelFlashSwc_Init(void);
void ParallelFlashSwc_MainFunction(void);
Dcm_ReturnType ParallelFlashSwc_ForwardCodingRequest(uint8 extendedAddress,
                                                     Dcm_OpStatusType opStatus,
                                                     const uint8 *udsRequest,
                                                     Dcm_PduLengthType udsRequestLength,
                                                     uint8 *response,
                                                     Dcm_PduLengthType *responseLength);
void ParallelFlashSwc_OnForwardTxConfirmation(PduIdType txPduId, Std_ReturnType result);
uint8 ParallelFlashSwc_CanAcceptRoutedRequest(uint8 extendedAddress);
uint8 ParallelFlashSwc_IsForwardTxPending(PduIdType txPduId);

/* Returns the node extended address last forwarded on the given diagnostic RX PDU id
 * (E_OK), so a forwarded slave response can be tagged before relaying it to the
 * tester. E_NOT_OK if no forward has been recorded for that PDU. */
Std_ReturnType ParallelFlashSwc_GetForwardResponseExt(PduIdType rxPduId, uint8 *extendedAddress);

/* Returns the most recently forwarded node extended address (E_OK), used as a fallback
 * tag when a single-frame reply surfaces on a sibling CAN channel's PDU. E_NOT_OK if no
 * routed request has been forwarded yet. */
Std_ReturnType ParallelFlashSwc_GetLastForwardExt(uint8 *extendedAddress);

/* Forwards a physical TesterPresent (3E 80) to every configured bus node, so a single
 * TesterPresent from the tester keeps every node's diagnostic session alive. Call when
 * the ZGW receives a TesterPresent from the tester. */
void ParallelFlashSwc_BroadcastTesterPresent(void);

#if PARALLELFLASHSWC_DEBUG_INSTRUMENTATION
extern volatile uint8 ParallelFlashSwc_DebugActiveCan;
extern volatile uint8 ParallelFlashSwc_DebugActiveCanFd;
extern volatile uint8 ParallelFlashSwc_DebugActiveLin;
extern volatile uint8 ParallelFlashSwc_DebugCompletedJobs;
extern volatile uint8 ParallelFlashSwc_DebugFailedJobs;
extern volatile uint32 ParallelFlashSwc_DebugLastError;
extern volatile uint32 ParallelFlashSwc_DebugForwardRequests;
extern volatile uint32 ParallelFlashSwc_DebugForwardCanOk;
extern volatile uint32 ParallelFlashSwc_DebugForwardCanFail;
extern volatile uint8 ParallelFlashSwc_DebugForwardLastExt;
extern volatile uint8 ParallelFlashSwc_DebugForwardLastSid;
extern volatile uint16 ParallelFlashSwc_DebugForwardLastPdu;
extern volatile uint8 ParallelFlashSwc_DebugForwardLastResult;
extern volatile uint32 ParallelFlashSwc_DebugForwardQueued;
extern volatile uint32 ParallelFlashSwc_DebugForwardQueueFull;
extern volatile uint32 ParallelFlashSwc_DebugForwardDispatched;
extern volatile uint32 ParallelFlashSwc_DebugForwardQueueDepth;
extern volatile uint32 ParallelFlashSwc_DebugForwardQueueMaxDepth;
extern volatile uint32 ParallelFlashSwc_DebugForwardLinBusy;
extern volatile uint32 ParallelFlashSwc_DebugForwardLinDropped;
extern volatile uint32 ParallelFlashSwc_DebugForwardLinFastFail;
#endif

#endif
