################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/Lin/Lin.c" \
"../BSW/Com/Lin/LinIf.c" \
"../BSW/Com/Lin/LinSM.c" \
"../BSW/Com/Lin/LinTp.c" \
"../BSW/Com/Lin/Lin_Cfg.c" \
"../BSW/Com/Lin/Lin_LowLevel.c" \
"../BSW/Com/Lin/Lin_Protocol.c" 

COMPILED_SRCS += \
"BSW/Com/Lin/Lin.src" \
"BSW/Com/Lin/LinIf.src" \
"BSW/Com/Lin/LinSM.src" \
"BSW/Com/Lin/LinTp.src" \
"BSW/Com/Lin/Lin_Cfg.src" \
"BSW/Com/Lin/Lin_LowLevel.src" \
"BSW/Com/Lin/Lin_Protocol.src" 

C_DEPS += \
"./BSW/Com/Lin/Lin.d" \
"./BSW/Com/Lin/LinIf.d" \
"./BSW/Com/Lin/LinSM.d" \
"./BSW/Com/Lin/LinTp.d" \
"./BSW/Com/Lin/Lin_Cfg.d" \
"./BSW/Com/Lin/Lin_LowLevel.d" \
"./BSW/Com/Lin/Lin_Protocol.d" 

OBJS += \
"BSW/Com/Lin/Lin.o" \
"BSW/Com/Lin/LinIf.o" \
"BSW/Com/Lin/LinSM.o" \
"BSW/Com/Lin/LinTp.o" \
"BSW/Com/Lin/Lin_Cfg.o" \
"BSW/Com/Lin/Lin_LowLevel.o" \
"BSW/Com/Lin/Lin_Protocol.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/Lin/Lin.src":"../BSW/Com/Lin/Lin.c" "BSW/Com/Lin/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Lin/Lin.o":"BSW/Com/Lin/Lin.src" "BSW/Com/Lin/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Lin/LinIf.src":"../BSW/Com/Lin/LinIf.c" "BSW/Com/Lin/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Lin/LinIf.o":"BSW/Com/Lin/LinIf.src" "BSW/Com/Lin/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Lin/LinSM.src":"../BSW/Com/Lin/LinSM.c" "BSW/Com/Lin/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Lin/LinSM.o":"BSW/Com/Lin/LinSM.src" "BSW/Com/Lin/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Lin/LinTp.src":"../BSW/Com/Lin/LinTp.c" "BSW/Com/Lin/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Lin/LinTp.o":"BSW/Com/Lin/LinTp.src" "BSW/Com/Lin/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Lin/Lin_Cfg.src":"../BSW/Com/Lin/Lin_Cfg.c" "BSW/Com/Lin/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Lin/Lin_Cfg.o":"BSW/Com/Lin/Lin_Cfg.src" "BSW/Com/Lin/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Lin/Lin_LowLevel.src":"../BSW/Com/Lin/Lin_LowLevel.c" "BSW/Com/Lin/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Lin/Lin_LowLevel.o":"BSW/Com/Lin/Lin_LowLevel.src" "BSW/Com/Lin/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Lin/Lin_Protocol.src":"../BSW/Com/Lin/Lin_Protocol.c" "BSW/Com/Lin/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Lin/Lin_Protocol.o":"BSW/Com/Lin/Lin_Protocol.src" "BSW/Com/Lin/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-Lin

clean-BSW-2f-Com-2f-Lin:
	-$(RM) ./BSW/Com/Lin/Lin.d ./BSW/Com/Lin/Lin.o ./BSW/Com/Lin/Lin.src ./BSW/Com/Lin/LinIf.d ./BSW/Com/Lin/LinIf.o ./BSW/Com/Lin/LinIf.src ./BSW/Com/Lin/LinSM.d ./BSW/Com/Lin/LinSM.o ./BSW/Com/Lin/LinSM.src ./BSW/Com/Lin/LinTp.d ./BSW/Com/Lin/LinTp.o ./BSW/Com/Lin/LinTp.src ./BSW/Com/Lin/Lin_Cfg.d ./BSW/Com/Lin/Lin_Cfg.o ./BSW/Com/Lin/Lin_Cfg.src ./BSW/Com/Lin/Lin_LowLevel.d ./BSW/Com/Lin/Lin_LowLevel.o ./BSW/Com/Lin/Lin_LowLevel.src ./BSW/Com/Lin/Lin_Protocol.d ./BSW/Com/Lin/Lin_Protocol.o ./BSW/Com/Lin/Lin_Protocol.src

.PHONY: clean-BSW-2f-Com-2f-Lin

