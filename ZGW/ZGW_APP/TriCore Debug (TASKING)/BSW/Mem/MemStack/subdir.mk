################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Mem/MemStack/MemStack_Error.c" 

COMPILED_SRCS += \
"BSW/Mem/MemStack/MemStack_Error.src" 

C_DEPS += \
"./BSW/Mem/MemStack/MemStack_Error.d" 

OBJS += \
"BSW/Mem/MemStack/MemStack_Error.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Mem/MemStack/MemStack_Error.src":"../BSW/Mem/MemStack/MemStack_Error.c" "BSW/Mem/MemStack/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Mem/MemStack/MemStack_Error.o":"BSW/Mem/MemStack/MemStack_Error.src" "BSW/Mem/MemStack/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Mem-2f-MemStack

clean-BSW-2f-Mem-2f-MemStack:
	-$(RM) ./BSW/Mem/MemStack/MemStack_Error.d ./BSW/Mem/MemStack/MemStack_Error.o ./BSW/Mem/MemStack/MemStack_Error.src

.PHONY: clean-BSW-2f-Mem-2f-MemStack

