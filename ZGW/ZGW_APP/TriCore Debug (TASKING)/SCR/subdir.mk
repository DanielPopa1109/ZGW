################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../SCR/main.c" 

COMPILED_SRCS += \
"SCR/main.src" 

C_DEPS += \
"./SCR/main.d" 

OBJS += \
"SCR/main.o" 


# Each subdirectory must supply rules for building sources it contributes
"SCR/main.src":"../SCR/main.c" "SCR/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"SCR/main.o":"SCR/main.src" "SCR/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-SCR

clean-SCR:
	-$(RM) ./SCR/main.d ./SCR/main.o ./SCR/main.src

.PHONY: clean-SCR

