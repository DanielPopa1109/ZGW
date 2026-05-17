################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_lib.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_msg.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/err.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/if_api.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netbuf.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netdb.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netifapi.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/sockets.c" \
"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/tcpip.c" 

COMPILED_SRCS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_lib.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_msg.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/err.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/if_api.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netbuf.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netdb.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netifapi.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/sockets.src" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/tcpip.src" 

C_DEPS += \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_lib.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_msg.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/err.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/if_api.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netbuf.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netdb.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netifapi.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/sockets.d" \
"./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/tcpip.d" 

OBJS += \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_lib.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_msg.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/err.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/if_api.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netbuf.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netdb.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netifapi.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/sockets.o" \
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/tcpip.o" 


# Each subdirectory must supply rules for building sources it contributes
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_lib.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_lib.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_lib.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_lib.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_msg.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_msg.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_msg.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_msg.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/err.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/err.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/err.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/err.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/if_api.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/if_api.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/if_api.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/if_api.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netbuf.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netbuf.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netbuf.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netbuf.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netdb.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netdb.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netdb.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netdb.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netifapi.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netifapi.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netifapi.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netifapi.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/sockets.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/sockets.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/sockets.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/sockets.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/tcpip.src":"../BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/tcpip.c" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_APP/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --integer-enumeration --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=0 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/tcpip.o":"BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/tcpip.src" "BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/subdir.mk"
	astc --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-api

clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-api:
	-$(RM) ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_lib.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_lib.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_lib.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_msg.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_msg.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/api_msg.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/err.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/err.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/err.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/if_api.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/if_api.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/if_api.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netbuf.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netbuf.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netbuf.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netdb.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netdb.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netdb.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netifapi.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netifapi.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/netifapi.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/sockets.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/sockets.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/sockets.src ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/tcpip.d ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/tcpip.o ./BSW/Com/Ethernet/GETH_LWIP_ILLD/lwip/src/api/tcpip.src

.PHONY: clean-BSW-2f-Com-2f-Ethernet-2f-GETH_LWIP_ILLD-2f-lwip-2f-src-2f-api

