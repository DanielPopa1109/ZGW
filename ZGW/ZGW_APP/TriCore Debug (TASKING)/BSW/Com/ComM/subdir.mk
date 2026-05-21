################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/ComM/ComM.c" \
"../BSW/Com/ComM/ComM_Cfg.c" 

COMPILED_SRCS += \
"BSW/Com/ComM/ComM.src" \
"BSW/Com/ComM/ComM_Cfg.src" 

C_DEPS += \
"./BSW/Com/ComM/ComM.d" \
"./BSW/Com/ComM/ComM_Cfg.d" 

OBJS += \
"BSW/Com/ComM/ComM.o" \
"BSW/Com/ComM/ComM_Cfg.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/ComM/ComM.src":"../BSW/Com/ComM/ComM.c" "BSW/Com/ComM/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/ComM/ComM.o":"BSW/Com/ComM/ComM.src" "BSW/Com/ComM/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/ComM/ComM_Cfg.src":"../BSW/Com/ComM/ComM_Cfg.c" "BSW/Com/ComM/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/ComM/ComM_Cfg.o":"BSW/Com/ComM/ComM_Cfg.src" "BSW/Com/ComM/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-ComM

clean-BSW-2f-Com-2f-ComM:
	-$(RM) ./BSW/Com/ComM/ComM.d ./BSW/Com/ComM/ComM.o ./BSW/Com/ComM/ComM.src ./BSW/Com/ComM/ComM_Cfg.d ./BSW/Com/ComM/ComM_Cfg.o ./BSW/Com/ComM/ComM_Cfg.src

.PHONY: clean-BSW-2f-Com-2f-ComM

