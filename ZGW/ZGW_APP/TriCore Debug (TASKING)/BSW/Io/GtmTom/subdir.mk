################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Io/GtmTom/GtmTom.c" 

COMPILED_SRCS += \
"BSW/Io/GtmTom/GtmTom.src" 

C_DEPS += \
"./BSW/Io/GtmTom/GtmTom.d" 

OBJS += \
"BSW/Io/GtmTom/GtmTom.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Io/GtmTom/GtmTom.src":"../BSW/Io/GtmTom/GtmTom.c" "BSW/Io/GtmTom/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Wc-w514 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Io/GtmTom/GtmTom.o":"BSW/Io/GtmTom/GtmTom.src" "BSW/Io/GtmTom/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Io-2f-GtmTom

clean-BSW-2f-Io-2f-GtmTom:
	-$(RM) ./BSW/Io/GtmTom/GtmTom.d ./BSW/Io/GtmTom/GtmTom.o ./BSW/Io/GtmTom/GtmTom.src

.PHONY: clean-BSW-2f-Io-2f-GtmTom

