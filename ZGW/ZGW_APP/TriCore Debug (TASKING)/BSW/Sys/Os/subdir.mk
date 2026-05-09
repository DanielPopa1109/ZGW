################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Sys/Os/Os.c" 

COMPILED_SRCS += \
"BSW/Sys/Os/Os.src" 

C_DEPS += \
"./BSW/Sys/Os/Os.d" 

OBJS += \
"BSW/Sys/Os/Os.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Sys/Os/Os.src":"../BSW/Sys/Os/Os.c" "BSW/Sys/Os/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Sys/Os/Os.o":"BSW/Sys/Os/Os.src" "BSW/Sys/Os/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Sys-2f-Os

clean-BSW-2f-Sys-2f-Os:
	-$(RM) ./BSW/Sys/Os/Os.d ./BSW/Sys/Os/Os.o ./BSW/Sys/Os/Os.src

.PHONY: clean-BSW-2f-Sys-2f-Os

