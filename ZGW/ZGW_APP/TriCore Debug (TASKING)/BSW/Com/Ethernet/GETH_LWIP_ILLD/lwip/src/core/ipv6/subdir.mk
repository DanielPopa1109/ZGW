################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/dhcp6.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ethip6.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/icmp6.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/inet6.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_addr.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_frag.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/mld6.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/nd6.c" 

COMPILED_SRCS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/dhcp6.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ethip6.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/icmp6.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/inet6.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_addr.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_frag.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/mld6.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/nd6.src" 

C_DEPS += \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/dhcp6.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ethip6.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/icmp6.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/inet6.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_addr.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_frag.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/mld6.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/nd6.d" 

OBJS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/dhcp6.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ethip6.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/icmp6.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/inet6.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_addr.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_frag.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/mld6.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/nd6.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/dhcp6.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/dhcp6.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/dhcp6.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/dhcp6.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ethip6.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ethip6.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ethip6.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ethip6.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/icmp6.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/icmp6.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/icmp6.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/icmp6.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/inet6.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/inet6.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/inet6.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/inet6.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_addr.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_addr.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_addr.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_addr.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_frag.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_frag.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_frag.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_frag.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/mld6.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/mld6.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/mld6.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/mld6.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/nd6.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/nd6.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/nd6.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/nd6.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-core-2f-ipv6

clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-core-2f-ipv6:
	-$(RM) ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/dhcp6.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/dhcp6.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/dhcp6.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ethip6.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ethip6.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ethip6.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/icmp6.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/icmp6.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/icmp6.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/inet6.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/inet6.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/inet6.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_addr.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_addr.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_addr.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_frag.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_frag.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/ip6_frag.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/mld6.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/mld6.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/mld6.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/nd6.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/nd6.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/core/ipv6/nd6.src

.PHONY: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-core-2f-ipv6

