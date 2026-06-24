#ifndef PDUR_H
#define PDUR_H

#include "ComStack_Types.h"
#include "TcpIpH.h"

#define PDUR_MAX_TP_BUFFER             8192u
#define PDUR_MAX_TP_RX_ROUTES          16u
#define PDUR_MAX_TP_RX_BUFFERS         4u
#define PDUR_MAX_IF_ROUTE_DATA_LEN     64u
#define PDUR_SOAD_INVALID_SOCON        0xFFu

#define PDUR_INIT_FAIL_NONE            0u
#define PDUR_INIT_FAIL_TP_RX_ROUTES    1u

void PduR_Init(void);
uint8 PduR_IsInitialized(void);

/* Upper IF API: COM -> PduR -> CanIf/LinIf/SoAd */
Std_ReturnType PduR_ComTransmit(PduIdType TxPduId,
                                const uint8* data,
                                PduLengthType len);

/* Upper TP API: DCM -> PduR -> CanTp/LinTp/DoIP */
Std_ReturnType PduR_DcmTransmit(PduIdType DcmTxPduId,
                                const uint8* data,
                                PduLengthType len);
void PduR_DcmReleaseNoResponse(PduIdType DcmTxPduId);

/* Relay a forwarded slave UDS response back to the DoIP tester that issued the
 * routed request. The payload is prefixed with the originating node's extended
 * address so the tester can correlate it. Returns E_NOT_OK if no routed return path
 * is currently captured or the DoIP TX mailbox is busy. */
Std_ReturnType PduR_DoIPRelayForwardedResponse(uint8 extendedAddress,
                                               const uint8* data,
                                               uint16 len);

/* Lower IF callbacks: CanIf/LinIf/SoAd -> PduR -> COM and/or raw IF gateway */
void PduR_CanIfRxIndication(PduIdType CanIfRxPduId,
                            const uint8* data,
                            PduLengthType len);
void PduR_CanIfTxConfirmation(PduIdType CanIfTxPduId);

void PduR_LinIfRxIndication(PduIdType LinIfRxPduId,
                            const uint8* data,
                            PduLengthType len);
void PduR_LinIfTxConfirmation(PduIdType LinIfTxPduId);

void PduR_SoAdIfRxIndication(uint8 soConId,
                             const TcpIp_SockAddrType* remoteAddr,
                             const uint8* data,
                             uint16 len);
void PduR_SoAdIfTxConfirmation(uint8 soConId);

/* Lower TP callbacks: CanTp/LinTp -> PduR -> DCM */
Std_ReturnType PduR_CanTpStartOfReception(PduIdType CanTpRxPduId,
                                          PduLengthType len);
Std_ReturnType PduR_CanTpCopyRxData(PduIdType CanTpRxPduId,
                                    const uint8* data,
                                    PduLengthType len);
void PduR_CanTpRxIndication(PduIdType CanTpRxPduId,
                            Std_ReturnType result);
void PduR_CanTpTxConfirmation(PduIdType CanTpTxPduId,
                              Std_ReturnType result);

Std_ReturnType PduR_LinTpStartOfReception(PduIdType LinTpRxPduId,
                                          PduLengthType len);
Std_ReturnType PduR_LinTpCopyRxData(PduIdType LinTpRxPduId,
                                    const uint8* data,
                                    PduLengthType len);
void PduR_LinTpRxIndication(PduIdType LinTpRxPduId,
                            Std_ReturnType result);
void PduR_LinTpTxConfirmation(PduIdType LinTpTxPduId,
                              Std_ReturnType result);

/* DoIP full UDS payload callback: DoIP -> PduR -> DCM */
Std_ReturnType PduR_DoIPRxIndication(uint16 sourceAddress,
                                     uint16 targetAddress,
                                     const uint8* uds,
                                     uint16 udsLen);
void PduR_DoIPCore0MainFunction(void);
void PduR_DoIPCore2MainFunction(void);
void PduR_DoIPResetSession(void);

extern volatile uint8 PduR_DebugInitialized;
extern volatile uint8 PduR_DebugInitFailure;
extern volatile uint8 PduR_DebugTpRxRouteCount;
extern volatile uint8 PduR_DebugMaxTpRxRoutes;
extern volatile uint32 PduR_DoIPSessionResetAgeTicks;
extern volatile uint32 PduR_DoIPSessionResetStallCounter;

#endif
