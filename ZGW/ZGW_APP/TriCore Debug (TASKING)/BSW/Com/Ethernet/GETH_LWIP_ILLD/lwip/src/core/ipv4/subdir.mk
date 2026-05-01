################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/acd.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/autoip.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/dhcp.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/etharp.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/icmp.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/igmp.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_addr.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_frag.c" 

COMPILED_SRCS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/acd.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/autoip.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/dhcp.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/etharp.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/icmp.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/igmp.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_addr.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_frag.src" 

C_DEPS += \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/acd.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/autoip.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/dhcp.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/etharp.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/icmp.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/igmp.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_addr.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_frag.d" 

OBJS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/acd.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/autoip.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/dhcp.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/etharp.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/icmp.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/igmp.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_addr.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_frag.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/acd.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/acd.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/acd.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/acd.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/autoip.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/autoip.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/autoip.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/autoip.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/dhcp.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/dhcp.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/dhcp.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/dhcp.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/etharp.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/etharp.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/etharp.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/etharp.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/icmp.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/icmp.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/icmp.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/icmp.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/igmp.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/igmp.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/igmp.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/igmp.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_addr.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_addr.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_addr.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_addr.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_frag.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_frag.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_frag.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_frag.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-core-2f-ipv4

clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-core-2f-ipv4:
	-$(RM) ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/acd.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/acd.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/acd.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/autoip.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/autoip.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/autoip.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/dhcp.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/dhcp.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/dhcp.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/etharp.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/etharp.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/etharp.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/icmp.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/icmp.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/icmp.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/igmp.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/igmp.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/igmp.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_addr.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_addr.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_addr.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_frag.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_frag.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv4/ip4_frag.src

.PHONY: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-core-2f-ipv4

