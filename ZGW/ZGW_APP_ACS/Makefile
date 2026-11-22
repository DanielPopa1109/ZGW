################################################################################
# ZGW_APP built with the TASKING TriCore toolset, driven from AURIX Configuration
# Studio (ACS). ACS 1.0.24 has no TASKING toolchain integration, so this makefile
# calls the TASKING tools directly and reproduces the "TriCore Debug (TASKING)"
# configuration that AURIX Development Studio (ADS) generated for ZGW_APP:
#   - per-source cctc/astc options  -> tasking_sources.mk (tools/gen_tasking_manifest.py)
#   - include paths                 -> tasking_includes.opt
#   - link step, LSL, hex, UCB filter, elfsize as in ZGW_APP/TriCore Debug (TASKING)/makefile
# Sources are read from ../ZGW_APP; nothing is copied. Run make in this directory.
#
# Not reproduced: ADS "dependents" step that builds the SCR (SDCC) sub-project.
# ZGW_APP/SCR_AURIX_TC3x.c is compiled as a normal source; rebuild SCR in ADS if it changes.
################################################################################

# TASKING compiler shipped with ADS (the non-commercial toolset ACS does not include).
TASKING_CTC ?= C:/Infineon/AURIX-Studio-1.10.40/tools/Compilers/Tasking_1.1r8/ctc
export PATH := $(TASKING_CTC)/bin;$(PATH)

SRC_ROOT   := ../ZGW_APP
BUILD      := Build/TASKING_Debug
NAME       := ZGW_APP
LSL        := $(SRC_ROOT)/Lcf_Tasking_Tricore_Tc.lsl
INC_OPT    := tasking_includes.opt

UCB_FILTER_SCRIPT := ../../Scripts/Build/filter_ihex_ranges.ps1
UCB_FILTER_RANGE  := 0xAF400000-0xAF405FFF

RM := rm -rf

include tasking_sources.mk

OBJS := $(addprefix $(BUILD)/,$(addsuffix .o,$(basename $(SRCS))))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean
all: $(BUILD)/$(NAME)_edited.hex $(BUILD)/$(NAME).siz

# C -> TASKING assembly. $* is the source path without extension (key of CC_VAR_<path>).
$(BUILD)/%.src: $(SRC_ROOT)/%.c $(INC_OPT) tasking_sources.mk
	@echo 'Compiling $<'
	@mkdir -p $(dir $@)
	cctc $($(CC_VAR_$*)) --dep-file="$(BUILD)/$*.d" "-f$(INC_OPT)" -o "$@" "$<"

# TASKING assembly -> object.
$(BUILD)/%.o: $(BUILD)/%.src tasking_sources.mk
	astc $($(AS_VAR_$*)) -o "$@" "$<"

# Link with the TASKING linker via the cctc control program.
$(BUILD)/$(NAME).elf: $(OBJS) $(LSL) Makefile
	@echo 'Linking $@'
	$(file >$(BUILD)/$(NAME).elf.opt,--lsl-file="$(LSL)" -Wl-OC -Wl-OL -Wl-OT -Wl-OX -Wl-OY -Wl--map-file="$(BUILD)/$(NAME).map" -Wl-mc -Wl-mf -Wl-mi -Wl-mk -Wl-ml -Wl-mm -Wl-md -Wl-mn -Wl-mo -Wl-mq -Wl-mr -Wl-ms -Wl-mu --no-warnings= -Wl--error-limit=42 --fp-model=3 -lrt --lsl-core=vtc --exceptions --strict --anachronisms --force-c++ -Ctc37x -o"$@" -Wl-o"$(BUILD)/$(NAME).hex:IHEX" $(OBJS))
	cctc -f $(BUILD)/$(NAME).elf.opt

$(BUILD)/$(NAME).siz: $(BUILD)/$(NAME).elf
	elfsize "$<"

$(BUILD)/$(NAME)_edited.hex: $(BUILD)/$(NAME).elf $(UCB_FILTER_SCRIPT)
	@echo 'Filtering UCB range from $(NAME).hex -> $(NAME)_edited.hex'
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(UCB_FILTER_SCRIPT)" -InputPath "$(BUILD)/$(NAME).hex" -OutputPath "$@" -ExcludeRange "$(UCB_FILTER_RANGE)"

clean:
	-$(RM) $(BUILD)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif
