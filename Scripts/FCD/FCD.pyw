import base64
import binascii
import json
import queue
import select
import socket
import struct
import threading
import time
import traceback
import zipfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from fcd_parallel import (
    build_schedule,
    bus_category,
    create_bundle,
    discover_nodes,
    load_bundle,
    manifest_from_targets,
    node_to_target,
    to_jsonable_events,
    validate_schedule,
)


APP_NAME = "FCD"
APP_TITLE = "FCD - Flash Coding Diagnostics"
DEFAULT_HOST = "192.168.1.10"
DEFAULT_PORT = 13400
DEFAULT_SOURCE_ADDR = 0x0710
DEFAULT_TARGET_ADDR = 0x1001
FCD_TRACE_MIRROR_HOST = "127.0.0.1"
FCD_TRACE_MIRROR_PORT = 54088
FCD_TRACE_MIRROR_MAGIC = b"FCDT"
DEFAULT_APP_START = 0xA0030000
DEFAULT_APP_END = 0xA05CFFFF
# Normal ZGW DoIP TransferData carries 4096 firmware bytes by default; the ECU
# advertises the active maximum in RequestDownload.
DEFAULT_BLOCK_SIZE = 4096
TRANSFER_DATA_REQUEST_LIMIT = 256
TRANSFER_DATA_OVERHEAD_BYTES = 2
TRANSFER_DATA_MAX_CHUNK_SIZE = TRANSFER_DATA_REQUEST_LIMIT - TRANSFER_DATA_OVERHEAD_BYTES
ZGW_ETHERNET_TRANSFER_DATA_REQUEST_LIMIT = 32770
ZGW_ETHERNET_TRANSFER_DATA_MAX_CHUNK_SIZE = (
    ZGW_ETHERNET_TRANSFER_DATA_REQUEST_LIMIT - TRANSFER_DATA_OVERHEAD_BYTES
)
ROUTED_TRANSFER_ADDRESS_OVERHEAD_BYTES = 1
ROUTED_TRANSFER_DATA_MAX_CHUNK_SIZE = (
    TRANSFER_DATA_REQUEST_LIMIT
    - ROUTED_TRANSFER_ADDRESS_OVERHEAD_BYTES
    - TRANSFER_DATA_OVERHEAD_BYTES
)
TRANSFER_LOG_FULL_HEX_LIMIT = 256
TRANSFER_LOG_EDGE_BYTES = 16
PARALLEL_BUNDLE_MAX_WORKERS = 6
ROUTED_BUS_MAX_IN_FLIGHT = 2
ROUTED_NODE_MAX_IN_FLIGHT = 1
ROUTED_RECOVERY_TIMEOUT_SECONDS = 3.0

# Minimum spacing between consecutive UDS requests on the normal request/response path.
# Routed simulated coding operations use explicit shared schedules below instead.
REQUEST_SPACING_SECONDS = 0.1
BUS_TARGET_DIAG_TIMEOUT_SECONDS = 1.0
SIMULATED_BUS_TARGET_DIAG_TIMEOUT_SECONDS = 0.25
ROUTED_FORWARD_RETRY_COUNT = 8
ROUTED_FORWARD_RETRY_DELAY_SECONDS = 0.1
ROUTED_READ_CODING_NODE_STAGGER_SECONDS = 0.005
ROUTED_READ_CODING_SERVICE_GAP_SECONDS = 0.100
ROUTED_LIN_REQUEST_SPACING_SECONDS = 1.000
ROUTED_READ_CODING_DRAIN_SECONDS = 0.020
ROUTED_WAITING_SETTLE_SECONDS = 0.150
ROUTED_CODE_VEHICLE_POST_RESET_GAP_SECONDS = 1.000
ROUTED_CODE_VEHICLE_SETTLE_SECONDS = 1.000
ROUTED_FLASH_POST_RESET_GAP_SECONDS = 1.000
ROUTED_SEND_BACKPRESSURE_TIMEOUT_SECONDS = 30.0
ROUTED_SEND_BACKPRESSURE_POLL_SECONDS = 0.050
ROUTED_TRANSPORT_ACK_TIMEOUT_SECONDS = 120.0
FBL_UPDATER_ENTRY_TIMEOUT_SECONDS = 30.0
FBL_ERASE_TIMEOUT_SECONDS = 120.0
TRACE_DRAIN_MAX_LINES = 100

# Fault-memory Snapshot Data readout can require hundreds of short 0x19 requests.
# Keep the normal conservative pacing for writes/programming, but use a tighter
# serialized gap for this read-only sequence.
FAULT_MEMORY_REQUEST_SPACING_SECONDS = 0.02
FAULT_MEMORY_DETAIL_TIMEOUT_SECONDS = 8.0

POST_RESET_RESPONSE_TIMEOUT_SECONDS = 0.1
POST_RESET_RECONNECT_DELAY_SECONDS = 0.1
POST_RESET_UDS_READY_TIMEOUT_SECONDS = 30.0
POST_RESET_UDS_READY_RETRY_SECONDS = 0.1

# Lab ZGW CodingApp identifiers. These mirror APP/CodingApp/CodingApp.h.
CODING_DID_STATUS = 0xF1C0
CODING_DID_IMAGE = 0xF1C1
CODING_DID_RX_MESSAGE_EXPECTED = 0xF1C2
CODING_DID_VERSION = 0xF1C3
CODING_ROUTINE_VALIDATE = 0x0201
CODING_ROUTINE_WRITE_ALL = 0x0202
CODING_ROUTINE_READ_NVM = 0x0203
CODING_ROUTINE_LOAD_DEFAULTS = 0x0204
TIMESYNC_ROUTINE_SET_UTC = 0xF190
TIMESYNC_ROUTINE_GET_STATUS = 0xF191

# The CodingApp routine result is a 10-byte status block (status/state/validation/
# dirty/rxMessageExpectedCount[2]/generation[4]) followed by the coding bitmask -
# see CodingApp_FillRoutineResponse / the read-NVM routine in CodingApp.c.
CODING_ROUTINE_STATUS_LEN = 10
CODING_ROUTINE_STATUS_OK = 0x00
CODING_ROUTINE_STATUS_PENDING = 0x01
CODING_ROUTINE_STATUS_NOT_CHANGED = 0x03

CODING_ROUTINE_STATUS_TEXT = {
    0x00: "OK",
    0x01: "PENDING",
    0x02: "FAILED",
    0x03: "NOT_CHANGED",
}

CODING_STATE_TEXT = {
    0x00: "NOT_CODED",
    0x01: "CODED",
    0x02: "INVALID",
}

CODING_VALIDATION_TEXT = {
    0x00: "OK",
    0x01: "NOT_CODED",
    0x02: "BAD_MAGIC",
    0x03: "BAD_VERSION",
    0x04: "BAD_LENGTH",
    0x05: "BAD_MESSAGE_COUNT",
    0x06: "BAD_CRC",
    0x07: "NVM_ERROR",
}

CODING_NVM_JOB_TEXT = {
    0x00: "NONE",
    0x01: "WRITE_ALL",
    0x02: "READ_BLOCK",
}

NVM_RESULT_TEXT = {
    0x00: "OK",
    0x01: "NOT_OK",
    0x02: "PENDING",
    0x03: "INTEGRITY_FAILED",
    0x04: "BLOCK_SKIPPED",
    0x05: "NV_INVALIDATED",
    0x06: "RESTORED_FROM_ROM",
}

NVM_STATUS_TEXT = {
    0x00: "UNINIT",
    0x01: "IDLE",
    0x02: "BUSY",
    0x03: "BUSY_INTERNAL",
}

MEMIF_STATUS_TEXT = {
    0x00: "UNINIT",
    0x01: "IDLE",
    0x02: "BUSY",
    0x03: "BUSY_INTERNAL",
}

MEMIF_JOB_TEXT = {
    0x00: "OK",
    0x01: "FAILED",
    0x02: "PENDING",
    0x03: "CANCELED",
    0x04: "BLOCK_INCONSISTENT",
    0x05: "BLOCK_INVALID",
}


def _build_coding_parameter_names():
    """Ordered coding parameter names.

    The first mask bytes are the CodingApp rxMessageExpected bits, in the same
    order as GatewaySwc_RxMessageDiagRanges. The remaining bytes are the
    compact txPduEnabled bits, ordered as CodingApp_TxPduCodingList. XCP and
    diagnostic request TX PDUs are intentionally not exposed because CodingApp
    always treats them as enabled.
    """
    rx_names = [
        # CAN: COM_RX_PDU_CENTRALLOCKDATA .. COM_RX_PDU_DMU_ALIVE
        "CENTRALLOCKDATA", "LIGHTDATA1", "STATUSACTUATOR", "OUTSIDETEMPERATURESTATUS",
        "CENTRALCOMMAND1", "DMUSTATUS", "BATTFULLSTAT", "PDCSTAT", "MILEAGE", "DMU_ALIVE",
        # CAN: COM_RX_PDU_VOLTAGECURRENT .. COM_RX_PDU_L1_I2T_COUNTER
        "VOLTAGECURRENT", "TEMPMEAS", "LOADSTATUS", "L1_I2T_COUNTER",
        # CAN: COM_RX_PDU_BATTSOCSOH .. COM_RX_PDU_BATTCAPRES
        "BATTSOCSOH", "BATTSOC", "BATTDIAGNOSIS", "BATTCURRENT", "BATTCAPDISCHARGE", "BATTCAPRES",
    ]
    # CAN-FD: PDM1 load/current/InputT30 receive diagnostics.
    rx_names += ["CANFD_PDM1_LOADSTATUS"]
    for pdm in (1,):
        rx_names += [f"CANFD_PDM{pdm}_CURRENTFEEDBACK_{i}" for i in range(1, 6)]
    rx_names += ["CANFD_PDM1_INPUTT30"]
    # LIN: HVDCDC is the only configured slave node.
    rx_names += ["LIN_HVDCDC_STATUS"]

    tx_names = [
        "TX_VEHICLESTATE",
        "TX_DISPLAYOUTTEMP",
        "TX_STATUSBODYDATA1",
        "TX_COMMANDDISPLAYSTATUS",
        "TX_SDAT",
        "TX_NM3",
        "TX_LOADREQUEST",
        "TX_CANFD_INFOTAINMENTDATA1",
        "TX_CANFD_ENERGYMANAGEMENTDATA2",
        "TX_CANFD_ENERGYMANAGEMENTDATA1",
        "TX_CANFD_VEHICLESTATE",
        "TX_CANFD_NM3",
        "TX_CANFD_SDAT",
        "TX_CANFD_LIGHTDATA1",
        "TX_CANFD_BODYDATA1",
        "TX_CANFD_COMMANDLOAD_PDM1",
        "TX_CANFD_ENERGYMANAGEMENTDATA3",
        "TX_LIN_ZGW_REQUEST_HVDCDC",
    ]

    rx_mask_bytes = (len(rx_names) + 7) // 8
    tx_bit_offset = rx_mask_bytes * 8
    names = list(rx_names)
    names.extend(f"(reserved bit {idx})" for idx in range(len(names), tx_bit_offset))
    names.extend(tx_names)
    return names


CODING_PARAMETER_NAMES = _build_coding_parameter_names()
CODING_PARAMETER_INDEX = {name: idx for idx, name in enumerate(CODING_PARAMETER_NAMES)}
CODING_VISIBLE_PARAMETER_NAMES = tuple(
    name for name in CODING_PARAMETER_NAMES if not name.startswith("(reserved")
)
CODING_RX_PARAMETER_COUNT = len([name for name in CODING_PARAMETER_NAMES if not name.startswith("TX_") and not name.startswith("(reserved")])
CODING_RX_MASK_BYTES = (CODING_RX_PARAMETER_COUNT + 7) // 8
CODING_TX_PDU_COUNT = len([name for name in CODING_PARAMETER_NAMES if name.startswith("TX_")])
CODING_MASK_BYTES = CODING_RX_MASK_BYTES + ((CODING_TX_PDU_COUNT + 7) // 8)
DUMMY_CODING_VALUES = (
    ("dummy_coding_value_1", 0, "1"),
    ("dummy_coding_value_2", 1, "0"),
    ("dummy_coding_value_3", 2, "1"),
)


def coding_parameter_index(name):
    text = str(name).strip()
    idx = CODING_PARAMETER_INDEX.get(text)
    if idx is not None:
        return idx
    for dummy_name, dummy_index, _dummy_value in DUMMY_CODING_VALUES:
        if text == dummy_name:
            return dummy_index
    prefix = "(reserved bit "
    if text.startswith(prefix) and text.endswith(")"):
        try:
            return int(text[len(prefix):-1].strip(), 10)
        except ValueError:
            return None
    return None


def is_reserved_coding_parameter(name):
    return str(name).strip().startswith("(reserved bit ")

DEM_DTC_MCUSM_SW_ERROR = 0x010101
DEM_DTC_PMS_ERRATA_STARTUP = 0x010102
DEM_DTC_CODING_ECU_NOT_CODED = 0x023000
DEM_DTC_CODING_INVALID = 0x023001
DEM_DTC_AIMODEL_INPUT_INVALID = 0x024000
DEM_DTC_AIMODEL_INFERENCE_INVALID = 0x024001
DEM_DTC_AIMODEL_DEADLINE_EXCEEDED = 0x024002
DEM_DTC_AIMODEL_OUTPUT_OUT_OF_RANGE = 0x024003
DEM_DTC_AIMODEL_CONSUMER_FAULT = 0x024100
DEM_AIMODEL_CONSUMER_EVENT_COUNT = 75
DEM_DTC_CAN_BUS_DIAG = 0x022100
DEM_CAN_BUS_DIAG_EVENT_COUNT = 8
DEM_DTC_CAN_CLASSIC_BUS_OFF = 0x022100
DEM_DTC_CAN_CLASSIC_ERROR_PASSIVE = 0x022101
DEM_DTC_CAN_CLASSIC_CONTROLLER_FAULT = 0x022102
DEM_DTC_CAN_CLASSIC_PROTOCOL_ERROR = 0x022103
DEM_DTC_CANFD_BUS_OFF = 0x022104
DEM_DTC_CANFD_ERROR_PASSIVE = 0x022105
DEM_DTC_CANFD_CONTROLLER_FAULT = 0x022106
DEM_DTC_CANFD_PROTOCOL_ERROR = 0x022107
DEM_DTC_LIN_BUS_DIAG = 0x022200
DEM_LIN_BUS_DIAG_EVENT_COUNT = 5
DEM_DTC_LIN1_HVDCDC_NO_COMMUNICATION = 0x022200
DEM_DTC_LIN1_PROTOCOL_ERROR = 0x022201
DEM_DTC_LIN1_CONTROLLER_FAULT = 0x022202
DEM_DTC_LIN1_WAKEUP_FAILURE = 0x022203
DEM_DTC_LIN1_SLEEP_FAILURE = 0x022204
DEM_DTC_ETHERNET_DIAG = 0x022000
DEM_ETHERNET_DIAG_EVENT_COUNT = 15
DEM_DTC_ETH_LINK_LOST = 0x022000
DEM_DTC_ETH_CTRL_DMA_FAILURE = 0x022001
DEM_DTC_ETH_RX_COMM_FAILURE = 0x022002
DEM_DTC_ETH_TX_COMM_FAILURE = 0x022003
DEM_DTC_ETH_TCP_UNEXPECTED_TERMINATION = 0x022004
DEM_DTC_ETH_TCP_ESTABLISHMENT_FAILURE = 0x022005
DEM_DTC_ETH_UDP_SUPERVISION_TIMEOUT = 0x022006
DEM_DTC_ETH_SERVICE_AVAILABILITY_FAILURE = 0x022007
DEM_DTC_ETH_DOIP_COMM_FAILURE = 0x022008
DEM_DTC_ETH_PARTNER_COMM_TERMINATED = 0x022009
DEM_DTC_ETH_PHY_COMMUNICATION_FAULT = 0x02200A
DEM_DTC_ETH_PHY_FAULT = 0x02200B
DEM_DTC_ETH_NEGOTIATION_FAILURE = 0x02200C
DEM_DTC_ETH_UNEXPECTED_LINK_MODE = 0x02200D
DEM_DTC_ETH_RESOURCE_EXHAUSTION = 0x02200E
DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT = 0x021000
DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT = CODING_RX_PARAMETER_COUNT
DEM_DTC_TIMESTAMP_DATA_SIZE = 22
MCUSM_SNAPSHOT_TIMESTAMP_OFFSET = 224

STATIC_DTC_DESCRIPTIONS = {
    DEM_DTC_MCUSM_SW_ERROR: "MCUSM software error",
    DEM_DTC_PMS_ERRATA_STARTUP: "PMS errata startup warning",
    DEM_DTC_CODING_ECU_NOT_CODED: "Coding ECU not coded",
    DEM_DTC_CODING_INVALID: "Coding invalid",
    DEM_DTC_AIMODEL_INPUT_INVALID: "AiModel input invalid or stale",
    DEM_DTC_AIMODEL_INFERENCE_INVALID: "AiModel inference invalid",
    DEM_DTC_AIMODEL_DEADLINE_EXCEEDED: "AiModel inference deadline exceeded",
    DEM_DTC_AIMODEL_OUTPUT_OUT_OF_RANGE: "AiModel output out of range",
    DEM_DTC_CAN_CLASSIC_BUS_OFF: "ZGW_CAN_3 Bus-Off",
    DEM_DTC_CAN_CLASSIC_ERROR_PASSIVE: "CAN Error Passive",
    DEM_DTC_CAN_CLASSIC_CONTROLLER_FAULT: "CAN Controller Fault",
    DEM_DTC_CAN_CLASSIC_PROTOCOL_ERROR: "CAN Excessive Protocol Error",
    DEM_DTC_CANFD_BUS_OFF: "ZGW_CANFD_2 Bus-Off",
    DEM_DTC_CANFD_ERROR_PASSIVE: "CANFD Error Passive",
    DEM_DTC_CANFD_CONTROLLER_FAULT: "CANFD Controller Fault",
    DEM_DTC_CANFD_PROTOCOL_ERROR: "CANFD Excessive Protocol Error",
    DEM_DTC_LIN1_HVDCDC_NO_COMMUNICATION: "LIN1 Slave HVDCDC - No Communication",
    DEM_DTC_LIN1_PROTOCOL_ERROR: "LIN1 Protocol Error",
    DEM_DTC_LIN1_CONTROLLER_FAULT: "LIN1 Controller Fault",
    DEM_DTC_LIN1_WAKEUP_FAILURE: "LIN1 Wakeup Failure",
    DEM_DTC_LIN1_SLEEP_FAILURE: "LIN1 Sleep Transition Failure",
    DEM_DTC_ETH_LINK_LOST: "ETH0 Link Down",
    DEM_DTC_ETH_CTRL_DMA_FAILURE: "ETH0 MAC Controller Fault",
    DEM_DTC_ETH_RX_COMM_FAILURE: "ETH0 Excessive RX Frame Errors",
    DEM_DTC_ETH_TX_COMM_FAILURE: "ETH0 Excessive TX Errors",
    DEM_DTC_ETH_TCP_UNEXPECTED_TERMINATION: "Ethernet TCP Unexpected Termination",
    DEM_DTC_ETH_TCP_ESTABLISHMENT_FAILURE: "Ethernet TCP Establishment Failure",
    DEM_DTC_ETH_UDP_SUPERVISION_TIMEOUT: "Ethernet UDP Supervision Timeout",
    DEM_DTC_ETH_SERVICE_AVAILABILITY_FAILURE: "SOME/IP Service 0x1234 Instance 0x0001 - Unavailable",
    DEM_DTC_ETH_DOIP_COMM_FAILURE: "DoIP Communication Fault",
    DEM_DTC_ETH_PARTNER_COMM_TERMINATED: "Ethernet Remote Partner - No Communication",
    DEM_DTC_ETH_PHY_COMMUNICATION_FAULT: "ETH0 PHY Communication Fault",
    DEM_DTC_ETH_PHY_FAULT: "ETH0 PHY Hardware Fault",
    DEM_DTC_ETH_NEGOTIATION_FAILURE: "ETH0 Auto-negotiation Failure",
    DEM_DTC_ETH_UNEXPECTED_LINK_MODE: "ETH0 Unexpected Link Speed/Duplex",
    DEM_DTC_ETH_RESOURCE_EXHAUSTION: "Ethernet Resource Exhaustion",
}

def is_aimodel_consumer_dtc(dtc):
    return DEM_DTC_AIMODEL_CONSUMER_FAULT <= dtc < (
        DEM_DTC_AIMODEL_CONSUMER_FAULT + DEM_AIMODEL_CONSUMER_EVENT_COUNT)

def describe_aimodel_consumer_dtc(dtc):
    channel = (dtc - DEM_DTC_AIMODEL_CONSUMER_FAULT) + 1
    return f"AiModel consumer {channel:02d} predicted fault"

# Plain-language maps used to decode the ZGW Dem Snapshot Data buffers. The
# byte layouts mirror the firmware capture callbacks and every multi-byte field
# is big-endian:
#   GatewaySwc_CaptureRxDiagSnapshotData (GatewaySwc.c)
#   SysMgr_CaptureMcuSmSnapshotData      (SysMgr.c)
#   Dem_DefaultSnapshotDataCapture       (Dem_Cfg.c)
GATEWAY_BUS_TEXT = {1: "CAN", 2: "CAN-FD", 3: "LIN"}
GATEWAY_RX_DIAG_STATUS_TEXT = {0x00: "OK", 0x01: "TIMEOUT", 0x02: "INVALID"}
CANDIAG_CONTROLLER_TEXT = {0: "ZGW_CAN_3", 1: "ZGW_CANFD_2"}
CANDIAG_FAULT_TEXT = {
    0: "Bus-Off",
    1: "Error Passive",
    2: "Controller Fault",
    3: "Excessive Protocol Error",
}
CANDIAG_CAN_STATE_TEXT = {0: "UNINIT", 1: "READY", 2: "SLEEP", 3: "BUS_OFF"}
CANDIAG_ERROR_STATE_TEXT = {
    0: "ERROR_ACTIVE",
    1: "ERROR_WARNING",
    2: "ERROR_PASSIVE",
    3: "BUS_OFF",
}
CANDIAG_CANSM_STATE_TEXT = {
    0: "UNINIT",
    1: "NO_COMMUNICATION",
    2: "SILENT_COMMUNICATION",
    3: "FULL_COMMUNICATION",
    4: "BUS_OFF_CHECK",
    5: "BUS_OFF_RECOVERY_L1",
    6: "BUS_OFF_RECOVERY_L2",
    7: "FAILED",
}
CANDIAG_COMM_MODE_TEXT = {0: "NO_COMMUNICATION", 1: "SILENT_COMMUNICATION", 2: "FULL_COMMUNICATION"}
CANDIAG_PDU_MODE_TEXT = {0: "OFFLINE", 1: "RX_ONLINE", 2: "TX_ONLINE", 3: "ONLINE"}
LINDIAG_CHANNEL_TEXT = {0: "LIN1"}
LINDIAG_FAULT_TEXT = {
    0: "Slave HVDCDC - No Communication",
    1: "Protocol Error",
    2: "Controller Fault",
    3: "Wakeup Failure",
    4: "Sleep Transition Failure",
}
LINDIAG_SCHEDULE_TEXT = {0: "Normal", 1: "Diagnostic Master Request", 2: "Diagnostic Slave Response"}
LINDIAG_LINSM_STATE_TEXT = {
    0: "UNINIT",
    1: "NO_COMMUNICATION",
    2: "FULL_COMMUNICATION",
    3: "GOTO_SLEEP",
    4: "SLEEP",
    5: "WAKEUP",
}
LINDIAG_LIN_STATE_TEXT = {
    0: "UNINIT",
    1: "INIT",
    2: "IDLE",
    3: "TX_BREAK",
    4: "TX_SYNC",
    5: "TX_PID",
    6: "TX_RESPONSE",
    7: "RX_RESPONSE",
    8: "SLEEP_PENDING",
    9: "TX_SLEEP",
    10: "SLEEP",
    11: "WAKEUP",
    12: "ERROR",
}
LINDIAG_RESULT_TEXT = {
    0: "OK",
    1: "NOT_OK",
    2: "NO_RESPONSE",
    3: "CHECKSUM_ERROR",
    4: "PID_ERROR",
    5: "FRAMING_ERROR",
    6: "SYNC_ERROR",
    7: "TIMEOUT",
    8: "HEADER_ERROR",
}
LINDIAG_ERROR_CLASS_TEXT = {
    0: "None",
    1: "No response",
    2: "Timeout",
    3: "Checksum",
    4: "PID/parity",
    5: "Framing",
    6: "Synchronization",
    7: "Header transmission",
    8: "Controller",
    9: "Diagnostic schedule timeout",
    10: "Schedule error",
}
LINDIAG_CHANNEL_STATE_TEXT = {
    0: "IDLE",
    1: "BUSY",
    2: "ERROR",
}
LINDIAG_DIAG_STATE_TEXT = {
    0: "IDLE",
    1: "MRF_PENDING",
    2: "MRF_ACTIVE",
    3: "SRF_PENDING",
    4: "SRF_ACTIVE",
    5: "DONE",
    6: "ERROR",
}
ETHDIAG_PROTOCOL_TEXT = {0: "UDP", 1: "TCP"}
ETHDIAG_SOAD_STATE_TEXT = {
    0: "CLOSED",
    1: "OPEN",
    2: "CONNECTED",
    3: "RECONNECT",
}
ETHDIAG_PHY_STATE_TEXT = {
    0: "UNINIT",
    1: "RESET",
    2: "WAIT_RESET_DONE",
    3: "CONFIGURE",
    4: "WAIT_AUTONEG",
    5: "LINK_DOWN",
    6: "LINK_UP",
    7: "ERROR",
}
ETHDIAG_RESOURCE_FLAG_TEXT = [
    (0x0001, "TX descriptor unavailable"),
    (0x0002, "TX pbuf allocation failed"),
    (0x0004, "TX pbuf chain allocation failed"),
    (0x0008, "RX pbuf allocation failed"),
]
TIMEBASE_SOURCE_TEXT = {
    0: "default compile-time",
    1: "UDS routine",
    2: "gPTP master",
    255: "invalid",
}
MCUSM_FAULT_SOURCE_BITS = [
    (0x01, "MCU reset latched"),
    (0x02, "SCR ECC double-bit error"),
    (0x04, "SCR watchdog"),
    (0x08, "SafetyKit failure"),
]
MCUSM_SCR_FAULT_PENDING_BITS = [
    (0x01, "ECC double-bit error"),
    (0x02, "watchdog"),
]
MCUSM_RESET_REASON_TEXT = {
    254: "SysMgr go-to-sleep final reset",
    371: "CPU trap class 1",
    372: "CPU trap class 2",
    373: "CPU trap class 3",
    374: "CPU trap class 4 bus/system trap",
    379: "FreeRTOS stack overflow",
    380: "FreeRTOS malloc failed",
    381: "FreeRTOS init failure",
    384: "SysMgr go-to-sleep failure",
    385: "SafetyKit reset reaction",
    386: "core2 TCB corrupt",
    387: "core2 stack pointer corrupt",
    388: "core2 list corrupt",
    390: "DFlash recovery reset",
}
SYSMGR_GO_SLEEP_FAIL_TEXT = {
    1: "NvM not idle before WriteAll",
    2: "NvM not idle after WriteAll",
    3: "Dem operation-cycle end failed",
    4: "Dem shutdown failed",
    5: "NvM WriteAll request failed",
    6: "Dem NvM block WriteAll did not complete",
}
MCUSM_SAFETYKIT_FAILURE_BITS = [
    (0x00000001, "LBIST"),
    (0x00000002, "MONBIST"),
    (0x00000004, "MCU FW check"),
    (0x00000008, "MCU startup"),
    (0x00000010, "alive alarm"),
    (0x00000020, "register monitor"),
    (0x00000040, "MBIST"),
    (0x00000080, "SMU keys"),
    (0x00000100, "SMU keys clear"),
    (0x00000200, "SMU init"),
]
MCUSM_FW_CHECK_RESULT_BITS = [
    (0x00000001, "SMU register check failed"),
    (0x00000002, "STMEM register check failed"),
    (0x00000004, "LCLCON register check failed"),
    (0x00000008, "SSH check failed"),
    (0x00000010, "LBIST SSH zero accepted"),
    (0x00000020, "LBIST SSH table accepted"),
    (0x00000040, "LBIST SSH no-init accepted"),
    (0x00000080, "LBIST SSH init accepted"),
]
PMS_ERRATA_FAILURE_BITS = [
    (0x00000001, "TC007 VDDP3 OV"),
    (0x00000002, "TC007 VDD OV"),
    (0x00000004, "TC013 RSTCTRIM"),
    (0x00000008, "TCH003 PREOVVAL"),
    (0x00000010, "TCH003 PREUVVAL"),
    (0x00000020, "TC007 MONSTAT1 stale"),
]
SAFETYKIT_RESET_TYPE_TEXT = {
    0: "cold power-on",
    1: "system",
    2: "application",
    3: "warm power-on",
    4: "debug",
    5: "undefined",
    15: "LBIST",
}
SCU_RESET_TRIGGER_TEXT = {
    0: "ESR0",
    1: "ESR1",
    3: "SMU",
    4: "software",
    5: "STM0",
    6: "STM1",
    7: "STM2",
    8: "STM3",
    9: "STM4",
    10: "STM5",
    16: "PORTST",
    18: "Cerberus system",
    19: "Cerberus debug",
    20: "Cerberus application",
    23: "EVRC",
    24: "EVR33",
    25: "supply watchdog",
    26: "HSM system",
    27: "HSM application",
    28: "standby regulator watchdog",
    255: "undefined",
}
SSW_STATUS_TEXT = {
    0: "not evaluated",
    1: "failed",
    2: "passed",
}
SMU_STATUS_TEXT = {
    0: "fail",
    1: "pass",
    255: "NA",
}
DEM_EVENT_ID_NAMES = {
    1: "MCUSM software error",
    2: "PMS errata startup warning",
    3: "Coding ECU not coded",
    4: "Coding invalid",
    5: "AiModel input invalid or stale",
    6: "AiModel inference invalid",
    7: "AiModel inference deadline exceeded",
    8: "AiModel output out of range",
    9: "ETH0 Link Down",
    10: "ETH0 MAC Controller Fault",
    11: "ETH0 Excessive RX Frame Errors",
    12: "ETH0 Excessive TX Errors",
    13: "Ethernet TCP Unexpected Termination",
    14: "Ethernet TCP Establishment Failure",
    15: "Ethernet UDP Supervision Timeout",
    16: "SOME/IP Service 0x1234 Instance 0x0001 - Unavailable",
    17: "DoIP Communication Fault",
    18: "Ethernet Remote Partner - No Communication",
    19: "ETH0 PHY Communication Fault",
    20: "ETH0 PHY Hardware Fault",
    21: "ETH0 Auto-negotiation Failure",
    22: "ETH0 Unexpected Link Speed/Duplex",
    23: "Ethernet Resource Exhaustion",
    24: "ZGW_CAN_3 Bus-Off",
    25: "CAN Error Passive",
    26: "CAN Controller Fault",
    27: "CAN Excessive Protocol Error",
    28: "ZGW_CANFD_2 Bus-Off",
    29: "CANFD Error Passive",
    30: "CANFD Controller Fault",
    31: "CANFD Excessive Protocol Error",
    32: "LIN1 Slave HVDCDC - No Communication",
    33: "LIN1 Protocol Error",
    34: "LIN1 Controller Fault",
    35: "LIN1 Wakeup Failure",
    36: "LIN1 Sleep Transition Failure",
}

ROUTINE_ERASE_MEMORY = 0x0001
ROUTINE_CHECK_MEMORY_CRC = 0x0002
ROUTINE_START_FBL_RAM_UPDATER = 0x0155
ROUTINE_SELECT_SW_BLOCK = 0x0200

DID_ACTIVE_SW_BLOCK = 0xF100
DID_APP_SW_VERSION = 0xF101
DID_ACTIVE_DIAG_SESSION = 0xF186

SESSION_DEFAULT = 0x01
SESSION_PROGRAMMING = 0x02
SESSION_EXTENDED = 0x03
ACTIVE_SW_BLOCK_APP = 0x01
ACTIVE_SW_BLOCK_FBL = 0x02
SESSION_CODING_REQUESTED = 0x41


SCRIPT_DIR = Path(__file__).resolve().parent
SETTINGS_PATH = SCRIPT_DIR / "fcd_settings.json"

DOIP_PROTO_VER = 0x02
DOIP_INV_PROTO_VER = 0xFD
DOIP_PT_VID_REQ = 0x0001
DOIP_PT_VID_RES = 0x0004
DOIP_PT_ROUTING_ACT_REQ = 0x0005
DOIP_PT_ROUTING_ACT_RES = 0x0006
DOIP_PT_ALIVE_CHECK_REQ = 0x0007
DOIP_PT_ALIVE_CHECK_RES = 0x0008
DOIP_PT_DIAG_MSG = 0x8001
DOIP_PT_DIAG_ACK = 0x8002
DOIP_PT_DIAG_NACK = 0x8003

NRC_TEXT = {
    0x10: "General Reject",
    0x11: "Service Not Supported",
    0x12: "SubFunction Not Supported",
    0x13: "Incorrect Message Length",
    0x14: "Response Too Long",
    0x22: "Conditions Not Correct",
    0x24: "Request Sequence Error",
    0x31: "Request Out Of Range",
    0x33: "Security Access Denied",
    0x35: "Invalid Key",
    0x36: "Exceeded Number Of Attempts",
    0x37: "Required Time Delay Not Expired",
    0x70: "Upload/Download Not Accepted",
    0x71: "Transfer Data Suspended",
    0x72: "General Programming Failure",
    0x73: "Wrong Block Sequence Counter",
    0x78: "Response Pending",
    0x7E: "SubFunction Not Supported In Active Session",
    0x7F: "Service Not Supported In Active Session",
}


class FcdError(Exception):
    pass


class NodeTimeout(FcdError):
    pass


class DoipError(FcdError):
    pass


class NegativeResponse(FcdError):
    def __init__(self, sid, nrc):
        self.sid = sid
        self.nrc = nrc
        super().__init__(
            f"Negative response for 0x{sid:02X}: NRC 0x{nrc:02X} "
            f"({NRC_TEXT.get(nrc, 'Unknown')})"
        )


@dataclass
class HexSegment:
    address: int
    data: bytes

    @property
    def end(self):
        return self.address + len(self.data)

    @property
    def crc32(self):
        return binascii.crc32(self.data) & 0xFFFFFFFF


def now_text():
    return datetime.now().strftime("%H:%M:%S")


def parse_int(value, default=None):
    text = str(value).strip()
    if not text:
        if default is not None:
            return default
        raise ValueError("Missing numeric value")
    return int(text, 0)


def int_hex(value, width=4):
    return f"0x{int(value):0{width}X}"


DEFAULT_EXTENDED_DIAG_ADDRESSES = {
    "ZGW": 0x41,
    "PDM1": 0x42,
    "HVDCDC": 0x51,
    "CBM": 0x54,
    "DMU": 0x56,
    "ELC": 0x58,
    "FRBE": 0x59,
}

LOCAL_UDS_SERVICE_IDS = {
    0x10, 0x11, 0x14, 0x19, 0x22, 0x28, 0x31, 0x34, 0x35, 0x36, 0x37, 0x3E, 0x85
}

EXTENDED_DIAG_FALLBACK_RANGES = {
    "CANFD": [(0x60, 0x7F)],
    "CAN": [(0x90, 0xAF)],
    "LIN": [(0x46, 0x4F)],
    "UNKNOWN": [(0x46, 0x4F), (0x60, 0x7F), (0x90, 0xAF)],
}


def default_extended_diag_address(node_name, bus_type=None):
    node_key = str(node_name).upper()
    if node_key in DEFAULT_EXTENDED_DIAG_ADDRESSES:
        return int_hex(DEFAULT_EXTENDED_DIAG_ADDRESSES[node_key], 2)

    seed = binascii.crc32(str(node_name).upper().encode("ascii", errors="ignore"))
    category = bus_category(bus_type or "UNKNOWN")
    ranges = EXTENDED_DIAG_FALLBACK_RANGES.get(category, EXTENDED_DIAG_FALLBACK_RANGES["UNKNOWN"])
    candidates = [value for start, end in ranges for value in range(start, end + 1)]
    reserved = set(DEFAULT_EXTENDED_DIAG_ADDRESSES.values()) | LOCAL_UDS_SERVICE_IDS

    for offset in range(len(candidates)):
        value = candidates[(seed + offset) % len(candidates)]
        if value not in reserved:
            return int_hex(value, 2)

    value = candidates[seed % len(candidates)]
    return int_hex(value, 2)


def ecu_name_from_hex_stem(stem):
    name = str(stem).strip().upper()
    changed = True
    while changed:
        changed = False
        for suffix in ("_EDITED", "_APP", "_FBL"):
            if name.endswith(suffix):
                name = name[: -len(suffix)]
                changed = True
    return name


def current_layout_node_names(root_path):
    try:
        nodes, _logs = discover_nodes(root_path)
    except Exception:
        return set()
    return {str(node.node_name).strip().upper() for node in nodes}


def filtered_hex_rows_for_layout(rows, root_path):
    valid_nodes = current_layout_node_names(root_path)
    if not valid_nodes:
        return list(rows)

    filtered = []
    seen = set()
    for row in rows:
        hex_path_text = str(row.get("hex", "")).strip()
        if not hex_path_text:
            continue
        ecu_name = ecu_name_from_hex_stem(row.get("ecu") or Path(hex_path_text).stem)
        if ecu_name not in valid_nodes:
            continue
        hex_path = Path(hex_path_text)
        if not hex_path.exists():
            continue
        dedupe_key = (ecu_name, str(hex_path).lower())
        if dedupe_key in seen:
            continue
        seen.add(dedupe_key)
        filtered.append(row)
    return filtered


def bytes_to_hex(data):
    return data.hex(" ").upper()


def uds_request_log_text(data):
    data = bytes(data)
    if len(data) > TRANSFER_LOG_FULL_HEX_LIMIT and data and data[0] == 0x36:
        edge = min(TRANSFER_LOG_EDGE_BYTES, len(data) // 2)
        payload = data[TRANSFER_DATA_OVERHEAD_BYTES:] if len(data) >= TRANSFER_DATA_OVERHEAD_BYTES else b""
        omitted = max(0, len(data) - (edge * 2))
        return (
            f"len={len(data)} payload_len={len(payload)} "
            f"first={bytes_to_hex(data[:edge])} "
            f"middle_omitted={omitted} bytes "
            f"last={bytes_to_hex(data[-edge:])} "
            f"payload_crc32=0x{binascii.crc32(payload) & 0xFFFFFFFF:08X}"
        )
    return bytes_to_hex(data)


class FcdTraceMirror:
    def __init__(self, host=FCD_TRACE_MIRROR_HOST, port=FCD_TRACE_MIRROR_PORT):
        self.addr = (host, int(port))
        self.lock = threading.Lock()
        self.sock = None

    def _socket(self):
        if self.sock is None:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        return self.sock

    def emit_doip(self, direction, payload_type, source_addr, target_addr, payload):
        payload = bytes(payload or b"")
        header = struct.pack(
            ">4scHHHI",
            FCD_TRACE_MIRROR_MAGIC,
            bytes(str(direction or "?")[:1], "ascii", errors="ignore") or b"?",
            int(payload_type) & 0xFFFF,
            int(source_addr) & 0xFFFF,
            int(target_addr) & 0xFFFF,
            len(payload),
        )
        try:
            with self.lock:
                self._socket().sendto(header + payload, self.addr)
        except OSError:
            pass

    def emit_raw_uds(self, direction, source_addr, target_addr, payload):
        self.emit_doip(direction, 0xFFFF, source_addr, target_addr, payload)


FCD_TRACE_MIRROR = FcdTraceMirror()


def u16_be(data, offset):
    return (data[offset] << 8) | data[offset + 1]

def s16_be(data, offset):
    value = u16_be(data, offset)
    if value >= 0x8000:
        value -= 0x10000
    return value

def u32_be(data, offset):
    return (
        (data[offset] << 24)
        | (data[offset + 1] << 16)
        | (data[offset + 2] << 8)
        | data[offset + 3]
    )

def ip4_to_text(value):
    return ".".join(str((value >> shift) & 0xFF) for shift in (24, 16, 8, 0))

def s32_be(data, offset):
    value = u32_be(data, offset)
    if value >= 0x80000000:
        value -= 0x100000000
    return value

def u64_be(data, offset):
    value = 0
    for byte in data[offset:offset + 8]:
        value = (value << 8) | byte
    return value


def format_dtc_timestamp_data(data, prefix="DTC occurrence time"):
    if len(data) < DEM_DTC_TIMESTAMP_DATA_SIZE:
        return ""
    vehicle_ns = u64_be(data, 0)
    utc_ns = u64_be(data, 8)
    utc_valid = data[16]
    source = data[17]
    mcu_temp_cdeg = s16_be(data, 18)
    snapshot_version = data[20]
    snapshot_kind = data[21]
    source_text = TIMEBASE_SOURCE_TEXT.get(source, f"source {source}")
    parts = [f"{prefix}: vehicleTimeNs={vehicle_ns}"]
    if utc_valid != 0 and utc_ns != 0:
        utc_dt = datetime.utcfromtimestamp(utc_ns / 1_000_000_000)
        parts.append(f"UTC {utc_dt.isoformat(timespec='milliseconds')}Z")
        parts.append(f"utcNs={utc_ns}")
    else:
        parts.append("UTC not valid")
        parts.append(f"utcNs={utc_ns}")
    parts.append(f"time source: {source_text} ({source})")
    parts.append(f"MCU temperature: {mcu_temp_cdeg / 100.0:.2f} deg C")
    parts.append(f"snapshot version: {snapshot_version}")
    parts.append(f"snapshot kind: {snapshot_kind}")
    return ", ".join(parts)


def flag_names(value, table, empty="none"):
    names = [name for mask, name in table if (value & mask) != 0]
    return ", ".join(names) if names else empty


def enum_name(value, table, prefix="value"):
    name = table.get(value)
    if name:
        return f"{name} ({value})"
    return f"{prefix} {value}"


def status_name(value, table):
    return table.get(value, f"0x{value:02X}")


def reset_reason_name(reason, info):
    name = MCUSM_RESET_REASON_TEXT.get(reason)
    if reason == 384:
        detail = SYSMGR_GO_SLEEP_FAIL_TEXT.get(info)
        if detail:
            return f"{name}: {detail}"
    if name:
        return name
    return "none" if reason == 0 else "unknown"




def local_ipv4_addresses():
    ips = set()
    try:
        host_name = socket.gethostname()
        for info in socket.getaddrinfo(host_name, None, socket.AF_INET, socket.SOCK_DGRAM):
            ip = info[4][0]
            if ip and not ip.startswith("127."):
                ips.add(ip)
    except OSError:
        pass

    for target in ("192.168.1.10", "8.8.8.8"):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                sock.connect((target, 9))
                ip = sock.getsockname()[0]
                if ip and not ip.startswith("127."):
                    ips.add(ip)
        except OSError:
            pass

    return ips


def same_ipv4_24(left, right):
    try:
        left_parts = [int(part) for part in str(left).split(".")]
        right_parts = [int(part) for part in str(right).split(".")]
    except ValueError:
        return False
    return len(left_parts) == 4 and len(right_parts) == 4 and left_parts[:3] == right_parts[:3]


def local_bind_candidates(target_host, preferred="auto"):
    text = str(preferred or "").strip()
    if text and text.lower() not in ("auto", "all", "any", "0.0.0.0"):
        return [text]

    candidates = [""]
    local_ips = sorted(local_ipv4_addresses(), key=lambda ip: (0 if same_ipv4_24(ip, target_host) else 1, ip))
    for ip in local_ips:
        if ip not in candidates:
            candidates.append(ip)
    return candidates


def configure_low_latency_tcp(sock):
    tcp_nodelay = getattr(socket, "TCP_NODELAY", None)
    if tcp_nodelay is None:
        return
    try:
        sock.setsockopt(socket.IPPROTO_TCP, tcp_nodelay, 1)
    except OSError:
        pass


def is_own_udp_packet(addr):
    return addr and addr[0] in local_ipv4_addresses()

def hex_to_bytes(text):
    cleaned = (
        text.replace("0x", "")
        .replace("0X", "")
        .replace(",", " ")
        .replace(";", " ")
        .replace("\n", " ")
        .replace("\r", " ")
        .replace("\t", " ")
    )
    parts = [p for p in cleaned.split(" ") if p]
    if not parts and cleaned.strip():
        cleaned = cleaned.strip()
        if len(cleaned) % 2:
            raise ValueError("Hex string has an odd number of digits")
        return bytes.fromhex(cleaned)
    return bytes(int(p, 16) for p in parts)


def require_positive_response(response, request_sid):
    if not response:
        raise FcdError("Empty UDS response")
    if response[0] == 0x7F:
        if len(response) >= 3:
            raise NegativeResponse(response[1], response[2])
        raise FcdError("Malformed negative response")
    expected = (request_sid + 0x40) & 0xFF
    if response[0] != expected:
        raise FcdError(
            f"Unexpected positive SID 0x{response[0]:02X}; expected 0x{expected:02X}"
        )


def uds_response_matches_request(response, request):
    if not response or not request:
        return True

    if len(response) > 1 and len(request) > 1 and response[0] == request[0]:
        return uds_response_matches_request(response[1:], request[1:])

    sid = request[0]

    if response[0] == 0x7F:
        return len(response) >= 2 and response[1] == sid

    if response[0] != ((sid + 0x40) & 0xFF):
        return False

    if sid in (0x10, 0x11, 0x27, 0x28, 0x3E, 0x85):
        return len(request) < 2 or (len(response) >= 2 and response[1] == request[1])

    if sid == 0x19:
        if len(request) < 2:
            return True
        if len(response) < 2 or response[1] != request[1]:
            return False
        if request[1] in (0x04, 0x06):
            return len(request) < 5 or (len(response) >= 5 and response[2:5] == request[2:5])
        return True

    if sid in (0x22, 0x2E, 0x2F):
        return len(request) < 3 or (len(response) >= 3 and response[1:3] == request[1:3])

    if sid == 0x31:
        return len(request) < 4 or (
            len(response) >= 4
            and response[1] == request[1]
            and response[2:4] == request[2:4]
        )

    if sid == 0x36:
        return len(request) < 2 or (len(response) >= 2 and response[1] == request[1])

    return True


def project_lab_key(seed, level):
    key = seed ^ 0x6A09E667
    key = (key + 0x13572468 + ((level & 0xFF) * 0x1F3D5B79)) & 0xFFFFFFFF
    key = ((key << 3) | (key >> 29)) & 0xFFFFFFFF
    key ^= (((seed << 16) & 0xFFFFFFFF) | (seed >> 16))
    return key & 0xFFFFFFFF


def parse_intel_hex(path):
    path = Path(path)
    chunks = []
    base = 0

    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line_no, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line:
                continue
            if not line.startswith(":"):
                raise ValueError(f"{path.name}:{line_no}: Intel HEX line must start with ':'")

            try:
                raw = bytes.fromhex(line[1:])
            except ValueError as exc:
                raise ValueError(f"{path.name}:{line_no}: invalid HEX characters") from exc

            if len(raw) < 5:
                raise ValueError(f"{path.name}:{line_no}: record too short")

            count = raw[0]
            if len(raw) != count + 5:
                raise ValueError(f"{path.name}:{line_no}: byte count mismatch")

            if (sum(raw) & 0xFF) != 0:
                raise ValueError(f"{path.name}:{line_no}: checksum mismatch")

            offset = (raw[1] << 8) | raw[2]
            rectype = raw[3]
            data = raw[4 : 4 + count]

            if rectype == 0x00:
                chunks.append((base + offset, data))
            elif rectype == 0x01:
                break
            elif rectype == 0x02:
                if count != 2:
                    raise ValueError(f"{path.name}:{line_no}: invalid segment base record")
                base = (((data[0] << 8) | data[1]) << 4) & 0xFFFFFFFF
            elif rectype == 0x04:
                if count != 2:
                    raise ValueError(f"{path.name}:{line_no}: invalid linear base record")
                base = (((data[0] << 8) | data[1]) << 16) & 0xFFFFFFFF
            elif rectype in (0x03, 0x05):
                continue
            else:
                raise ValueError(f"{path.name}:{line_no}: unsupported record type 0x{rectype:02X}")

    if not chunks:
        return []

    chunks.sort(key=lambda item: item[0])
    segments = []
    cur_start = None
    cur_data = bytearray()

    for start, data in chunks:
        if cur_start is None:
            cur_start = start
            cur_data = bytearray(data)
            continue

        cur_end = cur_start + len(cur_data)
        if start <= cur_end:
            offset = start - cur_start
            needed = offset + len(data) - len(cur_data)
            if needed > 0:
                cur_data.extend(b"\xFF" * needed)
            cur_data[offset : offset + len(data)] = data
        else:
            segments.append(HexSegment(cur_start, bytes(cur_data)))
            cur_start = start
            cur_data = bytearray(data)

    segments.append(HexSegment(cur_start, bytes(cur_data)))
    return segments


def download_address_from_hex_address(address):
    address = int(address)
    if 0x80000000 <= address < 0xA0000000:
        return address + 0x20000000
    return address


def hex_source_base(segments):
    if not segments:
        raise ValueError("No HEX data segments")
    return min(seg.address for seg in segments) & 0xFFFF0000


def hex_download_base(segments):
    return download_address_from_hex_address(hex_source_base(segments))


class DoipClient:
    def __init__(self, host, port, source_addr, target_addr, timeout=3.0, local_ip="auto"):
        self.host = host
        self.port = int(port)
        self.source_addr = int(source_addr)
        self.target_addr = int(target_addr)
        self.timeout = float(timeout)
        self.local_ip = str(local_ip or "auto").strip()
        self.bound_local_ip = ""
        self.sock = None
        self.lock = threading.Lock()
        self._last_request_ts = 0.0
        self.request_spacing_seconds = REQUEST_SPACING_SECONDS
        self.last_nrc78_count = 0

    @property
    def connected(self):
        return self.sock is not None

    def _pace(self):
        # Enforce the minimum gap since the previous request was issued on this socket.
        # Called inside self.lock so it also serializes concurrent callers.
        wait = self.request_spacing_seconds - (time.monotonic() - self._last_request_ts)
        if wait > 0:
            time.sleep(wait)
        self._last_request_ts = time.monotonic()

    def drain(self):
        # Discard any frames left unread in the socket buffer (e.g. a stale response
        # after a reset or a race) so the next request reads its own reply.
        with self.lock:
            if self.sock is None:
                return
            try:
                self.sock.setblocking(False)
                while True:
                    try:
                        if not self.sock.recv(4096):
                            break
                    except (BlockingIOError, OSError):
                        break
            finally:
                if self.sock is not None:
                    self.sock.setblocking(True)
                    self.sock.settimeout(self.timeout)

    def connect(self):
        self.close()
        last_error = None
        for bind_ip in local_bind_candidates(self.host, self.local_ip):
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                configure_low_latency_tcp(sock)
                sock.settimeout(self.timeout)
                if bind_ip:
                    sock.bind((bind_ip, 0))
                sock.connect((self.host, self.port))
                sock.settimeout(self.timeout)
                self.sock = sock
                self.bound_local_ip = bind_ip
                return
            except OSError as exc:
                last_error = exc
                try:
                    sock.close()
                except OSError:
                    pass
        if last_error is not None:
            raise last_error
        raise TimeoutError("No local TCP bind candidates available")

    def close(self):
        sock = self.sock
        self.sock = None
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass

    def _send_frame(self, payload_type, payload=b"", send_timeout=None):
        if self.sock is None:
            raise DoipError("Not connected")
        header = struct.pack(">BBHI", DOIP_PROTO_VER, DOIP_INV_PROTO_VER, payload_type, len(payload))
        data = header + payload
        view = memoryview(data)
        sent = 0
        timeout = self.timeout if send_timeout is None else float(send_timeout)
        deadline = time.monotonic() + timeout
        poll_timeout = self.timeout
        if send_timeout is not None:
            poll_timeout = min(timeout, ROUTED_SEND_BACKPRESSURE_POLL_SECONDS)
        try:
            while sent < len(data):
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError("Timed out sending DoIP data")
                self.sock.settimeout(max(0.001, min(poll_timeout, remaining)))
                try:
                    count = self.sock.send(view[sent:])
                except TimeoutError:
                    continue
                except BlockingIOError:
                    time.sleep(ROUTED_SEND_BACKPRESSURE_POLL_SECONDS)
                    continue
                except OSError:
                    self.close()
                    raise
                if count == 0:
                    self.close()
                    raise DoipError("TCP connection closed")
                sent += count
            FCD_TRACE_MIRROR.emit_doip("T", payload_type, self.source_addr, self.target_addr, payload)
        except TimeoutError:
            raise
        except OSError:
            self.close()
            raise
        finally:
            if self.sock is not None:
                self.sock.settimeout(self.timeout)

    def _recv_exact(self, length, deadline=None):
        sock = self.sock
        if sock is None:
            raise DoipError("Not connected")
        out = bytearray()
        try:
            while len(out) < length:
                if self.sock is not sock:
                    raise DoipError("Connection closed")
                if deadline is not None:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError("Timed out waiting for DoIP data")
                    sock.settimeout(remaining)
                else:
                    sock.settimeout(self.timeout)
                try:
                    chunk = sock.recv(length - len(out))
                except TimeoutError as exc:
                    raise TimeoutError("Timed out waiting for DoIP data")
                except OSError:
                    if self.sock is sock:
                        self.close()
                    raise
                if not chunk:
                    if self.sock is sock:
                        self.close()
                    raise DoipError("TCP connection closed")
                out.extend(chunk)
        finally:
            if self.sock is sock:
                sock.settimeout(self.timeout)
        return bytes(out)

    def _recv_frame(self, deadline=None):
        header = self._recv_exact(8, deadline)
        proto, inv, payload_type, payload_len = struct.unpack(">BBHI", header)
        if proto != DOIP_PROTO_VER or inv != DOIP_INV_PROTO_VER:
            raise DoipError(f"Invalid DoIP header {proto:02X} {inv:02X}")
        if payload_len > 1024 * 1024:
            raise DoipError(f"DoIP payload too large: {payload_len}")
        payload = self._recv_exact(payload_len, deadline) if payload_len else b""
        FCD_TRACE_MIRROR.emit_doip("R", payload_type, self.source_addr, self.target_addr, payload)
        return payload_type, payload

    def routing_activation(self, activation_type=0x00):
        payload = struct.pack(">HB4s", self.source_addr, activation_type, b"\x00\x00\x00\x00")
        with self.lock:
            self._send_frame(DOIP_PT_ROUTING_ACT_REQ, payload)
            payload_type, response = self._recv_frame(time.monotonic() + self.timeout)
        if payload_type != DOIP_PT_ROUTING_ACT_RES or len(response) < 5:
            raise DoipError(f"Unexpected routing activation response 0x{payload_type:04X}")
        tester, ecu, code = struct.unpack(">HHB", response[:5])
        if tester != self.source_addr:
            raise DoipError(f"Routing response for source 0x{tester:04X}, expected 0x{self.source_addr:04X}")
        if code != 0x10:
            raise DoipError(f"Routing activation denied: 0x{code:02X}")
        return ecu, code

    def alive_check(self):
        with self.lock:
            self._send_frame(DOIP_PT_ALIVE_CHECK_REQ, b"")
            payload_type, response = self._recv_frame(time.monotonic() + self.timeout)
        if payload_type != DOIP_PT_ALIVE_CHECK_RES or len(response) < 2:
            raise DoipError("Unexpected alive-check response")
        return struct.unpack(">H", response[:2])[0]

    def send_uds(self, request, timeout=None, allow_no_response=False):
        timeout = self.timeout if timeout is None else float(timeout)
        payload = struct.pack(">HH", self.source_addr, self.target_addr) + bytes(request)
        deadline = time.monotonic() + timeout
        resent_after_stale_response = False
        nrc78_count = 0

        with self.lock:
            self.last_nrc78_count = 0
            self._pace()
            self._send_frame(DOIP_PT_DIAG_MSG, payload)
            while True:
                try:
                    payload_type, response = self._recv_frame(deadline)
                except TimeoutError:
                    if allow_no_response:
                        return b""
                    raise
                if payload_type == DOIP_PT_ALIVE_CHECK_REQ:
                    self._send_frame(DOIP_PT_ALIVE_CHECK_RES, struct.pack(">H", self.source_addr))
                    continue
                if payload_type == DOIP_PT_DIAG_ACK:
                    if len(response) >= 5 and response[4] != 0:
                        raise DoipError(f"Diagnostic ACK code 0x{response[4]:02X}")
                    continue
                if payload_type == DOIP_PT_DIAG_NACK:
                    code = response[4] if len(response) >= 5 else 0xFF
                    raise DoipError(f"Diagnostic NACK code 0x{code:02X}")
                if payload_type != DOIP_PT_DIAG_MSG:
                    continue
                if len(response) < 5:
                    raise DoipError("Short diagnostic message")

                source, target = struct.unpack(">HH", response[:4])
                uds = response[4:]
                if target != self.source_addr:
                    raise DoipError(f"Diagnostic response target 0x{target:04X} does not match tester")
                if source != self.target_addr:
                    raise DoipError(f"Diagnostic response source 0x{source:04X} does not match target")

                if not uds_response_matches_request(uds, request):
                    if (resent_after_stale_response is False) and (time.monotonic() < deadline):
                        resent_after_stale_response = True
                        self._pace()
                        self._send_frame(DOIP_PT_DIAG_MSG, payload)
                    continue

                if len(uds) >= 3 and uds[0] == 0x7F and uds[2] == 0x78:
                    nrc78_count += 1
                    self.last_nrc78_count = nrc78_count
                    deadline = time.monotonic() + timeout
                    continue
                if len(uds) >= 3 and uds[0] == 0x7F and uds[2] == 0x21:
                    if time.monotonic() >= deadline:
                        return uds
                    time.sleep(0.05)
                    self._pace()
                    self._send_frame(DOIP_PT_DIAG_MSG, payload)
                    continue
                return uds

    def send_uds_no_wait(self, request):
        payload = struct.pack(">HH", self.source_addr, self.target_addr) + bytes(request)
        with self.lock:
            self._send_frame(
                DOIP_PT_DIAG_MSG,
                payload,
                send_timeout=ROUTED_SEND_BACKPRESSURE_TIMEOUT_SECONDS,
            )
            self._last_request_ts = time.monotonic()

    def recv_routed_transport_acks(self, expected_requests, timeout=None):
        timeout = ROUTED_TRANSPORT_ACK_TIMEOUT_SECONDS if timeout is None else float(timeout)
        deadline = time.monotonic() + timeout
        pending = []
        for expected in expected_requests:
            label, request = expected[:2]
            tolerate_negative_ack = bool(expected[2]) if len(expected) > 2 else False
            pending.append({
                "label": str(label),
                "request": bytes(request),
                "tolerate_negative_ack": tolerate_negative_ack,
            })
        completed = []

        with self.lock:
            while pending:
                try:
                    payload_type, response = self._recv_frame(deadline)
                except TimeoutError as exc:
                    labels = ", ".join(item["label"] for item in pending[:4])
                    if len(pending) > 4:
                        labels += f", ... +{len(pending) - 4} more"
                    raise TimeoutError(f"Timed out waiting for ZGW routed transport ack: {labels}") from exc

                if payload_type == DOIP_PT_ALIVE_CHECK_REQ:
                    self._send_frame(DOIP_PT_ALIVE_CHECK_RES, struct.pack(">H", self.source_addr))
                    continue
                if payload_type == DOIP_PT_DIAG_ACK:
                    if len(response) >= 5 and response[4] != 0:
                        raise DoipError(f"Diagnostic ACK code 0x{response[4]:02X}")
                    continue
                if payload_type == DOIP_PT_DIAG_NACK:
                    code = response[4] if len(response) >= 5 else 0xFF
                    raise DoipError(f"Diagnostic NACK code 0x{code:02X}")
                if payload_type != DOIP_PT_DIAG_MSG:
                    continue
                if len(response) < 5:
                    raise DoipError("Short diagnostic message")

                source, target = struct.unpack(">HH", response[:4])
                uds = response[4:]
                if target != self.source_addr or source != self.target_addr:
                    continue

                matched_index = None
                for index, item in enumerate(pending):
                    request = item["request"]
                    if not request:
                        continue
                    if (
                        len(uds) >= 4
                        and len(request) >= 2
                        and uds[0] == request[0]
                        and uds[1] == 0x7F
                        and uds[2] == request[1]
                    ):
                        if item["tolerate_negative_ack"]:
                            matched_index = index
                            break
                        nrc_text = NRC_TEXT.get(uds[3], "Unknown")
                        raise FcdError(
                            f"{item['label']}: ZGW routed transport ack failed "
                            f"for 0x{uds[2]:02X}: NRC 0x{uds[3]:02X} ({nrc_text})"
                        )
                    if uds_response_matches_request(uds, request):
                        matched_index = index
                        break

                if matched_index is None:
                    continue

                completed.append((pending[matched_index]["label"], uds))
                pending.pop(matched_index)

        return completed

    def send_uds_suppress_positive(self, request, timeout=None):
        timeout = self.timeout if timeout is None else float(timeout)
        payload = struct.pack(">HH", self.source_addr, self.target_addr) + bytes(request)
        deadline = time.monotonic() + timeout
        ack_received = False

        with self.lock:
            self._pace()
            self._send_frame(DOIP_PT_DIAG_MSG, payload)
            while True:
                recv_deadline = deadline
                if ack_received:
                    recv_deadline = min(deadline, time.monotonic() + 0.050)

                try:
                    payload_type, response = self._recv_frame(recv_deadline)
                except TimeoutError:
                    if ack_received:
                        return None
                    raise

                if payload_type == DOIP_PT_ALIVE_CHECK_REQ:
                    self._send_frame(DOIP_PT_ALIVE_CHECK_RES, struct.pack(">H", self.source_addr))
                    continue
                if payload_type == DOIP_PT_DIAG_ACK:
                    if len(response) >= 5 and response[4] != 0:
                        raise DoipError(f"Diagnostic ACK code 0x{response[4]:02X}")
                    ack_received = True
                    continue
                if payload_type == DOIP_PT_DIAG_NACK:
                    code = response[4] if len(response) >= 5 else 0xFF
                    raise DoipError(f"Diagnostic NACK code 0x{code:02X}")
                if payload_type != DOIP_PT_DIAG_MSG:
                    continue
                if len(response) < 5:
                    raise DoipError("Short diagnostic message")

                source, target = struct.unpack(">HH", response[:4])
                uds = response[4:]
                if target != self.source_addr:
                    raise DoipError(f"Diagnostic response target 0x{target:04X} does not match tester")
                if source != self.target_addr:
                    raise DoipError(f"Diagnostic response source 0x{source:04X} does not match target")
                return uds

    @staticmethod
    def vehicle_identification(host, port=DEFAULT_PORT, timeout=2.0, local_ip="auto"):
        req = struct.pack(">BBHI", DOIP_PROTO_VER, DOIP_INV_PROTO_VER, DOIP_PT_VID_REQ, 0)
        host = str(host).strip()
        port = int(port)
        deadline = time.monotonic() + float(timeout)
        local_ips = local_ipv4_addresses()

        sockets = []

        def make_sock(bind_port=None, bind_ip=""):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setblocking(False)
            if bind_port is not None:
                try:
                    sock.bind((bind_ip, int(bind_port)))
                except OSError:
                    sock.bind((bind_ip, 0))
            else:
                sock.bind((bind_ip, 0))
            sockets.append(sock)
            return sock

        try:
            # Some DoIP stacks answer to the request source port, some lab stacks
            # are easier to test when the tester also listens on 13400. Use both.
            make_sock(port)
            for bind_ip in local_bind_candidates(host, local_ip):
                sock = make_sock(None, bind_ip)
                try:
                    sock.sendto(req, (host, port))
                except OSError:
                    pass

            while time.monotonic() < deadline:
                remaining = max(0.01, deadline - time.monotonic())
                readable, _, _ = select.select(sockets, [], [], remaining)
                if not readable:
                    continue

                for sock in readable:
                    while True:
                        try:
                            data, addr = sock.recvfrom(4096)
                        except BlockingIOError:
                            break
                        except OSError:
                            break

                        if addr[0] in local_ips:
                            continue

                        try:
                            return DoipClient._parse_vehicle_identification(data, addr)
                        except DoipError:
                            continue

            raise TimeoutError(f"No DoIP vehicle-identification response from {host}:{port}")

        finally:
            for sock in sockets:
                try:
                    sock.close()
                except OSError:
                    pass

    @staticmethod
    def discover(port=DEFAULT_PORT, timeout=2.0, extra_hosts=None, local_ip="auto", include_broadcast=True):
        req = struct.pack(">BBHI", DOIP_PROTO_VER, DOIP_INV_PROTO_VER, DOIP_PT_VID_REQ, 0)
        port = int(port)
        targets = []
        candidate_targets = [*(extra_hosts or []), "192.168.1.10"]
        if include_broadcast:
            candidate_targets.extend(["192.168.1.255", "255.255.255.255"])
        for target in candidate_targets:
            target = str(target).strip()
            if target and target not in targets:
                targets.append(target)

        local_ips = local_ipv4_addresses()
        found = {}
        sockets = []

        def make_sock(bind_port=None, bind_ip=""):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setblocking(False)
            if bind_port is not None:
                try:
                    sock.bind((bind_ip, int(bind_port)))
                except OSError:
                    sock.bind((bind_ip, 0))
            else:
                sock.bind((bind_ip, 0))
            sockets.append(sock)
            return sock

        try:
            make_sock(port)
            tx_sockets = []
            for bind_ip in local_bind_candidates(targets[0] if targets else DEFAULT_HOST, local_ip):
                tx_sockets.append(make_sock(None, bind_ip))

            for sock in tx_sockets:
                for target in targets:
                    try:
                        sock.sendto(req, (target, port))
                    except OSError:
                        continue

            deadline = time.monotonic() + float(timeout)
            while time.monotonic() < deadline:
                remaining = max(0.01, deadline - time.monotonic())
                readable, _, _ = select.select(sockets, [], [], remaining)
                if not readable:
                    continue

                for sock in readable:
                    while True:
                        try:
                            data, addr = sock.recvfrom(4096)
                        except BlockingIOError:
                            break
                        except OSError:
                            break

                        if addr[0] in local_ips:
                            continue

                        try:
                            info = DoipClient._parse_vehicle_identification(data, addr)
                            found[info["ip"]] = info
                        except DoipError:
                            continue

            return list(found.values())

        finally:
            for sock in sockets:
                try:
                    sock.close()
                except OSError:
                    pass

    @staticmethod
    def _parse_vehicle_identification(data, addr):
        if len(data) < 8:
            raise DoipError("Short DoIP vehicle identification response")
        proto, inv, payload_type, payload_len = struct.unpack(">BBHI", data[:8])
        if proto != DOIP_PROTO_VER or inv != DOIP_INV_PROTO_VER or payload_type != DOIP_PT_VID_RES:
            raise DoipError("Unexpected DoIP vehicle identification response")
        payload = data[8 : 8 + payload_len]
        if len(payload) < 31:
            raise DoipError("Short vehicle identification payload")
        vin = payload[:17].decode("ascii", errors="replace").strip("\x00 ")
        logical_addr = struct.unpack(">H", payload[17:19])[0]
        eid = payload[19:25].hex(":").upper()
        gid = payload[25:31].hex(":").upper()
        return {
            "from": f"{addr[0]}:{addr[1]}",
            "ip": addr[0],
            "vin": vin,
            "logical_address": logical_addr,
            "eid": eid,
            "gid": gid,
        }

    @staticmethod
    def ethernet_probe(host, doip_port=DEFAULT_PORT, timeout=3.0, local_ip="auto"):
        doip_req = struct.pack(">BBHI", DOIP_PROTO_VER, DOIP_INV_PROTO_VER, DOIP_PT_VID_REQ, 0)
        heartbeat = b"PCHeartbeat"
        doip_port = int(doip_port)

        targets = []
        for target in [host, "192.168.1.10", "192.168.1.255", "255.255.255.255"]:
            target = str(target).strip()
            if target and target not in targets:
                targets.append(target)

        local_ips = local_ipv4_addresses()
        sockets = []
        results = []

        def add_socket(bind_port=None, bind_ip=""):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setblocking(False)
            if bind_port is not None:
                try:
                    sock.bind((bind_ip, int(bind_port)))
                except OSError as exc:
                    results.append({
                        "kind": "bind-error",
                        "from": f"{bind_ip or '0.0.0.0'}:{bind_port}",
                        "detail": str(exc),
                    })
                    sock.bind((bind_ip, 0))
            else:
                sock.bind((bind_ip, 0))
            sockets.append(sock)
            return sock

        # Bind RX before TX. Keep one ephemeral socket too, because the ECU may reply
        # to the tester source port instead of fixed 13400.
        rx_ports = []
        for p in (doip_port, 30490, 30500, 30600):
            if p not in rx_ports:
                rx_ports.append(p)
        for p in rx_ports:
            add_socket(p)
        tx_sockets = []
        for bind_ip in local_bind_candidates(host or DEFAULT_HOST, local_ip):
            tx_sockets.append(add_socket(None, bind_ip))

        for target in targets:
            for tx_sock in tx_sockets:
                local = tx_sock.getsockname()[0]
                for port, payload in ((30600, heartbeat), (doip_port, heartbeat), (doip_port, doip_req)):
                    try:
                        tx_sock.sendto(payload, (target, int(port)))
                    except OSError as exc:
                        results.append({
                            "kind": "send-error",
                            "from": f"{local}->{target}:{port}",
                            "detail": str(exc),
                        })

        deadline = time.monotonic() + float(timeout)

        try:
            while time.monotonic() < deadline:
                remaining = max(0.01, deadline - time.monotonic())
                readable, _, _ = select.select(sockets, [], [], remaining)
                if not readable:
                    continue

                for sock in readable:
                    while True:
                        try:
                            data, addr = sock.recvfrom(4096)
                        except BlockingIOError:
                            break
                        except OSError:
                            break

                        if addr[0] in local_ips:
                            # Ignore our own broadcast packets. They previously looked like
                            # false DoIP 0x0001 responses in the log.
                            continue

                        item = {
                            "kind": "udp",
                            "from": f"{addr[0]}:{addr[1]}",
                            "ip": addr[0],
                            "length": len(data),
                            "data": bytes_to_hex(data[:32]),
                        }

                        if data == b"AURIXHeartbeatAck":
                            item["kind"] = "aurix-heartbeat"

                        elif len(data) >= 8:
                            proto, inv, payload_type, payload_len = struct.unpack(">BBHI", data[:8])
                            if proto == DOIP_PROTO_VER and inv == DOIP_INV_PROTO_VER:
                                item["kind"] = f"doip-0x{payload_type:04X}"
                                item["payload_len"] = payload_len
                                if payload_type == DOIP_PT_VID_RES:
                                    try:
                                        item.update(DoipClient._parse_vehicle_identification(data, addr))
                                    except DoipError as exc:
                                        item["detail"] = str(exc)

                        results.append(item)

        finally:
            for sock in sockets:
                try:
                    sock.close()
                except OSError:
                    pass

        return results


class RawTcpUdsClient:
    def __init__(self, host, port, timeout=3.0, local_ip="auto"):
        self.host = host
        self.port = int(port)
        self.timeout = float(timeout)
        self.local_ip = str(local_ip or "auto").strip()
        self.bound_local_ip = ""
        self.sock = None
        self.lock = threading.Lock()
        self._last_request_ts = 0.0
        self.request_spacing_seconds = REQUEST_SPACING_SECONDS

    @property
    def connected(self):
        return self.sock is not None

    def _pace(self):
        wait = self.request_spacing_seconds - (time.monotonic() - self._last_request_ts)
        if wait > 0:
            time.sleep(wait)
        self._last_request_ts = time.monotonic()

    def drain(self):
        with self.lock:
            if self.sock is None:
                return
            try:
                self.sock.setblocking(False)
                while True:
                    try:
                        if not self.sock.recv(4096):
                            break
                    except (BlockingIOError, OSError):
                        break
            finally:
                if self.sock is not None:
                    self.sock.setblocking(True)
                    self.sock.settimeout(self.timeout)

    def connect(self):
        self.close()
        last_error = None
        for bind_ip in local_bind_candidates(self.host, self.local_ip):
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                configure_low_latency_tcp(sock)
                sock.settimeout(self.timeout)
                if bind_ip:
                    sock.bind((bind_ip, 0))
                sock.connect((self.host, self.port))
                sock.settimeout(self.timeout)
                self.sock = sock
                self.bound_local_ip = bind_ip
                return
            except OSError as exc:
                last_error = exc
                try:
                    sock.close()
                except OSError:
                    pass
        if last_error is not None:
            raise last_error
        raise TimeoutError("No local TCP bind candidates available")

    def close(self):
        sock = self.sock
        self.sock = None
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass

    def send_uds(self, request, timeout=None, allow_no_response=False):
        if self.sock is None:
            raise FcdError("Not connected")
        timeout = self.timeout if timeout is None else float(timeout)
        deadline = time.monotonic() + timeout
        nrc78_count = 0
        with self.lock:
            self.last_nrc78_count = 0
            self._pace()
            self.sock.settimeout(timeout)
            self.sock.sendall(bytes(request))
            FCD_TRACE_MIRROR.emit_raw_uds("T", self.source_addr, self.target_addr, request)
            if allow_no_response:
                return b""
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError("Timed out waiting for UDS data")
                self.sock.settimeout(remaining)
                response = self.sock.recv(4096)
                if len(response) >= 3 and response[0] == 0x7F and response[2] == 0x78:
                    FCD_TRACE_MIRROR.emit_raw_uds("R", self.source_addr, self.target_addr, response)
                    nrc78_count += 1
                    self.last_nrc78_count = nrc78_count
                    deadline = time.monotonic() + timeout
                    continue
                FCD_TRACE_MIRROR.emit_raw_uds("R", self.source_addr, self.target_addr, response)
                return response


class FcdApp:
    def __init__(self, root):
        self.root = root
        self.root.title(APP_TITLE)
        self.root.geometry("1280x820")
        self.root.minsize(1020, 680)

        self.log_queue = queue.Queue()
        self.client = None
        self.keepalive_stop = threading.Event()
        self.keepalive_thread = None
        self.worker_stop = threading.Event()
        self.worker_lock = threading.Lock()
        self.worker_running = False
        self.test_stop = threading.Event()
        self.test_lock = threading.Lock()
        self.test_running = False
        self.discovery_lock = threading.Lock()
        self.discovery_running = False
        self.generator_rows = {}
        self.discovered_nodes = []
        self.discovery_logs = []
        self.node_rows = {}
        self.coding_ecu_rows = {}
        self.coding_target_tabs = {}
        self.package_dir = None
        self.package_bundle = None
        self.package_manifest = None
        self.package_payloads = []
        self.payload_enabled = {}
        self.routed_bus_pace_lock = threading.RLock()
        self.routed_bus_semaphores = {}
        self.routed_node_semaphores = {}
        self.routed_bus_last_request_ts = {}
        self.routed_node_last_request_ts = {}
        self.coding_shared_client = None
        self.progress_run_id = 0
        self.settings = self._load_settings_file()

        self._build_style()
        self._build_ui()
        self._apply_persisted_settings()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self.root.after(100, self._drain_log_queue)

    def _load_settings_file(self):
        try:
            if SETTINGS_PATH.exists():
                return json.loads(SETTINGS_PATH.read_text(encoding="utf-8"))
        except Exception:
            pass
        return {}

    def _apply_persisted_settings(self):
        connection = self.settings.get("connection", {})
        for key, var in [
            ("host", self.host_var),
            ("port", self.port_var),
            ("source", self.source_var),
            ("target", self.target_var),
            ("timeout", self.timeout_var),
            ("local_ip", self.local_ip_var),
        ]:
            if key in connection:
                var.set(str(connection[key]))
        data_editor = self.settings.get("data_editor", {})
        if data_editor.get("output_folder"):
            self.gen_output_var.set(str(data_editor["output_folder"]))
        for row in filtered_hex_rows_for_layout(data_editor.get("hex_rows", []), SCRIPT_DIR.parent.parent):
            if row.get("hex"):
                self._insert_generator_row({
                    "ecu": row.get("ecu", Path(row["hex"]).stem.upper()),
                    "target": row.get("target", int_hex(DEFAULT_TARGET_ADDR)),
                    "req": row.get("req", "0x710"),
                    "resp": row.get("resp", "0x711"),
                    "did": row.get("did", "0xF186"),
                    "base": row.get("base", ""),
                    "hex": row["hex"],
                })

    def _save_settings_file(self):
        data = {
            "schema": "FCD_SETTINGS_v1",
            "connection": {
                "host": self.host_var.get(),
                "port": self.port_var.get(),
                "source": self.source_var.get(),
                "target": self.target_var.get(),
                "timeout": self.timeout_var.get(),
                "local_ip": self.local_ip_var.get(),
            },
            "data_editor": {
                "output_folder": self.gen_output_var.get(),
                "nodes": {},
                "hex_rows": [],
            },
        }
        for iid, target in self.node_rows.items():
            data["data_editor"]["nodes"][target.get("node_name", "")] = {
                "selected": bool(target.get("simulated_enabled", True)),
                "node_kind": target.get("node_kind", "Simulated"),
                "extended_diag_address": target.get("extended_diag_address", ""),
            }
        for iid in self.generator_tree.get_children():
            row = self.generator_rows.get(iid)
            if row:
                data["data_editor"]["hex_rows"].append(row)
        SETTINGS_PATH.write_text(json.dumps(data, indent=2), encoding="utf-8")

    def _build_style(self):
        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("Treeview", rowheight=24)
        style.configure("TButton", padding=(8, 4))
        style.configure("Danger.TButton", foreground="#9B1C1C")
        style.configure("Good.TLabel", foreground="#0F766E")
        style.configure("Bad.TLabel", foreground="#B91C1C")
        style.configure(
            "Green.Horizontal.TProgressbar",
            background="#16A34A",
            troughcolor="#E5E7EB",
            bordercolor="#D1D5DB",
            lightcolor="#16A34A",
            darkcolor="#15803D",
        )

    def _build_ui(self):
        self.main_pane = ttk.PanedWindow(self.root, orient=tk.VERTICAL)
        self.main_pane.pack(fill="both", expand=True, padx=8, pady=8)

        self.upper_frame = ttk.Frame(self.main_pane)
        self.trace_frame = ttk.Frame(self.main_pane)
        self.main_pane.add(self.upper_frame, weight=1)
        self.main_pane.add(self.trace_frame, weight=1)

        self.notebook = ttk.Notebook(self.upper_frame)
        self.notebook.pack(fill="both", expand=True)

        self.connection_tab = ttk.Frame(self.notebook)
        self.diagnostics_tab = ttk.Frame(self.notebook)
        self.coding_tab = ttk.Frame(self.notebook)
        self.generator_tab = ttk.Frame(self.notebook)
        self.tal_tab = ttk.Frame(self.notebook)
        self.tests_tab = ttk.Frame(self.notebook)

        self.notebook.add(self.connection_tab, text="Connection")
        self.notebook.add(self.diagnostics_tab, text="Diagnostics")
        self.notebook.add(self.coding_tab, text="Coding")
        self.notebook.add(self.generator_tab, text="Data Editor")
        self.notebook.add(self.tal_tab, text="Flash")
        self.notebook.add(self.tests_tab, text="Tests")

        self._build_connection_tab()
        self._build_diagnostics_tab()
        self._build_coding_tab()
        self._build_generator_tab()
        self._build_tal_tab()
        self._build_tests_tab()
        self._build_trace_tab()
        self.root.after(100, self._set_trace_split)

    def _set_trace_split(self):
        try:
            height = self.main_pane.winfo_height()
            if height > 80:
                self.main_pane.sashpos(0, height // 2)
        except tk.TclError:
            pass

    def _entry_row(self, parent, row, label, variable, width=18, column=0):
        ttk.Label(parent, text=label).grid(row=row, column=column, sticky="w", padx=6, pady=4)
        entry = ttk.Entry(parent, textvariable=variable, width=width)
        entry.grid(row=row, column=column + 1, sticky="ew", padx=6, pady=4)
        return entry

    def _button_row(self, parent, row, buttons):
        frame = ttk.Frame(parent)
        frame.grid(row=row, column=0, columnspan=8, sticky="w", padx=4, pady=8)
        for text, command, style in buttons:
            ttk.Button(frame, text=text, command=command, style=style or "TButton").pack(side="left", padx=4)
        return frame

    def _build_connection_tab(self):
        outer = ttk.Frame(self.connection_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(1, weight=1)
        outer.columnconfigure(3, weight=1)

        self.host_var = tk.StringVar(value=DEFAULT_HOST)
        self.port_var = tk.StringVar(value=str(DEFAULT_PORT))
        self.source_var = tk.StringVar(value=int_hex(DEFAULT_SOURCE_ADDR))
        self.target_var = tk.StringVar(value=int_hex(DEFAULT_TARGET_ADDR))
        self.timeout_var = tk.StringVar(value="1.0")
        self.local_ip_var = tk.StringVar(value="auto")
        self.transport_var = tk.StringVar(value="DoIP")
        self.conn_status_var = tk.StringVar(value="Disconnected")

        self._entry_row(outer, 0, "ZGW host", self.host_var, 22, 0)
        self._entry_row(outer, 0, "TCP port", self.port_var, 10, 2)
        self._entry_row(outer, 1, "Tester address", self.source_var, 14, 0)
        self._entry_row(outer, 1, "Target address", self.target_var, 14, 2)
        self._entry_row(outer, 2, "PC local IP", self.local_ip_var, 22, 0)
        self._entry_row(outer, 2, "Timeout seconds", self.timeout_var, 10, 2)

        status = ttk.Label(outer, textvariable=self.conn_status_var, style="Bad.TLabel")
        status.grid(row=3, column=0, columnspan=4, sticky="w", padx=6, pady=8)
        self.connection_status_label = status

        self._button_row(
            outer,
            4,
            [
                ("Connect", self.connect_clicked, None),
                ("Disconnect", self.disconnect_clicked, None),
                ("Sync Time", self.sync_time_clicked, None),
            ],
        )

        self.keepalive_var = tk.BooleanVar(value=True)
        self.keepalive_period_var = tk.StringVar(value="2.0")

        node_frame = ttk.LabelFrame(outer, text="Vehicle nodes")
        node_frame.grid(row=5, column=0, columnspan=4, sticky="nsew", padx=4, pady=8)
        node_frame.columnconfigure(0, weight=1)
        node_frame.rowconfigure(0, weight=1)
        self.connection_node_tree = ttk.Treeview(
            node_frame,
            columns=("kind", "node", "bus", "extended"),
            show="headings",
            selectmode="browse",
            height=9,
        )
        for col, text, width in [
            ("kind", "Type", 110),
            ("node", "Node", 140),
            ("bus", "Bus / Database", 180),
            ("extended", "Extended diagnostic address", 220),
        ]:
            self.connection_node_tree.heading(col, text=text)
            self.connection_node_tree.column(col, width=width, stretch=(col == "bus"))
        self.connection_node_tree.grid(row=0, column=0, sticky="nsew")
        self.connection_node_tree.bind("<Button-1>", self.connection_node_tree_click)
        self.connection_node_tree.bind("<Double-1>", lambda _event: self.edit_connection_node_clicked())
        scroll = ttk.Scrollbar(node_frame, orient="vertical", command=self.connection_node_tree.yview)
        scroll.grid(row=0, column=1, sticky="ns")
        self.connection_node_tree.configure(yscrollcommand=scroll.set)

    def _build_diagnostics_tab(self):
        outer = ttk.Frame(self.diagnostics_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(1, weight=1)

        controls = ttk.LabelFrame(outer, text="Fault memory")
        controls.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        controls.columnconfigure(1, weight=1)

        self.dtc_status_mask_var = tk.StringVar(value="0x09")
        self._entry_row(controls, 0, "Status mask", self.dtc_status_mask_var, 10, 0)
        ttk.Button(controls, text="Read Fault Memory", command=self.read_fault_memory_clicked).grid(
            row=0, column=2, sticky="w", padx=4, pady=4
        )
        ttk.Button(controls, text="Clear Results", command=self.clear_dtc_results_clicked).grid(
            row=0, column=3, sticky="w", padx=4, pady=4
        )
        ttk.Button(controls, text="Clear DTCs", command=self.clear_diagnostic_information_clicked).grid(
            row=0, column=4, sticky="w", padx=4, pady=4
        )
        ttk.Button(controls, text="Hard Reset", command=self.hard_reset_clicked).grid(
            row=0, column=5, sticky="w", padx=4, pady=4
        )

        columns = ("node", "dtc", "status", "description", "snapshot_data")
        self.dtc_tree = ttk.Treeview(outer, columns=columns, show="headings", selectmode="browse")
        for col, text, width in [
            ("node", "Node", 110),
            ("dtc", "DTC", 100),
            ("status", "Status", 90),
            ("description", "DTC Name", 520),
            ("snapshot_data", "Snapshot Data", 960),
        ]:
            self.dtc_tree.heading(col, text=text)
            # Fixed widths (no stretch) so the columns keep their full size and the
            # horizontal scrollbar can pan across them instead of squeezing them to fit.
            self.dtc_tree.column(col, width=width, stretch=False)
        self.dtc_tree.grid(row=1, column=0, sticky="nsew", padx=(4, 0), pady=6)
        scroll = ttk.Scrollbar(outer, orient="vertical", command=self.dtc_tree.yview)
        scroll.grid(row=1, column=1, sticky="ns", padx=(0, 4), pady=6)
        hscroll = ttk.Scrollbar(outer, orient="horizontal", command=self.dtc_tree.xview)
        hscroll.grid(row=2, column=0, sticky="ew", padx=(4, 0))
        self.dtc_tree.configure(yscrollcommand=scroll.set, xscrollcommand=hscroll.set)
        # Treeview clips cell text at the column edge, so the long Snapshot Data
        # decodes are only partly visible inline. A hover tooltip shows the full text.
        self._attach_tree_tooltip(self.dtc_tree)

        self.dtc_summary_var = tk.StringVar(value="No fault memory read yet")

    def _attach_tree_tooltip(self, tree):
        """Show the full text of the cell under the cursor in a hover tooltip.
        ttk.Treeview otherwise clips cell content at the column boundary, which
        would hide most of the Snapshot Data plain-language decodes."""
        state = {"window": None, "cell": None}

        def hide(_event=None):
            if state["window"] is not None:
                state["window"].destroy()
                state["window"] = None
            state["cell"] = None

        def on_motion(event):
            row = tree.identify_row(event.y)
            col = tree.identify_column(event.x)
            if (not row) or (not col):
                hide()
                return
            try:
                col_index = int(col.replace("#", "")) - 1
            except ValueError:
                hide()
                return
            values = tree.item(row, "values")
            if col_index < 0 or col_index >= len(values):
                hide()
                return
            text = values[col_index]
            if text in ("", None):
                hide()
                return
            if state["cell"] == (row, col):
                return
            hide()
            state["cell"] = (row, col)
            window = tk.Toplevel(tree)
            window.wm_overrideredirect(True)
            window.wm_geometry(f"+{event.x_root + 14}+{event.y_root + 16}")
            tk.Label(
                window,
                text=str(text),
                justify="left",
                wraplength=760,
                background="#FFFFE0",
                relief="solid",
                borderwidth=1,
                font=("Segoe UI", 9),
            ).pack(ipadx=4, ipady=2)
            state["window"] = window

        tree.bind("<Motion>", on_motion, add="+")
        tree.bind("<Leave>", hide, add="+")

    def _build_svt_tab(self):
        outer = ttk.Frame(self.svt_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(1, weight=1)

        top = ttk.LabelFrame(outer, text="Vehicle Order")
        top.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        for col in range(6):
            top.columnconfigure(col, weight=1)

        self.fa_project_var = tk.StringVar(value="ZGW_LAB")
        self.fa_vin_var = tk.StringVar(value="LABTC375DOIP0001")
        self.fa_type_var = tk.StringVar(value="ZGW")
        self._entry_row(top, 0, "Project", self.fa_project_var, 18, 0)
        self._entry_row(top, 0, "VIN", self.fa_vin_var, 22, 2)
        self._entry_row(top, 1, "Type", self.fa_type_var, 12, 0)

        svt_frame = ttk.LabelFrame(outer, text="SVT / ECU List")
        svt_frame.grid(row=1, column=0, sticky="nsew", padx=4, pady=8)
        svt_frame.columnconfigure(0, weight=1)
        svt_frame.rowconfigure(0, weight=1)

        columns = ("ecu", "target", "req", "resp", "did", "hex", "size", "crc")
        self.svt_tree = ttk.Treeview(svt_frame, columns=columns, show="headings", selectmode="browse")
        for col, text, width in [
            ("use", "Use", 55),
            ("ecu", "ECU", 150),
            ("target", "Target", 90),
            ("req", "Req ID", 80),
            ("resp", "Resp ID", 80),
            ("did", "DID", 80),
            ("hex", "Source HEX", 360),
            ("size", "Size", 90),
            ("crc", "CRC32", 110),
        ]:
            self.svt_tree.heading(col, text=text)
            self.svt_tree.column(col, width=width, stretch=(col == "hex"))
        self.svt_tree.grid(row=0, column=0, sticky="nsew")
        svt_scroll = ttk.Scrollbar(svt_frame, orient="vertical", command=self.svt_tree.yview)
        svt_scroll.grid(row=0, column=1, sticky="ns")
        self.svt_tree.configure(yscrollcommand=svt_scroll.set)

        buttons = ttk.Frame(outer)
        buttons.grid(row=2, column=0, sticky="w", padx=4, pady=6)
        ttk.Button(buttons, text="Import SVT JSON", command=self.import_svt_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Export SVT JSON", command=self.export_svt_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Copy From Generator", command=self.copy_generator_to_svt).pack(side="left", padx=4)

    def _build_coding_tab(self):
        outer = ttk.Frame(self.coding_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(2, weight=1)

        self.coding_mask_did_var = tk.StringVar(value=int_hex(CODING_DID_RX_MESSAGE_EXPECTED))
        self.coding_image_did_var = tk.StringVar(value=int_hex(CODING_DID_IMAGE))
        self.coding_status_did_var = tk.StringVar(value=int_hex(CODING_DID_STATUS))
        self.coding_version_did_var = tk.StringVar(value=int_hex(CODING_DID_VERSION))
        self.coding_validate_rid_var = tk.StringVar(value=int_hex(CODING_ROUTINE_VALIDATE))
        self.coding_write_all_rid_var = tk.StringVar(value=int_hex(CODING_ROUTINE_WRITE_ALL))
        self.coding_read_nvm_rid_var = tk.StringVar(value=int_hex(CODING_ROUTINE_READ_NVM))
        self.coding_defaults_rid_var = tk.StringVar(value=int_hex(CODING_ROUTINE_LOAD_DEFAULTS))

        file_frame = ttk.LabelFrame(outer, text="Coding configuration")
        file_frame.grid(row=0, column=0, sticky="ew", padx=4, pady=8)

        rc_frame = ttk.Frame(file_frame)
        rc_frame.pack(side="left", padx=4, pady=6)
        ttk.Button(rc_frame, text="Code Vehicle", command=self.code_ecu_clicked).pack(side="left", padx=4)
        ttk.Button(rc_frame, text="Read Coding", command=self.read_current_coding_clicked).pack(side="left", padx=4)
        ttk.Button(rc_frame, text="Load Coding Default", command=self.load_default_coding_clicked).pack(side="left", padx=4)
        ttk.Button(rc_frame, text="Check Coding", command=self.check_coding_clicked).pack(side="left", padx=4)

        file_ops = ttk.Frame(file_frame)
        file_ops.pack(side="left", padx=16, pady=6)
        ttk.Button(file_ops, text="Save Current", command=self.save_current_coding_clicked).pack(side="left", padx=4)
        ttk.Button(file_ops, text="Import", command=self.import_coding_clicked).pack(side="left", padx=4)
        ttk.Button(file_ops, text="Export", command=self.export_coding_clicked).pack(side="left", padx=4)
        ttk.Button(file_ops, text="Add Parameter", command=self.add_coding_param_clicked).pack(side="left", padx=4)
        ttk.Button(file_ops, text="Edit Selected", command=self.edit_coding_param_clicked).pack(side="left", padx=4)
        ttk.Button(file_ops, text="Remove Selected", command=self.remove_coding_param_clicked).pack(side="left", padx=4)

        self.coding_target_notebook = ttk.Notebook(outer)
        self.coding_target_notebook.grid(row=2, column=0, sticky="nsew", padx=4, pady=4)
        self._ensure_coding_tab("ZGW")

    def _build_generator_tab(self):
        outer = ttk.Frame(self.generator_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(3, weight=1)

        meta = ttk.LabelFrame(outer, text="FCD Input File Generator")
        meta.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        for col in range(8):
            meta.columnconfigure(col, weight=1)

        self.fa_project_var = tk.StringVar(value="ZGW_LAB")
        self.fa_vin_var = tk.StringVar(value="LABTC375DOIP0001")
        self.fa_type_var = tk.StringVar(value="ZGW")
        self.gen_project_var = self.fa_project_var
        self.gen_vin_var = self.fa_vin_var
        self.gen_output_var = tk.StringVar(value=str(SCRIPT_DIR / "Generated"))
        self._entry_row(meta, 0, "VIN", self.gen_vin_var, 22, 0)
        self._entry_row(meta, 1, "Output folder", self.gen_output_var, 80, 0)
        ttk.Button(meta, text="Browse", command=self.browse_gen_output_clicked).grid(row=1, column=2, padx=4, pady=4)
        buttons = ttk.Frame(outer)
        buttons.grid(row=1, column=0, sticky="w", padx=4, pady=6)
        ttk.Button(buttons, text="Discover Nodes", command=self.discover_nodes_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Add HEX", command=self.add_hex_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Remove Selected", command=self.remove_hex_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Move Up", command=lambda: self.move_hex_clicked(-1)).pack(side="left", padx=4)
        ttk.Button(buttons, text="Move Down", command=lambda: self.move_hex_clicked(1)).pack(side="left", padx=4)
        ttk.Button(buttons, text="Generate Bundle", command=self.generate_files_clicked).pack(side="left", padx=12)

        node_frame = ttk.LabelFrame(outer, text="Discovered nodes")
        node_frame.grid(row=2, column=0, sticky="nsew", padx=4, pady=4)
        node_frame.columnconfigure(0, weight=1)
        node_frame.rowconfigure(0, weight=1)
        node_columns = ("selected", "node", "bus", "extended")
        self.node_tree = ttk.Treeview(node_frame, columns=node_columns, show="headings", selectmode="browse", height=7)
        for col, text, width in [
            ("selected", "Selection", 85),
            ("node", "Node", 130),
            ("bus", "Bus / Database", 170),
            ("extended", "Extended diagnostic address", 190),
        ]:
            self.node_tree.heading(col, text=text)
            self.node_tree.column(col, width=width, stretch=(col == "bus"))
        self.node_tree.grid(row=0, column=0, sticky="nsew")
        self.node_tree.bind("<Button-1>", self.node_tree_click)
        self.node_tree.bind("<Double-1>", lambda _event: self.edit_node_extended_address_clicked())
        node_scroll = ttk.Scrollbar(node_frame, orient="vertical", command=self.node_tree.yview)
        node_scroll.grid(row=0, column=1, sticky="ns")
        self.node_tree.configure(yscrollcommand=node_scroll.set)

        columns = ("ecu", "hex")
        self.generator_tree = ttk.Treeview(outer, columns=columns, show="headings", selectmode="extended")
        for col, text, width in [
            ("ecu", "ECU", 160),
            ("hex", "HEX File", 520),
        ]:
            self.generator_tree.heading(col, text=text)
            self.generator_tree.column(col, width=width, stretch=(col == "hex"))
        self.generator_tree.grid(row=3, column=0, sticky="nsew", padx=4, pady=4)
        self.root.after(200, self.discover_nodes_startup)

    def _build_tal_tab(self):
        outer = ttk.Frame(self.tal_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(2, weight=1)

        load = ttk.LabelFrame(outer, text="Parallel flash/coding bundle")
        load.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        for col in range(8):
            load.columnconfigure(col, weight=1)

        self.pkg_dir_var = tk.StringVar(value="")
        self._entry_row(load, 0, "Bundle file or legacy folder", self.pkg_dir_var, 85, 0)
        ttk.Button(load, text="Browse", command=self.browse_package_clicked).grid(row=0, column=2, padx=4, pady=4)
        ttk.Button(load, text="Load", command=self.load_package_clicked).grid(row=0, column=3, padx=4, pady=4)

        options = ttk.LabelFrame(outer, text="Flash execution options")
        options.grid(row=1, column=0, sticky="ew", padx=4, pady=8)
        for col in range(10):
            options.columnconfigure(col, weight=1)

        self.erase_var = tk.BooleanVar(value=True)
        self.transfer_crc_var = tk.BooleanVar(value=True)
        self.verify_crc_var = tk.BooleanVar(value=True)
        self.flash_fbl_var = tk.BooleanVar(value=True)
        self.flash_appl_var = tk.BooleanVar(value=True)
        self.flash_coding_var = tk.BooleanVar(value=True)
        self.block_size_var = tk.StringVar(value=str(DEFAULT_BLOCK_SIZE))
        self.fbl_erase_timeout_var = tk.StringVar(value=str(FBL_ERASE_TIMEOUT_SECONDS))
        self.session_var = tk.StringVar(value="0x02")

        self._entry_row(options, 1, "Block size", self.block_size_var, 10, 0)

        columns = ("item", "value")
        self.payload_tree = ttk.Treeview(outer, columns=columns, show="headings", selectmode="browse")
        for col, text, width in [
            ("item", "Summary", 220),
            ("value", "Value", 820),
        ]:
            self.payload_tree.heading(col, text=text)
            self.payload_tree.column(col, width=width, stretch=(col == "value"))
        self.payload_tree.grid(row=2, column=0, sticky="nsew", padx=4, pady=4)

        actions = ttk.Frame(outer)
        actions.grid(row=3, column=0, sticky="ew", padx=4, pady=8)
        ttk.Button(actions, text="Start Flashing", command=self.execute_selected_clicked, style="Danger.TButton").pack(
            side="left", padx=4
        )
        self.progress = ttk.Progressbar(
            actions,
            mode="determinate",
            length=300,
            style="Green.Horizontal.TProgressbar",
        )
        self.progress.pack(side="left", padx=18)
        self.elapsed_var = tk.StringVar(value="Elapsed: 00:00")
        ttk.Label(actions, textvariable=self.elapsed_var).pack(side="left", padx=8)

    def _build_tests_tab(self):
        outer = ttk.Frame(self.tests_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)

        self.test_code_iterations_var = tk.StringVar(value="1")
        self.test_flash_iterations_var = tk.StringVar(value="1")
        self.test_fault_iterations_var = tk.StringVar(value="1")
        self.test_status_var = tk.StringVar(value="No test running")

        controls = ttk.LabelFrame(outer, text="Looped tests")
        controls.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        for col in range(4):
            controls.columnconfigure(col, weight=1 if col == 1 else 0)

        self._test_row(
            controls,
            0,
            "Coding Test",
            self.test_code_iterations_var,
            "Code Vehicle",
            self.start_coding_test_clicked,
        )
        self._test_row(
            controls,
            1,
            "Flashing Test",
            self.test_flash_iterations_var,
            "Start Flashing",
            self.start_flashing_test_clicked,
        )
        self._test_row(
            controls,
            2,
            "Fault Memory Test",
            self.test_fault_iterations_var,
            "Read Fault Memory",
            self.start_fault_memory_test_clicked,
        )

        status = ttk.Frame(outer)
        status.grid(row=1, column=0, sticky="ew", padx=4, pady=10)
        ttk.Button(status, text="Stop Test", command=self.stop_tests_clicked).pack(side="left", padx=4)
        ttk.Label(status, textvariable=self.test_status_var).pack(side="left", padx=12)

    def _test_row(self, parent, row, title, iterations_var, button_text, command):
        ttk.Label(parent, text=title).grid(row=row, column=0, sticky="w", padx=6, pady=5)
        ttk.Label(parent, text="Iterations").grid(row=row, column=1, sticky="e", padx=6, pady=5)
        ttk.Entry(parent, textvariable=iterations_var, width=10).grid(row=row, column=2, sticky="w", padx=6, pady=5)
        ttk.Button(parent, text=button_text, command=command).grid(row=row, column=3, sticky="w", padx=6, pady=5)

    def _parse_test_iterations(self, variable):
        iterations = parse_int(variable.get())
        if iterations < 1:
            raise ValueError("Iterations must be at least 1")
        return iterations

    def _set_test_status(self, text):
        self.root.after(0, lambda: self.test_status_var.set(text))

    def _begin_test(self, test_name):
        with self.test_lock:
            if self.test_running:
                return False
            self.test_running = True
        self.test_stop.clear()
        self._set_test_status(f"{test_name}: starting")
        return True

    def _finish_test(self):
        with self.test_lock:
            self.test_running = False

    def _start_test_loop(self, test_name, iterations_var, run_once):
        try:
            iterations = self._parse_test_iterations(iterations_var)
        except Exception as exc:
            messagebox.showerror(APP_NAME, str(exc))
            return

        if not self._begin_test(test_name):
            messagebox.showerror(APP_NAME, "A test is already running")
            return

        def action():
            completed = 0
            try:
                for iteration in range(1, iterations + 1):
                    if self.test_stop.is_set() or self.worker_stop.is_set():
                        raise FcdError(f"{test_name}: stopped before iteration {iteration}")
                    label = f"{test_name}: iteration {iteration}/{iterations}"
                    self._set_test_status(label)
                    self.log(f"{label} started")
                    self._assert_zgw_responds(f"{label} pre-check")
                    run_once()
                    self._assert_zgw_responds(f"{label} post-check")
                    completed = iteration
                    self.log(f"{label} passed")
                self._set_test_status(f"{test_name}: completed {completed}/{iterations}")
            except Exception as exc:
                self._set_test_status(f"{test_name}: stopped after {completed}/{iterations} ({exc})")
                raise
            finally:
                self._finish_test()

        self.worker(test_name, action)

    def _assert_zgw_responds(self, label):
        created_client = False
        try:
            client = self.require_client()
        except FcdError:
            if self.transport_var.get() != "DoIP":
                raise
            client = self._new_parallel_client(parse_int(self.target_var.get() or int_hex(DEFAULT_TARGET_ADDR)))
            created_client = True
        try:
            send = self._make_uds_sender(client, dry=False)
            send(b"\x3E\x00", f"{label}: ZGW TesterPresent", timeout=max(2.0, float(self.timeout_var.get())))
        finally:
            if created_client:
                client.close()

    def stop_tests_clicked(self):
        self.test_stop.set()
        self.worker_stop.set()
        self._set_test_status("Stopping test")
        self.log("Tests: stop requested")

    def start_coding_test_clicked(self):
        self._start_test_loop("Coding Test", self.test_code_iterations_var, self._run_coding_test_once)

    def start_flashing_test_clicked(self):
        self._start_test_loop("Flashing Test", self.test_flash_iterations_var, self._run_flashing_test_once)

    def start_fault_memory_test_clicked(self):
        self._start_test_loop("Fault Memory Test", self.test_fault_iterations_var, self._run_fault_memory_test_once)

    def _build_raw_tab(self):
        outer = ttk.Frame(self.raw_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(1, weight=1)

        top = ttk.Frame(outer)
        top.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        top.columnconfigure(1, weight=1)
        ttk.Label(top, text="UDS request bytes").grid(row=0, column=0, sticky="w", padx=4)
        self.raw_request_var = tk.StringVar(value="10 03")
        ttk.Entry(top, textvariable=self.raw_request_var).grid(row=0, column=1, sticky="ew", padx=4)
        ttk.Button(top, text="Send", command=self.raw_send_clicked).grid(row=0, column=2, padx=4)

        presets = ttk.Frame(outer)
        presets.grid(row=2, column=0, sticky="w", padx=4, pady=8)
        for label, data in [
            ("10 01", "10 01"),
            ("10 02", "10 02"),
            ("10 03", "10 03"),
            ("22 F1 86", "22 F1 86"),
            ("27 01", "27 01"),
            ("3E 00", "3E 00"),
            ("85 02", "85 02"),
        ]:
            ttk.Button(presets, text=label, command=lambda value=data: self.raw_request_var.set(value)).pack(
                side="left", padx=3
            )

        self.raw_response_text = tk.Text(outer, height=18, wrap="word", font=("Consolas", 10))
        self.raw_response_text.grid(row=1, column=0, sticky="nsew", padx=4, pady=6)

    def _build_trace_tab(self):
        outer = ttk.LabelFrame(self.trace_frame, text="Trace")
        outer.pack(fill="both", expand=True, padx=2, pady=(8, 2))
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(0, weight=1)

        self.trace_text = tk.Text(outer, wrap="word", font=("Consolas", 10))
        self.trace_text.grid(row=0, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(outer, orient="vertical", command=self.trace_text.yview)
        scroll.grid(row=0, column=1, sticky="ns")
        self.trace_text.configure(yscrollcommand=scroll.set)

        bottom = ttk.Frame(outer)
        bottom.grid(row=1, column=0, columnspan=2, sticky="w", pady=6)
        ttk.Button(bottom, text="Clear Trace", command=lambda: self.trace_text.delete("1.0", "end")).pack(side="left", padx=4)
        ttk.Button(bottom, text="Save Trace", command=self.save_trace_clicked).pack(side="left", padx=4)
        self.trace_autoscroll_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(bottom, text="Auto-scroll", variable=self.trace_autoscroll_var).pack(side="left", padx=12)

    def log(self, message):
        self.log_queue.put(f"[{now_text()}] {message}")

    def _drain_log_queue(self):
        drained = 0
        try:
            while drained < TRACE_DRAIN_MAX_LINES:
                line = self.log_queue.get_nowait()
                self.trace_text.insert("end", line + "\n")
                drained += 1
        except queue.Empty:
            pass
        if drained and self.trace_autoscroll_var.get():
            self.trace_text.see("end")
        self.root.after(10 if drained else 50, self._drain_log_queue)

    def worker(self, name, func):
        with self.worker_lock:
            if self.worker_running:
                self.log(f"{name}: skipped; another operation is already running")
                return
            self.worker_running = True

        def run():
            try:
                self.worker_stop.clear()
                self.log(f"{name}: started")
                func()
                self.log(f"{name}: done")
            except FcdError as exc:
                self.log(f"{name}: ERROR: {exc}")
            except Exception as exc:
                self.log(f"{name}: ERROR: {exc}")
                self.log(traceback.format_exc().strip())
            finally:
                with self.worker_lock:
                    self.worker_running = False

        threading.Thread(target=run, daemon=True).start()

    def make_client_from_fields(self):
        host = self.host_var.get().strip()
        port = parse_int(self.port_var.get())
        timeout = float(self.timeout_var.get())
        local_ip = self.local_ip_var.get().strip() or "auto"
        if self.transport_var.get() == "DoIP":
            return DoipClient(
                host=host,
                port=port,
                source_addr=parse_int(self.source_var.get()),
                target_addr=parse_int(self.target_var.get()),
                timeout=timeout,
                local_ip=local_ip,
            )
        return RawTcpUdsClient(host=host, port=port, timeout=timeout, local_ip=local_ip)

    def activate_doip(self, client):
        if isinstance(client, DoipClient):
            ecu, code = client.routing_activation()
            self.log(f"Routing activation OK: ecu={int_hex(ecu)} code=0x{code:02X}")

    def _reconnect_cancelled(self, client, stop_event=None, require_current_client=True):
        if self.worker_stop.is_set():
            return True
        if stop_event is not None and stop_event.is_set():
            return True
        return require_current_client and self.client is not client

    def reconnect_doip(self, client, reason, stop_event=None, log_success=True, deadline=None, require_current_client=True):
        if not isinstance(client, DoipClient):
            return

        timeout = max(1.0, float(self.timeout_var.get()))
        last_error = None
        attempt = 0

        if log_success:
            self.log(
                f"DoIP reconnect after {reason}: waiting for TCP {client.host}:{client.port} "
                "until Disconnect"
            )
        client.close()
        if require_current_client:
            self.root.after(0, self._set_connected_status, False)
        if self._reconnect_cancelled(client, stop_event, require_current_client=require_current_client):
            raise FcdError(f"DoIP reconnect after {reason} cancelled by Disconnect button")

        while not self._reconnect_cancelled(client, stop_event, require_current_client=require_current_client):
            if (deadline is not None) and (time.monotonic() >= deadline):
                break

            attempt += 1
            try:
                client.connect()
                if self._reconnect_cancelled(client, stop_event, require_current_client=require_current_client):
                    client.close()
                    break
                self.activate_doip(client)
                if self._reconnect_cancelled(client, stop_event, require_current_client=require_current_client):
                    client.close()
                    break
                if require_current_client:
                    self.root.after(0, self._set_connected_status, True)
                if log_success:
                    self.log(f"DoIP reconnected after {reason}")
                return
            except (OSError, TimeoutError, DoipError) as exc:
                last_error = exc
                client.close()
                if attempt == 1 or attempt % 10 == 0:
                    self.log(f"DoIP reconnect after {reason}: attempt {attempt} failed: {exc}")
                if self._reconnect_cancelled(client, stop_event, require_current_client=require_current_client):
                    break
                wait_time = 0.5
                if deadline is not None:
                    wait_time = min(wait_time, max(0.0, deadline - time.monotonic()))
                    if wait_time <= 0.0:
                        break
                if stop_event is not None:
                    if stop_event.wait(wait_time):
                        break
                elif self.worker_stop.wait(wait_time):
                    break

        if require_current_client:
            self.root.after(0, self._set_connected_status, False)
        if (deadline is not None) and (time.monotonic() >= deadline):
            raise FcdError(f"DoIP reconnect after {reason} timed out: {last_error}")
        if last_error is None:
            raise FcdError(f"DoIP reconnect after {reason} cancelled by Disconnect button")
        raise FcdError(f"DoIP reconnect after {reason} cancelled by Disconnect button: {last_error}")

    def _send_uds_with_reconnect(self, client, request, label, timeout=None, allow_no_response=False, max_retries=1):
        request = bytes(request)
        retries = 0
        # TransferData may be retried with the same BSC after a transport drop;
        # RequestDownload/TransferExit and flash routines change wider ECU state.
        no_auto_retry = request[:1] in (b"\x34", b"\x37")
        if (
            len(request) >= 4
            and request[0] == 0x31
            and request[1] == 0x01
            and struct.unpack(">H", request[2:4])[0] in (ROUTINE_ERASE_MEMORY, ROUTINE_CHECK_MEMORY_CRC)
        ):
            no_auto_retry = True
        while True:
            require_current_client = self.client is client
            if self._reconnect_cancelled(client, require_current_client=require_current_client):
                raise FcdError(f"{label}: cancelled by Disconnect button")
            try:
                return client.send_uds(request, timeout=timeout, allow_no_response=allow_no_response)
            except (OSError, TimeoutError, DoipError) as exc:
                if allow_no_response:
                    return b""
                if no_auto_retry or not isinstance(client, DoipClient) or retries >= max_retries:
                    raise
                retries += 1
                self.log(f"{label}: DoIP request failed ({exc}); reconnecting and retrying")
                self.reconnect_doip(client, label, require_current_client=require_current_client)
                self.log(f"TX {label} retry {retries}/{max_retries}: {uds_request_log_text(request)}")

    def connect_clicked(self):
        def action():
            # One button sequence: Discover DoIP -> Vehicle Identification -> TCP Connect + Routing Activation.
            # Logging intentionally keeps the three phases visible.
            host_before = self.host_var.get().strip()
            port = parse_int(self.port_var.get())
            timeout = float(self.timeout_var.get())
            local_ip = self.local_ip_var.get().strip() or "auto"
            selected_host = host_before
            selected_target = parse_int(self.target_var.get())
            self.log(
                "Local bind candidates: "
                + ", ".join(ip or "OS default" for ip in local_bind_candidates(host_before, local_ip))
            )

            self.log("Discover DoIP: started")
            try:
                results = DoipClient.discover(
                    port=port,
                    timeout=timeout,
                    extra_hosts=[host_before],
                    local_ip=local_ip,
                    include_broadcast=False,
                )
                if results:
                    for info in results:
                        self.log(
                            "DoIP discovered: "
                            f"ip={info['ip']} vin={info['vin']} logical={int_hex(info['logical_address'])} "
                            f"eid={info['eid']} gid={info['gid']}"
                        )
                    first = results[0]
                    selected_host = first["ip"]
                    selected_target = first["logical_address"]
                    self.root.after(0, lambda: self.host_var.set(first["ip"]))
                    self.root.after(0, lambda: self.target_var.set(int_hex(first["logical_address"])))
                    self.root.after(0, lambda: self.fa_vin_var.set(first["vin"] or self.fa_vin_var.get()))
                else:
                    self.log("Discover DoIP: no response; continuing with configured host")
            except Exception as exc:
                self.log(f"Discover DoIP: ERROR: {exc}; continuing with configured host")
            self.log("Discover DoIP: done")

            selected_host = selected_host or self.host_var.get().strip() or host_before
            if results:
                self.log("Vehicle Identification: skipped; using discovery response")
            else:
                self.log("Vehicle Identification: started")
                try:
                    info = DoipClient.vehicle_identification(selected_host, port=port, timeout=timeout, local_ip=local_ip)
                    self.log(
                        f"Vehicle ID: from={info['ip']}:13400 vin={info['vin']} "
                        f"logical={int_hex(info['logical_address'])} eid={info['eid']} gid={info['gid']}"
                    )
                    selected_host = info["ip"]
                    selected_target = info["logical_address"]
                    self.root.after(0, lambda: self.host_var.set(info["ip"]))
                    self.root.after(0, lambda: self.target_var.set(int_hex(info["logical_address"])))
                    if info.get("vin"):
                        self.root.after(0, lambda: self.fa_vin_var.set(info["vin"]))
                except Exception as exc:
                    self.log(f"Vehicle Identification: ERROR: {exc}; continuing with TCP connect")
                self.log("Vehicle Identification: done")

            # Close any existing client before opening a new one.
            self.stop_keepalive(update_var=False)
            if self.client is not None:
                self.client.close()
                self.client = None
                self.root.after(0, self._set_connected_status, False)

            client = self.make_client_from_fields()
            client.host = selected_host
            if isinstance(client, DoipClient):
                client.target_addr = selected_target
            for attempt in range(2):
                try:
                    client.connect()
                    break
                except TimeoutError as exc:
                    client.close()
                    if attempt == 0:
                        self.log("TCP connect timed out; retrying once after ARP warm-up")
                        time.sleep(0.75)
                        continue
                    raise FcdError(
                        "TCP connect timed out. No TCP listener answered at that IP:port. "
                        "The target may have a stale DoIP TCP socket, or the TCP listener is closed."
                    ) from exc
                except OSError as exc:
                    raise FcdError(f"TCP connect failed: {exc}") from exc

            try:
                self.activate_doip(client)
            except TimeoutError as exc:
                client.close()
                raise FcdError("DoIP routing activation timed out after TCP connect") from exc
            except DoipError as exc:
                client.close()
                raise FcdError(f"DoIP routing activation failed after TCP connect: {exc}") from exc

            self.client = client
            self.root.after(0, self._set_connected_status, True)
            bound = getattr(client, "bound_local_ip", "") or "OS default"
            self.log(
                f"Connected to {self.host_var.get()}:{self.port_var.get()} using "
                f"{self.transport_var.get()} from {bound}"
            )
            if self.keepalive_var.get():
                self.start_keepalive()

        self.worker("Connect", action)

    def discover_doip_clicked(self):
        def action():
            local_ip = self.local_ip_var.get().strip() or "auto"
            self.log(
                "Local bind candidates: "
                + ", ".join(ip or "OS default" for ip in local_bind_candidates(self.host_var.get().strip(), local_ip))
            )
            results = DoipClient.discover(
                port=parse_int(self.port_var.get()),
                timeout=float(self.timeout_var.get()),
                extra_hosts=[self.host_var.get().strip(), "192.168.1.1", "192.168.1.10"],
                local_ip=local_ip,
            )
            if not results:
                raise FcdError(
                    "No DoIP vehicle-identification response received on UDP 13400. "
                    "That usually means the target is not running the DoIP/FBL server, "
                    "the PC is on the wrong interface/subnet, or firewall/broadcast filtering is in the way."
                )
            for info in results:
                self.log(
                    "DoIP discovered: "
                    f"ip={info['ip']} vin={info['vin']} logical={int_hex(info['logical_address'])} "
                    f"eid={info['eid']} gid={info['gid']}"
                )
            first = results[0]
            self.root.after(0, lambda: self.host_var.set(first["ip"]))
            self.root.after(0, lambda: self.target_var.set(int_hex(first["logical_address"])))
            self.root.after(0, lambda: self.fa_vin_var.set(first["vin"] or self.fa_vin_var.get()))

        self.worker("Discover DoIP", action)

    def ethernet_probe_clicked(self):
        def action():
            local_ip = self.local_ip_var.get().strip() or "auto"
            self.log(
                "Local bind candidates: "
                + ", ".join(ip or "OS default" for ip in local_bind_candidates(self.host_var.get().strip(), local_ip))
            )
            results = DoipClient.ethernet_probe(
                self.host_var.get().strip(),
                doip_port=parse_int(self.port_var.get()),
                timeout=float(self.timeout_var.get()),
                local_ip=local_ip,
            )

            probe_reply_count = 0
            generic_udp_count = 0
            generic_udp_samples = []
            for item in results:
                kind = item.get("kind", "udp")
                origin = item.get("from", "?")

                if kind in ("bind-error", "send-error"):
                    self.log(f"Probe {kind}: {origin} {item.get('detail', '')}".rstrip())
                    continue

                if kind == "aurix-heartbeat":
                    probe_reply_count += 1
                    self.log(f"Probe RX: AURIX heartbeat from {origin}")
                elif kind == f"doip-0x{DOIP_PT_VID_RES:04X}":
                    probe_reply_count += 1
                    self.log(
                        "Probe RX: DoIP Vehicle ID "
                        f"from={origin} vin={item.get('vin', '')} "
                        f"logical={int_hex(item.get('logical_address', 0))} "
                        f"eid={item.get('eid', '')} gid={item.get('gid', '')}"
                    )
                    self.root.after(0, lambda value=item["ip"]: self.host_var.set(value))
                    self.root.after(0, lambda value=item["logical_address"]: self.target_var.set(int_hex(value)))
                    if item.get("vin"):
                        self.root.after(0, lambda value=item["vin"]: self.fa_vin_var.set(value))
                else:
                    generic_udp_count += 1
                    if len(generic_udp_samples) < 5:
                        generic_udp_samples.append(
                            f"{kind} from={origin} len={item.get('length', 0)} data={item.get('data', '')}"
                        )

            for sample in generic_udp_samples:
                self.log(f"Probe RX: {sample}")
            if generic_udp_count > len(generic_udp_samples):
                self.log(
                    f"Probe RX: {generic_udp_count - len(generic_udp_samples)} additional non-DoIP UDP frames suppressed"
                )

            if probe_reply_count == 0:
                raise FcdError(
                    f"No DoIP or heartbeat response to this probe. Received {generic_udp_count} background UDP "
                    "frame(s), so PC RX sees the ZGW, but the ZGW did not answer FCD traffic. "
                    "If ping reports 'Destination host unreachable' from the PC address, fix the ZGW ARP/RX path "
                    "before retrying TCP DoIP."
                )

        self.worker("Probe Ethernet", action)

    def _set_connected_status(self, connected):
        if connected:
            self.conn_status_var.set("Connected")
            self.connection_status_label.configure(style="Good.TLabel")
        else:
            self.conn_status_var.set("Disconnected")
            self.connection_status_label.configure(style="Bad.TLabel")

    def disconnect_clicked(self):
        self.test_stop.set()
        self.worker_stop.set()
        self.stop_keepalive()
        with self.worker_lock:
            operation_running = self.worker_running
        if operation_running:
            self.log("Disconnect requested; waiting for active operation to stop")
            return
        if self.client is not None:
            self.client.close()
            self.client = None
        self._set_connected_status(False)
        self.log("Disconnected")

    def require_client(self):
        if self.client is None or not self.client.connected:
            raise FcdError("Connect first")
        return self.client

    def routing_activation_clicked(self):
        def action():
            client = self.require_client()
            if not isinstance(client, DoipClient):
                raise FcdError("Routing activation is only for DoIP")
            ecu, code = client.routing_activation()
            self.log(f"Routing active: ECU 0x{ecu:04X}, code 0x{code:02X}")

        self.worker("Routing Activation", action)

    def alive_check_clicked(self):
        def action():
            client = self.require_client()
            if not isinstance(client, DoipClient):
                raise FcdError("Alive check is only for DoIP")
            ecu = client.alive_check()
            self.log(f"Alive response from ECU 0x{ecu:04X}")

        self.worker("Alive Check", action)

    def _log_time_sync_status(self, response, prefix):
        if not isinstance(response, bytes) or len(response) < 44:
            self.log(f"{prefix}: Time status raw: {bytes_to_hex(response) if isinstance(response, bytes) else response}")
            return

        data = response[4:]
        vehicle_ns = int.from_bytes(data[0:8], "big")
        utc_ns = int.from_bytes(data[8:16], "big")
        utc_valid = data[16]
        source = data[17]
        sync_status = int.from_bytes(data[18:22], "big")
        utc_dt = datetime.utcfromtimestamp(utc_ns / 1_000_000_000)
        self.log(
            f"{prefix}: Time status utc={utc_dt.isoformat(timespec='milliseconds')}Z "
            f"utcNs={utc_ns} vehicleNs={vehicle_ns} valid={utc_valid} "
            f"source={source} syncStatus=0x{sync_status:08X}"
        )

    def sync_time_clicked(self):
        def action():
            client = self.require_client()
            keepalive_was_on = bool(self.keepalive_var.get()) or self.keepalive_is_running()
            if keepalive_was_on:
                self.stop_keepalive(update_var=False)
                self.log("Sync Time: paused automatic tester present")

            try:
                send = self._make_uds_sender(client, dry=False)
                self._send_session(send, SESSION_EXTENDED, "Extended Session for time sync")
                pc_now = datetime.now()
                pc_ms = pc_now.microsecond // 1000
                request_payload = struct.pack(
                    ">HBBBBBHhB",
                    pc_now.year,
                    pc_now.month,
                    pc_now.day,
                    pc_now.hour,
                    pc_now.minute,
                    pc_now.second,
                    pc_ms,
                    0,
                    0,
                )
                request = b"\x31\x01" + struct.pack(">H", TIMESYNC_ROUTINE_SET_UTC) + request_payload
                response = send(
                    request,
                    f"RoutineControl F190 Set ZGW time from PC local {pc_now.isoformat(timespec='milliseconds')}",
                    timeout=5.0,
                )
                self._log_time_sync_status(response, "Sync Time")
                status = send(
                    b"\x31\x03" + struct.pack(">H", TIMESYNC_ROUTINE_GET_STATUS),
                    "RoutineControl F191 Time Status",
                    timeout=5.0,
                )
                self._log_time_sync_status(status, "Sync Time")
            finally:
                if keepalive_was_on and self.client is not None and self.client.connected:
                    self.start_keepalive()

        self.worker("Sync Time", action)

    def vehicle_id_clicked(self):
        def action():
            try:
                info = DoipClient.vehicle_identification(
                    self.host_var.get().strip(),
                    parse_int(self.port_var.get()),
                    float(self.timeout_var.get()),
                    local_ip=self.local_ip_var.get().strip() or "auto",
                )
            except TimeoutError as exc:
                raise FcdError(
                    "No DoIP vehicle-identification response from the selected host. "
                    "If the target is sending other Ethernet frames, APP/FBL is still not exposing DoIP UDP 13400."
                ) from exc
            self.log(
                "Vehicle ID: "
                f"from={info['from']} vin={info['vin']} logical={int_hex(info['logical_address'])} "
                f"eid={info['eid']} gid={info['gid']}"
            )
            self.root.after(0, lambda: self.fa_vin_var.set(info["vin"] or self.fa_vin_var.get()))

        self.worker("Vehicle Identification", action)

    def toggle_keepalive(self):
        # Kept for compatibility with older UI callbacks; tester present is automatic while connected.
        if self.client is not None and self.client.connected:
            self.start_keepalive()
        else:
            self.stop_keepalive(update_var=False)

    def start_keepalive(self):
        self.stop_keepalive(update_var=False)
        self.keepalive_stop.clear()
        try:
            period = max(1.0, float(self.keepalive_period_var.get()))
        except (TypeError, ValueError):
            period = 1.0
            self.keepalive_period_var.set(f"{period:.1f}")

        last_success_log = 0.0

        def log_keepalive_success(message):
            nonlocal last_success_log
            now = time.monotonic()
            if now - last_success_log >= 60.0:
                self.log(message)
                last_success_log = now

        def loop():
            while not self.keepalive_stop.is_set():
                client = self.client
                try:
                    if client is not None and client.connected:
                        if isinstance(client, DoipClient):
                            timeout = max(float(self.timeout_var.get()), 3.0)
                            client.send_uds_suppress_positive(b"\x3E\x80", timeout=timeout)
                            log_keepalive_success("TesterPresent OK: 3E 80 DoIP ACK")
                        else:
                            response = client.send_uds(b"\x3E\x00", timeout=float(self.timeout_var.get()))
                            require_positive_response(response, 0x3E)
                            log_keepalive_success(f"TesterPresent RX: {bytes_to_hex(response)}")
                except Exception as exc:
                    if self.keepalive_stop.is_set():
                        break
                    if isinstance(client, DoipClient) and self.client is client:
                        try:
                            self.reconnect_doip(
                                client,
                                "TesterPresent TCP refresh",
                                stop_event=self.keepalive_stop,
                                log_success=False,
                            )
                            if self.keepalive_stop.is_set():
                                break
                            log_keepalive_success("TesterPresent refreshed DoIP connection")
                        except Exception as reconnect_exc:
                            if self.keepalive_stop.is_set() or self.worker_stop.is_set() or self.client is not client:
                                break
                            self.log(f"TesterPresent ERROR: {exc}; reconnect stopped: {reconnect_exc}")
                            break
                    elif client is not None and self.client is client:
                        self.log(f"TesterPresent ERROR: {exc}")
                        client.close()
                        self.root.after(0, self._set_connected_status, False)
                self.keepalive_stop.wait(period)

        self.keepalive_thread = threading.Thread(target=loop, daemon=True)
        self.keepalive_thread.start()
        self.log(f"Automatic tester present started: {period:.1f} s")

    def keepalive_is_running(self):
        thread = self.keepalive_thread
        return thread is not None and thread.is_alive() and not self.keepalive_stop.is_set()

    def stop_keepalive(self, update_var=True):
        self.keepalive_stop.set()
        # Wait for the keepalive thread to actually exit before returning. Without this
        # join an in-flight TesterPresent could still be mid-exchange on the shared DoIP
        # socket and race the next request, desyncing request/response frames.
        thread = self.keepalive_thread
        if thread is not None and thread is not threading.current_thread() and thread.is_alive():
            try:
                join_timeout = max(8.0, float(self.timeout_var.get()) + 2.0)
            except (TypeError, ValueError):
                join_timeout = 8.0
            thread.join(timeout=join_timeout)
        self.keepalive_thread = None
        if update_var and hasattr(self, "keepalive_var"):
            self.keepalive_var.set(False)

    def raw_send_clicked(self):
        try:
            request = hex_to_bytes(self.raw_request_var.get())
        except Exception as exc:
            messagebox.showerror(APP_NAME, str(exc))
            return
        if not request:
            messagebox.showerror(APP_NAME, "Enter at least one request byte")
            return

        def zgw_worker(send, target, client):
            self._raw_target_worker(send, target, client, request)

        self._run_vehicle_uds_action("Raw UDS", [(request, "Raw")], zgw_worker=zgw_worker)

    def _append_raw_response(self, request, response, node=None):
        prefix = f"[{node}] " if node else ""
        self.raw_response_text.insert("end", f"{prefix}> {bytes_to_hex(request)}\n{prefix}< {bytes_to_hex(response)}\n\n")
        self.raw_response_text.see("end")

    def clear_dtc_results_clicked(self):
        self.dtc_tree.delete(*self.dtc_tree.get_children())
        self.dtc_summary_var.set("No fault memory read yet")

    def clear_diagnostic_information_clicked(self):
        self._run_vehicle_uds_action(
            "ClearDiagnosticInformation",
            [
                (bytes([0x10, SESSION_EXTENDED]), "Extended Session for ClearDiagnosticInformation"),
                (b"\x14\xff\xff\xff", "ClearDiagnosticInformation 14 FF FF FF"),
            ],
        )

    def hard_reset_clicked(self):
        requests = [
            (bytes([0x10, SESSION_EXTENDED]), "Extended Session for hard reset"),
            (b"\x11\x01", "ECUReset hardReset"),
        ]

        def zgw_worker(send, target, client):
            node = self._target_label(target)
            self._send_session(send, SESSION_EXTENDED, f"{node}: Extended Session for hard reset")
            self._send_ecu_reset_best_effort(send, f"{node}: ECUReset hardReset")
            self._recover_doip_after_reset(client, f"{node} hard reset", require_current_client=False)

        self._run_vehicle_uds_action("Hard Reset", requests, zgw_worker=zgw_worker)

    def read_fault_memory_clicked(self):
        try:
            status_mask = parse_int(self.dtc_status_mask_var.get())
            if status_mask < 0 or status_mask > 0xFF:
                raise ValueError("Status mask must be one byte")
        except Exception as exc:
            messagebox.showerror(APP_NAME, str(exc))
            return

        sink = {"lock": threading.Lock(), "rows": [], "summaries": []}
        requests = [
            (bytes([0x10, SESSION_EXTENDED]), "Extended Session for fault-memory read"),
            (bytes([0x19, 0x01, 0xFF]), "19 01 FF reportNumberOfDTCByStatusMask"),
            (bytes([0x19, 0x02, status_mask]), f"19 02 {status_mask:02X} reportDTCByStatusMask"),
        ]

        def zgw_worker(send, target, client):
            self._read_fault_memory_target_worker(send, target, client, status_mask, sink)

        def on_complete():
            with sink["lock"]:
                rows = list(sink["rows"])
                summaries = list(sink["summaries"])
            self.root.after(0, self._load_dtc_rows, rows, " | ".join(summaries))

        self._run_vehicle_uds_action(
            "Read Fault Memory",
            requests,
            zgw_worker=zgw_worker,
            on_complete=on_complete,
        )

    def _run_fault_memory_test_once(self):
        status_mask = parse_int(self.dtc_status_mask_var.get())
        if status_mask < 0 or status_mask > 0xFF:
            raise FcdError("Status mask must be one byte")

        strict_targets = [
            target for target in self._selected_vehicle_targets()
            if self._strict_response_for_target(target, True)
        ]
        sink = {"lock": threading.Lock(), "rows": [], "summaries": []}
        requests = [
            (bytes([0x10, SESSION_EXTENDED]), "Extended Session for fault-memory read"),
            (bytes([0x19, 0x01, 0xFF]), "19 01 FF reportNumberOfDTCByStatusMask"),
            (bytes([0x19, 0x02, status_mask]), f"19 02 {status_mask:02X} reportDTCByStatusMask"),
        ]

        def zgw_worker(send, target, client):
            self._read_fault_memory_target_worker(
                send,
                target,
                client,
                status_mask,
                sink,
                strict_response=self._strict_response_for_target(target, True),
            )

        def on_complete():
            with sink["lock"]:
                rows = list(sink["rows"])
                summaries = list(sink["summaries"])
            self.root.after(0, self._load_dtc_rows, rows, " | ".join(summaries))

        self._run_vehicle_uds_action_once(
            "Read Fault Memory",
            requests,
            zgw_worker=zgw_worker,
            on_complete=on_complete,
            strict_response=True,
        )

        with sink["lock"]:
            if strict_targets and not sink["summaries"]:
                raise FcdError("Read Fault Memory: no positive DTC response received")

    def _load_dtc_rows(self, rows, summary):
        self.dtc_tree.delete(*self.dtc_tree.get_children())
        for row in rows:
            self.dtc_tree.insert(
                "",
                "end",
                values=(
                    row.get("node", ""),
                    row["dtc"],
                    row["status"],
                    row["description"],
                    row.get("snapshot_data", ""),
                ),
            )
        self.dtc_summary_var.set(summary if summary else "Fault memory read completed")

    def _decode_dtc_detail_response(self, label, response, expected_dtc, expected_subfunction):
        if len(response) < 7 or response[0] != 0x59 or response[1] != expected_subfunction:
            raise FcdError(f"{label}: malformed detail response {bytes_to_hex(response)}")
        dtc = (response[2] << 16) | (response[3] << 8) | response[4]
        if dtc != expected_dtc:
            raise FcdError(f"{label}: response DTC 0x{dtc:06X} does not match request 0x{expected_dtc:06X}")
        status = response[5]
        record = response[6]
        data = bytes(response[7:])
        raw = bytes_to_hex(data) if data else "(none)"
        # A positive response carries the actual captured buffer, so translate it
        # into plain words. (NRCs never reach here; they raise NegativeResponse.)
        explanation = self._explain_dtc_detail(dtc, expected_subfunction, data)
        if explanation:
            return f"{explanation}   |   [rec 0x{record:02X}]"
        return f"status=0x{status:02X} record=0x{record:02X} data={raw}"

    def _explain_dtc_detail(self, dtc, subfunction, data):
        """Translate a Snapshot Data capture buffer into a one-line summary.

        Legacy targets can still return separate 0x19/04 and 0x19/06 records.
        New ZGW firmware returns one combined Snapshot Data buffer on 0x19/04.
        Returns "" when the DTC has no
        specific decoder so the caller falls back to the raw hex."""
        is_snapshot = (subfunction == 0x04)
        try:
            if dtc == DEM_DTC_MCUSM_SW_ERROR:
                return self._explain_mcusm_detail(data, is_snapshot)
            if dtc == DEM_DTC_PMS_ERRATA_STARTUP:
                return self._explain_pms_errata_detail(data, is_snapshot)
            if DEM_DTC_CAN_BUS_DIAG <= dtc < (DEM_DTC_CAN_BUS_DIAG + DEM_CAN_BUS_DIAG_EVENT_COUNT):
                return self._explain_can_bus_detail(dtc, data, is_snapshot)
            if DEM_DTC_LIN_BUS_DIAG <= dtc < (DEM_DTC_LIN_BUS_DIAG + DEM_LIN_BUS_DIAG_EVENT_COUNT):
                return self._explain_lin_bus_detail(dtc, data, is_snapshot)
            if DEM_DTC_ETHERNET_DIAG <= dtc < (DEM_DTC_ETHERNET_DIAG + DEM_ETHERNET_DIAG_EVENT_COUNT):
                return self._explain_ethernet_detail(dtc, data, is_snapshot)
            if DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT <= dtc < (
                    DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT + DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT):
                return self._explain_gateway_detail(dtc, data, is_snapshot)
            if dtc in (
                    DEM_DTC_AIMODEL_INPUT_INVALID,
                    DEM_DTC_AIMODEL_INFERENCE_INVALID,
                    DEM_DTC_AIMODEL_DEADLINE_EXCEEDED,
                    DEM_DTC_AIMODEL_OUTPUT_OUT_OF_RANGE):
                return self._explain_aimodel_detail(dtc, data, is_snapshot)
            if is_aimodel_consumer_dtc(dtc):
                return self._explain_aimodel_detail(dtc, data, is_snapshot)
            if dtc in (DEM_DTC_CODING_ECU_NOT_CODED, DEM_DTC_CODING_INVALID):
                return self._explain_default_detail(data, is_snapshot)
        except (IndexError, struct.error):
            return ""
        return ""

    def _explain_ethernet_detail(self, dtc, data, is_snapshot):
        if not is_snapshot:
            return ""
        timestamp_text = format_dtc_timestamp_data(data, "Ethernet DTC occurrence time")
        if len(data) < 96:
            return timestamp_text

        local_ip = u32_be(data, 22)
        remote_ip = u32_be(data, 26)
        local_port = u16_be(data, 30)
        remote_port = u16_be(data, 32)
        socon_id = data[34]
        connection_id = data[35]
        protocol = data[36]
        link_up = data[37]
        rx_errors = u32_be(data, 38)
        tx_errors = u32_be(data, 42)
        dma_errors = u32_be(data, 46)
        tcp_closes = u32_be(data, 50)
        open_fails = u32_be(data, 54)
        link_downs = u32_be(data, 58)
        listen_socket = u32_be(data, 62)
        active_socket = u32_be(data, 66)
        soad_state = data[70]
        phy_state = data[71]
        phy_addr = data[72]
        phy_init_done = data[73]
        phy_autoneg_done = data[74]
        phy_speed100 = data[75]
        phy_full_duplex = data[76]
        phy_reset_timeouts = u32_be(data, 77)
        phy_autoneg_timeouts = u32_be(data, 81)
        phy_mdio_errors = u32_be(data, 85)
        resource_errors = u32_be(data, 89)
        resource_flags = u16_be(data, 93)

        parts = [self._describe_zgw_dtc(dtc)]
        if timestamp_text:
            parts.append(timestamp_text)
        parts.extend([
            f"Port=ETH0",
            f"Link={'Up' if link_up else 'Down'}",
            f"PHY=0x{phy_addr:02X}",
            f"PHY state={ETHDIAG_PHY_STATE_TEXT.get(phy_state, phy_state)}",
            f"PHY initDone={phy_init_done}, autonegDone={phy_autoneg_done}",
            f"Speed={'100 Mbps' if phy_speed100 else '10 Mbps'}",
            f"Duplex={'Full' if phy_full_duplex else 'Half'}",
            f"SoCon={socon_id}, connection={connection_id}, protocol={ETHDIAG_PROTOCOL_TEXT.get(protocol, protocol)}",
            f"SoAd={ETHDIAG_SOAD_STATE_TEXT.get(soad_state, soad_state)}",
            f"Local={ip4_to_text(local_ip)}:{local_port}",
            f"Remote={ip4_to_text(remote_ip)}:{remote_port}",
            f"Sockets listen={listen_socket}, active={active_socket}",
            f"RX errors={rx_errors}, TX errors={tx_errors}, DMA errors={dma_errors}",
            f"TCP closes={tcp_closes}, socket open failures={open_fails}, link-down transitions={link_downs}",
            f"PHY reset timeouts={phy_reset_timeouts}, autoneg timeouts={phy_autoneg_timeouts}, MDIO errors={phy_mdio_errors}",
            f"Resource errors={resource_errors}, resource flags={flag_names(resource_flags, ETHDIAG_RESOURCE_FLAG_TEXT)} (0x{resource_flags:04X})",
        ])
        return " | ".join(parts)

    def _explain_can_bus_detail(self, dtc, data, is_snapshot):
        if not is_snapshot:
            return ""
        timestamp_text = format_dtc_timestamp_data(data, "CAN bus DTC occurrence time")
        if len(data) < 32:
            return timestamp_text
        base = DEM_DTC_TIMESTAMP_DATA_SIZE
        controller = data[base + 1]
        fault = data[base + 2]
        can_state = data[base + 3]
        error_state = data[base + 4]
        tx_error_counter = data[base + 5]
        rx_error_counter = data[base + 6]
        cansm_state = data[base + 7]
        comm_mode = data[base + 8]
        pdu_mode = data[base + 9]
        bus_off_count = int.from_bytes(data[base + 10:base + 14], "big")
        bus_text = CANDIAG_CONTROLLER_TEXT.get(controller, f"controller {controller}")
        fault_text = CANDIAG_FAULT_TEXT.get(fault, f"fault {fault}")
        parts = [f"{bus_text} {fault_text}"]
        if timestamp_text:
            parts.append(timestamp_text)
        parts.extend([
            f"controller={controller}",
            f"CAN state={CANDIAG_CAN_STATE_TEXT.get(can_state, can_state)}",
            f"error state={CANDIAG_ERROR_STATE_TEXT.get(error_state, error_state)}",
            f"TEC={tx_error_counter}, REC={rx_error_counter}",
            f"CanSM={CANDIAG_CANSM_STATE_TEXT.get(cansm_state, cansm_state)}",
            f"ComM={CANDIAG_COMM_MODE_TEXT.get(comm_mode, comm_mode)}",
            f"CanIf PDU mode={CANDIAG_PDU_MODE_TEXT.get(pdu_mode, pdu_mode)}",
            f"bus-off count={bus_off_count}",
        ])
        if len(data) >= 48:
            operational = data[base + 14]
            rx_enabled = data[base + 15]
            tx_enabled = data[base + 16]
            controller_pending = data[base + 17]
            recovered_pending = data[base + 18]
            error_passive_fail = int.from_bytes(data[base + 19:base + 21], "big")
            error_passive_pass = int.from_bytes(data[base + 21:base + 23], "big")
            protocol_fail = int.from_bytes(data[base + 23:base + 25], "big")
            protocol_pass = int.from_bytes(data[base + 25:base + 27], "big")
            error_passive_seen = data[base + 27]
            parts.extend([
                f"operational={'yes' if operational else 'no'}",
                f"normal RX enabled={'yes' if rx_enabled else 'no'}",
                f"normal TX enabled={'yes' if tx_enabled else 'no'}",
                f"controller fault pending={'yes' if controller_pending else 'no'}",
                f"recovery pending={'yes' if recovered_pending else 'no'}",
                f"error-passive debounce fail/pass={error_passive_fail}/{error_passive_pass}",
                f"protocol-error debounce fail/pass={protocol_fail}/{protocol_pass}",
                f"error-passive indication latched={'yes' if error_passive_seen else 'no'}",
            ])
        return " | ".join(parts)

    def _explain_lin_bus_detail(self, dtc, data, is_snapshot):
        if not is_snapshot:
            return ""
        timestamp_text = format_dtc_timestamp_data(data, "LIN bus DTC occurrence time")
        if len(data) < 36:
            return timestamp_text
        base = DEM_DTC_TIMESTAMP_DATA_SIZE
        channel = data[base + 1]
        fault = data[base + 2]
        frame_id = data[base + 3]
        pid = data[base + 4]
        slave_nad = data[base + 5]
        schedule = data[base + 6]
        linsm_state = data[base + 7]
        comm_mode = data[base + 8]
        lin_state = data[base + 9]
        last_result = data[base + 10]
        no_response_count = data[base + 11]
        protocol_count = data[base + 12]
        controller_count = data[base + 13]
        wakeup_count = data[base + 14] if len(data) > (base + 14) else 0
        sleep_count = data[base + 15] if len(data) > (base + 15) else 0
        channel_unavailable = data[base + 16] if len(data) > (base + 16) else 0
        error_class = data[base + 17] if len(data) > (base + 17) else 0
        checksum_count = data[base + 18] if len(data) > (base + 18) else 0
        pid_count = data[base + 19] if len(data) > (base + 19) else 0
        framing_count = data[base + 20] if len(data) > (base + 20) else 0
        sync_count = data[base + 21] if len(data) > (base + 21) else 0
        header_count = data[base + 22] if len(data) > (base + 22) else 0
        schedule_errors = u32_be(data, base + 23) if len(data) >= (base + 27) else 0
        diag_timeouts = u32_be(data, base + 27) if len(data) >= (base + 31) else 0
        linif_state = data[base + 31] if len(data) > (base + 31) else 0
        diag_state = data[base + 32] if len(data) > (base + 32) else 0
        comm_rx_enabled = data[base + 33] if len(data) > (base + 33) else 0
        comm_tx_enabled = data[base + 34] if len(data) > (base + 34) else 0
        slave_expected = data[base + 35] if len(data) > (base + 35) else 0
        diag_schedule_active = data[base + 36] if len(data) > (base + 36) else 0
        channel_text = LINDIAG_CHANNEL_TEXT.get(channel, f"LIN channel {channel}")
        fault_text = LINDIAG_FAULT_TEXT.get(fault, f"fault {fault}")
        parts = [f"{channel_text} {fault_text}"]
        if timestamp_text:
            parts.append(timestamp_text)
        parts.extend([
            f"channel={channel}",
            f"Last PID=0x{pid:02X}",
            f"Frame ID=0x{frame_id:02X}",
            f"Slave NAD={slave_nad}",
            f"Schedule={LINDIAG_SCHEDULE_TEXT.get(schedule, schedule)}",
            f"LinSM={LINDIAG_LINSM_STATE_TEXT.get(linsm_state, linsm_state)}",
            f"ComM={CANDIAG_COMM_MODE_TEXT.get(comm_mode, comm_mode)}",
            f"LIN state={LINDIAG_LIN_STATE_TEXT.get(lin_state, lin_state)}",
            f"Error type={LINDIAG_RESULT_TEXT.get(last_result, last_result)}",
            f"Error class={LINDIAG_ERROR_CLASS_TEXT.get(error_class, error_class)}",
            f"No-response counter={no_response_count}",
            f"Protocol error counter={protocol_count}",
            f"Controller fault counter={controller_count}",
            f"Checksum/PID/Framing/Sync/Header={checksum_count}/{pid_count}/{framing_count}/{sync_count}/{header_count}",
            f"Wakeup failures={wakeup_count}, sleep failures={sleep_count}",
            f"LinIf state={LINDIAG_CHANNEL_STATE_TEXT.get(linif_state, linif_state)}",
            f"Diag schedule={LINDIAG_DIAG_STATE_TEXT.get(diag_state, diag_state)}",
            f"Schedule errors={schedule_errors}, diag timeouts={diag_timeouts}",
            f"Normal communication rx/tx={comm_rx_enabled}/{comm_tx_enabled}",
            f"Slave response expected={slave_expected}, diag schedule active={diag_schedule_active}, channel unavailable={channel_unavailable}",
        ])
        return " | ".join(parts)

    def _explain_aimodel_detail(self, dtc, data, is_snapshot):
        if not is_snapshot:
            return ""
        timestamp_text = format_dtc_timestamp_data(data, "AiModel DTC occurrence time")
        if len(data) < 98:
            return timestamp_text
        base = DEM_DTC_TIMESTAMP_DATA_SIZE
        event_id = u32_be(data, base)
        error_flags = u32_be(data, base + 4)
        input_voltage = s32_be(data, base + 8) / 1000.0
        input_valid = data[base + 12]
        inference_valid = data[base + 13]
        window_ready = data[base + 14]
        channel = data[base + 15]
        predicted_fault = data[base + 16]
        undervoltage = data[base + 17]
        overvoltage = data[base + 18]
        overcurrent = data[base + 19]
        input_timestamp = u32_be(data, base + 20)
        inference_sequence = u32_be(data, base + 24)
        channel_us = u32_be(data, base + 28)
        total_us = u32_be(data, base + 32)
        processing_us = u32_be(data, base + 36)
        measured_current = s32_be(data, base + 40) / 1000.0
        current_rating = s32_be(data, base + 44) / 1000.0
        predicted_current = s32_be(data, base + 48) / 1000.0
        fault_soon = s32_be(data, base + 52) / 1000.0
        utilization = s32_be(data, base + 56) / 1000.0
        probabilities = [s32_be(data, base + 60 + (idx * 4)) / 1000.0 for idx in range(4)]
        parts = [self._describe_zgw_dtc(dtc)]
        if timestamp_text:
            parts.append(timestamp_text)
        parts.extend([
            f"event id {event_id}",
            f"error flags 0x{error_flags:08X}",
            f"input {input_voltage:.3f} V, valid={input_valid}, windowReady={window_ready}",
            f"inference valid={inference_valid}, sequence={inference_sequence}",
            f"channel {channel + 1}, predicted class={predicted_fault}",
            f"measured={measured_current:.3f} A, rating={current_rating:.3f} A, predicted={predicted_current:.3f} A",
            f"fault soon={fault_soon:.3f}, utilization={utilization:.3f}",
            f"fault probabilities={', '.join(f'{value:.3f}' for value in probabilities)}",
            f"limits: undervoltage={undervoltage}, overvoltage={overvoltage}, overcurrent={overcurrent}",
            f"inputTimestamp={input_timestamp} ms",
            f"timing: channel={channel_us} us, total={total_us} us, processing={processing_us} us",
        ])
        return " | ".join(parts)

    def _explain_gateway_detail(self, dtc, data, is_snapshot, is_message=True):
        # GatewaySwc_CaptureRxDiagSnapshotData layout, with legacy 0x19/06 fallback.
        if is_message:
            index = dtc - DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT
            name = CODING_PARAMETER_NAMES[index] if index < len(CODING_PARAMETER_NAMES) else f"index {index}"
        if is_snapshot:
            timestamp_text = format_dtc_timestamp_data(data)
            if timestamp_text:
                if is_message:
                    return f"RX message timeout - {name} | {timestamp_text}"
                return timestamp_text
            if len(data) < 30:
                return ""
            bus_text = GATEWAY_BUS_TEXT.get(data[1], f"bus {data[1]}")
            status_text = GATEWAY_RX_DIAG_STATUS_TEXT.get(data[6], f"0x{data[6]:02X}")
            object_id = u16_be(data, 4)
            main_cycles = u32_be(data, 8)
            active_count = u16_be(data, 24)
            event_count = u32_be(data, 26)
            if is_message:
                cycle_ticks = u16_be(data, 12)
                timeout_ticks = u16_be(data, 14)
                return " | ".join([
                    f"RX message timeout — {name} (COM PDU id {object_id}) on {bus_text}",
                    f"current status: {status_text}",
                    f"expected RX every {cycle_ticks} ticks, declared lost after {timeout_ticks} ticks",
                    f"ECU main-loop count when captured: {main_cycles}",
                    f"messages timed out right now: {active_count}",
                    f"total timeout events since power-up: {event_count}",
                ])
            return ""
        # Legacy 0x19/06 record (16 bytes): a smaller subset of the same fields.
        if len(data) < 16:
            return ""
        bus_text = GATEWAY_BUS_TEXT.get(data[1], f"bus {data[1]}")
        object_id = u16_be(data, 2)
        threshold = u32_be(data, 8)
        main_cycles = u32_be(data, 12)
        if is_message:
            return " | ".join([
                f"RX message timeout — {name} (COM PDU id {object_id}) on {bus_text}",
                f"timeout limit: {threshold} ticks",
                f"ECU main-loop count when captured: {main_cycles}",
            ])
        return ""

    def _explain_mcusm_detail(self, data, is_snapshot):
        # SysMgr_CaptureMcuSmSnapshotData layout, with legacy 0x19/06 fallback.
        if is_snapshot:
            if len(data) < 32:
                return ""
            event_id = u16_be(data, 0)
            fault_source = data[2]
            wake_reason = data[3]
            reset_reason = u32_be(data, 4)
            info_field = u32_be(data, 8)
            pms_wcr2 = u32_be(data, 12)
            pms_wstat2 = u32_be(data, 16)
            scr_pending = data[23]
            # buffer[8] holds the SafetyKit failure mask only when there was no MCU
            # reset and the mask is set; otherwise it is the last reset information.
            if reset_reason == 0 and info_field != 0:
                info_label = "SafetyKit failure mask"
            else:
                info_label = "last reset information"
            parts = [
                f"MCU safety-manager software error (event id {event_id})",
                f"fault source: {flag_names(fault_source, MCUSM_FAULT_SOURCE_BITS)} (0x{fault_source:02X})",
                f"last MCU reset reason: {reset_reason_name(reset_reason, info_field)} (0x{reset_reason:08X})",
                f"{info_label}: 0x{info_field:08X}",
                f"PMS wake-config reg2 (PMSWCR2): 0x{pms_wcr2:08X}",
                f"PMS wake-status reg2 (PMSWSTAT2): 0x{pms_wstat2:08X}",
                f"SCR fault/NMI/RST status: 0x{data[20]:02X}/0x{data[21]:02X}/0x{data[22]:02X}",
                f"SCR fault pending: {flag_names(scr_pending, MCUSM_SCR_FAULT_PENDING_BITS)} (0x{scr_pending:02X})",
                f"SCR wake reason: 0x{wake_reason:02X}",
                f"SCR-fault wake-ups: {u32_be(data, 24)}",
                f"go-to-sleep count: {u32_be(data, 28)}",
            ]
            if len(data) >= 64 and data[32] >= 2:
                trap_class = data[33]
                trap_id = data[34]
                trap_core = data[35]
                trap_taddr = u32_be(data, 36)
                trap4_error = u32_be(data, 40)
                smu_group = u32_be(data, 44)
                smu_alarm = u32_be(data, 48)
                dflash_request = u32_be(data, 52)
                dflash_info = u32_be(data, 56)
                dflash_physical = u32_be(data, 60)
                if trap_class != 0 or trap_id != 0 or trap_taddr != 0:
                    parts.append(
                        f"last trap: class {trap_class}, id {trap_id}, core {trap_core}, tAddr 0x{trap_taddr:08X}"
                    )
                if trap4_error != 0:
                    parts.append(f"trap4 error address: 0x{trap4_error:08X}")
                if smu_group != 0 or smu_alarm != 0:
                    parts.append(f"SMU reset alarm: group {smu_group}, alarm {smu_alarm}")
                if dflash_request != 0 or dflash_info != 0 or dflash_physical != 0:
                    parts.append(
                        f"DFlash recovery: request 0x{dflash_request:08X}, info 0x{dflash_info:08X}, "
                        f"physical 0x{dflash_physical:08X}"
                    )
            if len(data) >= 224:
                detail = self._explain_mcusm_detail(data[64:], False)
                if detail:
                    parts.append(f"snapshot detail: {detail}")
            if len(data) >= (MCUSM_SNAPSHOT_TIMESTAMP_OFFSET + DEM_DTC_TIMESTAMP_DATA_SIZE):
                timestamp_text = format_dtc_timestamp_data(data[MCUSM_SNAPSHOT_TIMESTAMP_OFFSET:])
                if timestamp_text:
                    parts.append(timestamp_text)
            return " | ".join(parts)
        if len(data) < 16:
            return ""
        event_id = u16_be(data, 0)
        fault_source = data[2]
        scr_pending = data[15]
        parts = [
            f"MCU safety-manager software error (event id {event_id})",
            f"fault source: {flag_names(fault_source, MCUSM_FAULT_SOURCE_BITS)} (0x{fault_source:02X})",
            f"SCR wake reason: 0x{data[3]:02X}",
            f"PMS wake-config reg2 (PMSWCR2): 0x{u32_be(data, 4):08X}",
            f"PMS wake-status reg2 (PMSWSTAT2): 0x{u32_be(data, 8):08X}",
            f"SCR fault/NMI/RST status: 0x{data[12]:02X}/0x{data[13]:02X}/0x{data[14]:02X}",
            f"SCR fault pending: {flag_names(scr_pending, MCUSM_SCR_FAULT_PENDING_BITS)} (0x{scr_pending:02X})",
        ]
        if len(data) >= 160 and data[16] >= 2:
            reset_reason = u32_be(data, 20)
            reset_info = u32_be(data, 24)
            safety_mask = u32_be(data, 28)
            fw_result = u32_be(data, 32)
            reset_type = u32_be(data, 36)
            reset_trigger = u32_be(data, 40)
            rststat = u32_be(data, 44)
            ssw_reset_reason = u16_be(data, 48)
            fw_last_ssh = u32_be(data, 64)
            actual_eccd = u32_be(data, 68)
            actual_faultsts = u32_be(data, 72)
            actual_errinfo = u32_be(data, 76)
            expected_eccd = u32_be(data, 80)
            expected_faultsts = u32_be(data, 84)
            expected_errinfo = u32_be(data, 88)
            trap_counter = u32_be(data, 92)
            trap_regs = [u32_be(data, offset) for offset in range(96, 124, 4)]
            smu_raw = u32_be(data, 124)
            smu_masked = u32_be(data, 128)
            dflash_counter = u32_be(data, 132)
            dflash_attempts = u32_be(data, 136)
            dflash_suppressed = u32_be(data, 140)
            dflash_access = u32_be(data, 144)
            dflash_physical = u32_be(data, 148)
            dflash_request = u32_be(data, 152)
            dflash_info = u32_be(data, 156)
            ssw_status = [
                f"LBIST={status_name(data[53], SSW_STATUS_TEXT)}",
                f"MONBIST={status_name(data[54], SSW_STATUS_TEXT)}",
                f"FW={status_name(data[55], SSW_STATUS_TEXT)}",
                f"MCU startup={status_name(data[56], SSW_STATUS_TEXT)}",
                f"alive={status_name(data[57], SSW_STATUS_TEXT)}",
                f"regmon={status_name(data[58], SSW_STATUS_TEXT)}",
                f"MBIST={status_name(data[59], SSW_STATUS_TEXT)}",
                f"SMU keys={status_name(data[60], SMU_STATUS_TEXT)}",
                f"SMU keys clear={status_name(data[61], SMU_STATUS_TEXT)}",
                f"SMU init={status_name(data[62], SMU_STATUS_TEXT)}",
            ]
            parts.extend([
                f"capture version: {data[16]}",
                f"last reset reason: {reset_reason_name(reset_reason, reset_info)} (0x{reset_reason:08X})",
                f"last reset information: 0x{reset_info:08X}",
                f"SafetyKit failure mask: {flag_names(safety_mask, MCUSM_SAFETYKIT_FAILURE_BITS)} (0x{safety_mask:08X})",
                f"FW-check result mask: {flag_names(fw_result, MCUSM_FW_CHECK_RESULT_BITS)} (0x{fw_result:08X})",
                (
                    f"SafetyKit reset: type {enum_name(reset_type, SAFETYKIT_RESET_TYPE_TEXT, 'type')}, "
                    f"trigger {enum_name(reset_trigger, SCU_RESET_TRIGGER_TEXT, 'trigger')}, "
                    f"RSTSTAT 0x{rststat:08X}, reason 0x{ssw_reset_reason:04X}"
                ),
                f"SafetyKit statuses: {', '.join(ssw_status)}",
                (
                    f"SafetyKit counters: inhibit={data[17]}, FBL resets={data[18]}, "
                    f"FBL request={data[19]}, LBIST request={data[50]}, "
                    f"LBIST runs={data[51]}, FW-check runs={data[52]}, standby wake={data[63]}"
                ),
                (
                    f"FW-check SSH: last={fw_last_ssh}, actual ECCD/FAULTSTS/ERRINFO="
                    f"0x{actual_eccd:08X}/0x{actual_faultsts:08X}/0x{actual_errinfo:08X}, "
                    f"expected=0x{expected_eccd:08X}/0x{expected_faultsts:08X}/0x{expected_errinfo:08X}"
                ),
                (
                    f"trap registers: count={trap_counter}, DSTR/DATR/DEADD/DIEAR/DIETR/PIEAR/PIETR="
                    f"{'/'.join(f'0x{value:08X}' for value in trap_regs)}"
                ),
                f"selected SMU alarm word: raw 0x{smu_raw:08X}, masked 0x{smu_masked:08X}",
                (
                    f"DFlash recovery: counter={dflash_counter}, attempts={dflash_attempts}, "
                    f"suppressed={dflash_suppressed}, Fee access=0x{dflash_access:08X}, "
                    f"physical=0x{dflash_physical:08X}, request=0x{dflash_request:08X}, info=0x{dflash_info:08X}"
                ),
            ])
        return " | ".join(parts)

    def _explain_pms_errata_detail(self, data, is_snapshot):
        if len(data) < 76:
            return ""
        event_id = u16_be(data, 0)
        version = data[2]
        wakeup_from_standby = data[3]
        reset_reason = u32_be(data, 4)
        reset_info = u32_be(data, 8)
        safety_mask = u32_be(data, 12)
        failure_mask = u32_be(data, 16)
        ignored_mask = u32_be(data, 20)
        sample_count = u32_be(data, 24)
        timeout_count = u32_be(data, 28)
        checked_evrstat = u32_be(data, 32)
        checked_monstat1 = u32_be(data, 36)
        evrstat = u32_be(data, 40)
        evradcstat = u32_be(data, 44)
        evrmonstat1 = u32_be(data, 48)
        evrrstcon = u32_be(data, 52)
        evrovmon2 = u32_be(data, 56)
        evruvmon2 = u32_be(data, 60)
        reset_type = u32_be(data, 64)
        reset_trigger = u32_be(data, 68)
        ssw_reset_reason = u16_be(data, 72)
        check_status = data[74]
        fw_status = data[75]
        parts = [
            f"PMS errata startup warning (event id {event_id})",
            f"capture version: {version}",
            f"last reset reason: {reset_reason_name(reset_reason, reset_info)} (0x{reset_reason:08X})",
            f"last reset information: 0x{reset_info:08X}",
            f"SafetyKit failure mask: {flag_names(safety_mask, MCUSM_SAFETYKIT_FAILURE_BITS)} (0x{safety_mask:08X})",
            f"PMS errata failure mask: {flag_names(failure_mask, PMS_ERRATA_FAILURE_BITS)} (0x{failure_mask:08X})",
            f"PMS standby ignored mask: {flag_names(ignored_mask, PMS_ERRATA_FAILURE_BITS)} (0x{ignored_mask:08X})",
            f"TC007 samples/refresh timeouts: {sample_count}/{timeout_count}",
            (
                f"SafetyKit reset: type {enum_name(reset_type, SAFETYKIT_RESET_TYPE_TEXT, 'type')}, "
                f"trigger {enum_name(reset_trigger, SCU_RESET_TRIGGER_TEXT, 'trigger')}, "
                f"reason 0x{ssw_reset_reason:04X}, standby wake={wakeup_from_standby}"
            ),
            f"PMS errata status={check_status}, FW-check status={fw_status}",
            f"checked EVRSTAT/MONSTAT1: 0x{checked_evrstat:08X}/0x{checked_monstat1:08X}",
            f"captured EVRSTAT/ADCSTAT/MONSTAT1: 0x{evrstat:08X}/0x{evradcstat:08X}/0x{evrmonstat1:08X}",
            f"EVRRSTCON/EVROVMON2/EVRUVMON2: 0x{evrrstcon:08X}/0x{evrovmon2:08X}/0x{evruvmon2:08X}",
        ]
        if len(data) >= (76 + DEM_DTC_TIMESTAMP_DATA_SIZE):
            timestamp_text = format_dtc_timestamp_data(data[76:])
            if timestamp_text:
                parts.append(timestamp_text)
        return " | ".join(parts)

    def _explain_default_detail(self, data, is_snapshot):
        # Current ZGW coding DTC snapshots are timestamp-only. Keep the legacy
        # default snapshot decode below for older targets.
        if is_snapshot:
            timestamp_text = format_dtc_timestamp_data(data)
            if timestamp_text:
                return f"Coding DTC snapshot | {timestamp_text}"
        if len(data) < 4:
            return ""
        event_id = u16_be(data, 0)
        name = DEM_EVENT_ID_NAMES.get(event_id, f"event id {event_id}")
        if is_snapshot:
            text = (
                f"Generic snapshot — {name} (event id {event_id}); "
                "no signal data is captured for this event, remaining bytes are reserved/zero"
            )
            if len(data) >= 48:
                detail = self._explain_default_detail(data[32:], False)
                if detail:
                    text = f"{text} | snapshot detail: {detail}"
            return text
        return (
            f"Generic snapshot detail record #{data[3]} — {name} (event id {event_id}); "
            "occurrence/aging bookkeeping only, no payload"
        )

    def _decode_read_dtc_response(self, label, response):
        if len(response) < 2 or response[0] != 0x59:
            raise FcdError(f"{label}: malformed ReadDTCInformation response {bytes_to_hex(response)}")

        subfunction = response[1]
        payload = response[2:]
        if subfunction == 0x01:
            if len(payload) < 4:
                raise FcdError(f"{label}: short DTC count response {bytes_to_hex(response)}")
            availability = payload[0]
            fmt = payload[1]
            count = u16_be(payload, 2)
            summary = (
                f"19 01 FF: count={count}, availability=0x{availability:02X}, "
                f"format=0x{fmt:02X}"
            )
            return [], summary

        if subfunction == 0x02:
            if len(payload) < 1:
                raise FcdError(f"{label}: missing DTC status availability mask")
            availability = payload[0]
            records = payload[1:]
            if (len(records) % 4) != 0:
                raise FcdError(f"{label}: DTC record length is not a multiple of 4")
            rows = []
            for offset in range(0, len(records), 4):
                dtc = (records[offset] << 16) | (records[offset + 1] << 8) | records[offset + 2]
                status = records[offset + 3]
                rows.append(
                    {
                        "source": "19 02",
                        "dtc_value": dtc,
                        "dtc": f"0x{dtc:06X}",
                        "status": f"0x{status:02X}",
                        "description": self._describe_zgw_dtc(dtc),
                    }
                )
            return rows, f"19 02: records={len(rows)}, availability=0x{availability:02X}"

        raise FcdError(f"{label}: unsupported positive subfunction 0x{subfunction:02X}")

    def _describe_zgw_dtc(self, dtc):
        if dtc in STATIC_DTC_DESCRIPTIONS:
            return STATIC_DTC_DESCRIPTIONS[dtc]

        if is_aimodel_consumer_dtc(dtc):
            return describe_aimodel_consumer_dtc(dtc)

        if DEM_DTC_CAN_BUS_DIAG <= dtc < (DEM_DTC_CAN_BUS_DIAG + DEM_CAN_BUS_DIAG_EVENT_COUNT):
            return STATIC_DTC_DESCRIPTIONS.get(dtc, "CAN bus diagnostic")

        if DEM_DTC_LIN_BUS_DIAG <= dtc < (DEM_DTC_LIN_BUS_DIAG + DEM_LIN_BUS_DIAG_EVENT_COUNT):
            return STATIC_DTC_DESCRIPTIONS.get(dtc, "LIN bus diagnostic")

        if DEM_DTC_ETHERNET_DIAG <= dtc < (DEM_DTC_ETHERNET_DIAG + DEM_ETHERNET_DIAG_EVENT_COUNT):
            return STATIC_DTC_DESCRIPTIONS.get(dtc, "Ethernet diagnostic")

        if DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT <= dtc < (DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT + DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT):
            index = dtc - DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT
            name = CODING_PARAMETER_NAMES[index] if index < len(CODING_PARAMETER_NAMES) else f"RX message index {index}"
            return f"Message Timeout: {name}"

        return "Unknown ZGW APP DTC"

    def _ensure_coding_tab(self, ecu_name):
        ecu_name = (ecu_name or "ZGW").strip()
        if ecu_name in self.coding_ecu_rows:
            return self.coding_ecu_rows[ecu_name]
        frame = ttk.Frame(self.coding_target_notebook)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(0, weight=1)
        columns = ("param", "expected")
        tree = ttk.Treeview(frame, columns=columns, show="headings", selectmode="extended")
        for col, text, width in [
            ("param", "Coding Field", 420),
            ("expected", "Value", 120),
        ]:
            tree.heading(col, text=text)
            tree.column(col, width=width, stretch=(col == "param"))
        tree.grid(row=0, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(frame, orient="vertical", command=tree.yview)
        scroll.grid(row=0, column=1, sticky="ns")
        tree.configure(yscrollcommand=scroll.set)
        tree.bind("<Double-1>", lambda _event: self.edit_coding_param_clicked())
        self.coding_target_notebook.add(frame, text=ecu_name)
        self.coding_ecu_rows[ecu_name] = tree
        self.coding_target_tabs[str(frame)] = ecu_name
        return tree

    def _active_coding_ecu(self):
        try:
            selected = self.coding_target_notebook.select()
        except tk.TclError:
            return "ZGW"
        return self.coding_target_tabs.get(selected, "ZGW")

    def _active_coding_tree(self):
        return self._ensure_coding_tab(self._active_coding_ecu())

    def _sync_coding_tabs_from_nodes(self):
        for target in self.node_rows.values():
            if target.get("simulated_enabled", True):
                self._ensure_coding_tab(target.get("node_name", ""))

    def add_coding_param_clicked(self):
        item = self._coding_param_dialog()
        if item:
            self._active_coding_tree().insert("", "end", values=(item["param"], item["expected"]))

    def edit_coding_param_clicked(self):
        tree = self._active_coding_tree()
        selection = tree.selection()
        if not selection:
            return
        values = tree.item(selection[0], "values")
        item = self._coding_param_dialog({"param": values[0], "expected": values[1]})
        if item:
            tree.item(selection[0], values=(item["param"], item["expected"]))

    def remove_coding_param_clicked(self):
        tree = self._active_coding_tree()
        for iid in tree.selection():
            tree.delete(iid)

    def _coding_param_dialog(self, existing=None):
        existing = existing or {"param": "", "expected": "1"}
        dialog = tk.Toplevel(self.root)
        dialog.title("Coding Parameter")
        dialog.transient(self.root)
        dialog.grab_set()
        result = {}

        vars_ = {
            "param": tk.StringVar(value=existing.get("param", "")),
            "expected": tk.StringVar(value=existing["expected"]),
        }

        ttk.Label(dialog, text="Coding Field").grid(row=0, column=0, sticky="w", padx=8, pady=6)
        field = ttk.Combobox(dialog, textvariable=vars_["param"], values=CODING_VISIBLE_PARAMETER_NAMES, width=70)
        field.grid(row=0, column=1, sticky="ew", padx=8, pady=6)
        ttk.Label(dialog, text="Value").grid(row=1, column=0, sticky="w", padx=8, pady=6)
        ttk.Combobox(dialog, textvariable=vars_["expected"], values=("1", "0"), width=10).grid(
            row=1, column=1, sticky="w", padx=8, pady=6
        )

        def ok():
            if vars_["param"].get().strip() not in CODING_PARAMETER_INDEX:
                messagebox.showerror(APP_NAME, "Select a known coding field", parent=dialog)
                return
            result.update({key: var.get().strip() for key, var in vars_.items()})
            dialog.destroy()

        ttk.Button(dialog, text="OK", command=ok).grid(row=2, column=0, padx=8, pady=10)
        ttk.Button(dialog, text="Cancel", command=dialog.destroy).grid(row=2, column=1, sticky="w", padx=8, pady=10)
        dialog.wait_window()
        return result or None

    def import_coding_clicked(self):
        path = filedialog.askopenfilename(filetypes=[("JSON", "*.json"), ("All files", "*.*")])
        if not path:
            return
        with open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
        tree = self._active_coding_tree()
        tree.delete(*tree.get_children())
        mask_hex = data.get("mask_hex") or data.get("mask")
        if mask_hex:
            self._load_mask_to_coding_tree(hex_to_bytes(mask_hex))
            return
        for item in data.get("parameters", []):
            index_value = item.get("index", item.get("path", ""))
            param_name = item.get("param", "")
            if not param_name:
                try:
                    idx = parse_int(index_value)
                    param_name = CODING_PARAMETER_NAMES[idx] if 0 <= idx < len(CODING_PARAMETER_NAMES) else ""
                except (ValueError, TypeError):
                    param_name = ""
            if param_name and not is_reserved_coding_parameter(param_name):
                tree.insert("", "end", values=(param_name, item.get("expected", item.get("value", ""))))
        self.log(f"Imported coding JSON: {path}")

    def export_coding_clicked(self):
        path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON", "*.json")])
        if not path:
            return
        parameters = []
        tree = self._active_coding_tree()
        for iid in tree.get_children():
            values = tree.item(iid, "values")
            parameters.append({"param": values[0], "index": coding_parameter_index(values[0]), "expected": values[1]})
        data = {
            "schema": "FCD_CODING_CONFIG_v1",
            "created": datetime.now().isoformat(timespec="seconds"),
            "vin": self.fa_vin_var.get(),
            "ecu": self._active_coding_ecu(),
            "coding_did": self.coding_mask_did_var.get(),
            "mask_hex": bytes_to_hex(self._coding_rows_to_mask()) if parameters else "",
            "parameters": parameters,
        }
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(data, handle, indent=2)
        self.log(f"Exported coding JSON: {path}")

    def _coding_rows_to_mask(self):
        return self._coding_tree_to_mask(self._active_coding_tree())

    def _coding_tree_to_mask(self, tree):
        bits = {}
        max_index = -1
        for iid in tree.get_children():
            values = tree.item(iid, "values")
            if not str(values[0]).strip():
                continue
            idx = coding_parameter_index(values[0])
            if idx is None:
                raise FcdError(f"Unknown coding field {values[0]}")
            expected_text = str(values[1]).strip().lower()
            expected = expected_text in ("1", "true", "yes", "y", "on", "expected")
            if idx < 0:
                raise FcdError("Coding index must be non-negative")
            bits[idx] = expected
            max_index = max(max_index, idx)
        if max_index < 0:
            raise FcdError("No coding parameters defined")
        mask = bytearray((max_index + 8) // 8)
        for idx, expected in bits.items():
            if expected:
                mask[idx >> 3] |= 1 << (idx & 0x07)
        return bytes(mask)

    def _all_coding_descriptors(self):
        descriptors = {}
        for ecu_name, tree in self.coding_ecu_rows.items():
            if not tree.get_children():
                continue
            mask = self._coding_tree_to_mask(tree)
            fields = []
            for iid in tree.get_children():
                values = tree.item(iid, "values")
                fields.append({
                    "field": values[0],
                    "index": coding_parameter_index(values[0]),
                    "value": values[1],
                })
            descriptors[ecu_name.upper()] = {
                "schema": "FCD_CODING_DESCRIPTOR_v1",
                "did": self.coding_mask_did_var.get(),
                "write_routine": self.coding_write_all_rid_var.get(),
                "validate_routine": self.coding_validate_rid_var.get(),
                "mask_hex": bytes_to_hex(mask),
                "fields": fields,
            }
        return descriptors

    def _load_mask_to_coding_tree(self, mask, source="ECU coding routine 0x0203", ecu_name=None):
        tree = self._ensure_coding_tab(ecu_name) if ecu_name else self._active_coding_tree()
        tree.delete(*tree.get_children())
        if not mask:
            tree.insert("", "end", values=("ECU returned no coding mask bytes", "0"))
            return
        total_bits = len(mask) * 8
        # Show every known Coding Parameter plus any extra mask bits so the table is a
        # complete, named view of the coding values read back from the ECU.
        row_count = max(total_bits, len(CODING_PARAMETER_NAMES))
        for idx in range(row_count):
            if idx < total_bits:
                expected = (mask[idx >> 3] & (1 << (idx & 0x07))) != 0
            else:
                expected = False
            if idx < len(CODING_PARAMETER_NAMES):
                name = CODING_PARAMETER_NAMES[idx]
            else:
                name = f"(reserved bit {idx})"
            if is_reserved_coding_parameter(name):
                continue
            tree.insert(
                "", "end",
                values=(name, "1" if expected else "0"),
            )

    def _load_dummy_coding_values_to_coding_tree(self, ecu_name):
        tree = self._ensure_coding_tab(ecu_name)
        tree.delete(*tree.get_children())
        for name, _index, value in DUMMY_CODING_VALUES:
            tree.insert("", "end", values=(name, value))

    def _send_ecu_reset_best_effort(self, send, label, timeout=POST_RESET_RESPONSE_TIMEOUT_SECONDS):
        try:
            return send(b"\x11\x01", label, timeout=timeout, allow_no_response=True)
        except NegativeResponse as exc:
            if exc.sid == 0x11:
                self.log(f"RX {label}: reset response accepted ({exc})")
                return b""
            raise

    def _tolerant_ecu_reset(self, client, reason, require_current_client=True):
        # A hard reset response is best-effort: it may be a clean 51 01, a stale
        # frame left in the buffer, or nothing at all. The ZGW defers the actual
        # reset briefly after 51 01 so Ethernet can transmit the response, so wait
        # before reconnecting or we can reconnect to the old pre-reset application.
        req = b"\x11\x01"
        self.log(f"TX ECUReset hardReset {reason}: {bytes_to_hex(req)}")
        try:
            resp = client.send_uds(req, timeout=POST_RESET_RESPONSE_TIMEOUT_SECONDS)
            self.log(f"RX ECUReset hardReset {reason}: {bytes_to_hex(resp)}")
        except Exception as exc:
            self.log(f"ECUReset hardReset {reason}: no clean response ({exc})")
        if isinstance(client, DoipClient):
            self.log(
                f"ECUReset hardReset {reason}: waiting {POST_RESET_RECONNECT_DELAY_SECONDS:.1f} s "
                "before DoIP reconnect"
            )
            if self.worker_stop.wait(POST_RESET_RECONNECT_DELAY_SECONDS):
                raise FcdError(f"ECUReset hardReset {reason}: cancelled by Disconnect button")
            self.reconnect_doip(client, reason, require_current_client=require_current_client)
            self._wait_for_post_reset_uds_ready(client, reason, require_current_client=require_current_client)

    def _recover_doip_after_reset(self, client, reason, require_current_client=True):
        if not isinstance(client, DoipClient):
            return
        self.log(
            f"ECUReset hardReset {reason}: waiting {POST_RESET_RECONNECT_DELAY_SECONDS:.1f} s "
            "before DoIP reconnect"
        )
        if self.worker_stop.wait(POST_RESET_RECONNECT_DELAY_SECONDS):
            raise FcdError(f"ECUReset hardReset {reason}: cancelled by Disconnect button")
        self.reconnect_doip(client, reason, require_current_client=require_current_client)
        self._wait_for_post_reset_uds_ready(client, reason, require_current_client=require_current_client)

    def _wait_for_post_reset_uds_ready(self, client, reason, timeout=POST_RESET_UDS_READY_TIMEOUT_SECONDS, require_current_client=True):
        request = bytes([0x10, SESSION_DEFAULT])
        deadline = time.monotonic() + float(timeout)
        attempt = 0
        last_error = None

        if not isinstance(client, DoipClient):
            return

        self.log(f"Post-reset UDS readiness after {reason}: waiting up to {timeout:.1f} s")

        while not self._reconnect_cancelled(client, require_current_client=require_current_client):
            remaining = deadline - time.monotonic()
            if remaining <= 0.0:
                break

            attempt += 1
            try:
                if attempt == 1 or attempt % 5 == 0:
                    self.log(f"TX Post-reset Default Session probe: {bytes_to_hex(request)}")
                response = client.send_uds(request, timeout=max(0.5, min(2.0, remaining)))
                if attempt == 1 or attempt % 5 == 0:
                    self.log(f"RX Post-reset Default Session probe: {bytes_to_hex(response)}")
                require_positive_response(response, 0x10)
                self.log(f"Post-reset UDS ready after {reason}")
                return
            except Exception as exc:
                last_error = exc
                if attempt == 1 or attempt % 5 == 0:
                    self.log(f"Post-reset UDS not ready after {reason}: attempt {attempt} failed ({exc})")

                client.close()
                self.root.after(0, self._set_connected_status, False)

                remaining = deadline - time.monotonic()
                if remaining <= 0.0:
                    break
                if self.worker_stop.wait(min(POST_RESET_UDS_READY_RETRY_SECONDS, remaining)):
                    raise FcdError(f"Post-reset UDS readiness after {reason}: cancelled by Disconnect button")

                try:
                    self.reconnect_doip(
                        client,
                        f"{reason} UDS readiness",
                        deadline=deadline,
                        require_current_client=require_current_client,
                    )
                except FcdError as reconnect_exc:
                    last_error = reconnect_exc
                    break

        raise FcdError(f"Post-reset UDS did not become ready after {reason}: {last_error}")

    def _parse_read_coding_result(self, result_resp):
        # Strip 0x71 + sub-function echo + 2-byte routine id, then the 10-byte status
        # block (CodingApp_FillRoutineResponse); the remainder is the coding bitmask.
        routine_data = result_resp[4:] if len(result_resp) > 4 else b""
        if len(routine_data) < CODING_ROUTINE_STATUS_LEN:
            raise FcdError(
                f"Read Coding routine returned {len(routine_data)} bytes; "
                f"expected at least {CODING_ROUTINE_STATUS_LEN} status bytes"
            )
        return routine_data[CODING_ROUTINE_STATUS_LEN:]

    def _coding_routine_status(self, response, label):
        require_positive_response(response, 0x31)
        if len(response) < 5:
            raise FcdError(f"{label} returned no CodingApp routine status byte")
        return response[4]

    def _describe_coding_routine_response(self, response):
        if not isinstance(response, bytes) or len(response) < 14:
            return f"raw={bytes_to_hex(response) if isinstance(response, bytes) else response}"

        status = response[4]
        state = response[5]
        validation = response[6]
        dirty = response[7]
        rx_expected = u16_be(response, 8)
        generation = u32_be(response, 10)
        return (
            f"status={CODING_ROUTINE_STATUS_TEXT.get(status, int_hex(status, 2))}, "
            f"state={CODING_STATE_TEXT.get(state, int_hex(state, 2))}, "
            f"validation={CODING_VALIDATION_TEXT.get(validation, int_hex(validation, 2))}, "
            f"dirty={dirty}, rxExpected={rx_expected}, generation=0x{generation:08X}"
        )

    def _decode_coding_status_payload(self, payload):
        if len(payload) < 20:
            return f"short payload len={len(payload)} data={bytes_to_hex(payload)}"

        initialized = payload[0]
        state = payload[1]
        validation = payload[2]
        dirty = payload[3]
        rx_count = u16_be(payload, 4)
        rx_expected = u16_be(payload, 6)
        nv_len = u16_be(payload, 8)
        generation = u32_be(payload, 10)
        validation_counter = u32_be(payload, 14)
        last_nvm_result = payload[18]
        pending_job = payload[19]
        text = (
            f"initialized={initialized}, "
            f"state={CODING_STATE_TEXT.get(state, int_hex(state, 2))}, "
            f"validation={CODING_VALIDATION_TEXT.get(validation, int_hex(validation, 2))}, "
            f"dirty={dirty}, rxCount={rx_count}, rxExpected={rx_expected}, "
            f"nvLen={nv_len}, generation=0x{generation:08X}, "
            f"validationCounter={validation_counter}, "
            f"lastNvMResult={NVM_RESULT_TEXT.get(last_nvm_result, int_hex(last_nvm_result, 2))}, "
            f"pendingNvMJob={CODING_NVM_JOB_TEXT.get(pending_job, int_hex(pending_job, 2))}"
        )

        if len(payload) >= 39:
            nvm_status = payload[20]
            fee_status = payload[21]
            fee_result = payload[22]
            fls_status = payload[23]
            fls_result = payload[24]
            poll_count = payload[25]
            poll_limit = payload[26]
            active_index = u16_be(payload, 27)
            active_block = u16_be(payload, 29)
            blocks_planned = u16_be(payload, 31)
            blocks_started = u16_be(payload, 33)
            blocks_written = u16_be(payload, 35)
            blocks_failed = u16_be(payload, 37)
            text += (
                f", NvM={NVM_STATUS_TEXT.get(nvm_status, int_hex(nvm_status, 2))}, "
                f"Fee={MEMIF_STATUS_TEXT.get(fee_status, int_hex(fee_status, 2))}/"
                f"{MEMIF_JOB_TEXT.get(fee_result, int_hex(fee_result, 2))}, "
                f"Fls={MEMIF_STATUS_TEXT.get(fls_status, int_hex(fls_status, 2))}/"
                f"{MEMIF_JOB_TEXT.get(fls_result, int_hex(fls_result, 2))}, "
                f"pendingPolls={poll_count}/{poll_limit}, "
                f"writeAllActiveIndex={active_index}, writeAllActiveBlock={active_block}, "
                f"blocks planned/started/written/failed="
                f"{blocks_planned}/{blocks_started}/{blocks_written}/{blocks_failed}"
            )

        return text

    def _read_coding_app_status(self, send, prefix, required=False):
        try:
            response = send(
                b"\x22" + struct.pack(">H", CODING_DID_STATUS),
                f"{prefix}: Read CodingApp Status F1C0",
                timeout=2.0,
            )
            if getattr(send, "dry_run", False):
                self.log(f"DRY {prefix}: CodingApp status decode skipped")
                return
            payload = response[3:] if len(response) >= 3 else b""
            self.log(f"{prefix}: CodingApp status decoded: {self._decode_coding_status_payload(payload)}")
        except Exception as exc:
            self.log(f"{prefix}: CodingApp status read failed: {exc}")
            if required:
                raise FcdError(f"{prefix}: CodingApp status read failed after reset") from exc

    def _ensure_coding_session_for_routine(self, send, label, timeout=3.0):
        if getattr(send, "dry_run", False):
            return
        try:
            active_session = self._read_active_diag_session(send, label, timeout=2.0)
            if active_session == SESSION_CODING_REQUESTED:
                return
            self.log(
                f"{label}: active session is 0x{active_session:02X}; "
                "requesting coding session before RoutineControl"
            )
        except Exception as exc:
            self.log(f"{label}: active session check failed ({exc}); requesting coding session")
        self._ensure_session(send, SESSION_CODING_REQUESTED, f"{label}: Coding Session requested as 10 41", timeout=timeout)

    def _run_coding_routine(self, send, rid, option, label, timeout=60.0, max_pending_polls=10):
        self._ensure_coding_session_for_routine(send, label)
        start_resp = send(
            b"\x31\x01" + struct.pack(">H", rid) + bytes(option),
            f"RoutineControl {rid:04X} {label} start",
            timeout=timeout,
        )
        if isinstance(start_resp, bytes) and not start_resp:
            self.log(f"{label}: no routine response accepted; continuing")
            return start_resp
        if getattr(send, "dry_run", False):
            self.log(f"DRY {label}: CodingApp routine status polling skipped")
            return start_resp
        status = self._coding_routine_status(start_resp, label)
        last_resp = start_resp
        deadline = time.monotonic() + timeout
        pending_polls = 0

        while status == CODING_ROUTINE_STATUS_PENDING:
            if self.worker_stop.is_set():
                raise FcdError(f"{label} cancelled")
            if max_pending_polls is not None and pending_polls >= max_pending_polls:
                raise FcdError(
                    f"{label} stayed pending after {pending_polls} result polls; "
                    f"{self._describe_coding_routine_response(last_resp)}"
                )
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise FcdError(f"{label} did not finish before timeout")
            time.sleep(min(1.0, remaining))
            try:
                self._ensure_coding_session_for_routine(send, label)
                result_resp = send(
                    b"\x31\x03" + struct.pack(">H", rid),
                    f"RoutineControl {rid:04X} {label} result",
                    timeout=max(1.0, min(2.0, remaining)),
                )
                pending_polls += 1
                last_resp = result_resp
            except TimeoutError:
                pending_polls += 1
                self.log(f"{label}: result poll timed out; still waiting")
                continue
            except NegativeResponse as exc:
                if exc.sid == 0x31 and exc.nrc == 0x7F:
                    raise FcdError(f"{label}: coding session expired while routine was pending") from exc
                raise
            status = self._coding_routine_status(result_resp, label)

        if status not in (CODING_ROUTINE_STATUS_OK, CODING_ROUTINE_STATUS_NOT_CHANGED):
            raise FcdError(f"{label} failed with CodingApp routine status 0x{status:02X}")

        return start_resp

    def _selected_vehicle_targets(self):
        if not self.node_rows:
            self.discover_nodes_clicked()
        self._sync_coding_tabs_from_nodes()
        targets = [dict(target) for target in self.node_rows.values() if target.get("simulated_enabled", True)]
        if not targets:
            targets = [{
                "node_name": "ZGW",
                "bus_type": "ETHERNET",
                "is_zgw": True,
                "route_metadata": {"target_logical_address": self.target_var.get()},
            }]
        return targets

    def _target_logical_address(self, target):
        return parse_int(
            target.get("route_metadata", {}).get("target_logical_address")
            or self.target_var.get()
            or int_hex(DEFAULT_TARGET_ADDR)
        )

    def _target_is_simulated(self, target):
        node_kind = target.get("node_kind")
        if node_kind is not None:
            return str(node_kind).strip().lower() == "simulated"
        if bool(target.get("is_zgw")):
            return False
        return bool(target.get("simulated_enabled", True))

    def _strict_response_for_target(self, target, strict_response):
        return bool(strict_response) and not self._target_is_simulated(target)

    def _connected_target_is_simulated(self):
        try:
            current_target = parse_int(self.target_var.get() or int_hex(DEFAULT_TARGET_ADDR))
        except Exception:
            current_target = DEFAULT_TARGET_ADDR
        for target in self.node_rows.values():
            try:
                target_addr = self._target_logical_address(target)
            except Exception:
                continue
            if target_addr == current_target:
                return self._target_is_simulated(target)
        return False

    def _run_vehicle_coding_action_once(self, action_name, worker, strict_response=False):
        targets = self._selected_vehicle_targets()
        target_by_name = {target.get("node_name", ""): target for target in targets}
        schedule = build_schedule(targets, include_flash=False, include_coding=True)
        coding_events = [event for event in schedule if event.phase == "coding"]

        keepalive_was_on = bool(self.keepalive_var.get()) or self.keepalive_is_running()
        if keepalive_was_on:
            self.stop_keepalive(update_var=False)
            self.log(f"{action_name}: paused automatic tester present")

        lanes = {}
        zgw_tasks = []
        for slot in sorted({event.time_slot for event in coding_events}):
            work_by_bus = {}
            zgw_work = []
            for event in coding_events:
                if event.time_slot != slot or event.node_name not in target_by_name:
                    continue
                target = target_by_name[event.node_name]
                item = (event, target)
                if self._target_is_zgw(target):
                    zgw_work.append(item)
                    continue
                bus = bus_category(self._work_item_bus_type(item))
                work_by_bus.setdefault(bus, []).append(item)
            for bus, work in work_by_bus.items():
                lanes.setdefault(bus, []).append((slot, work))
            if zgw_work:
                zgw_tasks.append((slot, zgw_work))

        def handle_worker_exception(event, target, exc):
            target_strict = self._strict_response_for_target(target, strict_response)
            if isinstance(exc, NodeTimeout):
                if target_strict:
                    raise FcdError(f"{action_name}: {event.node_name} node timeout ({exc})") from exc
                self.log(f"{action_name}: {event.node_name} node timeout; skipped ({exc})")
                return
            if target_strict:
                raise FcdError(
                    f"{action_name}: {event.node_name} did not respond or failed ({exc})"
                ) from exc
            self.log(f"{action_name}: {event.node_name} did not respond or failed; skipped ({exc})")

        def run_waiting_work(work):
            with ThreadPoolExecutor(max_workers=max(1, min(len(work), PARALLEL_BUNDLE_MAX_WORKERS))) as executor:
                future_to_event = {
                    executor.submit(worker, target): event
                    for event, target in work
                }
                for future in as_completed(future_to_event):
                    event = future_to_event[future]
                    target = target_by_name.get(event.node_name, {})
                    try:
                        future.result()
                    except Exception as exc:
                        handle_worker_exception(event, target, exc)

        shared_paced_client = None

        def run_lane(bus, tasks):
            for slot, work in tasks:
                if not work:
                    continue
                if len(work) > 1:
                    nodes = ", ".join(
                        f"{self._work_item_node_name(item)}({self._work_item_bus_type(item)})"
                        for item in work
                    )
                    self.log(f"{action_name}: slot {slot} bus={bus} parallel nodes={nodes}")
                if (
                    self.transport_var.get() == "DoIP"
                    and shared_paced_client is not None
                    and self._can_run_paced_coding_slot(action_name, work)
                ):
                    if action_name == "Read Coding":
                        self._run_paced_read_coding_slot(slot, work, shared_paced_client)
                    elif action_name == "Load Coding Default":
                        self._run_paced_load_default_coding_slot(slot, work, shared_paced_client)
                    elif action_name == "Check Coding":
                        self._run_paced_check_coding_slot(slot, work, shared_paced_client)
                    elif action_name == "Code Vehicle":
                        self._run_paced_code_vehicle_slot(slot, work, shared_paced_client)
                    continue
                run_waiting_work(work)

        try:
            if self.transport_var.get() != "DoIP":
                self.coding_shared_client = self.require_client()
            elif lanes and any(
                self._can_run_paced_coding_slot(action_name, work)
                for tasks in lanes.values()
                for _slot, work in tasks
            ):
                shared_paced_client = self._new_parallel_client(parse_int(self.target_var.get() or int_hex(DEFAULT_TARGET_ADDR)))
                shared_paced_client.request_spacing_seconds = max(shared_paced_client.request_spacing_seconds, REQUEST_SPACING_SECONDS)
                shared_paced_client.drain()
                self.log(f"{action_name}: using one DoIP TCP connection for routed bus schedule")

            if lanes:
                lane_text = ", ".join(f"{bus}:{len(tasks)} slot(s)" for bus, tasks in sorted(lanes.items()))
                self.log(f"{action_name}: bus lanes={lane_text}")
                if len(lanes) == 1:
                    bus, tasks = next(iter(lanes.items()))
                    run_lane(bus, tasks)
                else:
                    with ThreadPoolExecutor(max_workers=max(1, min(len(lanes), PARALLEL_BUNDLE_MAX_WORKERS))) as executor:
                        futures = [
                            executor.submit(run_lane, bus, tasks)
                            for bus, tasks in lanes.items()
                        ]
                        for future in as_completed(futures):
                            future.result()

            for slot, work in zgw_tasks:
                if self.worker_stop.is_set():
                    break
                self.log(f"{action_name}: slot {slot} ZGW last/alone")
                for item in work:
                    run_waiting_work([item])
        finally:
            if shared_paced_client is not None:
                shared_paced_client.close()
            self.coding_shared_client = None
            if keepalive_was_on and not self.worker_stop.is_set():
                self.start_keepalive()

    def _run_vehicle_coding_action(self, action_name, worker):
        self.worker(action_name, lambda: self._run_vehicle_coding_action_once(action_name, worker))

    def _target_label(self, target):
        return target.get("node_name") or "ZGW"

    def _target_is_zgw(self, target):
        return bool(target.get("is_zgw")) or self._target_label(target).upper() == "ZGW"

    def _can_pace_uds_target(self, target):
        """A routed bus node (simulated, CAN/CAN-FD/LIN) that can be addressed on the
        fire-and-forget paced schedule - the same gate Coding uses for routed slots."""
        if self._target_is_zgw(target):
            return False
        if not self._target_is_simulated(target):
            return False
        return bus_category(target.get("bus_type", "UNKNOWN")) in ("CAN", "CANFD", "LIN")

    def _uds_rounds_from_requests(self, requests):
        """Turn a flat ``[(request_bytes, label_suffix), ...]`` list into paced rounds
        for _run_paced_routed_coding_rounds: one round per request, spaced
        ROUTED_READ_CODING_SERVICE_GAP_SECONDS (100 ms) apart so a node is never
        re-addressed sooner than that, and within a round the (up to two) nodes per bus
        are staggered by ROUTED_READ_CODING_NODE_STAGGER_SECONDS (5 ms)."""
        rounds = []
        for idx, (req_bytes, label_suffix) in enumerate(requests):
            rounds.append((
                idx * ROUTED_READ_CODING_SERVICE_GAP_SECONDS,
                (lambda rb: (lambda _target, _item: bytes(rb)))(req_bytes),
                (lambda ls: (lambda target, _item: f"{target.get('node_name', '')}: {ls}"))(label_suffix),
            ))
        return tuple(rounds)

    def _default_uds_zgw_worker(self, requests):
        """Waiting per-target worker for the ZGW and any real node: fire each request in
        order and accept no response (so the routed relay / direct response is decoded
        when present, but a silent node does not stall the action)."""
        def worker(send, target, client):
            node = self._target_label(target)
            for req_bytes, label_suffix in requests:
                send(
                    bytes(req_bytes),
                    f"{node}: {label_suffix}",
                    timeout=max(5.0, float(self.timeout_var.get())),
                    allow_no_response=True,
                )
        return worker

    def _run_vehicle_uds_action_once(self, action_name, requests, zgw_worker=None, on_complete=None, strict_response=False):
        """Drive a one-shot UDS action across every selected vehicle target using the
        same routing rules - and the same pacing - as Coding/Flashing.

        ``requests`` is a flat ``[(request_bytes, label_suffix), ...]`` list. fcd_parallel
        .build_schedule slots the nodes two-per-bus; in each slot the simulated
        CAN/CAN-FD/LIN bus nodes are fired on the shared paced schedule (two nodes per bus
        together ~5 ms apart, 100 ms before a node is re-addressed), and the ZGW plus any
        real node fall back to waiting sends via ``zgw_worker`` (default: fire each request
        and accept no response) so their responses can still be decoded."""
        targets = self._selected_vehicle_targets()
        target_by_name = {target.get("node_name", ""): target for target in targets}
        schedule = build_schedule(targets, include_flash=False, include_coding=True)
        events = [event for event in schedule if event.phase == "coding"]
        rounds = self._uds_rounds_from_requests(requests)
        worker_fn = zgw_worker if zgw_worker is not None else self._default_uds_zgw_worker(requests)

        def run_waiting(target):
            client = self._coding_worker_client(target)
            try:
                send = self._make_target_uds_sender(
                    client,
                    target,
                    strict_response=self._strict_response_for_target(target, strict_response),
                )
                worker_fn(send, target, client)
            finally:
                self._release_coding_worker_client(client)

        keepalive_was_on = bool(self.keepalive_var.get()) or self.keepalive_is_running()
        if keepalive_was_on:
            self.stop_keepalive(update_var=False)
            self.log(f"{action_name}: paused automatic tester present")

        lanes = {}
        zgw_tasks = []
        for slot in sorted({event.time_slot for event in events}):
            work_by_bus = {}
            zgw_work = []
            for event in events:
                if event.time_slot != slot:
                    continue
                target = target_by_name.get(event.node_name)
                if target is None:
                    continue
                item = (event, target)
                if self._target_is_zgw(target):
                    zgw_work.append(item)
                    continue
                bus = bus_category(self._work_item_bus_type(item))
                work_by_bus.setdefault(bus, []).append(item)
            for bus, work in work_by_bus.items():
                lanes.setdefault(bus, []).append((slot, work))
            if zgw_work:
                zgw_tasks.append((slot, zgw_work))

        def handle_waiting_exception(target, exc):
            if isinstance(exc, NodeTimeout):
                if self._strict_response_for_target(target, strict_response):
                    raise FcdError(f"{action_name}: {self._target_label(target)} node timeout ({exc})") from exc
                self.log(f"{action_name}: {self._target_label(target)} node timeout; skipped ({exc})")
                return
            if self._strict_response_for_target(target, strict_response):
                raise FcdError(
                    f"{action_name}: {self._target_label(target)} did not respond or failed ({exc})"
                ) from exc
            self.log(f"{action_name}: {self._target_label(target)} did not respond or failed; skipped ({exc})")

        def run_waiting_item(item):
            _event, target = item
            try:
                run_waiting(target)
            except Exception as exc:
                handle_waiting_exception(target, exc)

        def run_waiting_items(work):
            for item in work:
                if self.worker_stop.is_set():
                    break
                run_waiting_item(item)

        shared_paced_client = None

        def run_uds_lane(bus, tasks):
            for slot, work in tasks:
                if self.worker_stop.is_set():
                    break
                paced_work = [item for item in work if self._can_pace_uds_target(item[1])]
                waiting_work = [item for item in work if not self._can_pace_uds_target(item[1])]

                if paced_work and self.transport_var.get() == "DoIP" and shared_paced_client is not None:
                    nodes = ", ".join(
                        f"{self._work_item_node_name(item)}({self._work_item_bus_type(item)})"
                        for item in paced_work
                    )
                    self.log(f"{action_name}: slot {slot} bus={bus} routed nodes={nodes}")
                    self._run_paced_routed_coding_rounds(paced_work, shared_paced_client, rounds)
                elif paced_work:
                    waiting_work = work

                if waiting_work:
                    self.log(
                        f"{action_name}: slot {slot} bus={bus} waiting nodes="
                        f"{', '.join(self._target_label(target) for _event, target in waiting_work)}"
                    )
                    run_waiting_items(waiting_work)

        try:
            if self.transport_var.get() != "DoIP":
                # Non-DoIP transport has no ZGW forwarder, so there is nothing to
                # route through; run every target against the single connected
                # client (extended-address prefixes are still applied but harmless).
                self.coding_shared_client = self.require_client()
            elif lanes and any(
                self._can_pace_uds_target(item[1])
                for tasks in lanes.values()
                for _slot, work in tasks
                for item in work
            ):
                shared_paced_client = self._new_parallel_client(parse_int(self.target_var.get() or int_hex(DEFAULT_TARGET_ADDR)))
                shared_paced_client.request_spacing_seconds = max(shared_paced_client.request_spacing_seconds, REQUEST_SPACING_SECONDS)
                shared_paced_client.drain()
                self.log(f"{action_name}: using one DoIP TCP connection for routed bus schedule")

            if lanes:
                lane_text = ", ".join(f"{bus}:{len(tasks)} slot(s)" for bus, tasks in sorted(lanes.items()))
                self.log(f"{action_name}: bus lanes={lane_text}")
                if len(lanes) == 1:
                    bus, tasks = next(iter(lanes.items()))
                    run_uds_lane(bus, tasks)
                else:
                    with ThreadPoolExecutor(max_workers=max(1, min(len(lanes), PARALLEL_BUNDLE_MAX_WORKERS))) as executor:
                        futures = [
                            executor.submit(run_uds_lane, bus, tasks)
                            for bus, tasks in lanes.items()
                        ]
                        for future in as_completed(futures):
                            future.result()

            for slot, work in zgw_tasks:
                if self.worker_stop.is_set():
                    break
                self.log(f"{action_name}: slot {slot} ZGW last/alone")
                run_waiting_items(work)
        finally:
            if shared_paced_client is not None:
                shared_paced_client.close()
            self.coding_shared_client = None
            if keepalive_was_on and not self.worker_stop.is_set():
                self.start_keepalive()
            if on_complete is not None:
                try:
                    on_complete()
                except Exception as exc:
                    self.log(f"{action_name}: result handling failed ({exc})")

    def _run_vehicle_uds_action(self, action_name, requests, zgw_worker=None, on_complete=None):
        self.worker(
            action_name,
            lambda: self._run_vehicle_uds_action_once(action_name, requests, zgw_worker, on_complete),
        )

    def _raw_target_worker(self, send, target, client, request):
        node = self._target_label(target)
        response = send(
            bytes(request),
            f"{node}: Raw",
            timeout=float(self.timeout_var.get()),
            allow_no_response=True,
        )
        self.root.after(0, self._append_raw_response, bytes(request), response or b"", node)

    def _routed_dtc_detail_read(self, send, node, dtc, subfunction, detail_name, strict_response=False):
        request = bytes([
            0x19,
            subfunction,
            (dtc >> 16) & 0xFF,
            (dtc >> 8) & 0xFF,
            dtc & 0xFF,
            0xFF,
        ])
        label = f"{node}: 19 {subfunction:02X} {dtc:06X} FF {detail_name}"
        try:
            response = send(
                request,
                label,
                timeout=max(FAULT_MEMORY_DETAIL_TIMEOUT_SECONDS, float(self.timeout_var.get())),
                allow_no_response=not strict_response,
            )
        except NegativeResponse as exc:
            if strict_response:
                raise
            text = f"NRC 0x{exc.nrc:02X} {NRC_TEXT.get(exc.nrc, 'Unknown')}"
            self.log(f"{label}: {text}")
            return text
        except Exception as exc:
            if self.worker_stop.is_set() or strict_response:
                raise
            self.log(f"{label}: {exc}")
            return f"ERROR: {exc}"
        if not response:
            if strict_response:
                raise FcdError(f"{label}: no response")
            return "NO RESPONSE ACCEPTED"
        try:
            return self._decode_dtc_detail_response(label, response, dtc, subfunction)
        except Exception as exc:
            if strict_response:
                raise
            self.log(f"{label}: {exc}")
            return f"ERROR: {exc}"

    def _combine_snapshot_data_text(self, *parts):
        unique = []
        skipped = []
        for part in parts:
            text = str(part or "").strip()
            if not text:
                continue
            if text == "NO RESPONSE ACCEPTED":
                skipped.append(text)
                continue
            if text not in unique:
                unique.append(text)
        if unique:
            return "   |   ".join(unique)
        if skipped:
            return skipped[0]
        return ""

    def _read_dtc_snapshot_data(self, send, target, node, dtc, strict_response=False):
        snapshot_data = self._routed_dtc_detail_read(
            send,
            node,
            dtc,
            0x04,
            "snapshot data",
            strict_response=strict_response,
        )

        if self._target_is_zgw(target):
            return snapshot_data

        legacy_record = self._routed_dtc_detail_read(
            send,
            node,
            dtc,
            0x06,
            "snapshot data record 0x06",
            strict_response=strict_response,
        )
        return self._combine_snapshot_data_text(snapshot_data, legacy_record)

    def _read_fault_memory_target_worker(self, send, target, client, status_mask, sink, strict_response=False):
        node = self._target_label(target)
        rows = []
        summaries = []
        send(
            bytes([0x10, SESSION_EXTENDED]),
            f"{node}: Extended Session for fault-memory read",
            timeout=10.0,
            allow_no_response=not strict_response,
        )
        for request, label in [
            (bytes([0x19, 0x01, 0xFF]), f"{node}: 19 01 FF reportNumberOfDTCByStatusMask"),
            (bytes([0x19, 0x02, status_mask]), f"{node}: 19 02 {status_mask:02X} reportDTCByStatusMask"),
        ]:
            try:
                response = send(
                    request,
                    label,
                    timeout=max(2.0, float(self.timeout_var.get())),
                    allow_no_response=not strict_response,
                )
            except NegativeResponse as exc:
                if strict_response:
                    raise
                self.log(f"{label}: {exc}")
                continue
            if not response:
                if strict_response:
                    raise FcdError(f"{label}: no response")
                continue
            try:
                parsed_rows, summary = self._decode_read_dtc_response(label, response)
            except FcdError as exc:
                if strict_response:
                    raise
                self.log(f"{label}: {exc}")
                continue
            for row in parsed_rows:
                row["node"] = node
            rows.extend(parsed_rows)
            summaries.append(f"{node}: {summary}")
            if request[1] == 0x02:
                for row in parsed_rows:
                    dtc = row["dtc_value"]
                    row["snapshot_data"] = self._read_dtc_snapshot_data(
                        send,
                        target,
                        node,
                        dtc,
                        strict_response=strict_response,
                    )
        if rows or summaries:
            with sink["lock"]:
                sink["rows"].extend(rows)
                sink["summaries"].extend(summaries)

    def _can_run_paced_coding_slot(self, action_name, work):
        if action_name not in ("Read Coding", "Load Coding Default", "Check Coding", "Code Vehicle"):
            return False
        if not work:
            return False
        for _event, target in work:
            node_name = str(target.get("node_name", ""))
            if bool(target.get("is_zgw")) or node_name.upper() == "ZGW":
                return False
            if not self._target_is_simulated(target):
                return False
            if bus_category(target.get("bus_type", "UNKNOWN")) not in ("CAN", "CANFD", "LIN"):
                return False
        return True

    def _schedule_event_value(self, event, key, default=""):
        if isinstance(event, dict):
            return event.get(key, default)
        return getattr(event, key, default)

    def _work_item_bus_type(self, item):
        event = item[0]
        target = item[1]
        return target.get("bus_type") or self._schedule_event_value(event, "bus_type", "UNKNOWN")

    def _work_item_node_name(self, item):
        event = item[0]
        target = item[1]
        return target.get("node_name") or self._schedule_event_value(event, "node_name", "")

    def _sleep_until_monotonic(self, deadline):
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return
            time.sleep(min(remaining, 0.005))

    def _scheduled_routed_due(self, bus, desired_due, next_due_by_bus):
        if bus != "LIN":
            return desired_due
        next_due = next_due_by_bus.get(bus)
        if next_due is None:
            with self.routed_bus_pace_lock:
                last_bus = self.routed_bus_last_request_ts.get(bus, 0.0)
            next_due = last_bus + ROUTED_LIN_REQUEST_SPACING_SECONDS
        due = max(desired_due, next_due)
        next_due_by_bus[bus] = due + ROUTED_LIN_REQUEST_SPACING_SECONDS
        return due

    def _send_routed_uds_no_wait(self, client, target, request, label, apply_pacing=True):
        node_name = target.get("node_name", "")
        extended = target.get("extended_diag_address", "")
        if not extended:
            extended = default_extended_diag_address(node_name, target.get("bus_type", "UNKNOWN"))
            target["extended_diag_address"] = extended
        extended_byte = parse_int(extended) & 0xFF
        prefixed_request = bytes([extended_byte]) + bytes(request)
        bus = bus_category(target.get("bus_type", "UNKNOWN"))

        if apply_pacing:
            self._pace_routed_bus_request(target)
        self.log(f"TX {label}: ext={int_hex(extended_byte, 2)} {uds_request_log_text(prefixed_request)}")
        client.send_uds_no_wait(prefixed_request)
        with self.routed_bus_pace_lock:
            self.routed_bus_last_request_ts[bus] = time.monotonic()
            self.routed_node_last_request_ts[(bus, extended_byte)] = time.monotonic()
        self.log(f"RX {label}: waiting for ZGW transport complete")
        return prefixed_request

    def _wait_routed_transport_acks(self, client, expected_requests):
        if not expected_requests:
            return
        wait_requests = [
            expected for expected in expected_requests
            if not (len(expected) > 2 and bool(expected[2]))
        ]
        for expected in expected_requests:
            if len(expected) > 2 and bool(expected[2]):
                label = expected[0]
                self.log(f"RX {label}: simulated target no response accepted")
        if not wait_requests:
            return
        completed = client.recv_routed_transport_acks(
            wait_requests,
            timeout=ROUTED_TRANSPORT_ACK_TIMEOUT_SECONDS,
        )
        for label, response in completed:
            if len(response) >= 4 and response[1] == 0x7F:
                nrc_text = NRC_TEXT.get(response[3], "Unknown")
                self.log(
                    f"RX {label}: simulated target transport failure accepted "
                    f"NRC 0x{response[3]:02X} ({nrc_text})"
                )
            else:
                self.log(f"RX {label}: ZGW transport complete {uds_request_log_text(response)}")

    def _wait_routed_transport_acks_and_progress(self, client, expected_requests, progress_cb=None):
        self._wait_routed_transport_acks(client, expected_requests)
        if progress_cb is None:
            return
        for expected in expected_requests:
            progress_count = expected[3] if len(expected) > 3 else 0
            if progress_count:
                progress_cb(progress_count)

    def _paced_grouped_no_wait_rounds(self, groups, client, rounds, progress_cb=None, drain_every_sends=0):
        bus_order = {"CAN": 0, "CANFD": 1, "LIN": 2}
        ordered_buses = sorted(groups, key=lambda bus: bus_order.get(bus, 99))
        if not ordered_buses:
            return 0

        max_depth = max(len(items) for items in groups.values())
        sends_since_drain = 0
        start_time = time.monotonic()
        next_due_by_bus = {}
        events = []
        order = 0

        for round_offset, request_factory, label_factory in rounds:
            for node_index in range(max_depth):
                for bus in ordered_buses:
                    items = groups[bus]
                    if node_index >= len(items):
                        continue
                    item = items[node_index]
                    target = item[1]
                    request = request_factory(target, item)
                    if request is None:
                        continue
                    desired_due = start_time + round_offset + (node_index * ROUTED_READ_CODING_NODE_STAGGER_SECONDS)
                    due = self._scheduled_routed_due(bus, desired_due, next_due_by_bus)
                    progress_count = 0
                    if len(item) > 4:
                        progress_count = item[4] or 0
                    label = label_factory(target, item)
                    events.append((
                        due,
                        bus_order.get(bus, 99),
                        node_index,
                        order,
                        bus,
                        target,
                        bytes(request),
                        label,
                        progress_count,
                    ))
                    order += 1

        expected_requests = []
        for due, _bus_rank, _node_index, _order, bus, target, request, label, progress_count in sorted(events):
            self._sleep_until_monotonic(due)
            prefixed_request = self._send_routed_uds_no_wait(
                client,
                target,
                request,
                label,
                apply_pacing=(bus != "LIN"),
            )
            # Simulated routed targets are allowed to stay silent after the
            # ZGW-forwarded request, regardless of bus. LIN simulations can
            # sometimes acknowledge one step and then omit the next, which
            # should not fail a non-strict paced read/flash flow.
            no_response_only = self._target_is_simulated(target)
            expected_requests.append((label, prefixed_request, no_response_only, progress_count))
            sends_since_drain += 1
            if drain_every_sends and sends_since_drain >= drain_every_sends:
                self._wait_routed_transport_acks_and_progress(client, expected_requests, progress_cb)
                expected_requests = []
                sends_since_drain = 0

        self._wait_routed_transport_acks_and_progress(client, expected_requests, progress_cb)

        return sends_since_drain

    def _run_paced_routed_coding_rounds(self, work, client, rounds):
        groups = {}
        for item in work:
            groups.setdefault(bus_category(self._work_item_bus_type(item)), []).append(item)
        if not groups:
            return

        self._paced_grouped_no_wait_rounds(groups, client, rounds)

        time.sleep(ROUTED_READ_CODING_DRAIN_SECONDS)
        client.drain()

    def _run_paced_read_coding_slot(self, slot, work, client):
        rid = parse_int(self.coding_read_nvm_rid_var.get())
        rounds = (
            (
                0.0,
                lambda _target, _item: bytes([0x10, SESSION_CODING_REQUESTED]),
                lambda target, _item: f"{target.get('node_name', '')}: Coding Session requested as 10 41",
            ),
            (
                ROUTED_READ_CODING_SERVICE_GAP_SECONDS,
                lambda _target, _item: b"\x31\x01" + struct.pack(">H", rid),
                lambda target, _item: f"{target.get('node_name', '')}: Read Coding start",
            ),
        )

        self._run_paced_routed_coding_rounds(work, client, rounds)

        for _event, target in work:
            node_name = target.get("node_name", "")
            self.root.after(0, self._load_dummy_coding_values_to_coding_tree, node_name)
            self.log(f"Read Coding: {node_name} no coding response accepted; wrote dummy coding values")

    def _run_paced_load_default_coding_slot(self, slot, work, client):
        rid = parse_int(self.coding_defaults_rid_var.get())
        rounds = (
            (
                0.0,
                lambda _target, _item: bytes([0x10, SESSION_CODING_REQUESTED]),
                lambda target, _item: f"{target.get('node_name', '')}: Coding Session requested as 10 41",
            ),
            (
                ROUTED_READ_CODING_SERVICE_GAP_SECONDS,
                lambda _target, _item: b"\x31\x01" + struct.pack(">H", rid),
                lambda target, _item: f"{target.get('node_name', '')}: Load Coding Default",
            ),
        )
        self._run_paced_routed_coding_rounds(work, client, rounds)

    def _run_paced_check_coding_slot(self, slot, work, client):
        rid = parse_int(self.coding_validate_rid_var.get())
        rounds = (
            (
                0.0,
                lambda _target, _item: bytes([0x10, SESSION_CODING_REQUESTED]),
                lambda target, _item: f"{target.get('node_name', '')}: Coding Session requested as 10 41",
            ),
            (
                ROUTED_READ_CODING_SERVICE_GAP_SECONDS,
                lambda _target, _item: b"\x31\x01" + struct.pack(">H", rid),
                lambda target, _item: f"RoutineControl {rid:04X} {target.get('node_name', '')}: Check Coding start",
            ),
        )
        self._run_paced_routed_coding_rounds(work, client, rounds)

    def _run_paced_code_vehicle_slot(self, slot, work, client):
        descriptors = self._all_coding_descriptors()
        read_rid = parse_int(self.coding_read_nvm_rid_var.get())
        prepared = []
        for event, target in work:
            node_name = target.get("node_name", "")
            descriptor = target.get("coding_descriptor") or descriptors.get(str(node_name).upper())
            if not descriptor:
                self.log(f"Code Vehicle: {node_name} has no configured coding values; skipped")
                continue
            mask = hex_to_bytes(descriptor.get("mask_hex", ""))
            if not mask:
                self.log(f"Code Vehicle: {node_name} coding mask is empty; skipped")
                continue
            try:
                write_rid = parse_int(descriptor.get("write_routine"))
                validate_rid = parse_int(descriptor.get("validate_routine"))
            except Exception as exc:
                self.log(f"Code Vehicle: {node_name} has invalid routine configuration; skipped ({exc})")
                continue
            prepared.append((event, target, write_rid, validate_rid, read_rid, mask))

        if not prepared:
            return

        def target_node_name(target):
            return target.get("node_name", "")

        post_reset_base = (
            7 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS
            + ROUTED_CODE_VEHICLE_POST_RESET_GAP_SECONDS
        )
        rounds = (
            (
                0.0,
                lambda _target, _item: bytes([0x10, SESSION_CODING_REQUESTED]),
                lambda target, _item: f"{target_node_name(target)}: Coding Session requested as 10 41",
            ),
            (
                1 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS,
                lambda _target, _item: b"\x22" + struct.pack(">H", CODING_DID_STATUS),
                lambda target, _item: f"{target_node_name(target)}: Before Coding: Read CodingApp Status F1C0",
            ),
            (
                2 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS,
                lambda _target, _item: b"\x22" + struct.pack(">H", DID_APP_SW_VERSION),
                lambda target, _item: f"{target_node_name(target)}: Before Coding: Read Software Version F101",
            ),
            (
                3 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS,
                lambda _target, _item: b"\x22" + struct.pack(">H", DID_ACTIVE_SW_BLOCK),
                lambda target, _item: f"{target_node_name(target)}: Before Coding: Read Active Software Block F100",
            ),
            (
                4 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS,
                lambda _target, _item: b"\x22" + struct.pack(">H", DID_ACTIVE_DIAG_SESSION),
                lambda target, _item: f"{target_node_name(target)}: Before Coding: Read Active Diagnostic Session F186",
            ),
            (
                5 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS,
                lambda _target, item: b"\x31\x01" + struct.pack(">H", item[2]) + item[5],
                lambda target, item: f"RoutineControl {item[2]:04X} {target_node_name(target)}: Write Coding start",
            ),
            (
                6 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS,
                lambda _target, item: b"\x31\x01" + struct.pack(">H", item[3]),
                lambda target, item: f"RoutineControl {item[3]:04X} {target_node_name(target)}: Check Coding start",
            ),
            (
                7 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS,
                lambda _target, _item: b"\x11\x01",
                lambda target, _item: f"{target_node_name(target)}: ECUReset hardReset after coding",
            ),
            (
                post_reset_base,
                lambda _target, _item: bytes([0x10, SESSION_CODING_REQUESTED]),
                lambda target, _item: f"{target_node_name(target)}: Coding Session after coding reset requested as 10 41",
            ),
            (
                post_reset_base + (1 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS),
                lambda _target, _item: b"\x22" + struct.pack(">H", CODING_DID_STATUS),
                lambda target, _item: f"{target_node_name(target)}: After Coding Reset: Read CodingApp Status F1C0",
            ),
            (
                post_reset_base + (2 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS),
                lambda _target, _item: b"\x22" + struct.pack(">H", DID_APP_SW_VERSION),
                lambda target, _item: f"{target_node_name(target)}: After Coding Reset: Read Software Version F101",
            ),
            (
                post_reset_base + (3 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS),
                lambda _target, _item: b"\x22" + struct.pack(">H", DID_ACTIVE_SW_BLOCK),
                lambda target, _item: f"{target_node_name(target)}: After Coding Reset: Read Active Software Block F100",
            ),
            (
                post_reset_base + (4 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS),
                lambda _target, _item: b"\x22" + struct.pack(">H", DID_ACTIVE_DIAG_SESSION),
                lambda target, _item: f"{target_node_name(target)}: After Coding Reset: Read Active Diagnostic Session F186",
            ),
            (
                post_reset_base + (5 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS),
                lambda _target, _item: bytes([0x10, SESSION_CODING_REQUESTED]),
                lambda target, _item: f"{target_node_name(target)}: Coding Session before Read Coding requested as 10 41",
            ),
            (
                post_reset_base + (6 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS),
                lambda _target, item: b"\x31\x01" + struct.pack(">H", item[4]),
                lambda target, _item: f"{target_node_name(target)}: After Coding Reset: Read Coding start",
            ),
            (
                post_reset_base + (7 * ROUTED_READ_CODING_SERVICE_GAP_SECONDS),
                lambda _target, _item: bytes([0x10, SESSION_DEFAULT]),
                lambda target, _item: f"{target_node_name(target)}: Default Session after coding",
            ),
        )
        self._run_paced_routed_coding_rounds(prepared, client, rounds)
        if self.worker_stop.wait(ROUTED_CODE_VEHICLE_SETTLE_SECONDS):
            raise FcdError("Code Vehicle: cancelled during routed post-reset settle")

    def _coding_worker_client(self, target):
        shared_client = getattr(self, "coding_shared_client", None)
        if shared_client is not None:
            return shared_client
        return self._new_parallel_client(parse_int(self.target_var.get() or int_hex(DEFAULT_TARGET_ADDR)))

    def _release_coding_worker_client(self, client):
        if client is not getattr(self, "coding_shared_client", None):
            client.close()

    def _pace_routed_bus_request(self, target):
        bus = bus_category(target.get("bus_type", "UNKNOWN"))
        if bus == "ETHERNET":
            return
        extended = target.get("extended_diag_address", "")
        if not extended:
            extended = default_extended_diag_address(target.get("node_name", ""), target.get("bus_type", "UNKNOWN"))
            target["extended_diag_address"] = extended
        node_key = (bus, parse_int(extended) & 0xFF)
        now = time.monotonic()
        with self.routed_bus_pace_lock:
            last_node = self.routed_node_last_request_ts.get(node_key, 0.0)
            last_bus = self.routed_bus_last_request_ts.get(bus, 0.0)
        node_spacing = ROUTED_LIN_REQUEST_SPACING_SECONDS if bus == "LIN" else REQUEST_SPACING_SECONDS
        node_wait = node_spacing - (now - last_node)
        bus_spacing = (
            ROUTED_LIN_REQUEST_SPACING_SECONDS
            if bus == "LIN"
            else ROUTED_READ_CODING_NODE_STAGGER_SECONDS
        )
        bus_wait = bus_spacing - (now - last_bus)
        wait = max(node_wait, bus_wait)
        if wait > 0:
            time.sleep(wait)

    def _routed_bus_semaphore(self, target):
        bus = bus_category(target.get("bus_type", "UNKNOWN"))
        if bus == "ETHERNET":
            return None, None
        with self.routed_bus_pace_lock:
            semaphore = self.routed_bus_semaphores.get(bus)
            if semaphore is None:
                semaphore = threading.BoundedSemaphore(ROUTED_BUS_MAX_IN_FLIGHT)
                self.routed_bus_semaphores[bus] = semaphore
        return bus, semaphore

    def _routed_node_semaphore(self, target):
        bus = bus_category(target.get("bus_type", "UNKNOWN"))
        if bus == "ETHERNET":
            return None, None
        extended = target.get("extended_diag_address", "")
        if not extended:
            extended = default_extended_diag_address(target.get("node_name", ""), target.get("bus_type", "UNKNOWN"))
            target["extended_diag_address"] = extended
        key = (bus, parse_int(extended) & 0xFF)
        with self.routed_bus_pace_lock:
            semaphore = self.routed_node_semaphores.get(key)
            if semaphore is None:
                semaphore = threading.BoundedSemaphore(ROUTED_NODE_MAX_IN_FLIGHT)
                self.routed_node_semaphores[key] = semaphore
        return key, semaphore

    def _recover_routed_client_after_accepted_error(self, client, node_name, name, exc):
        if not isinstance(client, DoipClient):
            return True
        if client.connected:
            return True
        self.log(f"{node_name}: reconnecting DoIP after accepted {name} error ({exc})")
        try:
            self.reconnect_doip(
                client,
                f"{node_name} accepted no-response recovery",
                log_success=False,
                deadline=time.monotonic() + ROUTED_RECOVERY_TIMEOUT_SECONDS,
                require_current_client=False,
            )
            return True
        except Exception as reconnect_exc:
            self.log(f"{node_name}: DoIP reconnect after accepted no-response failed ({reconnect_exc})")
            return False

    def _make_target_uds_sender(self, client, target, dry=False, strict_response=False):
        node_name = target.get("node_name", "")
        is_zgw = bool(target.get("is_zgw")) or node_name.upper() == "ZGW"
        simulated = self._target_is_simulated(target)
        target_strict = self._strict_response_for_target(target, strict_response)
        effective_dry = dry
        base_send = self._make_uds_sender(
            client,
            dry=effective_dry,
            accept_no_response=simulated,
        )
        extended = target.get("extended_diag_address", "")
        if is_zgw:
            if not simulated:
                return base_send

            def send_zgw(request, name, timeout=None, allow_no_response=False):
                response = base_send(
                    request,
                    name,
                    timeout=BUS_TARGET_DIAG_TIMEOUT_SECONDS,
                    allow_no_response=allow_no_response or simulated,
                )
                send_zgw.last_nrc78_count = getattr(base_send, "last_nrc78_count", 0)
                return response

            send_zgw.dry_run = getattr(base_send, "dry_run", effective_dry)
            send_zgw.last_nrc78_count = getattr(base_send, "last_nrc78_count", 0)
            return send_zgw
        if not extended:
            extended = default_extended_diag_address(node_name, target.get("bus_type", "UNKNOWN"))
            target["extended_diag_address"] = extended
        extended_byte = parse_int(extended) & 0xFF

        def send(request, name, timeout=None, allow_no_response=False):
            request = bytes(request)
            prefixed_request = bytes([extended_byte]) + request
            accept_no_response = allow_no_response or simulated
            request_timeout = (
                SIMULATED_BUS_TARGET_DIAG_TIMEOUT_SECONDS
                if simulated
                else BUS_TARGET_DIAG_TIMEOUT_SECONDS
            )
            if effective_dry:
                self.log(f"DRY {name}: ext={int_hex(extended_byte, 2)} {uds_request_log_text(prefixed_request)}")
                return b""
            bus, bus_semaphore = self._routed_bus_semaphore(target)
            _node_key, node_semaphore = self._routed_node_semaphore(target)

            def do_send():
                attempt = 0
                while True:
                    self._pace_routed_bus_request(target)
                    retry_note = "" if attempt == 0 else f" retry={attempt}"
                    self.log(f"TX {name}{retry_note}: ext={int_hex(extended_byte, 2)} {uds_request_log_text(prefixed_request)}")
                    try:
                        response = client.send_uds(prefixed_request, timeout=request_timeout, allow_no_response=accept_no_response)
                        send.last_nrc78_count = getattr(client, "last_nrc78_count", 0)
                        response = bytes(response)
                        if not response:
                            self.log(f"RX {name}: no response accepted")
                            return b""
                        self.log(f"RX {name}: {bytes_to_hex(response)}")
                        uds_response = response[1:] if response[:1] == bytes([extended_byte]) else response
                        require_positive_response(uds_response, request[0])
                        return uds_response
                    except NegativeResponse as exc:
                        if target_strict:
                            raise
                        if (
                            accept_no_response
                            and bus in ("CAN", "CANFD")
                            and exc.sid == request[0]
                            and exc.nrc in (0x21, 0x22)
                            and attempt < ROUTED_FORWARD_RETRY_COUNT
                        ):
                            attempt += 1
                            self.log(
                                f"RX {name}: ZGW forward path busy "
                                f"(NRC 0x{exc.nrc:02X}); retry {attempt}/{ROUTED_FORWARD_RETRY_COUNT}"
                            )
                            time.sleep(ROUTED_FORWARD_RETRY_DELAY_SECONDS)
                            continue
                        if accept_no_response:
                            if (
                                bus in ("CAN", "CANFD")
                                and exc.sid == request[0]
                                and exc.nrc in (0x21, 0x22)
                            ):
                                self.log(
                                    f"RX {name}: ZGW forward path still busy after "
                                    f"{attempt} retries; request was not forwarded ({exc})"
                                )
                            else:
                                self.log(f"RX {name}: no response accepted ({exc})")
                            return b""
                        raise NodeTimeout(f"{node_name}: node timeout via extended address {int_hex(extended_byte, 2)}") from exc
                    except Exception as exc:
                        if accept_no_response:
                            self.log(f"RX {name}: no response accepted ({exc})")
                            if not self._recover_routed_client_after_accepted_error(client, node_name, name, exc):
                                raise NodeTimeout(f"{node_name}: DoIP recovery failed after accepted {name} error") from exc
                            return b""
                        raise NodeTimeout(f"{node_name}: node timeout via extended address {int_hex(extended_byte, 2)}") from exc
                    finally:
                        if bus:
                            with self.routed_bus_pace_lock:
                                self.routed_bus_last_request_ts[bus] = time.monotonic()

            if bus_semaphore is None:
                return do_send()
            with node_semaphore:
                with bus_semaphore:
                    return do_send()

        send.dry_run = getattr(base_send, "dry_run", effective_dry)
        send.last_nrc78_count = 0
        return send

    def _request_read_coding_routine(self, send, rid, node_name, phase="", timeout=20.0, max_pending_polls=10):
        prefix = f"{node_name}: {phase}: " if phase else f"{node_name}: "
        start_label = f"{prefix}Read Coding start"
        result_label = f"{prefix}Read Coding result"
        self._ensure_coding_session_for_routine(send, start_label)
        start_resp = send(
            b"\x31\x01" + struct.pack(">H", rid),
            start_label,
            timeout=timeout,
        )
        if (not start_resp) or getattr(send, "dry_run", False):
            return start_resp

        status = self._coding_routine_status(start_resp, start_label)
        if status != CODING_ROUTINE_STATUS_PENDING:
            return start_resp

        self.log(f"{prefix}Read Coding pending; polling result")
        deadline = time.monotonic() + timeout
        pending_polls = 0
        last_resp = start_resp
        while status == CODING_ROUTINE_STATUS_PENDING:
            if self.worker_stop.is_set():
                raise FcdError(f"{prefix}Read Coding cancelled")
            if pending_polls >= max_pending_polls:
                raise FcdError(
                    f"{prefix}Read Coding stayed pending after {pending_polls} result polls; "
                    f"{self._describe_coding_routine_response(last_resp)}"
                )
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise FcdError(f"{prefix}Read Coding did not finish before timeout")
            time.sleep(min(0.2, remaining))
            self._ensure_coding_session_for_routine(send, result_label)
            result_resp = send(
                b"\x31\x03" + struct.pack(">H", rid),
                result_label,
                timeout=max(0.25, min(2.0, remaining)),
            )
            pending_polls += 1
            if not result_resp:
                return b""
            last_resp = result_resp
            status = self._coding_routine_status(result_resp, result_label)

            if status != CODING_ROUTINE_STATUS_PENDING:
                return result_resp

        return last_resp

    def _read_coding_for_target(self, target):
        node_name = target.get("node_name", "")
        rid = parse_int(self.coding_read_nvm_rid_var.get())
        client = self._coding_worker_client(target)
        try:
            send = self._make_target_uds_sender(client, target, dry=False)
            result_resp = self._request_read_coding_routine(send, rid, node_name, timeout=20.0)
            if not result_resp:
                if self._target_is_simulated(target):
                    self.root.after(0, self._load_dummy_coding_values_to_coding_tree, node_name)
                    self.log(f"Read Coding: {node_name} no coding response accepted; wrote dummy coding values")
                else:
                    self.log(f"Read Coding: {node_name} no coding response accepted; skipped result decode")
                return
            coding_payload = self._parse_read_coding_result(result_resp)
            self.root.after(0, self._load_mask_to_coding_tree, coding_payload, f"{node_name} coding routine 0x0203", node_name)
        finally:
            self._release_coding_worker_client(client)

    def _read_post_coding_status_for_target(self, send, target, phase, required=None):
        node_name = target.get("node_name", "")
        prefix = f"{node_name}: {phase}"
        if required is None:
            required = not self._target_is_simulated(target)
        self._read_coding_app_status(send, prefix, required=required)
        self._read_standard_status(send, prefix)

    def _assert_coding_mask_present(self, actual_mask, expected_mask, label):
        actual = bytes(actual_mask)
        expected = bytes(expected_mask)
        if len(actual) < len(expected):
            raise FcdError(
                f"{label}: coding readback too short "
                f"(expected {len(expected)} bytes, got {len(actual)} bytes)"
            )
        if actual[:len(expected)] != expected:
            raise FcdError(
                f"{label}: coding readback mismatch "
                f"(expected {bytes_to_hex(expected)}, got {bytes_to_hex(actual[:len(expected)])})"
            )

    def _read_back_coding_for_target(self, send, target, phase, strict_response=False, expected_mask=None):
        node_name = target.get("node_name", "")
        rid = parse_int(self.coding_read_nvm_rid_var.get())
        result_resp = self._request_read_coding_routine(send, rid, node_name, phase=phase, timeout=10.0)
        if not result_resp:
            if strict_response:
                raise FcdError(f"{node_name}: {phase}: no coding readback response")
            self.log(f"{node_name}: {phase}: no coding readback response accepted; skipped result decode")
            return None
        coding_payload = self._parse_read_coding_result(result_resp)
        if expected_mask is not None:
            self._assert_coding_mask_present(coding_payload, expected_mask, f"{node_name}: {phase}")
        self.root.after(0, self._load_mask_to_coding_tree, coding_payload, f"{node_name} {phase} coding routine 0x0203", node_name)
        return coding_payload

    def _code_target(self, target, strict_response=False, verify_coding_present=False):
        node_name = target.get("node_name", "")
        descriptor = self._all_coding_descriptors().get(node_name.upper())
        if not descriptor:
            if strict_response:
                raise FcdError(f"Code Vehicle: {node_name} has no configured coding values")
            self.log(f"Code Vehicle: {node_name} has no configured coding values; skipped")
            return
        mask = hex_to_bytes(descriptor.get("mask_hex", ""))
        if not mask:
            if strict_response:
                raise FcdError(f"Code Vehicle: {node_name} coding mask is empty")
            self.log(f"Code Vehicle: {node_name} coding mask is empty; skipped")
            return
        client = self._coding_worker_client(target)
        try:
            send = self._make_target_uds_sender(client, target, dry=False, strict_response=strict_response)
            self._ensure_session(send, SESSION_CODING_REQUESTED, f"{node_name}: Coding Session requested as 10 41")
            self._read_post_coding_status_for_target(send, target, "Before Coding", required=strict_response)
            self._run_coding_routine(send, parse_int(descriptor.get("write_routine")), mask, f"{node_name}: Write Coding", timeout=90.0)
            self._run_coding_routine(send, parse_int(descriptor.get("validate_routine")), b"", f"{node_name}: Check Coding", timeout=20.0)
            self._send_ecu_reset_best_effort(send, f"{node_name}: ECUReset hardReset after coding")
            if not self._target_is_simulated(target):
                self._recover_doip_after_reset(client, f"{node_name} after coding", require_current_client=False)
            send = self._make_target_uds_sender(client, target, dry=False, strict_response=strict_response)
            self._ensure_session(send, SESSION_CODING_REQUESTED, f"{node_name}: Coding Session after coding reset requested as 10 41")
            self._read_post_coding_status_for_target(send, target, "After Coding Reset", required=strict_response)
            self._read_back_coding_for_target(
                send,
                target,
                "After Coding Reset",
                strict_response=strict_response,
                expected_mask=mask if verify_coding_present else None,
            )
            self._send_session(send, SESSION_DEFAULT, f"{node_name}: Default Session after coding")
        finally:
            self._release_coding_worker_client(client)

    def _load_default_for_target(self, target):
        node_name = target.get("node_name", "")
        rid = parse_int(self.coding_defaults_rid_var.get())
        client = self._coding_worker_client(target)
        try:
            send = self._make_target_uds_sender(client, target, dry=False)
            self._ensure_session(send, SESSION_CODING_REQUESTED, f"{node_name}: Coding Session requested as 10 41")
            send(b"\x31\x01" + struct.pack(">H", rid), f"{node_name}: Load Coding Default", timeout=20.0)
        finally:
            self._release_coding_worker_client(client)

    def _check_coding_for_target(self, target):
        node_name = target.get("node_name", "")
        rid = parse_int(self.coding_validate_rid_var.get())
        client = self._coding_worker_client(target)
        try:
            send = self._make_target_uds_sender(client, target, dry=False)
            self._ensure_session(send, SESSION_CODING_REQUESTED, f"{node_name}: Coding Session requested as 10 41")
            self._run_coding_routine(send, rid, b"", f"{node_name}: Check Coding", timeout=20.0)
        finally:
            self._release_coding_worker_client(client)

    def _run_coding_test_once(self):
        self._run_vehicle_coding_action_once(
            "Code Vehicle",
            lambda target: self._code_target(
                target,
                strict_response=self._strict_response_for_target(target, True),
                verify_coding_present=self._strict_response_for_target(target, True),
            ),
            strict_response=True,
        )

    def code_ecu_clicked(self):
        self._run_vehicle_coding_action("Code Vehicle", self._code_target)
        return
        write_rid = parse_int(self.coding_write_all_rid_var.get())
        check_rid = parse_int(self.coding_validate_rid_var.get())
        read_rid = parse_int(self.coding_read_nvm_rid_var.get())
        # Build the mask from the table up front so a malformed row aborts before we
        # touch the ECU. The mask is pushed as the Write Coding routine option record.
        mask = self._coding_rows_to_mask()

        def action():
            client = self.require_client()
            # Pause automatic tester present for the whole sequence: it shares the DoIP
            # socket and would race the worker's requests (causing frame desync), and it
            # would flood errors while the ECU is reset and TCP is briefly down.
            keepalive_was_on = bool(self.keepalive_var.get()) or self.keepalive_is_running()
            if keepalive_was_on:
                self.stop_keepalive(update_var=False)
                self.log("Code ECU: paused automatic tester present for the coding sequence")

            # Clear any frame left buffered before the keepalive was stopped, so the
            # first request below reads its own response and not a stale one.
            client.drain()
            sequence_ok = False
            send = None

            try:
                # The full coding procedure is always transmitted.
                send = self._make_uds_sender(client, dry=False)

                # --- Code the ECU from the coding session ---
                self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session requested as 10 41")
                self._run_coding_routine(send, write_rid, mask, "Write Coding", timeout=90.0)
                self._run_coding_routine(send, check_rid, b"", "Check Coding", timeout=20.0)

                # --- Hard reset so the ECU re-evaluates the persisted coding ---
                self._tolerant_ecu_reset(client, "Code ECU reset")
                # A fresh sender after reconnect keeps using the live socket.
                send = self._make_uds_sender(client, dry=False)

                # --- Read back the standard status and the persisted coding ---
                self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session after coding reset requested as 10 41")
                self._read_coding_app_status(send, "Code ECU", required=True)
                for did, name in [
                    (DID_APP_SW_VERSION, "Read Software Version F101"),
                    (DID_ACTIVE_SW_BLOCK, "Read Active Software Block F100"),
                    (DID_ACTIVE_DIAG_SESSION, "Read Active Diagnostic Session F186"),
                ]:
                    send(b"\x22" + struct.pack(">H", did), f"Code ECU: {name}")

                result_resp = self._request_read_coding_routine(send, read_rid, "Code ECU", timeout=10.0)
                coding_payload = self._parse_read_coding_result(result_resp)
                self.root.after(0, self._load_mask_to_coding_tree, coding_payload)

                # --- Leave the ECU in the default session ---
                self._send_session(send, SESSION_DEFAULT, "Default Session")
                sequence_ok = True
            except Exception:
                if send is not None and self.client is client and client.connected:
                    self._read_coding_app_status(send, "Code ECU")
                raise
            finally:
                if keepalive_was_on and sequence_ok and self.client is not None and self.client.connected:
                    self.start_keepalive()
                elif (not sequence_ok) and (self.client is client) and (not client.connected):
                    self.client = None
                    self.root.after(0, self._set_connected_status, False)

        self.worker("Code ECU", action)

    def read_current_coding_clicked(self):
        self._run_vehicle_coding_action("Read Coding", self._read_coding_for_target)
        return
        rid = parse_int(self.coding_read_nvm_rid_var.get())

        def action():
            client = self.require_client()
            keepalive_was_on = bool(self.keepalive_var.get()) or self.keepalive_is_running()
            if keepalive_was_on:
                self.stop_keepalive(update_var=False)
                self.log("Read Coding: paused automatic tester present for the read sequence")

            client.drain()
            sequence_ok = False

            try:
                # Enter the extended session first, the same way the post-flash readback
                # flow (_execute_final_readback) does, so coding is read from a diagnostic
                # session rather than the default session of a fresh ethernet connection.
                session_req = bytes([0x10, SESSION_EXTENDED])
                self.log(f"TX Extended Session: {bytes_to_hex(session_req)}")
                session_resp = client.send_uds(session_req, timeout=10.0)
                self.log(f"RX Extended Session: {bytes_to_hex(session_resp)}")
                require_positive_response(session_resp, 0x10)

                # Coding is read back through the CodingApp read-NVM RoutineControl
                # (0x0203): 31 01 starts it, 31 03 fetches the result. CodingApp returns a
                # 10-byte status block (CodingApp_FillRoutineResponse) followed by the
                # rxMessageExpected bitmask - see CodingApp_RoutineControl in CodingApp.c.
                start_req = b"\x31\x01" + struct.pack(">H", rid)
                self.log(f"TX Read Coding start: {bytes_to_hex(start_req)}")
                start_resp = client.send_uds(start_req, timeout=20.0)
                self.log(f"RX Read Coding start: {bytes_to_hex(start_resp)}")
                require_positive_response(start_resp, 0x31)

                result_req = b"\x31\x03" + struct.pack(">H", rid)
                self.log(f"TX Read Coding result: {bytes_to_hex(result_req)}")
                result_resp = client.send_uds(result_req, timeout=20.0)
                self.log(f"RX Read Coding result: {bytes_to_hex(result_resp)}")
                require_positive_response(result_resp, 0x31)

                coding_payload = self._parse_read_coding_result(result_resp)
                self.root.after(0, self._load_mask_to_coding_tree, coding_payload)
                sequence_ok = True
            finally:
                if keepalive_was_on and sequence_ok and self.client is not None and self.client.connected:
                    self.start_keepalive()
                elif (not sequence_ok) and (self.client is client) and (not client.connected):
                    self.client = None
                    self.root.after(0, self._set_connected_status, False)

        self.worker("Read Coding", action)

    def save_current_coding_clicked(self):
        def action():
            client = self.require_client()
            # Coding DIDs are only readable from a non-default session; enter the
            # extended session first (a fresh ethernet connection starts in default).
            # Coding is never a dry run, so send the session request for real.
            send = self._make_uds_sender(
                client,
                dry=False,
                accept_no_response=self._connected_target_is_simulated(),
            )
            self._send_session(send, SESSION_EXTENDED, "Extended Session")
            dids = [parse_int(self.coding_status_did_var.get()), parse_int(self.coding_mask_did_var.get()), parse_int(self.coding_version_did_var.get())]
            data = {"schema": "FCD_CODING_CURRENT_v1", "created": datetime.now().isoformat(timespec="seconds"), "vin": self.fa_vin_var.get(), "responses": {}}
            for did in dids:
                req = bytes([0x22, (did >> 8) & 0xFF, did & 0xFF])
                resp = send(req, f"Read Coding DID {int_hex(did)}", timeout=float(self.timeout_var.get()))
                if not resp:
                    self.log(f"Save Current Coding: DID {int_hex(did)} no response accepted; skipped")
                    continue
                data["responses"][int_hex(did)] = bytes_to_hex(resp[3:] if len(resp) >= 3 else resp)
            path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON", "*.json")])
            if path:
                Path(path).write_text(json.dumps(data, indent=2), encoding="utf-8")
                self.log(f"Saved current coding: {path}")

        self.worker("Save Current Coding", action)

    def write_coding_config_clicked(self):
        if not messagebox.askyesno(APP_NAME, "Write coding via CodingApp routine 0x0202?"):
            return

        def action():
            client = self.require_client()
            # Write/check coding run from the coding session (10 41), matching the
            # post-flash coding flow (_execute_coding_after_flash). Coding is never a
            # dry run, so the session request is always sent for real.
            send = self._make_uds_sender(client, dry=False)
            try:
                self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session requested as 10 41")
                mask = self._coding_rows_to_mask()
                # The coding mask is delivered as the option record of the Write Coding
                # routine start (0x0202). WriteDataByIdentifier (0x2E) is no longer
                # supported by the ZGW - writes go through RoutineControl. Check Coding
                # (0x0201) takes no option record.
                self._run_coding_routine(send, parse_int(self.coding_write_all_rid_var.get()), mask, "Write Coding", timeout=90.0)
                self._run_coding_routine(send, parse_int(self.coding_validate_rid_var.get()), b"", "Check Coding", timeout=20.0)
            except Exception:
                if self.client is client and client.connected:
                    self._read_coding_app_status(send, "Write Coding")
                raise

        self.worker("Write Coding", action)

    def load_default_coding_clicked(self):
        self._run_vehicle_coding_action("Load Coding Default", self._load_default_for_target)
        return
        if not messagebox.askyesno(APP_NAME, "Load ECU default coding via routine 0x0204?"):
            return
        rid = parse_int(self.coding_defaults_rid_var.get())

        def action():
            client = self.require_client()
            # Loading default coding is a coding write routine; run it from the coding
            # session (10 41) so the ZGW does not reject 0x31 in the default session.
            # Coding is never a dry run - send the session and routine for real.
            send = self._make_uds_sender(client, dry=False)
            self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session requested as 10 41")
            send(b"\x31\x01" + struct.pack(">H", rid), "Load Coding Default", timeout=20.0)

        self.worker("Load Coding Default", action)

    def check_coding_clicked(self):
        self._run_vehicle_coding_action("Check Coding", self._check_coding_for_target)
        return
        rid = parse_int(self.coding_validate_rid_var.get())
        def action():
            client = self.require_client()
            # Check coding runs from the coding session (10 41), matching the
            # post-flash coding flow (_execute_coding_after_flash). Coding is never a
            # dry run, so the session request is always sent for real.
            send = self._make_uds_sender(client, dry=False)
            self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session requested as 10 41")
            for control_type, label in [(0x01, "start"), (0x03, "result")]:
                req = bytes([0x31, control_type]) + struct.pack(">H", rid)
                self.log(f"TX Check Coding {label}: {bytes_to_hex(req)}")
                resp = client.send_uds(req, timeout=20.0)
                self.log(f"RX Check Coding {label}: {bytes_to_hex(resp)}")
                require_positive_response(resp, 0x31)
        self.worker("Check Coding", action)

    def browse_gen_output_clicked(self):
        path = filedialog.askdirectory(initialdir=self.gen_output_var.get() or str(Path.cwd()))
        if path:
            self.gen_output_var.set(path)

    def add_hex_clicked(self):
        paths = filedialog.askopenfilenames(filetypes=[("Intel HEX", "*.hex;*.ihex"), ("All files", "*.*")])
        for path in paths:
            hex_path = Path(path)
            try:
                segments = parse_intel_hex(hex_path)
                if not segments:
                    raise FcdError(f"No data records in {hex_path}")
                base = int_hex(hex_download_base(segments), 8)
            except Exception as exc:
                messagebox.showerror(APP_NAME, f"{hex_path}: {exc}")
                continue
            stem = hex_path.stem
            row = {
                "ecu": ecu_name_from_hex_stem(stem),
                "target": int_hex(DEFAULT_TARGET_ADDR),
                "req": "0x710",
                "resp": "0x711",
                "did": "0xF186",
                "base": base,
                "hex": str(hex_path),
            }
            self._insert_generator_row(row)

    def discover_nodes_startup(self):
        self.discover_nodes_clicked(startup=True)

    def _apply_discovered_nodes(self, discovered_nodes, discovery_logs):
        self.discovered_nodes = discovered_nodes
        self.discovery_logs = discovery_logs
        self.node_rows = {}
        self.node_tree.delete(*self.node_tree.get_children())
        persisted_nodes = self.settings.get("data_editor", {}).get("nodes", {})
        for node in self.discovered_nodes:
            target = node_to_target(node)
            persisted = persisted_nodes.get(node.node_name, {})
            if "selected" in persisted:
                target["simulated_enabled"] = bool(persisted["selected"])
            target["node_kind"] = str(persisted.get("node_kind", target.get("node_kind", "Simulated")))
            if "extended_diag_address" in persisted:
                target["extended_diag_address"] = str(persisted["extended_diag_address"])
            elif not target.get("extended_diag_address"):
                target["extended_diag_address"] = default_extended_diag_address(node.node_name, node.bus_type)
            iid = self.node_tree.insert(
                "",
                "end",
                values=(
                    "☑" if target.get("simulated_enabled", True) else "☐",
                    node.node_name,
                    node.bus_type,
                    target.get("extended_diag_address", ""),
                ),
            )
            self.node_rows[iid] = target
            self._ensure_coding_tab(node.node_name)
        self._refresh_connection_node_tree()
        for line in self.discovery_logs:
            self.log(f"Node discovery: {line}")
        self.log(f"Node discovery: {len(self.discovered_nodes)} nodes")

    def discover_nodes_clicked(self, startup=False):
        with self.discovery_lock:
            if self.discovery_running:
                if not startup:
                    self.log("Node discovery: skipped; discovery already running")
                return
            self.discovery_running = True

        def run():
            try:
                discovered_nodes, discovery_logs = discover_nodes(SCRIPT_DIR.parent.parent)
            except Exception as exc:
                def handle_error():
                    if startup:
                        self.log(f"Node discovery: ERROR: {exc}")
                    else:
                        messagebox.showerror(APP_NAME, f"Node discovery failed: {exc}")

                self.root.after(0, handle_error)
            else:
                self.root.after(0, lambda: self._apply_discovered_nodes(discovered_nodes, discovery_logs))
            finally:
                with self.discovery_lock:
                    self.discovery_running = False

        threading.Thread(target=run, daemon=True).start()

    def _refresh_connection_node_tree(self):
        if not hasattr(self, "connection_node_tree"):
            return
        self.connection_node_tree.delete(*self.connection_node_tree.get_children())
        for target in self.node_rows.values():
            self.connection_node_tree.insert(
                "",
                "end",
                values=(
                    target.get("node_kind", "Simulated"),
                    target.get("node_name", ""),
                    target.get("bus_type", ""),
                    target.get("extended_diag_address", ""),
                ),
            )

    def _update_data_editor_node_row(self, node_name):
        for iid, target in self.node_rows.items():
            if target.get("node_name") == node_name:
                values = list(self.node_tree.item(iid, "values"))
                values[3] = target.get("extended_diag_address", "")
                self.node_tree.item(iid, values=values)
                break

    def connection_node_tree_click(self, event):
        region = self.connection_node_tree.identify("region", event.x, event.y)
        if region != "cell" or self.connection_node_tree.identify_column(event.x) != "#1":
            return
        row_id = self.connection_node_tree.identify_row(event.y)
        if not row_id:
            return
        values = list(self.connection_node_tree.item(row_id, "values"))
        node_name = values[1]
        next_kind = "Real" if values[0] == "Simulated" else "Simulated"
        values[0] = next_kind
        self.connection_node_tree.item(row_id, values=values)
        for target in self.node_rows.values():
            if target.get("node_name") == node_name:
                target["node_kind"] = next_kind
                break
        return "break"

    def edit_connection_node_clicked(self):
        selection = self.connection_node_tree.selection()
        if not selection:
            return
        row_id = selection[0]
        values = list(self.connection_node_tree.item(row_id, "values"))
        node_name = values[1]
        target = None
        for item in self.node_rows.values():
            if item.get("node_name") == node_name:
                target = item
                break
        if target is None:
            return
        dialog = tk.Toplevel(self.root)
        dialog.title("Node Diagnostic Configuration")
        dialog.transient(self.root)
        dialog.grab_set()
        kind = tk.StringVar(value=target.get("node_kind", "Simulated"))
        extended = tk.StringVar(value=target.get("extended_diag_address", ""))
        ttk.Label(dialog, text=f"{node_name} ({target.get('bus_type', '')})").grid(row=0, column=0, columnspan=2, sticky="w", padx=8, pady=6)
        ttk.Label(dialog, text="Type").grid(row=1, column=0, sticky="w", padx=8, pady=6)
        ttk.Combobox(dialog, textvariable=kind, values=("Simulated", "Real"), width=16, state="readonly").grid(row=1, column=1, sticky="w", padx=8, pady=6)
        ttk.Label(dialog, text="Extended diagnostic address").grid(row=2, column=0, sticky="w", padx=8, pady=6)
        ttk.Entry(dialog, textvariable=extended, width=28).grid(row=2, column=1, sticky="ew", padx=8, pady=6)

        def ok():
            text = extended.get().strip()
            if text:
                try:
                    parse_int(text)
                except Exception as exc:
                    messagebox.showerror(APP_NAME, str(exc), parent=dialog)
                    return
            target["node_kind"] = kind.get()
            target["extended_diag_address"] = text
            self._refresh_connection_node_tree()
            self._update_data_editor_node_row(node_name)
            dialog.destroy()

        ttk.Button(dialog, text="OK", command=ok).grid(row=3, column=0, padx=8, pady=10)
        ttk.Button(dialog, text="Cancel", command=dialog.destroy).grid(row=3, column=1, sticky="w", padx=8, pady=10)
        dialog.wait_window()

    def node_tree_click(self, event):
        region = self.node_tree.identify("region", event.x, event.y)
        if region != "cell" or self.node_tree.identify_column(event.x) != "#1":
            return
        iid = self.node_tree.identify_row(event.y)
        if not iid:
            return
        target = self.node_rows.get(iid)
        if not target:
            return
        target["simulated_enabled"] = not bool(target.get("simulated_enabled", True))
        values = list(self.node_tree.item(iid, "values"))
        values[0] = "☑" if target["simulated_enabled"] else "☐"
        self.node_tree.item(iid, values=values)
        return "break"

    def edit_node_extended_address_clicked(self):
        selection = self.node_tree.selection()
        if not selection:
            return
        iid = selection[0]
        target = self.node_rows.get(iid)
        if not target:
            return
        dialog = tk.Toplevel(self.root)
        dialog.title("Extended Diagnostic Addressing")
        dialog.transient(self.root)
        dialog.grab_set()
        value = tk.StringVar(value=target.get("extended_diag_address", ""))
        ttk.Label(dialog, text=f"{target.get('node_name')} ({target.get('bus_type')})").grid(
            row=0, column=0, columnspan=2, sticky="w", padx=8, pady=6
        )
        ttk.Label(dialog, text="Extended diagnostic address").grid(row=1, column=0, sticky="w", padx=8, pady=6)
        ttk.Entry(dialog, textvariable=value, width=32).grid(row=1, column=1, sticky="ew", padx=8, pady=6)

        def ok():
            text = value.get().strip()
            if text:
                try:
                    parse_int(text)
                except Exception as exc:
                    messagebox.showerror(APP_NAME, str(exc), parent=dialog)
                    return
            target["extended_diag_address"] = text
            values = list(self.node_tree.item(iid, "values"))
            values[3] = text
            self.node_tree.item(iid, values=values)
            self._refresh_connection_node_tree()
            dialog.destroy()

        ttk.Button(dialog, text="OK", command=ok).grid(row=2, column=0, padx=8, pady=10)
        ttk.Button(dialog, text="Cancel", command=dialog.destroy).grid(row=2, column=1, sticky="w", padx=8, pady=10)
        dialog.wait_window()

    def _insert_generator_row(self, row):
        iid = self.generator_tree.insert(
            "",
            "end",
            values=(row["ecu"], row["hex"]),
        )
        self.generator_rows[iid] = row

    def edit_hex_clicked(self):
        selection = self.generator_tree.selection()
        if not selection:
            return
        iid = selection[0]
        row = self.generator_rows.get(iid)
        if not row:
            return
        updated = self._hex_row_dialog(row)
        if updated:
            self.generator_rows[iid] = updated
            self.generator_tree.item(
                iid,
                values=(
                    updated["ecu"],
                    updated["hex"],
                ),
            )

    def remove_hex_clicked(self):
        for iid in self.generator_tree.selection():
            self.generator_rows.pop(iid, None)
            self.generator_tree.delete(iid)

    def move_hex_clicked(self, direction):
        selection = self.generator_tree.selection()
        if not selection:
            return
        iid = selection[0]
        siblings = list(self.generator_tree.get_children())
        index = siblings.index(iid)
        new_index = index + direction
        if new_index < 0 or new_index >= len(siblings):
            return
        self.generator_tree.move(iid, "", new_index)
        self.generator_tree.selection_set(iid)

    def _hex_row_dialog(self, row):
        dialog = tk.Toplevel(self.root)
        dialog.title("HEX ECU Mapping")
        dialog.transient(self.root)
        dialog.grab_set()
        result = {}
        fields = ["ecu", "target", "req", "resp", "did", "base", "hex"]
        labels = {
            "ecu": "ECU name",
            "target": "DoIP target",
            "req": "CAN request ID",
            "resp": "CAN response ID",
            "did": "Coding DID",
            "base": "Download base",
            "hex": "HEX file",
        }
        vars_ = {key: tk.StringVar(value=row.get(key, "")) for key in fields}
        for idx, key in enumerate(fields):
            ttk.Label(dialog, text=labels[key]).grid(row=idx, column=0, sticky="w", padx=8, pady=5)
            ttk.Entry(dialog, textvariable=vars_[key], width=78).grid(row=idx, column=1, sticky="ew", padx=8, pady=5)
            if key == "hex":
                ttk.Button(dialog, text="Browse", command=lambda: self._browse_dialog_file(vars_["hex"])).grid(
                    row=idx, column=2, padx=6, pady=5
                )

        def ok():
            candidate = {key: vars_[key].get().strip() for key in fields}
            try:
                parse_int(candidate["target"])
                if candidate["req"]:
                    parse_int(candidate["req"])
                if candidate["resp"]:
                    parse_int(candidate["resp"])
                if candidate["did"]:
                    parse_int(candidate["did"])
                if candidate["base"]:
                    parse_int(candidate["base"])
                if not candidate["ecu"]:
                    raise ValueError("ECU name is required")
                if not candidate["hex"]:
                    raise ValueError("HEX file is required")
            except Exception as exc:
                messagebox.showerror(APP_NAME, str(exc), parent=dialog)
                return
            result.update(candidate)
            dialog.destroy()

        ttk.Button(dialog, text="OK", command=ok).grid(row=len(fields), column=0, padx=8, pady=10)
        ttk.Button(dialog, text="Cancel", command=dialog.destroy).grid(row=len(fields), column=1, sticky="w", padx=8, pady=10)
        dialog.wait_window()
        return result or None

    def _browse_dialog_file(self, var):
        path = filedialog.askopenfilename(filetypes=[("Intel HEX", "*.hex;*.ihex"), ("All files", "*.*")])
        if path:
            var.set(path)

    def generate_files_clicked(self):
        def action():
            if not self.generator_rows:
                raise FcdError("Add at least one HEX file")
            if not self.node_rows:
                self.discover_nodes_clicked()
            out_root = Path(self.gen_output_var.get()).expanduser()
            package_dir = out_root / f"FCD_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
            payload_dir = package_dir / "payloads"
            payload_dir.mkdir(parents=True, exist_ok=True)

            ecus = []
            payloads = []
            bundle_payload_files = []
            tal_steps = []
            step_no = 1
            self._sync_coding_tabs_from_nodes()
            coding_descriptors = self._all_coding_descriptors()
            all_targets_by_name = {}
            for t in self.node_rows.values():
                if not t.get("simulated_enabled", True):
                    continue
                target = dict(t)
                target["payload_blocks"] = []
                all_targets_by_name[target["node_name"].upper()] = target

            selected_iids = [iid for iid in self.generator_tree.selection() if iid in self.generator_rows]
            all_iids = [iid for iid in self.generator_tree.get_children() if iid in self.generator_rows]
            if not selected_iids and len(all_iids) > 1:
                raise FcdError("Select the HEX row(s) to include in the generated bundle")
            source_iids = selected_iids or all_iids
            ordered_rows = [self.generator_rows[iid] for iid in source_iids]
            if selected_iids:
                self.log(f"Generate FCD Files: using {len(selected_iids)} selected HEX row(s)")
            else:
                self.log("Generate FCD Files: using the only HEX row")
            generator_ecu_names = {
                ecu_name_from_hex_stem(str(row.get("ecu") or Path(row["hex"]).stem).strip()).upper()
                for row in ordered_rows
            }
            targets_by_name = {
                name: target
                for name, target in all_targets_by_name.items()
                if name in generator_ecu_names
            }
            for target in targets_by_name.values():
                target["coding_descriptor"] = coding_descriptors.get(target["node_name"].upper(), {})

            seen_payload_blocks = set()
            for row in ordered_rows:
                hex_path = Path(row["hex"])
                source_ecu_name = str(row.get("ecu") or hex_path.stem).strip()
                ecu_name = ecu_name_from_hex_stem(source_ecu_name)
                flash_kind = "FBL" if "FBL" in source_ecu_name.upper() else "APPL"
                segments = parse_intel_hex(hex_path)
                if not segments:
                    raise FcdError(f"No data records in {hex_path}")

                derived_base = hex_download_base(segments)
                download_base = parse_int(row["base"]) if row.get("base") else derived_base
                base_delta = download_base - derived_base
                ecu = {
                    "name": ecu_name,
                    "target_logical_address": int_hex(parse_int(row["target"])),
                    "can_request_id": int_hex(parse_int(row["req"])) if row.get("req") else "",
                    "can_response_id": int_hex(parse_int(row["resp"])) if row.get("resp") else "",
                    "coding_did": int_hex(parse_int(row["did"])) if row.get("did") else "",
                    "source_hex": str(hex_path),
                    "segments": [],
                }
                ecus.append(ecu)
                target_entry = targets_by_name.get(ecu_name)
                if target_entry is None:
                    target_entry = {
                        "node_name": ecu_name,
                        "bus_type": "UNKNOWN",
                        "node_kind": "Simulated",
                        "simulated_enabled": True,
                        "extended_diag_address": "",
                        "route_metadata": {"source_file": "", "inferred": True, "notes": ["Created from HEX filename"]},
                        "is_zgw": ecu_name == "ZGW",
                        "flash_order_group": 99 if ecu_name == "ZGW" else 10,
                        "coding_order_group": 99 if ecu_name == "ZGW" else 10,
                        "payload_blocks": [],
                        "coding_descriptor": {},
                    }
                    targets_by_name[ecu_name] = target_entry

                tal_steps.append({"step": step_no, "ecu": ecu_name, "service": "DiagnosticSessionControl", "request": "10 03"})
                step_no += 1
                tal_steps.append({"step": step_no, "ecu": ecu_name, "service": "CommunicationControl", "request": "28 01 03"})
                step_no += 1
                tal_steps.append({"step": step_no, "ecu": ecu_name, "service": "ControlDTCSetting", "request": "85 02"})
                step_no += 1
                tal_steps.append({"step": step_no, "ecu": ecu_name, "service": "DiagnosticSessionControl", "request": "10 02"})
                step_no += 1

                for index, segment in enumerate(segments):
                    address = download_address_from_hex_address(segment.address) + base_delta
                    payload_key = (ecu_name, flash_kind, address, len(segment.data), str(hex_path).lower())
                    if payload_key in seen_payload_blocks:
                        continue
                    seen_payload_blocks.add(payload_key)
                    name = f"{ecu_name}_{index}_{address:08X}.bin".replace(" ", "_")
                    payload_path = payload_dir / name
                    payload_path.write_bytes(segment.data)
                    payload_entry = {
                        "ecu": ecu_name,
                        "node_name": ecu_name,
                        "bus_type": target_entry.get("bus_type", "UNKNOWN"),
                        "target_logical_address": int_hex(parse_int(row["target"])),
                        "address": int_hex(address, 8),
                        "size": len(segment.data),
                        "crc32": int_hex(binascii.crc32(segment.data) & 0xFFFFFFFF, 8),
                        "file": str(Path("payloads") / name),
                        "source_hex": str(hex_path),
                        "source_address": int_hex(segment.address, 8),
                        "flash_kind": flash_kind,
                    }
                    payloads.append(payload_entry)
                    bundle_payload_files.append((payload_path, str(Path("payloads") / name)))
                    target_entry.setdefault("payload_blocks", []).append(
                        {
                            "address": payload_entry["address"],
                            "size": payload_entry["size"],
                            "crc32": payload_entry["crc32"],
                            "file": payload_entry["file"],
                            "source_hex": payload_entry["source_hex"],
                            "source_address": payload_entry["source_address"],
                            "flash_kind": payload_entry["flash_kind"],
                        }
                    )
                    ecu_segment = {key: value for key, value in payload_entry.items() if key != "data_base64"}
                    ecu["segments"].append(ecu_segment)
                    tal_steps.append(
                        {
                            "step": step_no,
                            "ecu": ecu_name,
                            "service": "RequestDownload",
                            "address": int_hex(address, 8),
                            "size": len(segment.data),
                            "crc32": payload_entry["crc32"],
                        }
                    )
                    step_no += 1
                    tal_steps.append(
                        {
                            "step": step_no,
                            "ecu": ecu_name,
                            "service": "TransferData",
                            "source": str(Path("payloads") / name),
                        }
                    )
                    step_no += 1
                    tal_steps.append({"step": step_no, "ecu": ecu_name, "service": "RequestTransferExit"})
                    step_no += 1
                    if self.verify_crc_var.get() and (not self.transfer_crc_var.get()) and flash_kind != "FBL":
                        tal_steps.append(
                            {
                                "step": step_no,
                                "ecu": ecu_name,
                                "service": "RoutineControl CRC",
                                "rid": "0x0002",
                                "address": int_hex(address, 8),
                                "size": len(segment.data),
                                "crc32": payload_entry["crc32"],
                            }
                        )
                        step_no += 1

            fa = {
                "schema": "FCD_FA_v1",
                "created": datetime.now().isoformat(timespec="seconds"),
                "project": self.gen_project_var.get().strip(),
                "vin": self.gen_vin_var.get().strip(),
                "type": self.fa_type_var.get().strip(),
            }
            svt = {"schema": "FCD_SVT_v1", "created": fa["created"], "ecus": ecus}
            tal = {"schema": "FCD_TAL_v1", "created": fa["created"], "steps": tal_steps}
            psdz = {
                "schema": "FCD_PSDZDATA_v1",
                "created": fa["created"],
                "payload_format": "raw_bin",
                "payloads": payloads,
            }

            (package_dir / "fcd_fa.json").write_text(json.dumps(fa, indent=2), encoding="utf-8")
            (package_dir / "fcd_svt.json").write_text(json.dumps(svt, indent=2), encoding="utf-8")
            (package_dir / "fcd_tal.json").write_text(json.dumps(tal, indent=2), encoding="utf-8")
            (package_dir / "fcd_psdzdata.json").write_text(json.dumps(psdz, indent=2), encoding="utf-8")
            source_dbs = sorted({n.source_file for n in self.discovered_nodes})
            targets = list(targets_by_name.values())
            manifest = manifest_from_targets(
                self.gen_project_var.get().strip(),
                self.gen_vin_var.get().strip(),
                source_dbs,
                targets,
            )
            manifest["legacy_package_folder"] = str(package_dir)
            manifest["schedule"] = to_jsonable_events(build_schedule(manifest["targets"], include_flash=True, include_coding=True))
            schedule_errors = validate_schedule(build_schedule(manifest["targets"], include_flash=True, include_coding=True))
            if schedule_errors:
                raise FcdError("; ".join(schedule_errors))
            bundle_path = package_dir.with_suffix(".pfpkg")
            create_bundle(bundle_path, manifest, bundle_payload_files)
            self.log(f"Generated FCD legacy folder: {package_dir}")
            self.log(f"Generated primary parallel bundle: {bundle_path}")
            self.root.after(0, lambda: self.pkg_dir_var.set(str(bundle_path)))

        self.worker("Generate FCD Files", action)

    def copy_generator_to_svt(self):
        self.svt_tree.delete(*self.svt_tree.get_children())
        for row in self.generator_rows.values():
            size = ""
            crc = ""
            try:
                segments = parse_intel_hex(row["hex"])
                total = sum(len(seg.data) for seg in segments)
                crc_value = 0
                for seg in segments:
                    crc_value = binascii.crc32(seg.data, crc_value)
                size = str(total)
                crc = int_hex(crc_value & 0xFFFFFFFF, 8)
            except Exception:
                pass
            self.svt_tree.insert(
                "",
                "end",
                values=(row["ecu"], row["target"], row["req"], row["resp"], row["did"], row["hex"], size, crc),
            )

    def import_svt_clicked(self):
        path = filedialog.askopenfilename(filetypes=[("JSON", "*.json"), ("All files", "*.*")])
        if not path:
            return
        with open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
        self.svt_tree.delete(*self.svt_tree.get_children())
        for ecu in data.get("ecus", []):
            self.svt_tree.insert(
                "",
                "end",
                values=(
                    ecu.get("name", ""),
                    ecu.get("target_logical_address", ""),
                    ecu.get("can_request_id", ""),
                    ecu.get("can_response_id", ""),
                    ecu.get("coding_did", ""),
                    ecu.get("source_hex", ""),
                    sum(int(seg.get("size", 0)) for seg in ecu.get("segments", [])),
                    "",
                ),
            )
        self.log(f"Imported SVT: {path}")

    def export_svt_clicked(self):
        path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON", "*.json")])
        if not path:
            return
        ecus = []
        for iid in self.svt_tree.get_children():
            values = self.svt_tree.item(iid, "values")
            ecus.append(
                {
                    "name": values[0],
                    "target_logical_address": values[1],
                    "can_request_id": values[2],
                    "can_response_id": values[3],
                    "coding_did": values[4],
                    "source_hex": values[5],
                }
            )
        data = {"schema": "FCD_SVT_v1", "created": datetime.now().isoformat(timespec="seconds"), "ecus": ecus}
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(data, handle, indent=2)
        self.log(f"Exported SVT: {path}")

    def browse_package_clicked(self):
        path = filedialog.askdirectory(initialdir=self.pkg_dir_var.get() or self.gen_output_var.get() or str(Path.cwd()))
        if path:
            self.pkg_dir_var.set(path)

    def load_package_clicked(self):
        package_dir = Path(self.pkg_dir_var.get()).expanduser()
        psdz_path = package_dir / "fcd_psdzdata.json"
        if not psdz_path.exists():
            messagebox.showerror(APP_NAME, f"Missing {psdz_path}")
            return
        with psdz_path.open("r", encoding="utf-8") as handle:
            psdz = json.load(handle)
        self.package_dir = package_dir
        self.package_payloads = psdz.get("payloads", [])
        self.payload_enabled = {}
        self.payload_tree.delete(*self.payload_tree.get_children())
        for index, item in enumerate(self.package_payloads):
            iid = self.payload_tree.insert(
                "",
                "end",
                values=(
                    "☑",
                    item.get("ecu", ""),
                    item.get("target_logical_address", ""),
                    item.get("address", ""),
                    item.get("size", ""),
                    item.get("crc32", ""),
                    item.get("file", ""),
                ),
            )
            self.payload_enabled[iid] = True
        self.log(f"Loaded package: {package_dir} ({len(self.package_payloads)} payloads)")

    def payload_tree_click(self, event):
        region = self.payload_tree.identify("region", event.x, event.y)
        if region != "cell":
            return
        column = self.payload_tree.identify_column(event.x)
        if column != "#1":
            return
        iid = self.payload_tree.identify_row(event.y)
        if not iid:
            return
        enabled = not self.payload_enabled.get(iid, True)
        self.payload_enabled[iid] = enabled
        values = list(self.payload_tree.item(iid, "values"))
        values[0] = "☑" if enabled else "☐"
        self.payload_tree.item(iid, values=values)
        return "break"

    def selected_payloads(self):
        out = []
        for payload in self.package_payloads:
            is_fbl = self._payload_is_fbl(payload)
            if is_fbl and self.flash_fbl_var.get():
                out.append(payload)
            elif (not is_fbl) and self.flash_appl_var.get():
                out.append(payload)
        return out

    def browse_package_clicked(self):
        path = filedialog.askopenfilename(
            initialdir=self.gen_output_var.get() or str(Path.cwd()),
            filetypes=[("Parallel FCD bundle", "*.pfpkg;*.fcdpkg"), ("All files", "*.*")],
        )
        if not path:
            path = filedialog.askdirectory(initialdir=self.pkg_dir_var.get() or self.gen_output_var.get() or str(Path.cwd()))
        if path:
            self.pkg_dir_var.set(path)

    def load_package_clicked(self):
        package_path = Path(self.pkg_dir_var.get()).expanduser()
        self.package_bundle = None
        self.package_manifest = None
        if package_path.is_file() and package_path.suffix.lower() in (".pfpkg", ".fcdpkg"):
            try:
                manifest = load_bundle(package_path)
            except Exception as exc:
                messagebox.showerror(APP_NAME, f"Cannot load bundle: {exc}")
                return
            self.package_bundle = package_path
            self.package_manifest = manifest
            self.package_dir = package_path.parent
            self.package_payloads = []
            for target in manifest.get("targets", []):
                for block in target.get("payload_blocks", []):
                    item = dict(block)
                    item["ecu"] = target.get("node_name", "")
                    item["node_name"] = target.get("node_name", "")
                    item["bus_type"] = target.get("bus_type", "UNKNOWN")
                    item["is_zgw"] = target.get("is_zgw", False)
                    item["target_logical_address"] = target.get("route_metadata", {}).get(
                        "target_logical_address", int_hex(DEFAULT_TARGET_ADDR)
                    ) or int_hex(DEFAULT_TARGET_ADDR)
                    self.package_payloads.append(item)
            self._populate_payload_tree_from_schedule(manifest)
            self.log(f"Loaded parallel bundle: {package_path} ({len(self.package_payloads)} payload blocks)")
            return

        package_dir = package_path
        psdz_path = package_dir / "fcd_psdzdata.json"
        if not psdz_path.exists():
            messagebox.showerror(APP_NAME, f"Missing {psdz_path}")
            return
        with psdz_path.open("r", encoding="utf-8") as handle:
            psdz = json.load(handle)
        self.package_dir = package_dir
        self.package_payloads = psdz.get("payloads", [])
        self._populate_payload_tree_from_schedule({})
        self.log(f"Loaded legacy package: {package_dir} ({len(self.package_payloads)} payloads)")

    def _populate_payload_tree_from_schedule(self, manifest):
        self.payload_enabled = {}
        self.payload_tree.delete(*self.payload_tree.get_children())
        targets = manifest.get("targets", [])
        buses = sorted({t.get("bus_type", "") for t in targets if t.get("bus_type", "")})
        flash_events = [e for e in manifest.get("schedule", []) if e.get("phase") == "flash"]
        coding_events = [e for e in manifest.get("schedule", []) if e.get("phase") == "coding"]
        for event in manifest.get("schedule", []):
            self.log(
                "Bundle schedule: "
                f"slot={event.get('time_slot')} phase={event.get('phase')} bus={event.get('bus_type')} "
                f"node={event.get('node_name')} active_on_bus={event.get('active_on_bus')}"
            )
        total_bytes = sum(int(p.get("size", 0)) for p in self.package_payloads)
        total_mib = total_bytes / (1024 * 1024)
        flash_slot_count = len({e.get("time_slot", 0) for e in flash_events})
        coding_slot_count = len({e.get("time_slot", 0) for e in coding_events})
        rows = [
            ("Targets", str(len(targets) or len({p.get("ecu", "") for p in self.package_payloads}))),
            ("Payload blocks", str(len(self.package_payloads))),
            ("Payload bytes", f"{total_bytes} ({total_mib:.2f} MiB data records)"),
            ("TransferData requests", str(self._payloads_total_packets(self.package_payloads))),
            ("Buses", ", ".join(buses) if buses else "legacy package"),
            ("Flash slots", str(flash_slot_count)),
            ("Coding slots", str(coding_slot_count)),
        ]
        for item, value in rows:
            self.payload_tree.insert("", "end", values=(item, value))

    def _selected_flash_payloads_or_raise(self):
        payloads = self.selected_payloads()
        if not payloads and not self.flash_coding_var.get():
            raise FcdError("Load/select payloads or enable Coding first")
        return payloads

    def _execute_selected_payloads_once(self, payloads, strict_response=False):
        fbl_payloads = [p for p in payloads if self._payload_is_fbl(p)]
        appl_payloads = [p for p in payloads if not self._payload_is_fbl(p)]

        if self.package_manifest is not None:
            self._execute_parallel_bundle(payloads, strict_response=strict_response)
            return

        client = self.require_client()
        start_time = time.monotonic()
        progress_cb, complete_progress, stop_progress = self._make_package_progress(
            self._payloads_total_bytes(payloads),
            start_time,
        )

        try:
            self._execute_zgw_programming_preamble(
                client,
                is_fbl=bool(self.flash_fbl_var.get() and fbl_payloads),
            )

            if self.flash_fbl_var.get() and fbl_payloads:
                for payload in fbl_payloads:
                    self._execute_payload(
                        client,
                        payload,
                        is_fbl=True,
                        progress_cb=progress_cb,
                        strict_response=strict_response,
                )
                self._execute_hard_reset(client, "after FBL flash")
                send = self._make_uds_sender(client)
                self._send_session(send, SESSION_DEFAULT, "Default Session after FBL flash")
                self._send_session(send, SESSION_EXTENDED, "Extended Session after FBL flash")

            if self.flash_appl_var.get() and appl_payloads:
                for payload in appl_payloads:
                    self._execute_payload(
                        client,
                        payload,
                        is_fbl=False,
                        progress_cb=progress_cb,
                        strict_response=strict_response,
                )
                self._execute_hard_reset(client, "after APPL flash")

            complete_progress()
        finally:
            stop_progress()

    def _run_flashing_test_once(self):
        payloads = self._selected_flash_payloads_or_raise()
        self._execute_selected_payloads_once(payloads, strict_response=True)

    def execute_selected_clicked(self):
        try:
            payloads = self._selected_flash_payloads_or_raise()
        except FcdError as exc:
            messagebox.showerror(APP_NAME, str(exc))
            return

        action_name = "Execute Parallel Bundle" if self.package_manifest is not None else "Execute ZGW Sequence"
        self.worker(action_name, lambda: self._execute_selected_payloads_once(payloads))

    def _new_parallel_client(self, target_addr):
        if self.transport_var.get() != "DoIP":
            raise FcdError("Parallel bundle execution currently requires DoIP")
        client = DoipClient(
            self.host_var.get().strip(),
            parse_int(self.port_var.get()),
            source_addr=parse_int(self.source_var.get()),
            target_addr=target_addr,
            timeout=float(self.timeout_var.get()),
            local_ip=self.local_ip_var.get().strip(),
        )
        client.connect()
        client.routing_activation()
        return client

    def _update_elapsed(self, start_time):
        elapsed = int(time.monotonic() - start_time)
        self.root.after(0, lambda: self.elapsed_var.set(f"Elapsed: {elapsed // 60:02d}:{elapsed % 60:02d}"))

    def _payloads_total_bytes(self, payloads):
        total = 0
        for payload in payloads:
            try:
                total += int(payload.get("size", 0))
            except Exception:
                pass
        return total

    def _payload_transfer_block_size(self, payload):
        # Mirror _execute_payload exactly so the packet count below matches the
        # number of TransferData requests actually sent at runtime.
        target_info = self._target_for_payload(payload)
        target_is_zgw = self._payload_targets_zgw(payload, target_info)
        routed_target = bool(target_info) and not (
            self._payload_targets_zgw(payload, target_info)
        )
        configured_block_size = parse_int(self.block_size_var.get())
        max_chunk_size = (
            ZGW_ETHERNET_TRANSFER_DATA_MAX_CHUNK_SIZE if target_is_zgw
            else ROUTED_TRANSFER_DATA_MAX_CHUNK_SIZE if routed_target
            else TRANSFER_DATA_MAX_CHUNK_SIZE
        )
        return max(8, min(max_chunk_size, configured_block_size))

    @staticmethod
    def _request_download_max_block_length(response):
        if len(response) < 2 or response[0] != 0x74:
            raise FcdError("RequestDownload did not return a positive response")
        length_bytes = (response[1] >> 4) & 0x0F
        if length_bytes == 0 or len(response) < 2 + length_bytes:
            raise FcdError(
                f"RequestDownload returned invalid max block length format: {response.hex(' ').upper()}"
            )
        value = 0
        for b in response[2 : 2 + length_bytes]:
            value = (value << 8) | b
        return value

    def _payloads_total_packets(self, payloads):
        # Display-only estimate of TransferData requests. Progress itself is
        # byte-based so mixed ZGW DoIP and routed/CAN payloads share one unit.
        total = 0
        for payload in payloads:
            try:
                size = int(payload.get("size", 0))
            except Exception:
                continue
            if size <= 0:
                continue
            block_size = self._payload_transfer_block_size(payload)
            total += (size + block_size - 1) // block_size
        return total

    def _make_package_progress(self, total_units, start_time):
        maximum = max(1, int(total_units))
        self.progress_run_id += 1
        run_id = self.progress_run_id
        progress_state = {
            "units": 0,
            "active": True,
            "last_rendered_units": -1,
            "last_rendered_elapsed": -1,
        }
        progress_lock = threading.Lock()

        def render_progress(force=False):
            if run_id != self.progress_run_id:
                return
            with progress_lock:
                value = min(maximum, progress_state["units"])
                active = progress_state["active"]
            elapsed = int(time.monotonic() - start_time)
            if (not force) and value == progress_state["last_rendered_units"] and elapsed == progress_state["last_rendered_elapsed"]:
                if active:
                    self.root.after(200, render_progress)
                return
            self.progress.configure(maximum=maximum, value=value)
            self.elapsed_var.set(f"Elapsed: {elapsed // 60:02d}:{elapsed % 60:02d}")
            self.progress.update_idletasks()
            with progress_lock:
                progress_state["last_rendered_units"] = value
                progress_state["last_rendered_elapsed"] = elapsed
            if active:
                self.root.after(200, render_progress)

        def reset_progress():
            if run_id != self.progress_run_id:
                return
            self.progress.configure(maximum=maximum, value=0)
            elapsed = int(time.monotonic() - start_time)
            self.elapsed_var.set(f"Elapsed: {elapsed // 60:02d}:{elapsed % 60:02d}")
            self.root.after(200, render_progress)

        self.root.after(0, reset_progress)

        def advance(count):
            try:
                delta = max(0, int(count))
            except Exception:
                delta = 0
            with progress_lock:
                if delta:
                    progress_state["units"] = min(maximum, progress_state["units"] + delta)

        def complete():
            with progress_lock:
                progress_state["units"] = maximum
                progress_state["active"] = False
            self.root.after(0, lambda: render_progress(force=True))

        def stop():
            with progress_lock:
                progress_state["active"] = False
            self.root.after(0, lambda: render_progress(force=True))

        return advance, complete, stop

    def _execute_parallel_bundle(self, payloads, strict_response=False):
        manifest = self.package_manifest or {}
        by_node = {}
        for payload in payloads:
            node_key = payload.get("node_name") or ecu_name_from_hex_stem(payload.get("ecu", ""))
            by_node.setdefault(node_key, []).append(payload)
        targets = {t.get("node_name", ""): t for t in manifest.get("targets", [])}
        start_time = time.monotonic()
        progress_cb, complete_progress, stop_progress = self._make_package_progress(
            self._payloads_total_bytes(payloads),
            start_time,
        )

        keepalive_was_on = bool(self.keepalive_var.get()) or self.keepalive_is_running()
        if keepalive_was_on:
            self.stop_keepalive(update_var=False)
            self.log("Execute Parallel Bundle: paused automatic tester present")
        completed = False
        try:
            flash_events = [e for e in manifest.get("schedule", []) if e.get("phase") == "flash"]
            non_zgw_flash_events = [e for e in flash_events if not e.get("is_zgw")]
            zgw_flash_events = [e for e in flash_events if e.get("is_zgw")]

            self._execute_non_zgw_flash_by_bus(
                non_zgw_flash_events,
                by_node,
                targets,
                progress_cb,
                strict_response=strict_response,
            )

            self._execute_parallel_flash_phase(
                zgw_flash_events,
                by_node,
                targets,
                progress_cb,
                is_fbl=True,
                strict_response=strict_response,
            )
            self._execute_parallel_flash_phase(
                zgw_flash_events,
                by_node,
                targets,
                progress_cb,
                is_fbl=False,
                strict_response=strict_response,
            )
            completed = True
            complete_progress()
        finally:
            if not completed:
                stop_progress()
            self._update_elapsed(start_time)
            if keepalive_was_on and completed and not self.worker_stop.is_set():
                self.start_keepalive()

    def _execute_non_zgw_flash_by_bus(self, events, by_node, targets, progress_cb, strict_response=False):
        by_bus = {}
        for event in events:
            by_bus.setdefault(bus_category(event.get("bus_type", "UNKNOWN")), []).append(event)
        if not by_bus:
            return

        fbl_lanes = self._collect_parallel_flash_lanes(events, by_node, targets, is_fbl=True)
        app_lanes = self._collect_parallel_flash_lanes(events, by_node, targets, is_fbl=False)
        phase_lanes = {}
        for bus, tasks in fbl_lanes.items():
            phase_lanes.setdefault(bus, []).extend(("FBL", True, slot, work) for slot, work in tasks)
        for bus, tasks in app_lanes.items():
            phase_lanes.setdefault(bus, []).extend(("APP", False, slot, work) for slot, work in tasks)
        if phase_lanes and all(
            self._can_run_paced_flash_slot(work)
            for tasks in phase_lanes.values()
            for _phase_name, _is_fbl, _slot, work in tasks
        ):
            client = self._new_parallel_client(parse_int(self.target_var.get() or int_hex(DEFAULT_TARGET_ADDR)))
            try:
                client.request_spacing_seconds = max(client.request_spacing_seconds, REQUEST_SPACING_SECONDS)
                client.drain()
                self.log(
                    "Execute Parallel Bundle: non-ZGW flash uses one DoIP TCP connection "
                    "for independent routed bus lanes"
                )
                self._execute_paced_simulated_flash_phase_lanes(
                    phase_lanes,
                    client,
                    progress_cb,
                )
            finally:
                client.close()
            return

        def run_bus(bus_events):
            self._execute_parallel_flash_phase(
                bus_events,
                by_node,
                targets,
                progress_cb,
                is_fbl=True,
                strict_response=strict_response,
            )
            self._execute_parallel_flash_phase(
                bus_events,
                by_node,
                targets,
                progress_cb,
                is_fbl=False,
                strict_response=strict_response,
            )

        if len(by_bus) == 1:
            run_bus(next(iter(by_bus.values())))
            return

        with ThreadPoolExecutor(max_workers=max(1, min(len(by_bus), PARALLEL_BUNDLE_MAX_WORKERS))) as executor:
            futures = [executor.submit(run_bus, bus_events) for bus_events in by_bus.values()]
            for future in as_completed(futures):
                future.result()

    def _collect_parallel_flash_lanes(self, events, by_node, targets, is_fbl):
        lanes = {}
        for slot in sorted({e.get("time_slot", 0) for e in events}):
            slot_events = [e for e in events if e.get("time_slot", 0) == slot]
            work_by_bus = {}
            for event in slot_events:
                node_name = event.get("node_name", "")
                node_payloads = [
                    payload for payload in by_node.get(node_name, [])
                    if self._payload_is_fbl(payload) == is_fbl
                ]
                node_payloads = self._unique_payloads(node_payloads)
                if node_payloads:
                    target = targets.get(node_name, {})
                    bus = bus_category(target.get("bus_type") or event.get("bus_type", "UNKNOWN"))
                    work_by_bus.setdefault(bus, []).append((node_name, node_payloads, target))
            for bus, work in work_by_bus.items():
                lanes.setdefault(bus, []).append((slot, work))
        return lanes

    def _execute_parallel_flash_phase(self, events, by_node, targets, progress_cb, is_fbl, strict_response=False):
        phase_name = "FBL" if is_fbl else "APP"
        lanes = self._collect_parallel_flash_lanes(events, by_node, targets, is_fbl)

        if not lanes:
            return

        lane_text = ", ".join(
            f"{bus}:{len(tasks)} slot(s)"
            for bus, tasks in sorted(lanes.items())
        )
        self.log(f"Execute Parallel Bundle: {phase_name} bus lanes={lane_text}")

        if self._can_run_paced_flash_lanes(lanes):
            client = self._new_parallel_client(parse_int(self.target_var.get() or int_hex(DEFAULT_TARGET_ADDR)))
            try:
                client.request_spacing_seconds = max(client.request_spacing_seconds, REQUEST_SPACING_SECONDS)
                client.drain()
                self.log(
                    f"Execute Parallel Bundle: {phase_name} uses one DoIP TCP connection "
                    "for routed bus schedule"
                )
                self._execute_paced_simulated_flash_lanes(
                    phase_name,
                    lanes,
                    client,
                    progress_cb,
                    is_fbl,
                )
            finally:
                client.close()
            return

        def run_lane(bus, tasks):
            for slot, work in tasks:
                self._execute_parallel_flash_slot_work(
                    phase_name,
                    slot,
                    bus,
                    work,
                    progress_cb,
                    is_fbl,
                    strict_response,
                )

        if len(lanes) == 1:
            bus, tasks = next(iter(lanes.items()))
            run_lane(bus, tasks)
            return

        with ThreadPoolExecutor(max_workers=max(1, min(len(lanes), PARALLEL_BUNDLE_MAX_WORKERS))) as executor:
            futures = [
                executor.submit(run_lane, bus, tasks)
                for bus, tasks in lanes.items()
            ]
            for future in as_completed(futures):
                future.result()

    def _execute_parallel_flash_slot_work(self, phase_name, slot, bus, work, progress_cb, is_fbl, strict_response):
        if not work:
            return
        self.log(
            f"Execute Parallel Bundle: {phase_name} slot {slot} bus={bus} "
            f"nodes={', '.join(item[0] for item in work)}"
        )
        if self._can_run_paced_flash_slot(work):
            client = self._new_parallel_client(parse_int(self.target_var.get() or int_hex(DEFAULT_TARGET_ADDR)))
            try:
                self._execute_paced_simulated_flash_slot(slot, work, client, progress_cb, is_fbl)
            finally:
                client.close()
            return
        with ThreadPoolExecutor(max_workers=max(1, min(len(work), PARALLEL_BUNDLE_MAX_WORKERS))) as executor:
            futures = [
                executor.submit(
                    self._execute_payloads_parallel_worker,
                    node_payloads,
                    target,
                    progress_cb,
                    self._strict_response_for_target(target, strict_response),
                )
                for _node_name, node_payloads, target in work
            ]
            for future in as_completed(futures):
                future.result()

    def _unique_payloads(self, payloads):
        unique = []
        seen = set()
        for payload in payloads:
            key = (
                str(payload.get("ecu", "")),
                str(payload.get("node_name", "")),
                str(payload.get("address", "")),
                str(payload.get("size", "")),
                str(payload.get("crc32", "")),
                str(payload.get("file", "")),
            )
            if key in seen:
                continue
            seen.add(key)
            unique.append(payload)
        return unique

    def _can_run_paced_flash_slot(self, work):
        if not work:
            return False
        for _node_name, payloads, target in work:
            if not payloads:
                return False
            node_name = str(target.get("node_name", ""))
            if bool(target.get("is_zgw")) or node_name.upper() == "ZGW":
                return False
            if not self._target_is_simulated(target):
                return False
            if bus_category(target.get("bus_type", "UNKNOWN")) not in ("CAN", "CANFD", "LIN"):
                return False
        return True

    def _can_run_paced_flash_lanes(self, lanes):
        if not lanes:
            return False
        for tasks in lanes.values():
            for _slot, work in tasks:
                if not self._can_run_paced_flash_slot(work):
                    return False
        return True

    def _execute_paced_simulated_flash_lanes(self, phase_name, lanes, client, progress_cb, is_fbl):
        bus_order = {"CAN": 0, "CANFD": 1, "LIN": 2}
        start_time = time.monotonic()
        next_due_by_bus = {}
        events = []
        order = 0

        if "LIN" in lanes:
            self.log(
                "Execute Parallel Bundle: LIN routed flash pacing "
                f"{int(ROUTED_LIN_REQUEST_SPACING_SECONDS * 1000)} ms; "
                "other bus lanes continue independently on the shared DoIP connection"
            )

        for bus in sorted(lanes, key=lambda item: bus_order.get(item, 99)):
            lane_start = start_time
            for slot, work in sorted(lanes[bus], key=lambda item: item[0]):
                prepared = []
                for node_name, payloads, target in work:
                    sequence = self._simulated_flash_request_sequence(node_name, payloads, target, is_fbl)
                    if sequence:
                        prepared.append((node_name, payloads, target, sequence))
                if not prepared:
                    continue

                self.log(
                    f"Execute Parallel Bundle: {phase_name} slot {slot} bus={bus} "
                    f"nodes={', '.join(item[0] for item in prepared)}"
                )

                max_steps = max(len(item[3]) for item in prepared)
                for step_index in range(max_steps):
                    for node_index, (_node_name, _payloads, target, sequence) in enumerate(prepared):
                        if step_index >= len(sequence):
                            continue
                        request, label, progress_count = sequence[step_index]
                        if request is None:
                            continue
                        desired_due = (
                            lane_start
                            + (step_index * ROUTED_READ_CODING_SERVICE_GAP_SECONDS)
                            + (node_index * ROUTED_READ_CODING_NODE_STAGGER_SECONDS)
                        )
                        due = self._scheduled_routed_due(bus, desired_due, next_due_by_bus)
                        events.append((
                            due,
                            bus_order.get(bus, 99),
                            slot,
                            node_index,
                            order,
                            bus,
                            target,
                            bytes(request),
                            label,
                            progress_count,
                        ))
                        order += 1

                lane_start += (
                    max_steps * ROUTED_READ_CODING_SERVICE_GAP_SECONDS
                    + max(0, len(prepared) - 1) * ROUTED_READ_CODING_NODE_STAGGER_SECONDS
                    + ROUTED_READ_CODING_DRAIN_SECONDS
                )

        expected_requests = []
        for due, _bus_rank, _slot, _node_index, _order, bus, target, request, label, progress_count in sorted(events):
            self._sleep_until_monotonic(due)
            prefixed_request = self._send_routed_uds_no_wait(
                client,
                target,
                request,
                label,
                apply_pacing=(bus != "LIN"),
            )
            expected_requests.append((label, prefixed_request, self._target_is_simulated(target), progress_count))

        self._wait_routed_transport_acks_and_progress(client, expected_requests, progress_cb)
        time.sleep(ROUTED_READ_CODING_DRAIN_SECONDS)
        client.drain()

    def _execute_paced_simulated_flash_phase_lanes(self, phase_lanes, client, progress_cb):
        bus_order = {"CAN": 0, "CANFD": 1, "LIN": 2}
        start_time = time.monotonic()
        next_due_by_bus = {}
        events = []
        order = 0

        if "LIN" in phase_lanes:
            self.log(
                "Execute Parallel Bundle: LIN routed flash pacing "
                f"{int(ROUTED_LIN_REQUEST_SPACING_SECONDS * 1000)} ms; "
                "CAN/CANFD lanes continue independently on the shared DoIP connection"
            )

        for bus in sorted(phase_lanes, key=lambda item: bus_order.get(item, 99)):
            lane_start = start_time
            for phase_name, is_fbl, slot, work in sorted(phase_lanes[bus], key=lambda item: (item[2], 0 if item[1] else 1)):
                prepared = []
                for node_name, payloads, target in work:
                    sequence = self._simulated_flash_request_sequence(node_name, payloads, target, is_fbl)
                    if sequence:
                        prepared.append((node_name, payloads, target, sequence))
                if not prepared:
                    continue

                self.log(
                    f"Execute Parallel Bundle: {phase_name} slot {slot} bus={bus} "
                    f"nodes={', '.join(item[0] for item in prepared)}"
                )

                max_steps = max(len(item[3]) for item in prepared)
                for step_index in range(max_steps):
                    for node_index, (_node_name, _payloads, target, sequence) in enumerate(prepared):
                        if step_index >= len(sequence):
                            continue
                        request, label, progress_count = sequence[step_index]
                        if request is None:
                            continue
                        desired_due = (
                            lane_start
                            + (step_index * ROUTED_READ_CODING_SERVICE_GAP_SECONDS)
                            + (node_index * ROUTED_READ_CODING_NODE_STAGGER_SECONDS)
                        )
                        due = self._scheduled_routed_due(bus, desired_due, next_due_by_bus)
                        events.append((
                            due,
                            bus_order.get(bus, 99),
                            slot,
                            node_index,
                            order,
                            bus,
                            target,
                            bytes(request),
                            label,
                            progress_count,
                        ))
                        order += 1

                lane_start += (
                    max_steps * ROUTED_READ_CODING_SERVICE_GAP_SECONDS
                    + max(0, len(prepared) - 1) * ROUTED_READ_CODING_NODE_STAGGER_SECONDS
                    + ROUTED_READ_CODING_DRAIN_SECONDS
                )

        expected_requests = []
        for due, _bus_rank, _slot, _node_index, _order, bus, target, request, label, progress_count in sorted(events):
            self._sleep_until_monotonic(due)
            prefixed_request = self._send_routed_uds_no_wait(
                client,
                target,
                request,
                label,
                apply_pacing=(bus != "LIN"),
            )
            expected_requests.append((label, prefixed_request, self._target_is_simulated(target), progress_count))

        self._wait_routed_transport_acks_and_progress(client, expected_requests, progress_cb)
        time.sleep(ROUTED_READ_CODING_DRAIN_SECONDS)
        client.drain()

    def _execute_paced_simulated_flash_slot(self, slot, work, client, progress_cb, is_fbl):
        prepared = []
        for node_name, payloads, target in work:
            sequence = self._simulated_flash_request_sequence(node_name, payloads, target, is_fbl)
            if sequence:
                prepared.append((node_name, payloads, target, sequence))

        if not prepared:
            return

        groups = {}
        for item in prepared:
            target = item[2]
            groups.setdefault(bus_category(target.get("bus_type", "UNKNOWN")), []).append(item)

        bus_order = {"CAN": 0, "CANFD": 1, "LIN": 2}
        ordered_buses = sorted(groups, key=lambda bus: bus_order.get(bus, 99))
        max_depth = max(len(items) for items in groups.values())
        max_steps = 0
        for items in groups.values():
            max_steps = max(max_steps, max(len(item[3]) for item in items))
        start_time = time.monotonic()
        if "LIN" in groups:
            self.log(
                "Execute Parallel Bundle: LIN routed flash pacing "
                f"{int(ROUTED_LIN_REQUEST_SPACING_SECONDS * 1000)} ms; "
                "other bus lanes continue independently"
            )

        next_due_by_bus = {}
        events = []
        order = 0
        for step_index in range(max_steps):
            for node_index in range(max_depth):
                desired_due = (
                    start_time
                    + (step_index * ROUTED_READ_CODING_SERVICE_GAP_SECONDS)
                    + (node_index * ROUTED_READ_CODING_NODE_STAGGER_SECONDS)
                )
                for bus in ordered_buses:
                    items = groups[bus]
                    if node_index >= len(items):
                        continue
                    _node_name, _payloads, target, sequence = items[node_index]
                    if step_index >= len(sequence):
                        continue
                    request, label, progress_count = sequence[step_index]
                    if request is None:
                        continue
                    due = self._scheduled_routed_due(bus, desired_due, next_due_by_bus)
                    events.append((
                        due,
                        bus_order.get(bus, 99),
                        node_index,
                        order,
                        bus,
                        target,
                        bytes(request),
                        label,
                        progress_count,
                    ))
                    order += 1

        expected_requests = []
        for due, _bus_rank, _node_index, _order, bus, target, request, label, progress_count in sorted(events):
            self._sleep_until_monotonic(due)
            prefixed_request = self._send_routed_uds_no_wait(
                client,
                target,
                request,
                label,
                apply_pacing=(bus != "LIN"),
            )
            # Simulated routed targets are allowed to stay silent after the
            # ZGW-forwarded request, regardless of bus. LIN simulations can
            # sometimes acknowledge one step and then omit the next, which
            # should not fail a non-strict paced read/flash flow.
            no_response_only = self._target_is_simulated(target)
            expected_requests.append((label, prefixed_request, no_response_only, progress_count))
        self._wait_routed_transport_acks_and_progress(client, expected_requests, progress_cb)

        time.sleep(ROUTED_READ_CODING_DRAIN_SECONDS)
        client.drain()

    def _simulated_flash_request_sequence(self, node_name, payloads, target, is_fbl):
        sequence = []

        def add(request, label, progress_count=0):
            sequence.append((bytes(request) if request is not None else None, label, progress_count))

        if is_fbl:
            add(bytes([0x10, SESSION_EXTENDED]), f"{node_name}: Extended Session before flash controls")
            add(b"\x28\x01\x03", f"{node_name}: CommunicationControl enableRxAndDisableTx")
            add(b"\x85\x02", f"{node_name}: ControlDTCSetting off")
            add(bytes([0x10, SESSION_PROGRAMMING]), f"{node_name}: Programming Session")

        configured_block_size = parse_int(self.block_size_var.get())
        block_size = max(8, min(ROUTED_TRANSFER_DATA_MAX_CHUNK_SIZE, configured_block_size))
        for payload in payloads:
            address = parse_int(payload["address"])
            size = int(payload["size"])
            expected_crc = parse_int(payload["crc32"])
            data = self._read_payload_data(payload)
            if len(data) != size:
                raise FcdError(f"{payload['ecu']}: payload size mismatch {len(data)} != {size}")
            if (binascii.crc32(data) & 0xFFFFFFFF) != expected_crc:
                raise FcdError(f"{payload['ecu']}: payload CRC mismatch")

            block_name = "FBL" if self._payload_is_fbl(payload) else "APPL"
            label = f"{payload['ecu']} {block_name} {int_hex(address, 8)} size={size}"
            self.log(
                f"Flash dry-run routed start: {label} target={int_hex(parse_int(payload['target_logical_address']))} "
                f"block_data={block_size} routed_max_block_data={ROUTED_TRANSFER_DATA_MAX_CHUNK_SIZE}"
            )

            if block_name == "FBL":
                add(
                    b"\x31\x01" + struct.pack(">H", ROUTINE_SELECT_SW_BLOCK) + bytes([ACTIVE_SW_BLOCK_FBL]),
                    f"{node_name}: RoutineControl 0200 Select FBL software block",
                )
                add(
                    b"\x31\x01" + struct.pack(">H", ROUTINE_START_FBL_RAM_UPDATER),
                    f"{node_name}: RoutineControl 0155 Start FBL RAM Updater",
                )
                add(
                    b"\x31\x01" + struct.pack(">H", ROUTINE_ERASE_MEMORY) + struct.pack(">II", address, size),
                    f"{node_name}: RoutineControl Erase {block_name}",
                )
            elif self.erase_var.get():
                add(
                    b"\x31\x01" + struct.pack(">H", ROUTINE_ERASE_MEMORY) + struct.pack(">II", address, size),
                    f"{node_name}: RoutineControl Erase {block_name}",
                )

            add(
                b"\x34\x00\x44" + struct.pack(">II", address, size),
                f"{node_name}: RequestDownload {block_name}",
            )

            block_counter = 1
            transfer_index = 1
            offset = 0
            while offset < size:
                chunk = data[offset : offset + block_size]
                add(
                    bytes([0x36, block_counter]) + chunk,
                    (
                        f"{node_name}: TransferData {block_name} #{transfer_index} "
                        f"bsc=0x{block_counter:02X} offset=0x{offset:X} len={len(chunk)}"
                    ),
                    progress_count=len(chunk),
                )
                offset += len(chunk)
                block_counter = (block_counter + 1) & 0xFF
                transfer_index += 1

            transfer_exit = b"\x37"
            if self.transfer_crc_var.get():
                transfer_exit += struct.pack(">I", expected_crc)
            add(transfer_exit, f"{node_name}: RequestTransferExit {block_name}")

            if self.verify_crc_var.get() and (not self.transfer_crc_var.get()) and block_name != "FBL":
                add(
                    b"\x31\x01"
                    + struct.pack(">H", ROUTINE_CHECK_MEMORY_CRC)
                    + struct.pack(">III", address, size, expected_crc),
                    f"{node_name}: RoutineControl CRC {block_name}",
                )
            elif self.verify_crc_var.get():
                self.log(f"{node_name}: skipped separate CRC routine for {block_name}; RequestTransferExit carries CRC")
            self.log(f"Flash dry-run routed queued: {label}")

        add(b"\x11\x01", f"{node_name}: ECUReset hardReset after {node_name} {'FBL' if is_fbl else 'APP'} flash")
        wait_steps = max(1, int(round(ROUTED_FLASH_POST_RESET_GAP_SECONDS / ROUTED_READ_CODING_SERVICE_GAP_SECONDS)))
        for _index in range(wait_steps):
            add(None, "")
        add(bytes([0x10, SESSION_DEFAULT]), f"{node_name}: Post-reset UDS readiness probe")
        return sequence

    def _parallel_target_addr(self, target, payloads=None):
        if target and not (bool(target.get("is_zgw")) or str(target.get("node_name", "")).upper() == "ZGW"):
            return parse_int(self.target_var.get() or int_hex(DEFAULT_TARGET_ADDR))
        route_target = target.get("route_metadata", {}).get("target_logical_address", "")
        if route_target:
            return parse_int(route_target)
        if payloads:
            for payload in payloads:
                payload_target = payload.get("target_logical_address", "")
                if payload_target:
                    return parse_int(payload_target)
        return DEFAULT_TARGET_ADDR

    def _execute_payloads_parallel_worker(self, payloads, target, progress_cb, strict_response=False):
        if not payloads:
            return
        target_addr = self._parallel_target_addr(target, payloads)
        client = self._new_parallel_client(target_addr)
        try:
            stage_is_fbl = any(self._payload_is_fbl(p) for p in payloads)
            self._execute_target_flash_preamble(
                client,
                target,
                is_fbl=stage_is_fbl,
                strict_response=strict_response,
            )
            for payload in payloads:
                self._execute_payload(
                    client,
                    payload,
                    is_fbl=self._payload_is_fbl(payload),
                    progress_cb=progress_cb,
                    target_info=target,
                    strict_response=strict_response,
                )
            self._execute_target_flash_postamble(
                client,
                target,
                f"{target.get('node_name', payloads[0].get('ecu', 'target'))} {('FBL' if stage_is_fbl else 'APP')} flash",
                strict_response=strict_response,
            )
        finally:
            client.close()

    def _execute_target_flash_preamble(self, client, target, is_fbl=False, strict_response=False):
        node_name = target.get("node_name", "target")
        send = self._make_target_uds_sender(
            client,
            target,
            dry=False,
            strict_response=strict_response,
        )
        if (not is_fbl) and target.get("_fcd_fbl_flash_reset_ready"):
            return
        target_is_zgw = bool(target.get("is_zgw")) or str(node_name).upper() == "ZGW"
        if target_is_zgw:
            self._execute_zgw_flash_entry(send, node_name, is_fbl=is_fbl)
            return
        self._send_session(send, SESSION_EXTENDED, f"{node_name}: Extended Session before flash controls")
        send(b"\x28\x01\x03", f"{node_name}: CommunicationControl enableRxAndDisableTx")
        send(b"\x85\x02", f"{node_name}: ControlDTCSetting off")
        self._send_session(send, SESSION_PROGRAMMING, f"{node_name}: Programming Session")
        if is_fbl:
            self.log(f"{node_name}: FBL RAM-updater entry deferred until 0x0200 block selection")

    def _execute_target_flash_postamble(self, client, target, reason, strict_response=False):
        node_name = target.get("node_name", "target")
        send = self._make_target_uds_sender(
            client,
            target,
            dry=False,
            strict_response=strict_response,
        )
        self._send_ecu_reset_best_effort(send, f"{node_name}: ECUReset hardReset after {reason}")
        if not getattr(send, "dry_run", False) and not self._target_is_simulated(target):
            self._recover_doip_after_reset(client, reason, require_current_client=False)
        else:
            self._send_session(send, SESSION_DEFAULT, f"{node_name}: Post-reset UDS readiness probe")
        if "FBL" in str(reason).upper():
            target["_fcd_fbl_flash_reset_ready"] = True

    def _execute_parallel_coding(self, manifest, progress_cb, include_zgw=True, only_zgw=False, strict_response=False):
        targets = {t.get("node_name", ""): t for t in manifest.get("targets", [])}
        live_descriptors = self._all_coding_descriptors()
        coding_events = [e for e in manifest.get("schedule", []) if e.get("phase") == "coding"]
        if only_zgw:
            coding_events = [e for e in coding_events if e.get("is_zgw")]
        elif not include_zgw:
            coding_events = [e for e in coding_events if not e.get("is_zgw")]
        lanes = {}
        for slot in sorted({e.get("time_slot", 0) for e in coding_events}):
            events = [e for e in coding_events if e.get("time_slot", 0) == slot]
            work_by_bus = {}
            for event in events:
                target = targets.get(event.get("node_name", ""), {})
                node_name = str(target.get("node_name") or event.get("node_name", ""))
                descriptor = target.get("coding_descriptor") or live_descriptors.get(node_name.upper())
                if not descriptor:
                    self.log(f"Execute Parallel Bundle: {node_name} has no configured coding values; skipped")
                    continue
                if not hex_to_bytes(descriptor.get("mask_hex", "")):
                    self.log(f"Execute Parallel Bundle: {node_name} coding mask is empty; skipped")
                    continue
                if target.get("coding_descriptor") is not descriptor:
                    target = dict(target)
                    target["coding_descriptor"] = descriptor
                bus = bus_category(target.get("bus_type") or event.get("bus_type", "UNKNOWN"))
                work_by_bus.setdefault(bus, []).append((event, target))
            for bus, work in work_by_bus.items():
                lanes.setdefault(bus, []).append((slot, work))

        if not lanes:
            return

        lane_text = ", ".join(
            f"{bus}:{len(tasks)} slot(s)"
            for bus, tasks in sorted(lanes.items())
        )
        self.log(f"Execute Parallel Bundle: coding bus lanes={lane_text}")

        shared_paced_client = None

        def run_lane(bus, tasks):
            for slot, work in tasks:
                self._execute_parallel_coding_slot_work(
                    slot,
                    bus,
                    work,
                    shared_paced_client,
                    strict_response,
                )

        try:
            if (
                self.transport_var.get() == "DoIP"
                and any(
                    self._can_run_paced_coding_slot("Code Vehicle", work)
                    for tasks in lanes.values()
                    for _slot, work in tasks
                )
            ):
                shared_paced_client = self._new_parallel_client(parse_int(self.target_var.get() or int_hex(DEFAULT_TARGET_ADDR)))
                shared_paced_client.request_spacing_seconds = max(shared_paced_client.request_spacing_seconds, REQUEST_SPACING_SECONDS)
                shared_paced_client.drain()
                self.log("Execute Parallel Bundle: coding uses one DoIP TCP connection for routed bus schedule")

            if len(lanes) == 1:
                bus, tasks = next(iter(lanes.items()))
                run_lane(bus, tasks)
                return

            with ThreadPoolExecutor(max_workers=max(1, min(len(lanes), PARALLEL_BUNDLE_MAX_WORKERS))) as executor:
                futures = [
                    executor.submit(run_lane, bus, tasks)
                    for bus, tasks in lanes.items()
                ]
                for future in as_completed(futures):
                    future.result()
        finally:
            if shared_paced_client is not None:
                shared_paced_client.close()

    def _execute_parallel_coding_slot_work(self, slot, bus, work, shared_client, strict_response=False):
        if not work:
            return shared_client

        if (
            self.transport_var.get() == "DoIP"
            and self._can_run_paced_coding_slot("Code Vehicle", work)
        ):
            owns_client = False
            if shared_client is None:
                shared_client = self._new_parallel_client(parse_int(self.target_var.get() or int_hex(DEFAULT_TARGET_ADDR)))
                shared_client.request_spacing_seconds = max(shared_client.request_spacing_seconds, REQUEST_SPACING_SECONDS)
                shared_client.drain()
                owns_client = True
                self.log(f"Execute Parallel Bundle: coding bus={bus} uses one DoIP TCP connection for routed requests")
            nodes = ", ".join(f"{event.get('node_name', '')}({event.get('bus_type', 'UNKNOWN')})" for event, _target in work)
            self.log(f"Execute Parallel Bundle: coding slot {slot} bus={bus} parallel nodes={nodes}")
            try:
                self._run_paced_code_vehicle_slot(slot, work, shared_client)
            finally:
                if owns_client:
                    shared_client.close()
                    shared_client = None
            return shared_client

        self.log(
            f"Execute Parallel Bundle: coding slot {slot} bus={bus} "
            f"nodes={', '.join(event.get('node_name', '') for event, _target in work)}"
        )
        with ThreadPoolExecutor(max_workers=max(1, min(len(work), PARALLEL_BUNDLE_MAX_WORKERS))) as executor:
            futures = [
                executor.submit(
                    self._execute_coding_descriptor_worker,
                    target,
                    self._strict_response_for_target(target, strict_response),
                )
                for _event, target in work
            ]
            for future in as_completed(futures):
                future.result()
        return shared_client

    def _execute_coding_descriptor_worker(self, target, strict_response=False):
        descriptor = target.get("coding_descriptor", {})
        mask = hex_to_bytes(descriptor.get("mask_hex", ""))
        if not mask:
            return
        target_addr = self._parallel_target_addr(target)
        client = self._new_parallel_client(target_addr)
        try:
            send = self._make_target_uds_sender(client, target, dry=False, strict_response=strict_response)
            self._ensure_session(send, SESSION_CODING_REQUESTED, f"{target.get('node_name')}: Coding Session requested as 10 41")
            self._run_coding_routine(send, parse_int(descriptor.get("write_routine", int_hex(CODING_ROUTINE_WRITE_ALL))), mask, f"{target.get('node_name')}: Write Coding", timeout=90.0)
            self._run_coding_routine(send, parse_int(descriptor.get("validate_routine", int_hex(CODING_ROUTINE_VALIDATE))), b"", f"{target.get('node_name')}: Check Coding", timeout=20.0)
        finally:
            client.close()

    def _payload_is_fbl(self, payload):
        flash_kind = str(payload.get("flash_kind", "")).strip().upper()
        if flash_kind:
            return flash_kind == "FBL"
        name = str(payload.get("ecu", "") + " " + payload.get("file", "")).upper()
        address = parse_int(payload.get("address", "0"))
        return "FBL" in name or address < DEFAULT_APP_START

    def _target_for_payload(self, payload):
        names = {
            str(payload.get("node_name", "")).upper(),
            str(payload.get("ecu", "")).upper(),
        }
        names.discard("")
        manifest = self.package_manifest or {}
        for target in manifest.get("targets", []):
            if str(target.get("node_name", "")).upper() in names:
                return target
        for target in self.node_rows.values():
            if str(target.get("node_name", "")).upper() in names:
                return target
        return {}

    def _payload_targets_zgw(self, payload, target_info=None):
        if target_info is None:
            target_info = self._target_for_payload(payload)

        names = {
            str(payload.get("node_name", "")).upper(),
            str(payload.get("ecu", "")).upper(),
            str(target_info.get("node_name", "")).upper(),
        }
        names.discard("")

        if bool(payload.get("is_zgw")) or bool(target_info.get("is_zgw")) or ("ZGW" in names):
            return True

        try:
            payload_target = parse_int(payload.get("target_logical_address", int_hex(DEFAULT_TARGET_ADDR)))
        except Exception:
            payload_target = 0

        return payload_target == DEFAULT_TARGET_ADDR

    def _make_uds_sender(self, client, dry=None, accept_no_response=False):
        if dry is None:
            dry = False

        def send(request, name, timeout=None, allow_no_response=False):
            request = bytes(request)
            no_response_ok = allow_no_response or accept_no_response
            if dry:
                self.log(f"DRY {name}: {uds_request_log_text(request)}")
                time.sleep(0.001)
                return b""
            self.log(f"TX {name}: {uds_request_log_text(request)}")
            try:
                if isinstance(client, DoipClient):
                    response = self._send_uds_with_reconnect(
                        client,
                        request,
                        name,
                        timeout=timeout or float(self.timeout_var.get()),
                        allow_no_response=no_response_ok,
                    )
                    send.last_nrc78_count = getattr(client, "last_nrc78_count", 0)
                else:
                    response = client.send_uds(
                        request,
                        timeout=timeout or float(self.timeout_var.get()),
                        allow_no_response=no_response_ok,
                    )
                    send.last_nrc78_count = 0
            except Exception:
                if no_response_ok:
                    self.log(f"RX {name}: no response accepted")
                    return b""
                raise
            if no_response_ok and not response:
                self.log(f"RX {name}: no response accepted")
                return b""
            self.log(f"RX {name}: {bytes_to_hex(response)}")
            require_positive_response(response, request[0])
            return response

        send.dry_run = dry
        send.last_nrc78_count = 0
        return send

    def _send_session(self, send, session, label, allow_no_response=False, timeout=10.0):
        response = send(
            bytes([0x10, session & 0xFF]),
            label,
            timeout=timeout,
            allow_no_response=allow_no_response,
        )
        if isinstance(response, bytes) and len(response) >= 2:
            self.log(f"{label}: active_session_response=0x{response[1]:02X}")
            if response[1] != (session & 0xFF):
                raise FcdError(
                    f"{label}: requested session 0x{session:02X}, response echoed 0x{response[1]:02X}"
                )
        return response

    def _read_active_diag_session(self, send, prefix, timeout=2.0):
        response = send(
            b"\x22" + struct.pack(">H", DID_ACTIVE_DIAG_SESSION),
            f"{prefix}: Read Active Diagnostic Session F186",
            timeout=timeout,
        )
        require_positive_response(response, 0x22)
        if (len(response) < 4) or (response[1] != 0xF1) or (response[2] != 0x86):
            raise FcdError(f"{prefix}: malformed active-session DID response {bytes_to_hex(response)}")
        session = response[3]
        self.log(f"{prefix}: active_session=0x{session:02X}")
        return session

    def _read_active_sw_block(self, send, prefix, timeout=2.0):
        response = send(
            b"\x22" + struct.pack(">H", DID_ACTIVE_SW_BLOCK),
            f"{prefix}: Read Active Software Block F100",
            timeout=timeout,
        )
        require_positive_response(response, 0x22)
        if (len(response) < 4) or (response[1] != 0xF1) or (response[2] != 0x00):
            raise FcdError(f"{prefix}: malformed active-sw-block DID response {bytes_to_hex(response)}")
        block = response[3]
        self.log(f"{prefix}: active_sw_block=0x{block:02X}")
        return block

    def _ensure_session(self, send, session, label, timeout=3.0):
        try:
            return self._send_session(send, session, label, timeout=timeout)
        except Exception as session_exc:
            self.log(f"{label}: no positive session response ({session_exc}); verifying active session")
            try:
                active_session = self._read_active_diag_session(send, label, timeout=2.0)
            except Exception as verify_exc:
                raise FcdError(
                    f"{label}: session response missing and active session could not be verified"
                ) from verify_exc

            if active_session == (session & 0xFF):
                self.log(f"{label}: active session is 0x{active_session:02X}; continuing")
                return b""

            raise FcdError(
                f"{label}: requested session 0x{session:02X}, active session is 0x{active_session:02X}"
            ) from session_exc

    def _try_send_session(self, send, session, label, timeout=3.0):
        try:
            return send(bytes([0x10, session & 0xFF]), label, timeout=timeout, allow_no_response=True)
        except Exception as exc:
            self.log(f"{label}: no usable response ({exc}); continuing")
            return b""

    def _try_send(self, send, request, label, timeout=3.0):
        try:
            return send(request, label, timeout=timeout, allow_no_response=True)
        except Exception as exc:
            self.log(f"{label}: no usable response ({exc}); continuing")
            return b""

    def _run_flash_routine_control(self, send, rid, option, label, timeout, expected_payload=None):
        request = b"\x31\x01" + struct.pack(">H", rid) + bytes(option or b"")
        expected_payload = bytes(expected_payload) if expected_payload is not None else None
        start = time.monotonic()
        self.log(
            f"{label}: routine=0x{rid:04X} request_time={datetime.now().isoformat(timespec='milliseconds')}"
        )

        try:
            response = send(request, label, timeout=timeout)
        except NegativeResponse as exc:
            elapsed = time.monotonic() - start
            self.log(
                f"{label}: routine=0x{rid:04X} final=negative elapsed={elapsed:.3f}s "
                f"nrc78=handled-by-transport nrc=0x{exc.nrc:02X}"
            )
            raise

        if len(response) < 4 or response[0] != 0x71 or response[1] != 0x01:
            raise FcdError(f"{label}: malformed RoutineControl response {bytes_to_hex(response)}")
        echoed_rid = struct.unpack(">H", response[2:4])[0]
        if echoed_rid != rid:
            raise FcdError(
                f"{label}: RoutineControl RID echo mismatch sent=0x{rid:04X} received=0x{echoed_rid:04X}"
            )
        if expected_payload is not None:
            actual_payload = response[4 : 4 + len(expected_payload)]
            if actual_payload != expected_payload:
                raise FcdError(
                    f"{label}: RoutineControl payload mismatch expected={bytes_to_hex(expected_payload)} "
                    f"received={bytes_to_hex(response[4:])}"
                )
        elapsed = time.monotonic() - start
        nrc78_count = getattr(send, "last_nrc78_count", 0)
        self.log(
            f"{label}: routine=0x{rid:04X} final=positive elapsed={elapsed:.3f}s nrc78={nrc78_count}"
        )
        return response

    def _select_fbl_software_block(self, send, label_prefix):
        self._run_flash_routine_control(
            send,
            ROUTINE_SELECT_SW_BLOCK,
            bytes([ACTIVE_SW_BLOCK_FBL]),
            f"{label_prefix}: RoutineControl 0200 Select FBL software block",
            timeout=FBL_UPDATER_ENTRY_TIMEOUT_SECONDS,
            expected_payload=bytes([ACTIVE_SW_BLOCK_FBL]),
        )

    def _read_standard_status(self, send, prefix):
        for did, name in [
            (DID_APP_SW_VERSION, "Read Software Version F101"),
            (DID_ACTIVE_SW_BLOCK, "Read Active Software Block F100"),
            (DID_ACTIVE_DIAG_SESSION, "Read Active Diagnostic Session F186"),
        ]:
            send(b"\x22" + struct.pack(">H", did), f"{prefix}: {name}")

    def _is_nrc(self, exc, sid, nrc):
        return f"Negative response for 0x{sid:02X}: NRC 0x{nrc:02X}" in str(exc)

    def _execute_zgw_flash_entry(self, send, node_name, is_fbl=False):
        self._send_session(send, SESSION_EXTENDED, f"{node_name}: Extended Session before flash controls")
        send(b"\x28\x01\x03", f"{node_name}: CommunicationControl enableRxAndDisableTx")
        send(b"\x85\x02", f"{node_name}: ControlDTCSetting off")
        self._send_session(send, SESSION_PROGRAMMING, f"{node_name}: Programming Session")

        if is_fbl:
            self.log(f"{node_name}: FBL RAM-updater entry deferred until 0x0200 block selection")

    def _execute_zgw_programming_preamble(self, client, is_fbl=False):
        send = self._make_uds_sender(client)
        self._execute_zgw_flash_entry(send, "ZGW", is_fbl=is_fbl)

    def _execute_start_fbl_ram_updater(self, client):
        send = self._make_uds_sender(client)
        self._select_fbl_software_block(send, "FBL updater entry")
        self._run_flash_routine_control(
            send,
            ROUTINE_START_FBL_RAM_UPDATER,
            b"",
            "RoutineControl 0155 Start FBL RAM Updater",
            timeout=FBL_UPDATER_ENTRY_TIMEOUT_SECONDS,
        )

    def _execute_status_readback(self, client, prefix):
        send = self._make_uds_sender(client)
        self._read_standard_status(send, prefix)

    def _execute_hard_reset(self, client, reason):
        self._tolerant_ecu_reset(client, reason)

    def _execute_post_programming_extended(self, client):
        send = self._make_uds_sender(client)
        self._send_session(send, SESSION_DEFAULT, "Default Session after programming")
        self._send_session(send, SESSION_EXTENDED, "Extended Session after programming")
        self._read_standard_status(send, "Post-programming")
        send(b"\x28\x00\x03", "CommunicationControl enableRxAndTx")
        send(b"\x85\x01", "ControlDTCSetting on")

    def _execute_coding_after_flash(self, client):
        # Match the Coding tab: send the generated coding mask as the Write Coding
        # routine option record. Without it, the ECU only receives "31 01 02 02".
        mask = self._coding_rows_to_mask()
        send = self._make_uds_sender(client)
        self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session requested as 10 41")
        self._run_coding_routine(send, CODING_ROUTINE_WRITE_ALL, mask, "Write Coding", timeout=90.0)
        self._run_coding_routine(send, CODING_ROUTINE_VALIDATE, b"", "Check Coding", timeout=20.0)
        self._execute_hard_reset(client, "after coding")

    def _execute_final_readback(self, client):
        send = self._make_uds_sender(client)
        self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session after coding reset requested as 10 41")
        self._read_coding_app_status(send, "Final", required=True)
        for did, name in [
            (DID_APP_SW_VERSION, "Read Software Version F101"),
            (DID_ACTIVE_SW_BLOCK, "Read Active Software Block F100"),
            (DID_ACTIVE_DIAG_SESSION, "Read Active Diagnostic Session F186"),
        ]:
            send(b"\x22" + struct.pack(">H", did), f"Final: {name}")
        self._request_read_coding_routine(send, CODING_ROUTINE_READ_NVM, "Final", timeout=10.0)

    def _read_payload_data(self, payload):
        if payload.get("data_base64"):
            return base64.b64decode(payload["data_base64"])
        if self.package_bundle is not None:
            with zipfile.ZipFile(self.package_bundle, "r") as archive:
                return archive.read(str(payload["file"]).replace("\\", "/"))
        if self.package_dir is None:
            raise FcdError("Package directory is not loaded")
        path = self.package_dir / payload["file"]
        return path.read_bytes()

    def _execute_payload(self, client, payload, is_fbl=False, progress_cb=None, target_info=None, strict_response=False):
        target = parse_int(payload["target_logical_address"])
        address = parse_int(payload["address"])
        size = int(payload["size"])
        expected_crc = parse_int(payload["crc32"])
        if target_info is None:
            target_info = self._target_for_payload(payload)
        data = self._read_payload_data(payload)
        if len(data) != size:
            raise FcdError(f"{payload['ecu']}: payload size mismatch {len(data)} != {size}")
        if (binascii.crc32(data) & 0xFFFFFFFF) != expected_crc:
            raise FcdError(f"{payload['ecu']}: payload CRC mismatch")
        target_is_zgw = self._payload_targets_zgw(payload, target_info)
        if isinstance(client, DoipClient) and target_is_zgw:
            client.target_addr = target

        configured_block_size = parse_int(self.block_size_var.get())
        routed_target = bool(target_info) and not (
            target_is_zgw
        )
        if target_is_zgw:
            max_chunk_size = ZGW_ETHERNET_TRANSFER_DATA_MAX_CHUNK_SIZE
            transfer_request_limit = ZGW_ETHERNET_TRANSFER_DATA_REQUEST_LIMIT
            block_size = max(8, min(max_chunk_size, configured_block_size))
        else:
            max_chunk_size = ROUTED_TRANSFER_DATA_MAX_CHUNK_SIZE if routed_target else TRANSFER_DATA_MAX_CHUNK_SIZE
            transfer_request_limit = TRANSFER_DATA_REQUEST_LIMIT
            block_size = max(8, min(max_chunk_size, configured_block_size))
        block_name = "FBL" if is_fbl else "APPL"
        label = f"{payload['ecu']} {block_name} {int_hex(address, 8)} size={size}"
        block_note = (
            f"block_data={block_size} transfer_request_limit={transfer_request_limit}"
        )
        if configured_block_size != block_size:
            block_note += f" configured_block_size={configured_block_size}"
        if routed_target:
            block_note += f" routed_max_block_data={ROUTED_TRANSFER_DATA_MAX_CHUNK_SIZE}"
        self.log(f"Flash start: {label} target={int_hex(target)} {block_note}")

        if target_info:
            send = self._make_target_uds_sender(
                client,
                target_info,
                dry=False,
                strict_response=strict_response,
            )
        else:
            send = self._make_uds_sender(client)

        if is_fbl:
            self._select_fbl_software_block(send, block_name)
            self._run_flash_routine_control(
                send,
                ROUTINE_START_FBL_RAM_UPDATER,
                b"",
                f"RoutineControl 0155 Start FBL RAM Updater {block_name}",
                timeout=FBL_UPDATER_ENTRY_TIMEOUT_SECONDS,
            )
            self._run_flash_routine_control(
                send,
                ROUTINE_ERASE_MEMORY,
                struct.pack(">II", address, size),
                f"RoutineControl Erase {block_name}",
                timeout=max(FBL_ERASE_TIMEOUT_SECONDS, float(self.fbl_erase_timeout_var.get())),
                expected_payload=b"\x00",
            )
        elif self.erase_var.get():
            self._run_flash_routine_control(
                send,
                ROUTINE_ERASE_MEMORY,
                struct.pack(">II", address, size),
                f"RoutineControl Erase {block_name}",
                timeout=max(FBL_ERASE_TIMEOUT_SECONDS, float(self.fbl_erase_timeout_var.get())),
            )

        req_download = b"\x34\x00\x44" + struct.pack(">II", address, size)
        download_response = send(req_download, f"RequestDownload {block_name}", timeout=15.0)
        transfer_request_limit = self._request_download_max_block_length(download_response)
        ecu_payload_limit = max(0, transfer_request_limit - TRANSFER_DATA_OVERHEAD_BYTES)
        if ecu_payload_limit < 8:
            raise FcdError(
                f"{payload['ecu']}: RequestDownload max block length too small "
                f"({transfer_request_limit}, payload {ecu_payload_limit})"
            )
        old_block_size = block_size
        block_size = max(8, min(block_size, ecu_payload_limit))
        if block_size != old_block_size:
            self.log(
                f"{payload['ecu']}: TransferData block size clamped by ECU "
                f"RequestDownload limit: {old_block_size} -> {block_size} "
                f"(max_len={transfer_request_limit})"
            )

        block_counter = 1
        transfer_index = 1
        offset = 0
        while offset < size:
            chunk = data[offset : offset + block_size]
            request = bytes([0x36, block_counter]) + chunk
            transfer_label = (
                f"TransferData {block_name} #{transfer_index} "
                f"bsc=0x{block_counter:02X} offset=0x{offset:X} len={len(chunk)}"
            )
            response = send(request, transfer_label, timeout=20.0 if target_is_zgw else 8.0)
            if len(response) >= 2 and response[1] != block_counter:
                raise FcdError(
                    f"TransferData block echo mismatch at #{transfer_index} "
                    f"(sent bsc=0x{block_counter:02X}, received 0x{response[1]:02X})"
                )
            offset += len(chunk)
            if progress_cb is not None:
                progress_cb(len(chunk))
            else:
                self.root.after(0, lambda done=offset, total=size, name=block_name: self.progress.configure(value=min(done, total), maximum=max(1, total)))
            block_counter = (block_counter + 1) & 0xFF
            transfer_index += 1

        transfer_exit = b"\x37"
        if self.transfer_crc_var.get():
            transfer_exit += struct.pack(">I", expected_crc)
        send(transfer_exit, f"RequestTransferExit {block_name}", timeout=15.0)

        if self.verify_crc_var.get() and (not self.transfer_crc_var.get()) and not is_fbl:
            crc_request = (
                b"\x31\x01"
                + struct.pack(">H", ROUTINE_CHECK_MEMORY_CRC)
                + struct.pack(">III", address, size, expected_crc)
            )
            send(crc_request, f"RoutineControl CRC {block_name}", timeout=30.0)
        elif self.verify_crc_var.get():
            self.log(f"Skipped separate CRC routine for {block_name}; RequestTransferExit carries CRC")

        self.log(f"Flash complete: {label}")

    def save_trace_clicked(self):
        path = filedialog.asksaveasfilename(defaultextension=".log", filetypes=[("Log", "*.log"), ("Text", "*.txt")])
        if not path:
            return
        Path(path).write_text(self.trace_text.get("1.0", "end"), encoding="utf-8")

    def _on_close(self):
        try:
            self._save_settings_file()
        except Exception as exc:
            self.log(f"Settings save failed: {exc}")
        self.test_stop.set()
        self.worker_stop.set()
        self.stop_keepalive()
        if self.client is not None:
            self.client.close()
        self.root.destroy()


def main():
    root = tk.Tk()
    app = FcdApp(root)
    app.log(f"{APP_TITLE} started")
    root.mainloop()


if __name__ == "__main__":
    main()
