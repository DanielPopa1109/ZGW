# Supporting tools, simulation and data

These are developer/PC-side assets, not firmware runtime modules. They may communicate with a live ECU or generate programming packages; inspect target and operation before execution.

## FCD diagnostic/coding/flash desktop app

`Scripts/FCD/FCD.pyw` is a Python/Tkinter engineering GUI. It uses `fcd_parallel.py` for FCD parallel package metadata/scheduling utilities and JSON configuration in `fcd_settings.json`; `README.md` records a maintenance rule that ZGW-changing operations should be callable from the loopable Tests tab. `test_fcd_parallel.py` and `test_diagnostic_acquisition.py` are Python tests. `Generated/` contains example `.pfpkg` packages and unpacked JSON/payload outputs, not source or proof of successful programming. Execution is Windows desktop Python by opening the `.pyw`; dependencies/vehicle connection configuration must be reviewed in source/settings. Avoid treating package generation or a positive GUI step as proof of complete downstream flash success.

## Ethernet trace logger

`Scripts/ETH_LOGG/GW_ETH_LOG.pyw` is a Python/Tkinter UDP/TCP receiver and trace/graph tool for gateway frames. Default ports, packet formats, filters and decode names are documented in `GW_ETH_LOG_FRAMES.md`; `gw_eth_log_graph_settings.json` stores graph preferences. Run on a PC with Python/Tkinter; configure compatible host/network/ports. It is observational tooling and does not implement gateway services.

## Build scripts

`Scripts/Build/filter_ihex_ranges.ps1` reads Intel HEX input and writes an output with records in a selected address range excluded; app/FBL make hooks use it to omit UCB. `validate_flash_artifacts.py` is a command-line artifact checker for ELF/HEX validity, forbidden UCB/BMHD placement and combined HEX address conflicts. Use `python Scripts/Build/validate_flash_artifacts.py --help` for CLI options; provide only the intended files. Neither tool flashes hardware.

## LoadSim and model workflow

`Scripts/LoadSim/LoadSim.pyw` is a TSMaster Python API simulation script for BCM load simulation; it expects the TSMaster environment/API and matching test bench. `Load Predictor ML/` contains Python dataset/model/train/export/inspection/smoke-test utilities, dependency list (`requirements.txt`), generated PyTorch/ONNX/model artifacts, and embedded-C model/inference source. README documents invocation: `python smoke_test.py`, `python train.py --forever`, `python export_c.py`, and `python export.py` after training. Dataset is synthetic and prototype-oriented. The `__RUN__.pyw` file is the GUI/launcher entry point; inspect it and environment requirements before running. `.venv`, caches, checkpoints and ONNX/PTH outputs are environment/generated data, not project source.

## TSMaster simulation project

`SIM/` is a TSMaster project with CAN/CAN FD/LIN databases, Ethernet TCP/IP profile, simulation configuration, layouts and UI settings. `SIM/Readme.txt` identifies it as a TSMaster project directory. It supplies test/simulation setup, not gateway runtime code. `SIM/MPLibrary/test_report.py` is an API wrapper that looks up TSMaster test-report functions at runtime, not a standalone test runner. No executable test suite or custom firmware simulator was identified in SIM's listed project files.

## Databases

`DB/ZGW_CAN_3.dbc`, `ZGW_CANFD_2.dbc`, and `ZGW_LIN_4.ldf` are engineering bus-description inputs; corresponding TSMaster-format copies exist under `SIM/DB`. They support decoding/simulation and may inform generated COM configuration, but do not alone establish what the checked-in firmware transmits or receives.

## Scope and gaps

A prebuilt `Binary/` image, FCD-generated package, TSMaster simulation profile, and ML export are artifacts. None alone proves current-source correspondence, ECU compatibility, successful flashing, runtime safety or test coverage. Python/FCD tests exercise tool logic, not the complete gateway firmware. Tool-specific dependencies beyond checked-in `requirements.txt`/launcher code may be installed on the user's workstation and are not fully captured here.
