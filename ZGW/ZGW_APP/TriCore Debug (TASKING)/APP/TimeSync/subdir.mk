################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../APP/TimeSync/Gptp_Lab.c" \
"../APP/TimeSync/TimeBase.c" 

COMPILED_SRCS += \
"APP/TimeSync/Gptp_Lab.src" \
"APP/TimeSync/TimeBase.src" 

C_DEPS += \
"./APP/TimeSync/Gptp_Lab.d" \
"./APP/TimeSync/TimeBase.d" 

OBJS += \
"APP/TimeSync/Gptp_Lab.o" \
"APP/TimeSync/TimeBase.o" 


# Each subdirectory must supply rules for building sources it contributes
"APP/TimeSync/Gptp_Lab.src":"../APP/TimeSync/Gptp_Lab.c" "APP/TimeSync/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Wc-w514 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"APP/TimeSync/Gptp_Lab.o":"APP/TimeSync/Gptp_Lab.src" "APP/TimeSync/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"APP/TimeSync/TimeBase.src":"../APP/TimeSync/TimeBase.c" "APP/TimeSync/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Wc-w514 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"APP/TimeSync/TimeBase.o":"APP/TimeSync/TimeBase.src" "APP/TimeSync/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-APP-2f-TimeSync

clean-APP-2f-TimeSync:
	-$(RM) ./APP/TimeSync/Gptp_Lab.d ./APP/TimeSync/Gptp_Lab.o ./APP/TimeSync/Gptp_Lab.src ./APP/TimeSync/TimeBase.d ./APP/TimeSync/TimeBase.o ./APP/TimeSync/TimeBase.src

.PHONY: clean-APP-2f-TimeSync

