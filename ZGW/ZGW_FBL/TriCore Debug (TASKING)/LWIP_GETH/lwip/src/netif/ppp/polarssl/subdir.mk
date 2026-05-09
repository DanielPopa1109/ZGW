################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../LWIP_GETH/lwip/src/netif/ppp/polarssl/arc4.c" \
"../LWIP_GETH/lwip/src/netif/ppp/polarssl/des.c" \
"../LWIP_GETH/lwip/src/netif/ppp/polarssl/md4.c" \
"../LWIP_GETH/lwip/src/netif/ppp/polarssl/md5.c" \
"../LWIP_GETH/lwip/src/netif/ppp/polarssl/sha1.c" 

COMPILED_SRCS += \
"LWIP_GETH/lwip/src/netif/ppp/polarssl/arc4.src" \
"LWIP_GETH/lwip/src/netif/ppp/polarssl/des.src" \
"LWIP_GETH/lwip/src/netif/ppp/polarssl/md4.src" \
"LWIP_GETH/lwip/src/netif/ppp/polarssl/md5.src" \
"LWIP_GETH/lwip/src/netif/ppp/polarssl/sha1.src" 

C_DEPS += \
"./LWIP_GETH/lwip/src/netif/ppp/polarssl/arc4.d" \
"./LWIP_GETH/lwip/src/netif/ppp/polarssl/des.d" \
"./LWIP_GETH/lwip/src/netif/ppp/polarssl/md4.d" \
"./LWIP_GETH/lwip/src/netif/ppp/polarssl/md5.d" \
"./LWIP_GETH/lwip/src/netif/ppp/polarssl/sha1.d" 

OBJS += \
"LWIP_GETH/lwip/src/netif/ppp/polarssl/arc4.o" \
"LWIP_GETH/lwip/src/netif/ppp/polarssl/des.o" \
"LWIP_GETH/lwip/src/netif/ppp/polarssl/md4.o" \
"LWIP_GETH/lwip/src/netif/ppp/polarssl/md5.o" \
"LWIP_GETH/lwip/src/netif/ppp/polarssl/sha1.o" 


# Each subdirectory must supply rules for building sources it contributes
"LWIP_GETH/lwip/src/netif/ppp/polarssl/arc4.src":"../LWIP_GETH/lwip/src/netif/ppp/polarssl/arc4.c" "LWIP_GETH/lwip/src/netif/ppp/polarssl/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/ppp/polarssl/arc4.o":"LWIP_GETH/lwip/src/netif/ppp/polarssl/arc4.src" "LWIP_GETH/lwip/src/netif/ppp/polarssl/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/netif/ppp/polarssl/des.src":"../LWIP_GETH/lwip/src/netif/ppp/polarssl/des.c" "LWIP_GETH/lwip/src/netif/ppp/polarssl/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/ppp/polarssl/des.o":"LWIP_GETH/lwip/src/netif/ppp/polarssl/des.src" "LWIP_GETH/lwip/src/netif/ppp/polarssl/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/netif/ppp/polarssl/md4.src":"../LWIP_GETH/lwip/src/netif/ppp/polarssl/md4.c" "LWIP_GETH/lwip/src/netif/ppp/polarssl/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/ppp/polarssl/md4.o":"LWIP_GETH/lwip/src/netif/ppp/polarssl/md4.src" "LWIP_GETH/lwip/src/netif/ppp/polarssl/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/netif/ppp/polarssl/md5.src":"../LWIP_GETH/lwip/src/netif/ppp/polarssl/md5.c" "LWIP_GETH/lwip/src/netif/ppp/polarssl/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/ppp/polarssl/md5.o":"LWIP_GETH/lwip/src/netif/ppp/polarssl/md5.src" "LWIP_GETH/lwip/src/netif/ppp/polarssl/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/netif/ppp/polarssl/sha1.src":"../LWIP_GETH/lwip/src/netif/ppp/polarssl/sha1.c" "LWIP_GETH/lwip/src/netif/ppp/polarssl/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/netif/ppp/polarssl/sha1.o":"LWIP_GETH/lwip/src/netif/ppp/polarssl/sha1.src" "LWIP_GETH/lwip/src/netif/ppp/polarssl/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-LWIP_GETH-2f-lwip-2f-src-2f-netif-2f-ppp-2f-polarssl

clean-LWIP_GETH-2f-lwip-2f-src-2f-netif-2f-ppp-2f-polarssl:
	-$(RM) ./LWIP_GETH/lwip/src/netif/ppp/polarssl/arc4.d ./LWIP_GETH/lwip/src/netif/ppp/polarssl/arc4.o ./LWIP_GETH/lwip/src/netif/ppp/polarssl/arc4.src ./LWIP_GETH/lwip/src/netif/ppp/polarssl/des.d ./LWIP_GETH/lwip/src/netif/ppp/polarssl/des.o ./LWIP_GETH/lwip/src/netif/ppp/polarssl/des.src ./LWIP_GETH/lwip/src/netif/ppp/polarssl/md4.d ./LWIP_GETH/lwip/src/netif/ppp/polarssl/md4.o ./LWIP_GETH/lwip/src/netif/ppp/polarssl/md4.src ./LWIP_GETH/lwip/src/netif/ppp/polarssl/md5.d ./LWIP_GETH/lwip/src/netif/ppp/polarssl/md5.o ./LWIP_GETH/lwip/src/netif/ppp/polarssl/md5.src ./LWIP_GETH/lwip/src/netif/ppp/polarssl/sha1.d ./LWIP_GETH/lwip/src/netif/ppp/polarssl/sha1.o ./LWIP_GETH/lwip/src/netif/ppp/polarssl/sha1.src

.PHONY: clean-LWIP_GETH-2f-lwip-2f-src-2f-netif-2f-ppp-2f-polarssl

