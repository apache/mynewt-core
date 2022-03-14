#******************************************************************************
#
# Makefile - Rules for building the libraries, examples and docs.
#
# Copyright (c) 2021, Ambiq Micro, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from this
# software without specific prior written permission.
#
# Third party software included in this distribution is subject to the
# additional license terms as defined in the /docs/licenses directory.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# This is part of revision release_sdk_3_0_0-742e5ac27c of the AmbiqSuite Development Package.
#
#******************************************************************************
TARGET := libam_hal
COMPILERNAME := gcc
PROJECT := libam_hal_gcc
CONFIG := bin

VERBOSE ?= 0

SHELL:=/bin/bash

# Enable printing explicit commands with 'make VERBOSE=1'
ifneq ($(VERBOSE),1)
Q:=@
endif

#### Setup ####

TOOLCHAIN ?= arm-none-eabi
PART = apollo3
CPU = cortex-m4
FPU = fpv4-sp-d16
# Default to FPU hardware calling convention.  However, some customers and/or
# applications may need the software calling convention.
#FABI = softfp
FABI = hard

#### Required Executables ####
CC = $(TOOLCHAIN)-gcc
GCC = $(TOOLCHAIN)-gcc
CPP = $(TOOLCHAIN)-cpp
LD = $(TOOLCHAIN)-ld
CP = $(TOOLCHAIN)-objcopy
OD = $(TOOLCHAIN)-objdump
RD = $(TOOLCHAIN)-readelf
AR = $(TOOLCHAIN)-ar
SIZE = $(TOOLCHAIN)-size
RM = $(shell which rm 2>/dev/null)

EXECUTABLES = CC LD CP OD AR RD SIZE GCC
K := $(foreach exec,$(EXECUTABLES),\
        $(if $(shell which $($(exec)) 2>/dev/null),,\
        $(info $(exec) not found on PATH ($($(exec))).)$(exec)))
$(if $(strip $(value K)),$(info Required Program(s) $(strip $(value K)) not found))

ifneq ($(strip $(value K)),)
all clean:
	$(info Tools $(TOOLCHAIN)-$(COMPILERNAME) not installed.)
	$(RM) -rf bin
else

DEFINES = -DAM_PART_APOLLO3
DEFINES+= -DAM_ASSERT_INVALID_THRESHOLD=0
DEFINES+= -DAM_DEBUG_ASSERT
DEFINES+= -Dgcc

INCLUDES = -I../../../../mcu/apollo3
INCLUDES+= -I../../../../CMSIS/ARM/Include
INCLUDES+= -I../../../../CMSIS/AmbiqMicro/Include

VPATH = ..

SRC = am_hal_adc.c
SRC += am_hal_ble.c
SRC += am_hal_ble_patch.c
SRC += am_hal_ble_patch_b0.c
SRC += am_hal_burst.c
SRC += am_hal_cachectrl.c
SRC += am_hal_clkgen.c
SRC += am_hal_cmdq.c
SRC += am_hal_ctimer.c
SRC += am_hal_debug.c
SRC += am_hal_entropy.c
SRC += am_hal_flash.c
SRC += am_hal_global.c
SRC += am_hal_gpio.c
SRC += am_hal_interrupt.c
SRC += am_hal_iom.c
SRC += am_hal_ios.c
SRC += am_hal_itm.c
SRC += am_hal_mcuctrl.c
SRC += am_hal_mspi.c
SRC += am_hal_pdm.c
SRC += am_hal_pwrctrl.c
SRC += am_hal_queue.c
SRC += am_hal_reset.c
SRC += am_hal_rtc.c
SRC += am_hal_scard.c
SRC += am_hal_secure_ota.c
SRC += am_hal_security.c
SRC += am_hal_stimer.c
SRC += am_hal_sysctrl.c
SRC += am_hal_systick.c
SRC += am_hal_tpiu.c
SRC += am_hal_uart.c
SRC += am_hal_wdt.c

CSRC = $(filter %.c,$(SRC))
ASRC = $(filter %.s,$(SRC))

OBJS = $(CSRC:%.c=$(CONFIG)/%.o)
OBJS+= $(ASRC:%.s=$(CONFIG)/%.o)

DEPS = $(CSRC:%.c=$(CONFIG)/%.d)
DEPS+= $(ASRC:%.s=$(CONFIG)/%.d)

CFLAGS = -mthumb -mcpu=$(CPU) -mfpu=$(FPU) -mfloat-abi=$(FABI)
CFLAGS+= -ffunction-sections -fdata-sections
CFLAGS+= -MMD -MP -std=c99 -Wall
# Libraries O3 for production, examples O0 for debug.
CFLAGS+= -O3
CFLAGS+= $(DEFINES)
CFLAGS+= $(INCLUDES)
CFLAGS+= 

# Additional user specified CFLAGS
CFLAGS+=$(EXTRA_CFLAGS)

ODFLAGS = -S

#### Rules ####
all: directories $(CONFIG)/$(TARGET).a

directories: $(CONFIG)

$(CONFIG):
	@mkdir -p $@

$(CONFIG)/%.o: %.c $(CONFIG)/%.d $(INCS)
	@echo " Compiling $(COMPILERNAME) $<"
	$(Q) $(CC) -c $(CFLAGS) $< -o $@

$(CONFIG)/%.o: %.s $(CONFIG)/%.d $(INCS)
	@echo " Assembling $(COMPILERNAME) $<"
	$(Q) $(CC) -c $(CFLAGS) $< -o $@

$(CONFIG)/$(TARGET).a: $(OBJS)
	@echo " Library $(COMPILERNAME) $@"
	$(Q) $(AR) rsvc $@ $(OBJS)

clean:
	@echo "Cleaning..."
	$(Q) $(RM) -f $(OBJS) $(DEPS) \
	    $(CONFIG)/$(TARGET).a

$(CONFIG)/%.d: ;

# Automatically include any generated dependencies
-include $(DEPS)
endif
.PHONY: all clean directories
