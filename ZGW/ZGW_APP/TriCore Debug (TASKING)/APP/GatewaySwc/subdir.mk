################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../APP/GatewaySwc/GatewaySwc.c" 

COMPILED_SRCS += \
"APP/GatewaySwc/GatewaySwc.src" 

C_DEPS += \
"./APP/GatewaySwc/GatewaySwc.d" 

OBJS += \
"APP/GatewaySwc/GatewaySwc.o" 


# Each subdirectory must supply rules for building sources it contributes
"APP/GatewaySwc/GatewaySwc.src":"../APP/GatewaySwc/GatewaySwc.c" "APP/GatewaySwc/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"APP/GatewaySwc/GatewaySwc.o":"APP/GatewaySwc/GatewaySwc.src" "APP/GatewaySwc/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-APP-2f-GatewaySwc

clean-APP-2f-GatewaySwc:
	-$(RM) ./APP/GatewaySwc/GatewaySwc.d ./APP/GatewaySwc/GatewaySwc.o ./APP/GatewaySwc/GatewaySwc.src

.PHONY: clean-APP-2f-GatewaySwc

