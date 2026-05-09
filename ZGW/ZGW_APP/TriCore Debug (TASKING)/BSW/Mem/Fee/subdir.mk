################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Mem/Fee/Fee.c" \
"../BSW/Mem/Fee/Fee_Cfg.c" 

COMPILED_SRCS += \
"BSW/Mem/Fee/Fee.src" \
"BSW/Mem/Fee/Fee_Cfg.src" 

C_DEPS += \
"./BSW/Mem/Fee/Fee.d" \
"./BSW/Mem/Fee/Fee_Cfg.d" 

OBJS += \
"BSW/Mem/Fee/Fee.o" \
"BSW/Mem/Fee/Fee_Cfg.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Mem/Fee/Fee.src":"../BSW/Mem/Fee/Fee.c" "BSW/Mem/Fee/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Mem/Fee/Fee.o":"BSW/Mem/Fee/Fee.src" "BSW/Mem/Fee/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Mem/Fee/Fee_Cfg.src":"../BSW/Mem/Fee/Fee_Cfg.c" "BSW/Mem/Fee/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Mem/Fee/Fee_Cfg.o":"BSW/Mem/Fee/Fee_Cfg.src" "BSW/Mem/Fee/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Mem-2f-Fee

clean-BSW-2f-Mem-2f-Fee:
	-$(RM) ./BSW/Mem/Fee/Fee.d ./BSW/Mem/Fee/Fee.o ./BSW/Mem/Fee/Fee.src ./BSW/Mem/Fee/Fee_Cfg.d ./BSW/Mem/Fee/Fee_Cfg.o ./BSW/Mem/Fee/Fee_Cfg.src

.PHONY: clean-BSW-2f-Mem-2f-Fee

