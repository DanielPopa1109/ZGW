# FCD maintenance notes

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
