################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../LWIP_GETH/lwip/src/core/altcp.c" \
"../LWIP_GETH/lwip/src/core/altcp_alloc.c" \
"../LWIP_GETH/lwip/src/core/altcp_tcp.c" \
"../LWIP_GETH/lwip/src/core/def.c" \
"../LWIP_GETH/lwip/src/core/dns.c" \
"../LWIP_GETH/lwip/src/core/inet_chksum.c" \
"../LWIP_GETH/lwip/src/core/init.c" \
"../LWIP_GETH/lwip/src/core/ip.c" \
"../LWIP_GETH/lwip/src/core/mem.c" \
"../LWIP_GETH/lwip/src/core/memp.c" \
"../LWIP_GETH/lwip/src/core/netif.c" \
"../LWIP_GETH/lwip/src/core/pbuf.c" \
"../LWIP_GETH/lwip/src/core/raw.c" \
"../LWIP_GETH/lwip/src/core/stats.c" \
"../LWIP_GETH/lwip/src/core/sys.c" \
"../LWIP_GETH/lwip/src/core/tcp.c" \
"../LWIP_GETH/lwip/src/core/tcp_in.c" \
"../LWIP_GETH/lwip/src/core/tcp_out.c" \
"../LWIP_GETH/lwip/src/core/timeouts.c" \
"../LWIP_GETH/lwip/src/core/udp.c" 

COMPILED_SRCS += \
"LWIP_GETH/lwip/src/core/altcp.src" \
"LWIP_GETH/lwip/src/core/altcp_alloc.src" \
"LWIP_GETH/lwip/src/core/altcp_tcp.src" \
"LWIP_GETH/lwip/src/core/def.src" \
"LWIP_GETH/lwip/src/core/dns.src" \
"LWIP_GETH/lwip/src/core/inet_chksum.src" \
"LWIP_GETH/lwip/src/core/init.src" \
"LWIP_GETH/lwip/src/core/ip.src" \
"LWIP_GETH/lwip/src/core/mem.src" \
"LWIP_GETH/lwip/src/core/memp.src" \
"LWIP_GETH/lwip/src/core/netif.src" \
"LWIP_GETH/lwip/src/core/pbuf.src" \
"LWIP_GETH/lwip/src/core/raw.src" \
"LWIP_GETH/lwip/src/core/stats.src" \
"LWIP_GETH/lwip/src/core/sys.src" \
"LWIP_GETH/lwip/src/core/tcp.src" \
"LWIP_GETH/lwip/src/core/tcp_in.src" \
"LWIP_GETH/lwip/src/core/tcp_out.src" \
"LWIP_GETH/lwip/src/core/timeouts.src" \
"LWIP_GETH/lwip/src/core/udp.src" 

C_DEPS += \
"./LWIP_GETH/lwip/src/core/altcp.d" \
"./LWIP_GETH/lwip/src/core/altcp_alloc.d" \
"./LWIP_GETH/lwip/src/core/altcp_tcp.d" \
"./LWIP_GETH/lwip/src/core/def.d" \
"./LWIP_GETH/lwip/src/core/dns.d" \
"./LWIP_GETH/lwip/src/core/inet_chksum.d" \
"./LWIP_GETH/lwip/src/core/init.d" \
"./LWIP_GETH/lwip/src/core/ip.d" \
"./LWIP_GETH/lwip/src/core/mem.d" \
"./LWIP_GETH/lwip/src/core/memp.d" \
"./LWIP_GETH/lwip/src/core/netif.d" \
"./LWIP_GETH/lwip/src/core/pbuf.d" \
"./LWIP_GETH/lwip/src/core/raw.d" \
"./LWIP_GETH/lwip/src/core/stats.d" \
"./LWIP_GETH/lwip/src/core/sys.d" \
"./LWIP_GETH/lwip/src/core/tcp.d" \
"./LWIP_GETH/lwip/src/core/tcp_in.d" \
"./LWIP_GETH/lwip/src/core/tcp_out.d" \
"./LWIP_GETH/lwip/src/core/timeouts.d" \
"./LWIP_GETH/lwip/src/core/udp.d" 

OBJS += \
"LWIP_GETH/lwip/src/core/altcp.o" \
"LWIP_GETH/lwip/src/core/altcp_alloc.o" \
"LWIP_GETH/lwip/src/core/altcp_tcp.o" \
"LWIP_GETH/lwip/src/core/def.o" \
"LWIP_GETH/lwip/src/core/dns.o" \
"LWIP_GETH/lwip/src/core/inet_chksum.o" \
"LWIP_GETH/lwip/src/core/init.o" \
"LWIP_GETH/lwip/src/core/ip.o" \
"LWIP_GETH/lwip/src/core/mem.o" \
"LWIP_GETH/lwip/src/core/memp.o" \
"LWIP_GETH/lwip/src/core/netif.o" \
"LWIP_GETH/lwip/src/core/pbuf.o" \
"LWIP_GETH/lwip/src/core/raw.o" \
"LWIP_GETH/lwip/src/core/stats.o" \
"LWIP_GETH/lwip/src/core/sys.o" \
"LWIP_GETH/lwip/src/core/tcp.o" \
"LWIP_GETH/lwip/src/core/tcp_in.o" \
"LWIP_GETH/lwip/src/core/tcp_out.o" \
"LWIP_GETH/lwip/src/core/timeouts.o" \
"LWIP_GETH/lwip/src/core/udp.o" 


# Each subdirectory must supply rules for building sources it contributes
"LWIP_GETH/lwip/src/core/altcp.src":"../LWIP_GETH/lwip/src/core/altcp.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/altcp.o":"LWIP_GETH/lwip/src/core/altcp.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/altcp_alloc.src":"../LWIP_GETH/lwip/src/core/altcp_alloc.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/altcp_alloc.o":"LWIP_GETH/lwip/src/core/altcp_alloc.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/altcp_tcp.src":"../LWIP_GETH/lwip/src/core/altcp_tcp.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/altcp_tcp.o":"LWIP_GETH/lwip/src/core/altcp_tcp.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/def.src":"../LWIP_GETH/lwip/src/core/def.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/def.o":"LWIP_GETH/lwip/src/core/def.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/dns.src":"../LWIP_GETH/lwip/src/core/dns.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/dns.o":"LWIP_GETH/lwip/src/core/dns.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/inet_chksum.src":"../LWIP_GETH/lwip/src/core/inet_chksum.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/inet_chksum.o":"LWIP_GETH/lwip/src/core/inet_chksum.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/init.src":"../LWIP_GETH/lwip/src/core/init.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/init.o":"LWIP_GETH/lwip/src/core/init.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/ip.src":"../LWIP_GETH/lwip/src/core/ip.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/ip.o":"LWIP_GETH/lwip/src/core/ip.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/mem.src":"../LWIP_GETH/lwip/src/core/mem.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/mem.o":"LWIP_GETH/lwip/src/core/mem.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/memp.src":"../LWIP_GETH/lwip/src/core/memp.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/memp.o":"LWIP_GETH/lwip/src/core/memp.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/netif.src":"../LWIP_GETH/lwip/src/core/netif.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/netif.o":"LWIP_GETH/lwip/src/core/netif.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/pbuf.src":"../LWIP_GETH/lwip/src/core/pbuf.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/pbuf.o":"LWIP_GETH/lwip/src/core/pbuf.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/raw.src":"../LWIP_GETH/lwip/src/core/raw.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/raw.o":"LWIP_GETH/lwip/src/core/raw.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/stats.src":"../LWIP_GETH/lwip/src/core/stats.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/stats.o":"LWIP_GETH/lwip/src/core/stats.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/sys.src":"../LWIP_GETH/lwip/src/core/sys.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/sys.o":"LWIP_GETH/lwip/src/core/sys.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/tcp.src":"../LWIP_GETH/lwip/src/core/tcp.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/tcp.o":"LWIP_GETH/lwip/src/core/tcp.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/tcp_in.src":"../LWIP_GETH/lwip/src/core/tcp_in.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/tcp_in.o":"LWIP_GETH/lwip/src/core/tcp_in.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/tcp_out.src":"../LWIP_GETH/lwip/src/core/tcp_out.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/tcp_out.o":"LWIP_GETH/lwip/src/core/tcp_out.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/timeouts.src":"../LWIP_GETH/lwip/src/core/timeouts.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/timeouts.o":"LWIP_GETH/lwip/src/core/timeouts.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"LWIP_GETH/lwip/src/core/udp.src":"../LWIP_GETH/lwip/src/core/udp.c" "LWIP_GETH/lwip/src/core/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Users/Daniel/Desktop/ZGW_Repo/ZGW/ZGW_FBL/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Wc-w508 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"LWIP_GETH/lwip/src/core/udp.o":"LWIP_GETH/lwip/src/core/udp.src" "LWIP_GETH/lwip/src/core/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-LWIP_GETH-2f-lwip-2f-src-2f-core

clean-LWIP_GETH-2f-lwip-2f-src-2f-core:
	-$(RM) ./LWIP_GETH/lwip/src/core/altcp.d ./LWIP_GETH/lwip/src/core/altcp.o ./LWIP_GETH/lwip/src/core/altcp.src ./LWIP_GETH/lwip/src/core/altcp_alloc.d ./LWIP_GETH/lwip/src/core/altcp_alloc.o ./LWIP_GETH/lwip/src/core/altcp_alloc.src ./LWIP_GETH/lwip/src/core/altcp_tcp.d ./LWIP_GETH/lwip/src/core/altcp_tcp.o ./LWIP_GETH/lwip/src/core/altcp_tcp.src ./LWIP_GETH/lwip/src/core/def.d ./LWIP_GETH/lwip/src/core/def.o ./LWIP_GETH/lwip/src/core/def.src ./LWIP_GETH/lwip/src/core/dns.d ./LWIP_GETH/lwip/src/core/dns.o ./LWIP_GETH/lwip/src/core/dns.src ./LWIP_GETH/lwip/src/core/inet_chksum.d ./LWIP_GETH/lwip/src/core/inet_chksum.o ./LWIP_GETH/lwip/src/core/inet_chksum.src ./LWIP_GETH/lwip/src/core/init.d ./LWIP_GETH/lwip/src/core/init.o ./LWIP_GETH/lwip/src/core/init.src ./LWIP_GETH/lwip/src/core/ip.d ./LWIP_GETH/lwip/src/core/ip.o ./LWIP_GETH/lwip/src/core/ip.src ./LWIP_GETH/lwip/src/core/mem.d ./LWIP_GETH/lwip/src/core/mem.o ./LWIP_GETH/lwip/src/core/mem.src ./LWIP_GETH/lwip/src/core/memp.d ./LWIP_GETH/lwip/src/core/memp.o ./LWIP_GETH/lwip/src/core/memp.src ./LWIP_GETH/lwip/src/core/netif.d ./LWIP_GETH/lwip/src/core/netif.o ./LWIP_GETH/lwip/src/core/netif.src ./LWIP_GETH/lwip/src/core/pbuf.d ./LWIP_GETH/lwip/src/core/pbuf.o ./LWIP_GETH/lwip/src/core/pbuf.src ./LWIP_GETH/lwip/src/core/raw.d ./LWIP_GETH/lwip/src/core/raw.o ./LWIP_GETH/lwip/src/core/raw.src ./LWIP_GETH/lwip/src/core/stats.d ./LWIP_GETH/lwip/src/core/stats.o ./LWIP_GETH/lwip/src/core/stats.src ./LWIP_GETH/lwip/src/core/sys.d ./LWIP_GETH/lwip/src/core/sys.o ./LWIP_GETH/lwip/src/core/sys.src ./LWIP_GETH/lwip/src/core/tcp.d ./LWIP_GETH/lwip/src/core/tcp.o ./LWIP_GETH/lwip/src/core/tcp.src ./LWIP_GETH/lwip/src/core/tcp_in.d ./LWIP_GETH/lwip/src/core/tcp_in.o ./LWIP_GETH/lwip/src/core/tcp_in.src ./LWIP_GETH/lwip/src/core/tcp_out.d ./LWIP_GETH/lwip/src/core/tcp_out.o ./LWIP_GETH/lwip/src/core/tcp_out.src ./LWIP_GETH/lwip/src/core/timeouts.d ./LWIP_GETH/lwip/src/core/timeouts.o ./LWIP_GETH/lwip/src/core/timeouts.src ./LWIP_GETH/lwip/src/core/udp.d ./LWIP_GETH/lwip/src/core/udp.o ./LWIP_GETH/lwip/src/core/udp.src

.PHONY: clean-LWIP_GETH-2f-lwip-2f-src-2f-core

