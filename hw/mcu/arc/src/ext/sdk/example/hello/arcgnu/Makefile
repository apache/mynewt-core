## Small Functions ##
get_csrcs = $(foreach subdir, $(1), $(wildcard $(subdir)/*.c $(subdir)/*.C))
get_asmsrcs = $(foreach subdir, $(1), $(wildcard $(subdir)/*.s $(subdir)/*.S))
get_cxxsrcs = $(foreach subdir, $(1), $(wildcard $(subdir)/*.cpp $(subdir)/*.CPP))
## MAKEFILE COMPILE MESSAGE CONTROL ##
ECHO=echo
ifeq ($(V),1)
	Q=
else
	Q=@
endif

## Suppress All Message ##
ifeq ($(SILENT), 1)
	TRACE_CREATE_DIR	=
	TRACE_COMPILE		=
	TRACE_ASSEMBLE		=
	TRACE_LINK		=
	TRACE_ARCHIVE		=
	TRACE_GEN_LINKFILE	=
	## Overwrite Q Value set by V option ##
	override Q=@
else
	TRACE_CREATE_DIR	= @$(ECHO) "Creating Directory    : " $(@D)
	TRACE_COMPILE		= @$(ECHO) "Compiling             : " $<
	TRACE_ASSEMBLE		= @$(ECHO) "Assembling            : " $<
	TRACE_LINK		= @$(ECHO) "Linking               : " $@
	TRACE_ARCHIVE		= @$(ECHO) "Archiving             : " $@
	TRACE_GEN_LINKFILE	= @$(ECHO) "Generating Linkfile   : " $@
endif
###

APPL_FULL_NAME = hello
###
# Compiler Settings
###
CC=~/dev/arc_gnu_2017.09-rc2_ide_linux_install/bin/arc-elf32-gcc
OBJCOPY=arc-elf32-objcopy
DUMP=arc-elf32-objdump
AR=arc-elf32-ar
SIZE=arc-elf32-size
DBG=arc-elf32-gdb

###
# Configurations need to be changed according to your environment
###
DEBUG = 1
EMBARC_BSP_ROOT = ../../..
ARC_CORE_CONFIG_DIR = .
OPENOCD_SCRIPT_ROOT = ~/dev/arc_gnu_2017.09-rc2_ide_linux_install/share/openocd/scripts

COMPILE_OPT_ARGFILE = $(ARC_CORE_CONFIG_DIR)/gcc.arg
APP_LINK_FILE = $(ARC_CORE_CONFIG_DIR)/arc_core.ld
OPENOCD_CFG_FILE = $(OPENOCD_SCRIPT_ROOT)/board/snps_em_sk_v2.2.cfg
OPENOCD_OPTIONS  = -s $(OPENOCD_SCRIPT_ROOT) -f $(OPENOCD_CFG_FILE)

###
# embARC EMSK BSP source directories configuration
###
EMBARC_EMSK_BSP_ROOT = $(EMBARC_BSP_ROOT)/board/emsk

EMBARC_EMSK_BSP_CSRC_DIRS = $(EMBARC_EMSK_BSP_ROOT)/common \
				$(EMBARC_EMSK_BSP_ROOT)/gpio \
				$(EMBARC_EMSK_BSP_ROOT)/iic \
				$(EMBARC_EMSK_BSP_ROOT)/spi \
				$(EMBARC_EMSK_BSP_ROOT)/uart

EMBARC_BSP_CSRC_DIRS = $(EMBARC_BSP_ROOT)/arc/startup $(EMBARC_BSP_ROOT)/arc \
			$(EMBARC_BSP_ROOT)/board $(EMBARC_EMSK_BSP_CSRC_DIRS) \
			$(EMBARC_BSP_ROOT)/device/designware/gpio \
			$(EMBARC_BSP_ROOT)/device/designware/iic \
			$(EMBARC_BSP_ROOT)/device/designware/spi \
			$(EMBARC_BSP_ROOT)/device/designware/uart \
			$(EMBARC_BSP_ROOT)/common

EMBARC_BSP_ASMSRC_DIRS = $(EMBARC_BSP_ROOT)/arc/startup $(EMBARC_BSP_ROOT)/arc
EMBARC_BSP_INC_DIRS = $(EMBARC_BSP_ROOT)

###
# embARC EMSK BSP c/asm source files configuration
##
EMBARC_EMSK_BSP_CSRCS = $(call get_csrcs, $(EMBARC_BSP_CSRC_DIRS))
EMBARC_EMSK_BSP_ASMSRCS = $(call get_asmsrcs, $(EMBARC_BSP_ASMSRC_DIRS))

###
# C/ASM source files
# Defined your own extra c/asm source code files, and include folders
###
EXTRA_CSRCS =

EXTRA_ASMSRCS =

EXTRA_INC_DIRS =

###
# All C/ASM source files
###
ALL_C_SRCS = ../main.c \
		$(EXTRA_CSRCS) \
		$(EMBARC_EMSK_BSP_CSRCS)

ALL_ASM_SRCS = $(EXTRA_ASMSRCS) $(EMBARC_EMSK_BSP_ASMSRCS)
ALL_INC_DIRS = . $(ARC_CORE_CONFIG_DIR) $(EMBARC_BSP_INC_DIRS) \
		$(EXTRA_INC_DIRS)

###
# Compiler Options
###

ifeq ($(DEBUG), 1)
	COMMON_COMPILE_OPT += -g3 -gdwarf-2
endif

ALL_INCLUDES = $(foreach dir,$(ALL_INC_DIRS),-I$(dir))
ALL_DEFINES = -DMID_COMMON

LMAP_OPTION = -Wl,-M,-Map=$(APPL_FULL_NAME).map
MKDEP_OPT = -MMD -MT $@ -MF $@.d
COMMON_COMPILE_OPT += -O2 -mno-sdata $(ALL_INCLUDES) $(MKDEP_OPT) $(ALL_DEFINES)

CFLAGS = @$(COMPILE_OPT_ARGFILE) $(COMMON_COMPILE_OPT) -std=gnu99
CXXFLAGS = @$(COMPILE_OPT_ARGFILE) $(COMMON_COMPILE_OPT)
ASFLAGS = @$(COMPILE_OPT_ARGFILE) $(COMMON_COMPILE_OPT) -x assembler-with-cpp
LDFLAGS = @$(COMPILE_OPT_ARGFILE) -mno-sdata -nostartfiles \
		$(LMAP_OPTION) -Wl,--script=$(APP_LINK_FILE)
LDFLAGS_LIBS = -Wl,--start-group -lm -lc -lgcc -Wl,--end-group

ifeq ($(DBG), arc-elf32-gdb)
DBG_HW_FLAGS = -ex "target remote | openocd --pipe $(OPENOCD_OPTIONS)" -ex "load"
else
DBG_HW_FLAGS = -nooptions -nogoifmain -toggle=include_local_symbols=1 -digilent
endif

## Run target options
ifeq ($(MAKECMDGOALS),run)
ifeq ($(DBG), mdb)
DBG_HW_FLAGS += -run
endif
ifeq ($(DBG), arc-elf32-gdb)
DBG_HW_FLAGS += -ex "c"
endif
endif
#
# Define all object files.
#
COBJS = $(addsuffix .o, $(ALL_C_SRCS))
CXXOBJS = $(addsuffix .o, $(ALL_CXX_SRCS))
ASMOBJS = $(addsuffix .o, $(ALL_ASM_SRCS))
ALLOBJS = $(ASMOBJS) $(COBJS) $(CXXOBJS)
ALLDEPS = $(ALLOBJS:.o=.o.d)

.PHONY : all bin hex size clean run gui

all : $(APPL_FULL_NAME).elf

bin : $(APPL_FULL_NAME).bin

hex : $(APPL_FULL_NAME).hex

size : $(APPL_FULL_NAME).elf
	@$(ECHO) "Print Application Program Size"
	$(Q)$(SIZE) $(SIZE_OPT) $<

$(APPL_FULL_NAME).hex : $(APPL_FULL_NAME).elf
	@$(ECHO) "Generating Intel Hex File $@"
	$(Q)$(OBJCOPY) -O ihex $< $@

$(APPL_FULL_NAME).bin : $(APPL_FULL_NAME).elf
	@$(ECHO) "Generating Binary $@"
	$(Q)$(OBJCOPY) -O binary $< $@

$(APPL_FULL_NAME).elf : $(ALLOBJS) Makefile
	$(TRACE_LINK)
	$(Q)$(CC) $(LDFLAGS) $(ALLOBJS) $(LDFLAGS_LIBS) -o $@

$(COBJS) : %.o : %  Makefile
	$(TRACE_COMPILE)
	$(Q)$(CC) -c $(CFLAGS) $< -o $@

$(CXXOBJS) : %.o : %  Makefile
	$(TRACE_COMPILE)
	$(Q)$(CC) -c $(CXXFLAGS) $< -o $@

$(ASMOBJS) : %.o : %  Makefile
	$(TRACE_ASSEMBLE)
	$(Q)$(CC) -c $(ASFLAGS) $< -o $@

run : $(APPL_FULL_NAME).elf
	@$(ECHO) "Download & Run $<"
	~/dev/arc_gnu_2017.09-rc2_ide_linux_install/bin/$(DBG) $(DBG_HW_FLAGS) $<

gui : $(APPL_FULL_NAME).elf
	@$(ECHO) "Download & Debug $<"
	$(DBG) $(DBG_HW_FLAGS) $<

clean :
	@$(ECHO) "Clean all object files and dependency files"
	-$(Q)rm -rf $(ALLOBJS)
	-$(Q)rm -rf $(ALLDEPS)
	-$(Q)rm -rf $(APPL_FULL_NAME).elf $(APPL_FULL_NAME).map
	-$(Q)rm -rf openocd.log .sc.project *.log
	@$(ECHO) "Clean finished"

-include $(ALLDEPS)
