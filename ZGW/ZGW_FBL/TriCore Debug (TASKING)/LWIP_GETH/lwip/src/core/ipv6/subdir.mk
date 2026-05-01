################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../LWIP_GETH/lwip/src/core/ipv6/dhcp6.c" \
"../LWIP_GETH/lwip/src/core/ipv6/ethip6.c" \
"../LWIP_GETH/lwip/src/core/ipv6/icmp6.c" \
"../LWIP_GETH/lwip/src/core/ipv6/inet6.c" \
"../LWIP_GETH/lwip/src/core/ipv6/ip6.c" \
"../LWIP_GETH/lwip/src/core/ipv6/ip6_addr.c" \
"../LWIP_GETH/lwip/src/core/ipv6/ip6_frag.c" \
"../LWIP_GETH/lwip/src/core/ipv6/mld6.c" \
"../LWIP_GETH/lwip/src/core/ipv6/nd6.c" 

COMPILED_SRCS += \
"LWIP_GETH/lwip/src/core/ipv6/dhcp6.src" \
"LWIP_GETH/lwip/src/core/ipv6/ethip6.src" \
"LWIP_GETH/lwip/src/core/ipv6/icmp6.src" \
"LWIP_GETH/lwip/src/core/ipv6/inet6.src" \
"LWIP_GETH/lwip/src/core/ipv6/ip6.src" \
"LWIP_GETH/lwip/src/core/ipv6/ip6_addr.src" \
"LWIP_GETH/lwip/src/core/ipv6/ip6_frag.src" \
"LWIP_GETH/lwip/src/core/ipv6/mld6.src" \
"LWIP_GETH/lwip/src/core/ipv6/nd6.src" 

C_DEPS += \
"./LWIP_GETH/lwip/src/core/ipv6/dhcp6.d" \
"./LWIP_GETH/lwip/src/core/ipv6/ethip6.d" \
"./LWIP_GETH/lwip/src/core/ipv6/icmp6.d" \
"./LWIP_GETH/lwip/src/core/ipv6/inet6.d" \
"./LWIP_GETH/lwip/src/core/ipv6/ip6.d" \
"./LWIP_GETH/lwip/src/core/ipv6/ip6_addr.d" \
"./LWIP_GETH/lwip/src/core/ipv6/ip6_frag.d" \
"./LWIP_GETH/lwip/src/core/ipv6/mld6.d" \
"./LWIP_GETH/lwip/src/core/ipv6/nd6.d" 

OBJS += \
"LWIP_GETH/lwip/src/core/ipv6/dhcp6.o" \
"LWIP_GETH/lwip/src/core/ipv6/ethip6.o" \
"LWIP_GETH/lwip/src/core/ipv6/icmp6.o" \
"LWIP_GETH/lwip/src/core/ipv6/inet6.o" \
"LWIP_GETH/lwip/src/core/ipv6/ip6.o" \
"LWIP_GETH/lwip/src/core/ipv6/ip6_addr.o" \
"LWIP_GETH/lwip/src/core/ipv6/ip6_frag.o" \
"LWIP_GETH/lwip/src/core/ipv6/mld6.o" \
"LWIP_GETH/lwip/src/core/ipv6/nd6.o" 


# Each subdirectory must supply rules for building sources it contributes
"LWIP_GETH/lwip/src/core/ipv6/dhcp6.src":"../LWIP_GETH/lwip/src/core/ipv6/dhcp6.c" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/dhcp6.o":"LWIP_GETH/lwip/src/core/ipv6/dhcp6.src" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/ethip6.src":"../LWIP_GETH/lwip/src/core/ipv6/ethip6.c" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/ethip6.o":"LWIP_GETH/lwip/src/core/ipv6/ethip6.src" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/icmp6.src":"../LWIP_GETH/lwip/src/core/ipv6/icmp6.c" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/icmp6.o":"LWIP_GETH/lwip/src/core/ipv6/icmp6.src" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/inet6.src":"../LWIP_GETH/lwip/src/core/ipv6/inet6.c" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/inet6.o":"LWIP_GETH/lwip/src/core/ipv6/inet6.src" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/ip6.src":"../LWIP_GETH/lwip/src/core/ipv6/ip6.c" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/ip6.o":"LWIP_GETH/lwip/src/core/ipv6/ip6.src" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/ip6_addr.src":"../LWIP_GETH/lwip/src/core/ipv6/ip6_addr.c" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/ip6_addr.o":"LWIP_GETH/lwip/src/core/ipv6/ip6_addr.src" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/ip6_frag.src":"../LWIP_GETH/lwip/src/core/ipv6/ip6_frag.c" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/ip6_frag.o":"LWIP_GETH/lwip/src/core/ipv6/ip6_frag.src" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/mld6.src":"../LWIP_GETH/lwip/src/core/ipv6/mld6.c" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/mld6.o":"LWIP_GETH/lwip/src/core/ipv6/mld6.src" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/nd6.src":"../LWIP_GETH/lwip/src/core/ipv6/nd6.c" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ipv6/nd6.o":"LWIP_GETH/lwip/src/core/ipv6/nd6.src" "LWIP_GETH/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-LWIP_GETH-2f-lwip-2f-src-2f-core-2f-ipv6

clean-LWIP_GETH-2f-lwip-2f-src-2f-core-2f-ipv6:
	-$(RM) ./LWIP_GETH/lwip/src/core/ipv6/dhcp6.d ./LWIP_GETH/lwip/src/core/ipv6/dhcp6.o ./LWIP_GETH/lwip/src/core/ipv6/dhcp6.src ./LWIP_GETH/lwip/src/core/ipv6/ethip6.d ./LWIP_GETH/lwip/src/core/ipv6/ethip6.o ./LWIP_GETH/lwip/src/core/ipv6/ethip6.src ./LWIP_GETH/lwip/src/core/ipv6/icmp6.d ./LWIP_GETH/lwip/src/core/ipv6/icmp6.o ./LWIP_GETH/lwip/src/core/ipv6/icmp6.src ./LWIP_GETH/lwip/src/core/ipv6/inet6.d ./LWIP_GETH/lwip/src/core/ipv6/inet6.o ./LWIP_GETH/lwip/src/core/ipv6/inet6.src ./LWIP_GETH/lwip/src/core/ipv6/ip6.d ./LWIP_GETH/lwip/src/core/ipv6/ip6.o ./LWIP_GETH/lwip/src/core/ipv6/ip6.src ./LWIP_GETH/lwip/src/core/ipv6/ip6_addr.d ./LWIP_GETH/lwip/src/core/ipv6/ip6_addr.o ./LWIP_GETH/lwip/src/core/ipv6/ip6_addr.src ./LWIP_GETH/lwip/src/core/ipv6/ip6_frag.d ./LWIP_GETH/lwip/src/core/ipv6/ip6_frag.o ./LWIP_GETH/lwip/src/core/ipv6/ip6_frag.src ./LWIP_GETH/lwip/src/core/ipv6/mld6.d ./LWIP_GETH/lwip/src/core/ipv6/mld6.o ./LWIP_GETH/lwip/src/core/ipv6/mld6.src ./LWIP_GETH/lwip/src/core/ipv6/nd6.d ./LWIP_GETH/lwip/src/core/ipv6/nd6.o ./LWIP_GETH/lwip/src/core/ipv6/nd6.src

.PHONY: clean-LWIP_GETH-2f-lwip-2f-src-2f-core-2f-ipv6

