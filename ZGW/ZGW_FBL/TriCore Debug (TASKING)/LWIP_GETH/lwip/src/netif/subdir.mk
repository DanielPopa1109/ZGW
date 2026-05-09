################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../LWIP_GETH/lwip/src/netif/bridgeif.c" \
"../LWIP_GETH/lwip/src/netif/bridgeif_fdb.c" \
"../LWIP_GETH/lwip/src/netif/ethernet.c" \
"../LWIP_GETH/lwip/src/netif/lowpan6.c" \
"../LWIP_GETH/lwip/src/netif/lowpan6_ble.c" \
"../LWIP_GETH/lwip/src/netif/lowpan6_common.c" \
"../LWIP_GETH/lwip/src/netif/slipif.c" \
"../LWIP_GETH/lwip/src/netif/zepif.c" 

COMPILED_SRCS += \
"LWIP_GETH/lwip/src/netif/bridgeif.src" \
"LWIP_GETH/lwip/src/netif/bridgeif_fdb.src" \
"LWIP_GETH/lwip/src/netif/ethernet.src" \
"LWIP_GETH/lwip/src/netif/lowpan6.src" \
"LWIP_GETH/lwip/src/netif/lowpan6_ble.src" \
"LWIP_GETH/lwip/src/netif/lowpan6_common.src" \
"LWIP_GETH/lwip/src/netif/slipif.src" \
"LWIP_GETH/lwip/src/netif/zepif.src" 

C_DEPS += \
"./LWIP_GETH/lwip/src/netif/bridgeif.d" \
"./LWIP_GETH/lwip/src/netif/bridgeif_fdb.d" \
"./LWIP_GETH/lwip/src/netif/ethernet.d" \
"./LWIP_GETH/lwip/src/netif/lowpan6.d" \
"./LWIP_GETH/lwip/src/netif/lowpan6_ble.d" \
"./LWIP_GETH/lwip/src/netif/lowpan6_common.d" \
"./LWIP_GETH/lwip/src/netif/slipif.d" \
"./LWIP_GETH/lwip/src/netif/zepif.d" 

OBJS += \
"LWIP_GETH/lwip/src/netif/bridgeif.o" \
"LWIP_GETH/lwip/src/netif/bridgeif_fdb.o" \
"LWIP_GETH/lwip/src/netif/ethernet.o" \
"LWIP_GETH/lwip/src/netif/lowpan6.o" \
"LWIP_GETH/lwip/src/netif/lowpan6_ble.o" \
"LWIP_GETH/lwip/src/netif/lowpan6_common.o" \
"LWIP_GETH/lwip/src/netif/slipif.o" \
"LWIP_GETH/lwip/src/netif/zepif.o" 


# Each subdirectory must supply rules for building sources it contributes
"LWIP_GETH/lwip/src/netif/bridgeif.src":"../LWIP_GETH/lwip/src/netif/bridgeif.c" "LWIP_GETH/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/bridgeif.o":"LWIP_GETH/lwip/src/netif/bridgeif.src" "LWIP_GETH/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/netif/bridgeif_fdb.src":"../LWIP_GETH/lwip/src/netif/bridgeif_fdb.c" "LWIP_GETH/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/bridgeif_fdb.o":"LWIP_GETH/lwip/src/netif/bridgeif_fdb.src" "LWIP_GETH/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/netif/ethernet.src":"../LWIP_GETH/lwip/src/netif/ethernet.c" "LWIP_GETH/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/ethernet.o":"LWIP_GETH/lwip/src/netif/ethernet.src" "LWIP_GETH/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/netif/lowpan6.src":"../LWIP_GETH/lwip/src/netif/lowpan6.c" "LWIP_GETH/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/lowpan6.o":"LWIP_GETH/lwip/src/netif/lowpan6.src" "LWIP_GETH/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/netif/lowpan6_ble.src":"../LWIP_GETH/lwip/src/netif/lowpan6_ble.c" "LWIP_GETH/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/lowpan6_ble.o":"LWIP_GETH/lwip/src/netif/lowpan6_ble.src" "LWIP_GETH/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/netif/lowpan6_common.src":"../LWIP_GETH/lwip/src/netif/lowpan6_common.c" "LWIP_GETH/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/lowpan6_common.o":"LWIP_GETH/lwip/src/netif/lowpan6_common.src" "LWIP_GETH/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/netif/slipif.src":"../LWIP_GETH/lwip/src/netif/slipif.c" "LWIP_GETH/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/slipif.o":"LWIP_GETH/lwip/src/netif/slipif.src" "LWIP_GETH/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/netif/zepif.src":"../LWIP_GETH/lwip/src/netif/zepif.c" "LWIP_GETH/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/zepif.o":"LWIP_GETH/lwip/src/netif/zepif.src" "LWIP_GETH/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-LWIP_GETH-2f-lwip-2f-src-2f-netif

clean-LWIP_GETH-2f-lwip-2f-src-2f-netif:
	-$(RM) ./LWIP_GETH/lwip/src/netif/bridgeif.d ./LWIP_GETH/lwip/src/netif/bridgeif.o ./LWIP_GETH/lwip/src/netif/bridgeif.src ./LWIP_GETH/lwip/src/netif/bridgeif_fdb.d ./LWIP_GETH/lwip/src/netif/bridgeif_fdb.o ./LWIP_GETH/lwip/src/netif/bridgeif_fdb.src ./LWIP_GETH/lwip/src/netif/ethernet.d ./LWIP_GETH/lwip/src/netif/ethernet.o ./LWIP_GETH/lwip/src/netif/ethernet.src ./LWIP_GETH/lwip/src/netif/lowpan6.d ./LWIP_GETH/lwip/src/netif/lowpan6.o ./LWIP_GETH/lwip/src/netif/lowpan6.src ./LWIP_GETH/lwip/src/netif/lowpan6_ble.d ./LWIP_GETH/lwip/src/netif/lowpan6_ble.o ./LWIP_GETH/lwip/src/netif/lowpan6_ble.src ./LWIP_GETH/lwip/src/netif/lowpan6_common.d ./LWIP_GETH/lwip/src/netif/lowpan6_common.o ./LWIP_GETH/lwip/src/netif/lowpan6_common.src ./LWIP_GETH/lwip/src/netif/slipif.d ./LWIP_GETH/lwip/src/netif/slipif.o ./LWIP_GETH/lwip/src/netif/slipif.src ./LWIP_GETH/lwip/src/netif/zepif.d ./LWIP_GETH/lwip/src/netif/zepif.o ./LWIP_GETH/lwip/src/netif/zepif.src

.PHONY: clean-LWIP_GETH-2f-lwip-2f-src-2f-netif

