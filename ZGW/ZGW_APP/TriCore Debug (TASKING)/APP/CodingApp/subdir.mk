################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../APP/CodingApp/CodingApp.c" 

COMPILED_SRCS += \
"APP/CodingApp/CodingApp.src" 

C_DEPS += \
"./APP/CodingApp/CodingApp.d" 

OBJS += \
"APP/CodingApp/CodingApp.o" 


# Each subdirectory must supply rules for building sources it contributes
"APP/CodingApp/CodingApp.src":"../APP/CodingApp/CodingApp.c" "APP/CodingApp/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"APP/CodingApp/CodingApp.o":"APP/CodingApp/CodingApp.src" "APP/CodingApp/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-APP-2f-CodingApp

clean-APP-2f-CodingApp:
	-$(RM) ./APP/CodingApp/CodingApp.d ./APP/CodingApp/CodingApp.o ./APP/CodingApp/CodingApp.src

.PHONY: clean-APP-2f-CodingApp

