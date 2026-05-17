################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Diag/Dcm/Dcm.c" \
"../BSW/Diag/Dcm/Dcm_Cfg.c" \
"../BSW/Diag/Dcm/Dcm_DoIP_Adapter.c" \
"../BSW/Diag/Dcm/Dcm_EthTpBridge.c" 

COMPILED_SRCS += \
"BSW/Diag/Dcm/Dcm.src" \
"BSW/Diag/Dcm/Dcm_Cfg.src" \
"BSW/Diag/Dcm/Dcm_DoIP_Adapter.src" \
"BSW/Diag/Dcm/Dcm_EthTpBridge.src" 

C_DEPS += \
"./BSW/Diag/Dcm/Dcm.d" \
"./BSW/Diag/Dcm/Dcm_Cfg.d" \
"./BSW/Diag/Dcm/Dcm_DoIP_Adapter.d" \
"./BSW/Diag/Dcm/Dcm_EthTpBridge.d" 

OBJS += \
"BSW/Diag/Dcm/Dcm.o" \
"BSW/Diag/Dcm/Dcm_Cfg.o" \
"BSW/Diag/Dcm/Dcm_DoIP_Adapter.o" \
"BSW/Diag/Dcm/Dcm_EthTpBridge.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Diag/Dcm/Dcm.src":"../BSW/Diag/Dcm/Dcm.c" "BSW/Diag/Dcm/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Diag/Dcm/Dcm.o":"BSW/Diag/Dcm/Dcm.src" "BSW/Diag/Dcm/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Diag/Dcm/Dcm_Cfg.src":"../BSW/Diag/Dcm/Dcm_Cfg.c" "BSW/Diag/Dcm/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Diag/Dcm/Dcm_Cfg.o":"BSW/Diag/Dcm/Dcm_Cfg.src" "BSW/Diag/Dcm/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Diag/Dcm/Dcm_DoIP_Adapter.src":"../BSW/Diag/Dcm/Dcm_DoIP_Adapter.c" "BSW/Diag/Dcm/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Diag/Dcm/Dcm_DoIP_Adapter.o":"BSW/Diag/Dcm/Dcm_DoIP_Adapter.src" "BSW/Diag/Dcm/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Diag/Dcm/Dcm_EthTpBridge.src":"../BSW/Diag/Dcm/Dcm_EthTpBridge.c" "BSW/Diag/Dcm/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Diag/Dcm/Dcm_EthTpBridge.o":"BSW/Diag/Dcm/Dcm_EthTpBridge.src" "BSW/Diag/Dcm/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Diag-2f-Dcm

clean-BSW-2f-Diag-2f-Dcm:
	-$(RM) ./BSW/Diag/Dcm/Dcm.d ./BSW/Diag/Dcm/Dcm.o ./BSW/Diag/Dcm/Dcm.src ./BSW/Diag/Dcm/Dcm_Cfg.d ./BSW/Diag/Dcm/Dcm_Cfg.o ./BSW/Diag/Dcm/Dcm_Cfg.src ./BSW/Diag/Dcm/Dcm_DoIP_Adapter.d ./BSW/Diag/Dcm/Dcm_DoIP_Adapter.o ./BSW/Diag/Dcm/Dcm_DoIP_Adapter.src ./BSW/Diag/Dcm/Dcm_EthTpBridge.d ./BSW/Diag/Dcm/Dcm_EthTpBridge.o ./BSW/Diag/Dcm/Dcm_EthTpBridge.src

.PHONY: clean-BSW-2f-Diag-2f-Dcm

