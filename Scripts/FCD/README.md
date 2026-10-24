# FCD maintenance notes

## Desktop application layout

`FCD.pyw` contains the current bundle parser, scheduler, ZGW diagnostics, and
CM5 panels. `cm5_ethernet.py` provides the fail-closed Ethernet path used by
CM5 backup. `dtc_names.json` is a local copy of the CM5 DiagnosticManager
display-name catalog. The older `fcd_parallel.py` remains for callers that
import its original API; the GUI uses the implementations in `FCD.pyw`.

`fcd_settings.json` keeps this checkout's ZGW node selections and points its
HEX rows at `Binary/`. CM5 connection settings are included, but no
authentication token or generated update package is shipped by this merge.
Select and verify both SOTA packages before an update. `Generated/` holds
existing example artifacts and is not evidence of a successful flash.

Run offline tests from the repository root with
`python -m unittest discover -s Scripts/FCD -p 'test_*.py'`.

## G26 lab profile

FCD discovers G26 on Classic CAN route `0x55` and accepts one contiguous APP
image at `0x08020200` or resident FBL image at `0x08000000`. HEX gaps become
erased `0xFF` bytes; image bounds and vectors are checked before a programming
request. The APP path proves `G26A2` to `G26F2` handover using DID `F187`, then
uses G26 erase/download/transfer/CRC32 TransferExit and checks APP restart.
The FBL path omits ZGW-specific RID `0200` and requests G26 SRAM updater RID
`0155`. Its activation is disabled in G26 firmware until board wiring and
recovery are demonstrated. Host tests and package creation do not prove a
connected CAN or flash session.

## ZGW operation buttons and the Tests tab

Every FCD button that sends a request to, changes state on, codes, or flashes
the ZGW must also be available as a loopable operation in the **Tests** tab.

When adding or changing such a button:

1. Put the operation's callable in `FcdApp.test_operations` in `FCD.pyw` and use
   the same user-facing name as the original button.
2. The callable must complete synchronously when it is invoked from the FCD
   worker thread. Existing button handlers that call `FcdApp.worker` already do
   this through the worker's nested-operation path.
3. Prefer a strict dedicated `*_test_once` callable when success requires a
   positive ECU response. Coding, flashing, and fault-memory reads are examples.
4. Confirm both **Run Selected** and **Run All ZGW Operations** can stop through
   **Stop Test**, and that the operation is covered by the pre/post ZGW response
   checks.

Do not add local-only controls to the list. Connect/Disconnect, file pickers,
editors, result clearing, and trace controls do not themselves test a ZGW
operation.
