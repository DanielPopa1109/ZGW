################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/CanSM/CanSM.c" 

COMPILED_SRCS += \
"BSW/Com/CanSM/CanSM.src" 

C_DEPS += \
"./BSW/Com/CanSM/CanSM.d" 

OBJS += \
"BSW/Com/CanSM/CanSM.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/CanSM/CanSM.src":"../BSW/Com/CanSM/CanSM.c" "BSW/Com/CanSM/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/CanSM/CanSM.o":"BSW/Com/CanSM/CanSM.src" "BSW/Com/CanSM/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-CanSM

clean-BSW-2f-Com-2f-CanSM:
	-$(RM) ./BSW/Com/CanSM/CanSM.d ./BSW/Com/CanSM/CanSM.o ./BSW/Com/CanSM/CanSM.src

.PHONY: clean-BSW-2f-Com-2f-CanSM

