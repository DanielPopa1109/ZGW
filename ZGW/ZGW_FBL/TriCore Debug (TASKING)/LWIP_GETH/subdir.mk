################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../LWIP_GETH/lwip_geth.c" \
"../LWIP_GETH/lwip_geth_conf.c" \
"../LWIP_GETH/lwip_geth_private_phy_dp83825i.c" \
"../LWIP_GETH/lwip_geth_private_phy_rtl8211f.c" 

COMPILED_SRCS += \
"LWIP_GETH/lwip_geth.src" \
"LWIP_GETH/lwip_geth_conf.src" \
"LWIP_GETH/lwip_geth_private_phy_dp83825i.src" \
"LWIP_GETH/lwip_geth_private_phy_rtl8211f.src" 

C_DEPS += \
"./LWIP_GETH/lwip_geth.d" \
"./LWIP_GETH/lwip_geth_conf.d" \
"./LWIP_GETH/lwip_geth_private_phy_dp83825i.d" \
"./LWIP_GETH/lwip_geth_private_phy_rtl8211f.d" 

OBJS += \
"LWIP_GETH/lwip_geth.o" \
"LWIP_GETH/lwip_geth_conf.o" \
"LWIP_GETH/lwip_geth_private_phy_dp83825i.o" \
"LWIP_GETH/lwip_geth_private_phy_rtl8211f.o" 


# Each subdirectory must supply rules for building sources it contributes
"LWIP_GETH/lwip_geth.src":"../LWIP_GETH/lwip_geth.c" "LWIP_GETH/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip_geth.o":"LWIP_GETH/lwip_geth.src" "LWIP_GETH/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip_geth_conf.src":"../LWIP_GETH/lwip_geth_conf.c" "LWIP_GETH/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip_geth_conf.o":"LWIP_GETH/lwip_geth_conf.src" "LWIP_GETH/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip_geth_private_phy_dp83825i.src":"../LWIP_GETH/lwip_geth_private_phy_dp83825i.c" "LWIP_GETH/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip_geth_private_phy_dp83825i.o":"LWIP_GETH/lwip_geth_private_phy_dp83825i.src" "LWIP_GETH/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip_geth_private_phy_rtl8211f.src":"../LWIP_GETH/lwip_geth_private_phy_rtl8211f.c" "LWIP_GETH/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip_geth_private_phy_rtl8211f.o":"LWIP_GETH/lwip_geth_private_phy_rtl8211f.src" "LWIP_GETH/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-LWIP_GETH

clean-LWIP_GETH:
	-$(RM) ./LWIP_GETH/lwip_geth.d ./LWIP_GETH/lwip_geth.o ./LWIP_GETH/lwip_geth.src ./LWIP_GETH/lwip_geth_conf.d ./LWIP_GETH/lwip_geth_conf.o ./LWIP_GETH/lwip_geth_conf.src ./LWIP_GETH/lwip_geth_private_phy_dp83825i.d ./LWIP_GETH/lwip_geth_private_phy_dp83825i.o ./LWIP_GETH/lwip_geth_private_phy_dp83825i.src ./LWIP_GETH/lwip_geth_private_phy_rtl8211f.d ./LWIP_GETH/lwip_geth_private_phy_rtl8211f.o ./LWIP_GETH/lwip_geth_private_phy_rtl8211f.src

.PHONY: clean-LWIP_GETH

