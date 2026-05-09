################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif_fdb.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ethernet.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_ble.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_common.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/zepif.c" 

COMPILED_SRCS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif_fdb.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ethernet.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_ble.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_common.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/zepif.src" 

C_DEPS += \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif_fdb.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ethernet.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_ble.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_common.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/zepif.d" 

OBJS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif_fdb.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ethernet.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_ble.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_common.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/zepif.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif_fdb.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif_fdb.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif_fdb.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif_fdb.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ethernet.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ethernet.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ethernet.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ethernet.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_ble.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_ble.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_ble.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_ble.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_common.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_common.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_common.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_common.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/zepif.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/zepif.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/zepif.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/zepif.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-netif

clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-netif:
	-$(RM) ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif_fdb.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif_fdb.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/bridgeif_fdb.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ethernet.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ethernet.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ethernet.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_ble.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_ble.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_ble.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_common.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_common.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/lowpan6_common.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/zepif.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/zepif.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/zepif.src

.PHONY: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-netif

