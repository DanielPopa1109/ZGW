################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/PduR/PduR.c" 

COMPILED_SRCS += \
"BSW/Com/PduR/PduR.src" 

C_DEPS += \
"./BSW/Com/PduR/PduR.d" 

OBJS += \
"BSW/Com/PduR/PduR.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/PduR/PduR.src":"../BSW/Com/PduR/PduR.c" "BSW/Com/PduR/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/PduR/PduR.o":"BSW/Com/PduR/PduR.src" "BSW/Com/PduR/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-PduR

clean-BSW-2f-Com-2f-PduR:
	-$(RM) ./BSW/Com/PduR/PduR.d ./BSW/Com/PduR/PduR.o ./BSW/Com/PduR/PduR.src

.PHONY: clean-BSW-2f-Com-2f-PduR

