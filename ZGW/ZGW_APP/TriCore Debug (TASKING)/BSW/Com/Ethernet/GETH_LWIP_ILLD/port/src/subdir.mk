################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_lwip.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_netif.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/sys_arch.c" 

COMPILED_SRCS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_lwip.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_netif.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/sys_arch.src" 

C_DEPS += \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_lwip.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_netif.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/sys_arch.d" 

OBJS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_lwip.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_netif.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/sys_arch.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_lwip.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_lwip.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_lwip.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_lwip.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_netif.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_netif.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_netif.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_netif.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/sys_arch.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/sys_arch.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/sys_arch.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/sys_arch.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-port-2f-src

clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-port-2f-src:
	-$(RM) ./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_lwip.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_lwip.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_lwip.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_netif.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_netif.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/lwip_geth_netif.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/sys_arch.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/sys_arch.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/port/src/sys_arch.src

.PHONY: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-port-2f-src

