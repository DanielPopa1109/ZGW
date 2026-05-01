################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_conf.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_rtl8211f.c" 

COMPILED_SRCS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_conf.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_rtl8211f.src" 

C_DEPS += \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_conf.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_rtl8211f.d" 

OBJS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_conf.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_rtl8211f.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_conf.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_conf.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_conf.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_conf.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_rtl8211f.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_rtl8211f.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_rtl8211f.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_rtl8211f.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD

clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD:
	-$(RM) ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_conf.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_conf.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_conf.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_dp83825i.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_rtl8211f.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_rtl8211f.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip_geth_private_phy_rtl8211f.src

.PHONY: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD

