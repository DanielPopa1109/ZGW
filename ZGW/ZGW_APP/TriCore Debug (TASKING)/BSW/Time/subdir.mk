################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Time/Dcm_TimeRoutine.c" \
"../BSW/Time/EthTimeSync.c" \
"../BSW/Time/Gptp_Lab.c" \
"../BSW/Time/TimeBase.c" 

COMPILED_SRCS += \
"BSW/Time/Dcm_TimeRoutine.src" \
"BSW/Time/EthTimeSync.src" \
"BSW/Time/Gptp_Lab.src" \
"BSW/Time/TimeBase.src" 

C_DEPS += \
"./BSW/Time/Dcm_TimeRoutine.d" \
"./BSW/Time/EthTimeSync.d" \
"./BSW/Time/Gptp_Lab.d" \
"./BSW/Time/TimeBase.d" 

OBJS += \
"BSW/Time/Dcm_TimeRoutine.o" \
"BSW/Time/EthTimeSync.o" \
"BSW/Time/Gptp_Lab.o" \
"BSW/Time/TimeBase.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Time/Dcm_TimeRoutine.src":"../BSW/Time/Dcm_TimeRoutine.c" "BSW/Time/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Time/Dcm_TimeRoutine.o":"BSW/Time/Dcm_TimeRoutine.src" "BSW/Time/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Time/EthTimeSync.src":"../BSW/Time/EthTimeSync.c" "BSW/Time/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Time/EthTimeSync.o":"BSW/Time/EthTimeSync.src" "BSW/Time/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Time/Gptp_Lab.src":"../BSW/Time/Gptp_Lab.c" "BSW/Time/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Time/Gptp_Lab.o":"BSW/Time/Gptp_Lab.src" "BSW/Time/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Time/TimeBase.src":"../BSW/Time/TimeBase.c" "BSW/Time/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Time/TimeBase.o":"BSW/Time/TimeBase.src" "BSW/Time/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Time

clean-BSW-2f-Time:
	-$(RM) ./BSW/Time/Dcm_TimeRoutine.d ./BSW/Time/Dcm_TimeRoutine.o ./BSW/Time/Dcm_TimeRoutine.src ./BSW/Time/EthTimeSync.d ./BSW/Time/EthTimeSync.o ./BSW/Time/EthTimeSync.src ./BSW/Time/Gptp_Lab.d ./BSW/Time/Gptp_Lab.o ./BSW/Time/Gptp_Lab.src ./BSW/Time/TimeBase.d ./BSW/Time/TimeBase.o ./BSW/Time/TimeBase.src

.PHONY: clean-BSW-2f-Time

