################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../LWIP_GETH/port/src/lwip_geth_lwip.c" \
"../LWIP_GETH/port/src/lwip_geth_netif.c" \
"../LWIP_GETH/port/src/sys_arch.c" 

COMPILED_SRCS += \
"LWIP_GETH/port/src/lwip_geth_lwip.src" \
"LWIP_GETH/port/src/lwip_geth_netif.src" \
"LWIP_GETH/port/src/sys_arch.src" 

C_DEPS += \
"./LWIP_GETH/port/src/lwip_geth_lwip.d" \
"./LWIP_GETH/port/src/lwip_geth_netif.d" \
"./LWIP_GETH/port/src/sys_arch.d" 

OBJS += \
"LWIP_GETH/port/src/lwip_geth_lwip.o" \
"LWIP_GETH/port/src/lwip_geth_netif.o" \
"LWIP_GETH/port/src/sys_arch.o" 


# Each subdirectory must supply rules for building sources it contributes
"LWIP_GETH/port/src/lwip_geth_lwip.src":"../LWIP_GETH/port/src/lwip_geth_lwip.c" "LWIP_GETH/port/src/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/port/src/lwip_geth_lwip.o":"LWIP_GETH/port/src/lwip_geth_lwip.src" "LWIP_GETH/port/src/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/port/src/lwip_geth_netif.src":"../LWIP_GETH/port/src/lwip_geth_netif.c" "LWIP_GETH/port/src/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/port/src/lwip_geth_netif.o":"LWIP_GETH/port/src/lwip_geth_netif.src" "LWIP_GETH/port/src/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/port/src/sys_arch.src":"../LWIP_GETH/port/src/sys_arch.c" "LWIP_GETH/port/src/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/port/src/sys_arch.o":"LWIP_GETH/port/src/sys_arch.src" "LWIP_GETH/port/src/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-LWIP_GETH-2f-port-2f-src

clean-LWIP_GETH-2f-port-2f-src:
	-$(RM) ./LWIP_GETH/port/src/lwip_geth_lwip.d ./LWIP_GETH/port/src/lwip_geth_lwip.o ./LWIP_GETH/port/src/lwip_geth_lwip.src ./LWIP_GETH/port/src/lwip_geth_netif.d ./LWIP_GETH/port/src/lwip_geth_netif.o ./LWIP_GETH/port/src/lwip_geth_netif.src ./LWIP_GETH/port/src/sys_arch.d ./LWIP_GETH/port/src/sys_arch.o ./LWIP_GETH/port/src/sys_arch.src

.PHONY: clean-LWIP_GETH-2f-port-2f-src

