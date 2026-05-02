################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../LWIP_GETH/lwip/src/core/ipv4/acd.c" \
"../LWIP_GETH/lwip/src/core/ipv4/autoip.c" \
"../LWIP_GETH/lwip/src/core/ipv4/dhcp.c" \
"../LWIP_GETH/lwip/src/core/ipv4/etharp.c" \
"../LWIP_GETH/lwip/src/core/ipv4/icmp.c" \
"../LWIP_GETH/lwip/src/core/ipv4/igmp.c" \
"../LWIP_GETH/lwip/src/core/ipv4/ip4.c" \
"../LWIP_GETH/lwip/src/core/ipv4/ip4_addr.c" \
"../LWIP_GETH/lwip/src/core/ipv4/ip4_frag.c" 

COMPILED_SRCS += \
"LWIP_GETH/lwip/src/core/ipv4/acd.src" \
"LWIP_GETH/lwip/src/core/ipv4/autoip.src" \
"LWIP_GETH/lwip/src/core/ipv4/dhcp.src" \
"LWIP_GETH/lwip/src/core/ipv4/etharp.src" \
"LWIP_GETH/lwip/src/core/ipv4/icmp.src" \
"LWIP_GETH/lwip/src/core/ipv4/igmp.src" \
"LWIP_GETH/lwip/src/core/ipv4/ip4.src" \
"LWIP_GETH/lwip/src/core/ipv4/ip4_addr.src" \
"LWIP_GETH/lwip/src/core/ipv4/ip4_frag.src" 

C_DEPS += \
"./LWIP_GETH/lwip/src/core/ipv4/acd.d" \
"./LWIP_GETH/lwip/src/core/ipv4/autoip.d" \
"./LWIP_GETH/lwip/src/core/ipv4/dhcp.d" \
"./LWIP_GETH/lwip/src/core/ipv4/etharp.d" \
"./LWIP_GETH/lwip/src/core/ipv4/icmp.d" \
"./LWIP_GETH/lwip/src/core/ipv4/igmp.d" \
"./LWIP_GETH/lwip/src/core/ipv4/ip4.d" \
"./LWIP_GETH/lwip/src/core/ipv4/ip4_addr.d" \
"./LWIP_GETH/lwip/src/core/ipv4/ip4_frag.d" 

OBJS += \
"LWIP_GETH/lwip/src/core/ipv4/acd.o" \
"LWIP_GETH/lwip/src/core/ipv4/autoip.o" \
"LWIP_GETH/lwip/src/core/ipv4/dhcp.o" \
"LWIP_GETH/lwip/src/core/ipv4/etharp.o" \
"LWIP_GETH/lwip/src/core/ipv4/icmp.o" \
"LWIP_GETH/lwip/src/core/ipv4/igmp.o" \
"LWIP_GETH/lwip/src/core/ipv4/ip4.o" \
"LWIP_GETH/lwip/src/core/ipv4/ip4_addr.o" \
"LWIP_GETH/lwip/src/core/ipv4/ip4_frag.o" 


# Each subdirectory must supply rules for building sources it contributes
"LWIP_GETH/lwip/src/core/ipv4/acd.src":"../LWIP_GETH/lwip/src/core/ipv4/acd.c" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O3 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/acd.o":"LWIP_GETH/lwip/src/core/ipv4/acd.src" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/autoip.src":"../LWIP_GETH/lwip/src/core/ipv4/autoip.c" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O3 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/autoip.o":"LWIP_GETH/lwip/src/core/ipv4/autoip.src" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/dhcp.src":"../LWIP_GETH/lwip/src/core/ipv4/dhcp.c" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O3 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/dhcp.o":"LWIP_GETH/lwip/src/core/ipv4/dhcp.src" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/etharp.src":"../LWIP_GETH/lwip/src/core/ipv4/etharp.c" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O3 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/etharp.o":"LWIP_GETH/lwip/src/core/ipv4/etharp.src" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/icmp.src":"../LWIP_GETH/lwip/src/core/ipv4/icmp.c" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O3 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/icmp.o":"LWIP_GETH/lwip/src/core/ipv4/icmp.src" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/igmp.src":"../LWIP_GETH/lwip/src/core/ipv4/igmp.c" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O3 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/igmp.o":"LWIP_GETH/lwip/src/core/ipv4/igmp.src" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/ip4.src":"../LWIP_GETH/lwip/src/core/ipv4/ip4.c" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O3 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/ip4.o":"LWIP_GETH/lwip/src/core/ipv4/ip4.src" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/ip4_addr.src":"../LWIP_GETH/lwip/src/core/ipv4/ip4_addr.c" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O3 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/ip4_addr.o":"LWIP_GETH/lwip/src/core/ipv4/ip4_addr.src" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/ip4_frag.src":"../LWIP_GETH/lwip/src/core/ipv4/ip4_frag.c" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O3 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv4/ip4_frag.o":"LWIP_GETH/lwip/src/core/ipv4/ip4_frag.src" "LWIP_GETH/lwip/src/core/ipv4/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-LWIP_GETH-2f-lwip-2f-src-2f-core-2f-ipv4

clean-LWIP_GETH-2f-lwip-2f-src-2f-core-2f-ipv4:
	-$(RM) ./LWIP_GETH/lwip/src/core/ipv4/acd.d ./LWIP_GETH/lwip/src/core/ipv4/acd.o ./LWIP_GETH/lwip/src/core/ipv4/acd.src ./LWIP_GETH/lwip/src/core/ipv4/autoip.d ./LWIP_GETH/lwip/src/core/ipv4/autoip.o ./LWIP_GETH/lwip/src/core/ipv4/autoip.src ./LWIP_GETH/lwip/src/core/ipv4/dhcp.d ./LWIP_GETH/lwip/src/core/ipv4/dhcp.o ./LWIP_GETH/lwip/src/core/ipv4/dhcp.src ./LWIP_GETH/lwip/src/core/ipv4/etharp.d ./LWIP_GETH/lwip/src/core/ipv4/etharp.o ./LWIP_GETH/lwip/src/core/ipv4/etharp.src ./LWIP_GETH/lwip/src/core/ipv4/icmp.d ./LWIP_GETH/lwip/src/core/ipv4/icmp.o ./LWIP_GETH/lwip/src/core/ipv4/icmp.src ./LWIP_GETH/lwip/src/core/ipv4/igmp.d ./LWIP_GETH/lwip/src/core/ipv4/igmp.o ./LWIP_GETH/lwip/src/core/ipv4/igmp.src ./LWIP_GETH/lwip/src/core/ipv4/ip4.d ./LWIP_GETH/lwip/src/core/ipv4/ip4.o ./LWIP_GETH/lwip/src/core/ipv4/ip4.src ./LWIP_GETH/lwip/src/core/ipv4/ip4_addr.d ./LWIP_GETH/lwip/src/core/ipv4/ip4_addr.o ./LWIP_GETH/lwip/src/core/ipv4/ip4_addr.src ./LWIP_GETH/lwip/src/core/ipv4/ip4_frag.d ./LWIP_GETH/lwip/src/core/ipv4/ip4_frag.o ./LWIP_GETH/lwip/src/core/ipv4/ip4_frag.src

.PHONY: clean-LWIP_GETH-2f-lwip-2f-src-2f-core-2f-ipv4

