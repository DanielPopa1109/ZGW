#include "Lin_Cfg.h"

const IfxAsclin_Lin_Pins Lin_Pins =
{
    &IfxAsclin1_RXA_P15_1_IN,
    IfxPort_InputMode_pullUp,
    &IfxAsclin1_TX_P15_0_OUT,
    IfxPort_OutputMode_pushPull,
    IfxPort_PadDriver_cmosAutomotiveSpeed4
};
