################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../LWIP_GETH/lwip/src/api/api_lib.c" \
"../LWIP_GETH/lwip/src/api/api_msg.c" \
"../LWIP_GETH/lwip/src/api/err.c" \
"../LWIP_GETH/lwip/src/api/if_api.c" \
"../LWIP_GETH/lwip/src/api/netbuf.c" \
"../LWIP_GETH/lwip/src/api/netdb.c" \
"../LWIP_GETH/lwip/src/api/netifapi.c" \
"../LWIP_GETH/lwip/src/api/sockets.c" \
"../LWIP_GETH/lwip/src/api/tcpip.c" 

COMPILED_SRCS += \
"LWIP_GETH/lwip/src/api/api_lib.src" \
"LWIP_GETH/lwip/src/api/api_msg.src" \
"LWIP_GETH/lwip/src/api/err.src" \
"LWIP_GETH/lwip/src/api/if_api.src" \
"LWIP_GETH/lwip/src/api/netbuf.src" \
"LWIP_GETH/lwip/src/api/netdb.src" \
"LWIP_GETH/lwip/src/api/netifapi.src" \
"LWIP_GETH/lwip/src/api/sockets.src" \
"LWIP_GETH/lwip/src/api/tcpip.src" 

C_DEPS += \
"./LWIP_GETH/lwip/src/api/api_lib.d" \
"./LWIP_GETH/lwip/src/api/api_msg.d" \
"./LWIP_GETH/lwip/src/api/err.d" \
"./LWIP_GETH/lwip/src/api/if_api.d" \
"./LWIP_GETH/lwip/src/api/netbuf.d" \
"./LWIP_GETH/lwip/src/api/netdb.d" \
"./LWIP_GETH/lwip/src/api/netifapi.d" \
"./LWIP_GETH/lwip/src/api/sockets.d" \
"./LWIP_GETH/lwip/src/api/tcpip.d" 

OBJS += \
"LWIP_GETH/lwip/src/api/api_lib.o" \
"LWIP_GETH/lwip/src/api/api_msg.o" \
"LWIP_GETH/lwip/src/api/err.o" \
"LWIP_GETH/lwip/src/api/if_api.o" \
"LWIP_GETH/lwip/src/api/netbuf.o" \
"LWIP_GETH/lwip/src/api/netdb.o" \
"LWIP_GETH/lwip/src/api/netifapi.o" \
"LWIP_GETH/lwip/src/api/sockets.o" \
"LWIP_GETH/lwip/src/api/tcpip.o" 


# Each subdirectory must supply rules for building sources it contributes
"LWIP_GETH/lwip/src/api/api_lib.src":"../LWIP_GETH/lwip/src/api/api_lib.c" "LWIP_GETH/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/api/api_lib.o":"LWIP_GETH/lwip/src/api/api_lib.src" "LWIP_GETH/lwip/src/api/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/api/api_msg.src":"../LWIP_GETH/lwip/src/api/api_msg.c" "LWIP_GETH/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/api/api_msg.o":"LWIP_GETH/lwip/src/api/api_msg.src" "LWIP_GETH/lwip/src/api/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/api/err.src":"../LWIP_GETH/lwip/src/api/err.c" "LWIP_GETH/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/api/err.o":"LWIP_GETH/lwip/src/api/err.src" "LWIP_GETH/lwip/src/api/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/api/if_api.src":"../LWIP_GETH/lwip/src/api/if_api.c" "LWIP_GETH/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/api/if_api.o":"LWIP_GETH/lwip/src/api/if_api.src" "LWIP_GETH/lwip/src/api/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/api/netbuf.src":"../LWIP_GETH/lwip/src/api/netbuf.c" "LWIP_GETH/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/api/netbuf.o":"LWIP_GETH/lwip/src/api/netbuf.src" "LWIP_GETH/lwip/src/api/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/api/netdb.src":"../LWIP_GETH/lwip/src/api/netdb.c" "LWIP_GETH/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/api/netdb.o":"LWIP_GETH/lwip/src/api/netdb.src" "LWIP_GETH/lwip/src/api/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/api/netifapi.src":"../LWIP_GETH/lwip/src/api/netifapi.c" "LWIP_GETH/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/api/netifapi.o":"LWIP_GETH/lwip/src/api/netifapi.src" "LWIP_GETH/lwip/src/api/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/api/sockets.src":"../LWIP_GETH/lwip/src/api/sockets.c" "LWIP_GETH/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/api/sockets.o":"LWIP_GETH/lwip/src/api/sockets.src" "LWIP_GETH/lwip/src/api/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/api/tcpip.src":"../LWIP_GETH/lwip/src/api/tcpip.c" "LWIP_GETH/lwip/src/api/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/api/tcpip.o":"LWIP_GETH/lwip/src/api/tcpip.src" "LWIP_GETH/lwip/src/api/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-LWIP_GETH-2f-lwip-2f-src-2f-api

clean-LWIP_GETH-2f-lwip-2f-src-2f-api:
	-$(RM) ./LWIP_GETH/lwip/src/api/api_lib.d ./LWIP_GETH/lwip/src/api/api_lib.o ./LWIP_GETH/lwip/src/api/api_lib.src ./LWIP_GETH/lwip/src/api/api_msg.d ./LWIP_GETH/lwip/src/api/api_msg.o ./LWIP_GETH/lwip/src/api/api_msg.src ./LWIP_GETH/lwip/src/api/err.d ./LWIP_GETH/lwip/src/api/err.o ./LWIP_GETH/lwip/src/api/err.src ./LWIP_GETH/lwip/src/api/if_api.d ./LWIP_GETH/lwip/src/api/if_api.o ./LWIP_GETH/lwip/src/api/if_api.src ./LWIP_GETH/lwip/src/api/netbuf.d ./LWIP_GETH/lwip/src/api/netbuf.o ./LWIP_GETH/lwip/src/api/netbuf.src ./LWIP_GETH/lwip/src/api/netdb.d ./LWIP_GETH/lwip/src/api/netdb.o ./LWIP_GETH/lwip/src/api/netdb.src ./LWIP_GETH/lwip/src/api/netifapi.d ./LWIP_GETH/lwip/src/api/netifapi.o ./LWIP_GETH/lwip/src/api/netifapi.src ./LWIP_GETH/lwip/src/api/sockets.d ./LWIP_GETH/lwip/src/api/sockets.o ./LWIP_GETH/lwip/src/api/sockets.src ./LWIP_GETH/lwip/src/api/tcpip.d ./LWIP_GETH/lwip/src/api/tcpip.o ./LWIP_GETH/lwip/src/api/tcpip.src

.PHONY: clean-LWIP_GETH-2f-lwip-2f-src-2f-api

