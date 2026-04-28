################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/CanIf/CanIf.c" 

COMPILED_SRCS += \
"BSW/Com/CanIf/CanIf.src" 

C_DEPS += \
"./BSW/Com/CanIf/CanIf.d" 

OBJS += \
"BSW/Com/CanIf/CanIf.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/CanIf/CanIf.src":"../BSW/Com/CanIf/CanIf.c" "BSW/Com/CanIf/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/CanIf/CanIf.o":"BSW/Com/CanIf/CanIf.src" "BSW/Com/CanIf/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-CanIf

clean-BSW-2f-Com-2f-CanIf:
	-$(RM) ./BSW/Com/CanIf/CanIf.d ./BSW/Com/CanIf/CanIf.o ./BSW/Com/CanIf/CanIf.src

.PHONY: clean-BSW-2f-Com-2f-CanIf

