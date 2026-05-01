################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/Ethernet/DoIP.c" \
"../BSW/Com/Ethernet/EthStack.c" \
"../BSW/Com/Ethernet/EthStack_Cfg.c" \
"../BSW/Com/Ethernet/SoAd.c" \
"../BSW/Com/Ethernet/SomeIp.c" \
"../BSW/Com/Ethernet/SomeIpSd.c" \
"../BSW/Com/Ethernet/TcpIpH.c" 

COMPILED_SRCS += \
"BSW/Com/Ethernet/DoIP.src" \
"BSW/Com/Ethernet/EthStack.src" \
"BSW/Com/Ethernet/EthStack_Cfg.src" \
"BSW/Com/Ethernet/SoAd.src" \
"BSW/Com/Ethernet/SomeIp.src" \
"BSW/Com/Ethernet/SomeIpSd.src" \
"BSW/Com/Ethernet/TcpIpH.src" 

C_DEPS += \
"./BSW/Com/Ethernet/DoIP.d" \
"./BSW/Com/Ethernet/EthStack.d" \
"./BSW/Com/Ethernet/EthStack_Cfg.d" \
"./BSW/Com/Ethernet/SoAd.d" \
"./BSW/Com/Ethernet/SomeIp.d" \
"./BSW/Com/Ethernet/SomeIpSd.d" \
"./BSW/Com/Ethernet/TcpIpH.d" 

OBJS += \
"BSW/Com/Ethernet/DoIP.o" \
"BSW/Com/Ethernet/EthStack.o" \
"BSW/Com/Ethernet/EthStack_Cfg.o" \
"BSW/Com/Ethernet/SoAd.o" \
"BSW/Com/Ethernet/SomeIp.o" \
"BSW/Com/Ethernet/SomeIpSd.o" \
"BSW/Com/Ethernet/TcpIpH.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/Ethernet/DoIP.src":"../BSW/Com/Ethernet/DoIP.c" "BSW/Com/Ethernet/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/DoIP.o":"BSW/Com/Ethernet/DoIP.src" "BSW/Com/Ethernet/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/EthStack.src":"../BSW/Com/Ethernet/EthStack.c" "BSW/Com/Ethernet/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/EthStack.o":"BSW/Com/Ethernet/EthStack.src" "BSW/Com/Ethernet/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/EthStack_Cfg.src":"../BSW/Com/Ethernet/EthStack_Cfg.c" "BSW/Com/Ethernet/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/EthStack_Cfg.o":"BSW/Com/Ethernet/EthStack_Cfg.src" "BSW/Com/Ethernet/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/SoAd.src":"../BSW/Com/Ethernet/SoAd.c" "BSW/Com/Ethernet/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/SoAd.o":"BSW/Com/Ethernet/SoAd.src" "BSW/Com/Ethernet/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/SomeIp.src":"../BSW/Com/Ethernet/SomeIp.c" "BSW/Com/Ethernet/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/SomeIp.o":"BSW/Com/Ethernet/SomeIp.src" "BSW/Com/Ethernet/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/SomeIpSd.src":"../BSW/Com/Ethernet/SomeIpSd.c" "BSW/Com/Ethernet/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/SomeIpSd.o":"BSW/Com/Ethernet/SomeIpSd.src" "BSW/Com/Ethernet/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/TcpIpH.src":"../BSW/Com/Ethernet/TcpIpH.c" "BSW/Com/Ethernet/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/TcpIpH.o":"BSW/Com/Ethernet/TcpIpH.src" "BSW/Com/Ethernet/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-Ethernet

clean-BSW-2f-Com-2f-Ethernet:
	-$(RM) ./BSW/Com/Ethernet/DoIP.d ./BSW/Com/Ethernet/DoIP.o ./BSW/Com/Ethernet/DoIP.src ./BSW/Com/Ethernet/EthStack.d ./BSW/Com/Ethernet/EthStack.o ./BSW/Com/Ethernet/EthStack.src ./BSW/Com/Ethernet/EthStack_Cfg.d ./BSW/Com/Ethernet/EthStack_Cfg.o ./BSW/Com/Ethernet/EthStack_Cfg.src ./BSW/Com/Ethernet/SoAd.d ./BSW/Com/Ethernet/SoAd.o ./BSW/Com/Ethernet/SoAd.src ./BSW/Com/Ethernet/SomeIp.d ./BSW/Com/Ethernet/SomeIp.o ./BSW/Com/Ethernet/SomeIp.src ./BSW/Com/Ethernet/SomeIpSd.d ./BSW/Com/Ethernet/SomeIpSd.o ./BSW/Com/Ethernet/SomeIpSd.src ./BSW/Com/Ethernet/TcpIpH.d ./BSW/Com/Ethernet/TcpIpH.o ./BSW/Com/Ethernet/TcpIpH.src

.PHONY: clean-BSW-2f-Com-2f-Ethernet

