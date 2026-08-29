#include "Lin_LowLevel.h"
#include "Lin.h"
#include "Lin_Cfg.h"

#include "IfxPort.h"

static IfxAsclin_Lin Lin_Ild;
static uint8 Lin_LastPid;
static uint8 Lin_ResponseLen;
static uint8 Lin_RxEnabled;

#define LIN_PIN_OUTPUT_IDX   IfxPort_OutputIdx_general

static IfxAsclin_Checksum Lin_LowLevel_ToHwChecksum(Lin_ChecksumType checksumType)
{
    if (checksumType == LIN_CS_CLASSIC)
    {
        return IfxAsclin_Checksum_classic;
    }

    return IfxAsclin_Checksum_enhanced;
}

static Lin_ResultType Lin_LowLevel_GetErrorResult(void)
{
    if (Lin_Ild.errorFlagsStatus.linChecksumError != 0u)
    {
        return LIN_RES_CHECKSUM_ERROR;
    }

    if (Lin_Ild.errorFlagsStatus.frameError != 0u)
    {
        return LIN_RES_FRAMING_ERROR;
    }

    if (Lin_Ild.errorFlagsStatus.linParityError != 0u)
    {
        return LIN_RES_PID_ERROR;
    }

    if ((Lin_Ild.errorFlagsStatus.responseTimeout != 0u) ||
        (Lin_Ild.errorFlagsStatus.headerTimeout != 0u))
    {
        return LIN_RES_TIMEOUT;
    }

    if ((Lin_Ild.errorFlagsStatus.breakDetected != 0u) ||
        (Lin_Ild.errorFlagsStatus.collisionDetectionError != 0u) ||
        (Lin_Ild.errorFlagsStatus.rxFifoOverflow != 0u) ||
        (Lin_Ild.errorFlagsStatus.txFifoOverflow != 0u))
    {
        return LIN_RES_NOT_OK;
    }

    return LIN_RES_OK;
}

void Lin_LowLevel_Init(void)
{
    IfxAsclin_Lin_Config cfg;

    IfxAsclin_Lin_initModuleConfig(&cfg, &LIN_ASCLIN_MODULE);

    cfg.linMode = IfxAsclin_LinMode_master;
    cfg.brg.baudrate = LIN_DEFAULT_BAUDRATE;
    cfg.lin.breakLength = 13u;
    cfg.data.checksum = IfxAsclin_Checksum_enhanced;
    cfg.data.responseTimeout = 255u;
    cfg.lin.csEnable = TRUE;
    cfg.lin.csi = IfxAsclin_ChecksumInjection_notWritten;
    cfg.isInterruptMode = FALSE;
    cfg.pins = &LIN_PINS;

    IfxAsclin_Lin_initModule(&Lin_Ild, &cfg);

    Lin_RxEnabled = FALSE;
    Lin_LastPid = 0u;
    Lin_ResponseLen = 0u;
}

Lin_ResultType Lin_LowLevel_TransferFrame(uint8 Channel,
                                          uint8 pid,
                                          uint8 len,
                                          uint8 direction,
                                          Lin_ChecksumType checksumType,
                                          const uint8* txData,
                                          uint8* rxData)
{
    IfxAsclin_Lin_PduType pdu;
    Lin_ResultType error;
    boolean waitError;

    (void)Channel;

    if ((len == 0u) || (len > LIN_MAX_DATA_LEN))
    {
        return LIN_RES_NOT_OK;
    }

    if ((direction == LIN_MASTER_RESPONSE) && (txData == NULL_PTR))
    {
        return LIN_RES_NOT_OK;
    }

    if ((direction == LIN_SLAVE_RESPONSE) && (rxData == NULL_PTR))
    {
        return LIN_RES_NOT_OK;
    }

    pdu.pid = pid;
    pdu.dataLength = len;
    pdu.dataPtr = (uint8*)txData;
    pdu.checksumMode = Lin_LowLevel_ToHwChecksum(checksumType);

    if (direction == LIN_MASTER_RESPONSE)
    {
        pdu.direction = IfxAsclin_Lin_Direction_TransmitHeaderAndResponse;
        IfxAsclin_Lin_sendFrame(&Lin_Ild, &pdu);

        waitError = IfxAsclin_Lin_waitForTransmittedHeader(&Lin_Ild);
        error = Lin_LowLevel_GetErrorResult();

        if ((waitError != FALSE) && (error == LIN_RES_OK))
        {
            error = LIN_RES_HEADER_ERROR;
        }

        if (error != LIN_RES_OK)
        {
            return error;
        }

        waitError = IfxAsclin_Lin_waitForTransmittedResponse(&Lin_Ild);
        error = Lin_LowLevel_GetErrorResult();

        if ((waitError != FALSE) && (error == LIN_RES_OK))
        {
            error = LIN_RES_HEADER_ERROR;
        }

        return error;
    }

    if (direction == LIN_SLAVE_RESPONSE)
    {
        pdu.direction = IfxAsclin_Lin_Direction_TransmitHeaderAndReceiveResponse;
        pdu.dataPtr = NULL_PTR;

        IfxAsclin_Lin_sendFrame(&Lin_Ild, &pdu);

        waitError = IfxAsclin_Lin_waitForTransmittedHeader(&Lin_Ild);
        error = Lin_LowLevel_GetErrorResult();

        if ((waitError != FALSE) && (error == LIN_RES_OK))
        {
            error = LIN_RES_TIMEOUT;
        }

        if (error != LIN_RES_OK)
        {
            return error;
        }

        waitError = IfxAsclin_Lin_waitForReceivedResponse(&Lin_Ild);
        error = Lin_LowLevel_GetErrorResult();

        if ((waitError != FALSE) && (error == LIN_RES_OK))
        {
            error = LIN_RES_TIMEOUT;
        }

        if (error == LIN_RES_OK)
        {
            IfxAsclin_Lin_readResponse(&Lin_Ild, rxData, len);
        }

        return error;
    }

    {
        uint8 headerPid = pid;

        /*
         * In polling mode sendHeader() already waits for THE. Calling the
         * wait helper again would clear the software flag and report a
         * timeout even though the header was transmitted.
         */
        IfxAsclin_Lin_sendHeader(&Lin_Ild, &headerPid);
    }

    error = Lin_LowLevel_GetErrorResult();

    if ((error == LIN_RES_OK) &&
        (Lin_Ild.acknowledgmentFlags.txHeaderEnd == 0u))
    {
        error = LIN_RES_HEADER_ERROR;
    }

    return error;
}

void Lin_LowLevel_SendBreak(uint8 Channel)
{
    /*
     * iLLD LIN API sends break+sync+PID through sendHeader().
     * Upper Lin state machine still models BREAK/SYNC/PID logically.
     */
    Lin_IsrTxDone(Channel);
}

void Lin_LowLevel_SendByte(uint8 Channel, uint8 byte)
{
    static uint8 txData[8u];
    static uint8 txLen = 0u;
    Lin_StateType state;

    state = Lin_GetState(Channel);

    if ((state == LIN_TX_SYNC) && (byte == 0x55u))
    {
        txLen = 0u;
        Lin_IsrTxDone(Channel);
        return;
    }

    if (state == LIN_TX_PID)
    {
        Lin_LastPid = byte;
        txLen = 0u;

        {
            Lin_ResultType error;
            uint8 pid = Lin_LastPid;

            IfxAsclin_Lin_sendHeader(&Lin_Ild, &pid);
            error = Lin_LowLevel_GetErrorResult();

            if ((error == LIN_RES_OK) &&
                (Lin_Ild.acknowledgmentFlags.txHeaderEnd == 0u))
            {
                error = LIN_RES_HEADER_ERROR;
            }

            if (error != LIN_RES_OK)
            {
                Lin_IsrError(Channel, error);
                return;
            }
        }

        Lin_IsrTxDone(Channel);
        return;
    }

    if (txLen >= sizeof(txData))
    {
        txLen = 0u;
        Lin_IsrError(Channel, LIN_RES_NOT_OK);
        return;
    }

    txData[txLen] = byte;
    txLen++;

    if (txLen >= Lin_ResponseLen)
    {
        Lin_ResultType error;

        IfxAsclin_Lin_sendResponse(&Lin_Ild, txData, txLen);
        txLen = 0u;

        error = Lin_LowLevel_GetErrorResult();

        if ((error == LIN_RES_OK) &&
            (Lin_Ild.acknowledgmentFlags.txResponseEnd == 0u))
        {
            error = LIN_RES_TIMEOUT;
        }

        if (error != LIN_RES_OK)
        {
            Lin_IsrError(Channel, error);
            return;
        }

        Lin_IsrTxDone(Channel);
        return;
    }

    Lin_IsrTxDone(Channel);
}

void Lin_LowLevel_EnableRx(uint8 Channel)
{
    uint8 rx[LIN_MAX_DATA_LEN + 1u];
    uint32 i;
    Lin_ResultType error;

    Lin_RxEnabled = TRUE;

    if (Lin_ResponseLen == 0u)
    {
        return;
    }

    IfxAsclin_Lin_receiveResponse(&Lin_Ild, rx, Lin_ResponseLen);
    error = Lin_LowLevel_GetErrorResult();

    if ((error == LIN_RES_OK) &&
        (Lin_Ild.acknowledgmentFlags.rxResponseEnd == 0u))
    {
        error = LIN_RES_TIMEOUT;
    }

    if (error != LIN_RES_OK)
    {
        Lin_RxEnabled = FALSE;
        Lin_IsrError(Channel, error);
        return;
    }

    if (Lin_RxEnabled != FALSE)
    {
        for (i = 0u; i < Lin_ResponseLen; i++)
        {
            Lin_IsrRxByte(Channel, rx[i]);
        }
    }
}

void Lin_LowLevel_DisableRx(uint8 Channel)
{
    (void)Channel;
    Lin_RxEnabled = FALSE;
}

void Lin_LowLevel_WakeupPulse(uint8 Channel)
{
    volatile uint32 delay;

    (void)Channel;

    IfxPort_setPinModeOutput(LIN_TX_PORT,
            LIN_TX_PIN_INDEX,
            IfxPort_OutputMode_pushPull,
            LIN_PIN_OUTPUT_IDX);

    IfxPort_setPinLow(LIN_TX_PORT, LIN_TX_PIN_INDEX);

    for (delay = 0u; delay < LIN_WAKEUP_DELAY_TICKS; delay++)
    {
        __asm("nop");
    }

    IfxPort_setPinHigh(LIN_TX_PORT, LIN_TX_PIN_INDEX);
}

void Lin_LowLevel_SetResponseLength(uint8 len)
{
    Lin_ResponseLen = len;
}

void Lin_LowLevel_SetChecksumType(Lin_ChecksumType checksumType)
{
    IfxAsclin_setChecksumMode(Lin_Ild.asclin,
                              Lin_LowLevel_ToHwChecksum(checksumType));
}
