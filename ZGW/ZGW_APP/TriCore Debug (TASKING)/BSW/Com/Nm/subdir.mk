################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/Nm/Nm.c" \
"../BSW/Com/Nm/Nm_Cfg.c" 

COMPILED_SRCS += \
"BSW/Com/Nm/Nm.src" \
"BSW/Com/Nm/Nm_Cfg.src" 

C_DEPS += \
"./BSW/Com/Nm/Nm.d" \
"./BSW/Com/Nm/Nm_Cfg.d" 

OBJS += \
"BSW/Com/Nm/Nm.o" \
"BSW/Com/Nm/Nm_Cfg.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/Nm/Nm.src":"../BSW/Com/Nm/Nm.c" "BSW/Com/Nm/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Nm/Nm.o":"BSW/Com/Nm/Nm.src" "BSW/Com/Nm/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Nm/Nm_Cfg.src":"../BSW/Com/Nm/Nm_Cfg.c" "BSW/Com/Nm/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Nm/Nm_Cfg.o":"BSW/Com/Nm/Nm_Cfg.src" "BSW/Com/Nm/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-Nm

clean-BSW-2f-Com-2f-Nm:
	-$(RM) ./BSW/Com/Nm/Nm.d ./BSW/Com/Nm/Nm.o ./BSW/Com/Nm/Nm.src ./BSW/Com/Nm/Nm_Cfg.d ./BSW/Com/Nm/Nm_Cfg.o ./BSW/Com/Nm/Nm_Cfg.src

.PHONY: clean-BSW-2f-Com-2f-Nm

