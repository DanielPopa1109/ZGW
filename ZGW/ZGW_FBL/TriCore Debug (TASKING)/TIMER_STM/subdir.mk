################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../TIMER_STM/timer_stm.c" \
"../TIMER_STM/timer_stm_conf.c" 

COMPILED_SRCS += \
"TIMER_STM/timer_stm.src" \
"TIMER_STM/timer_stm_conf.src" 

C_DEPS += \
"./TIMER_STM/timer_stm.d" \
"./TIMER_STM/timer_stm_conf.d" 

OBJS += \
"TIMER_STM/timer_stm.o" \
"TIMER_STM/timer_stm_conf.o" 


# Each subdirectory must supply rules for building sources it contributes
"TIMER_STM/timer_stm.src":"../TIMER_STM/timer_stm.c" "TIMER_STM/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"TIMER_STM/timer_stm.o":"TIMER_STM/timer_stm.src" "TIMER_STM/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"TIMER_STM/timer_stm_conf.src":"../TIMER_STM/timer_stm_conf.c" "TIMER_STM/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"TIMER_STM/timer_stm_conf.o":"TIMER_STM/timer_stm_conf.src" "TIMER_STM/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-TIMER_STM

clean-TIMER_STM:
	-$(RM) ./TIMER_STM/timer_stm.d ./TIMER_STM/timer_stm.o ./TIMER_STM/timer_stm.src ./TIMER_STM/timer_stm_conf.d ./TIMER_STM/timer_stm_conf.o ./TIMER_STM/timer_stm_conf.src

.PHONY: clean-TIMER_STM

