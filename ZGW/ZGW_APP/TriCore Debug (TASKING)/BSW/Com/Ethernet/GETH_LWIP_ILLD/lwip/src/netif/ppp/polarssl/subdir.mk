################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/arc4.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/des.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md4.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md5.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/sha1.c" 

COMPILED_SRCS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/arc4.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/des.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md4.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md5.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/sha1.src" 

C_DEPS += \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/arc4.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/des.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md4.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md5.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/sha1.d" 

OBJS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/arc4.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/des.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md4.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md5.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/sha1.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/arc4.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/arc4.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/arc4.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/arc4.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/des.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/des.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/des.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/des.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md4.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md4.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md4.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md4.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md5.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md5.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md5.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md5.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/sha1.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/sha1.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/sha1.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/sha1.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-netif-2f-ppp-2f-polarssl

clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-netif-2f-ppp-2f-polarssl:
	-$(RM) ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/arc4.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/arc4.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/arc4.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/des.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/des.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/des.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md4.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md4.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md4.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md5.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md5.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/md5.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/sha1.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/sha1.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/netif/ppp/polarssl/sha1.src

.PHONY: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-netif-2f-ppp-2f-polarssl

