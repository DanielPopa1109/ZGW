################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Sys/SmM/01_Smu/SMU/SMU.c" 

COMPILED_SRCS += \
"BSW/Sys/SmM/01_Smu/SMU/SMU.src" 

C_DEPS += \
"./BSW/Sys/SmM/01_Smu/SMU/SMU.d" 

OBJS += \
"BSW/Sys/SmM/01_Smu/SMU/SMU.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Sys/SmM/01_Smu/SMU/SMU.src":"../BSW/Sys/SmM/01_Smu/SMU/SMU.c" "BSW/Sys/SmM/01_Smu/SMU/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Sys/SmM/01_Smu/SMU/SMU.o":"BSW/Sys/SmM/01_Smu/SMU/SMU.src" "BSW/Sys/SmM/01_Smu/SMU/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Sys-2f-SmM-2f-01_Smu-2f-SMU

clean-BSW-2f-Sys-2f-SmM-2f-01_Smu-2f-SMU:
	-$(RM) ./BSW/Sys/SmM/01_Smu/SMU/SMU.d ./BSW/Sys/SmM/01_Smu/SMU/SMU.o ./BSW/Sys/SmM/01_Smu/SMU/SMU.src

.PHONY: clean-BSW-2f-Sys-2f-SmM-2f-01_Smu-2f-SMU

