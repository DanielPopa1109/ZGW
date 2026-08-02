################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../APP/AiModel/AiModel.c" \
"../APP/AiModel/bcm_infer.c" 

COMPILED_SRCS += \
"APP/AiModel/AiModel.src" \
"APP/AiModel/bcm_infer.src" 

C_DEPS += \
"./APP/AiModel/AiModel.d" \
"./APP/AiModel/bcm_infer.d" 

OBJS += \
"APP/AiModel/AiModel.o" \
"APP/AiModel/bcm_infer.o" 


# Each subdirectory must supply rules for building sources it contributes
"APP/AiModel/AiModel.src":"../APP/AiModel/AiModel.c" "APP/AiModel/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Wc-w514 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"APP/AiModel/AiModel.o":"APP/AiModel/AiModel.src" "APP/AiModel/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"APP/AiModel/bcm_infer.src":"../APP/AiModel/bcm_infer.c" "APP/AiModel/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Wc-w514 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"APP/AiModel/bcm_infer.o":"APP/AiModel/bcm_infer.src" "APP/AiModel/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-APP-2f-AiModel

clean-APP-2f-AiModel:
	-$(RM) ./APP/AiModel/AiModel.d ./APP/AiModel/AiModel.o ./APP/AiModel/AiModel.src ./APP/AiModel/bcm_infer.d ./APP/AiModel/bcm_infer.o ./APP/AiModel/bcm_infer.src

.PHONY: clean-APP-2f-AiModel

