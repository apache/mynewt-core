#******************************************************************************
#
# Makefile - Rules for building the libraries, examples and docs.
#
# Copyright (c) 2017, Ambiq Micro
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
# This is part of revision v1.2.10-2-gea660ad-hotfix2 of the AmbiqSuite Development Package.
#
#******************************************************************************
TARGET = libam_hal

#### Setup ####
TOOLCHAIN = arm-atollic-eabi
PART = apollo2
CPU = cortex-m4
FPU = fpv4-sp-d16
FABI = softfp
WITHCC=w/GCC

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

EXECUTABLES = CC LD CP OD AR RD SIZE GCC
K := $(foreach exec,$(EXECUTABLES),\
        $(if $(shell which $($(exec)) 2>/dev/null),,\
        $(info $(exec) not found on PATH ($($(exec))).)$(exec)))
$(if $(strip $(value K)),$(info Required Program(s) $(strip $(value K)) not found))

ifneq ($(strip $(value K)),)
all clean:
	$(info Tools $(WITHCC) not installed.)
else
DEFINES = -DPART_$(PART)
DEFINES+= -Dgcc
DEFINES+= -DAM_DEBUG_ASSERT
DEFINES += -DAM_ASSERT_INVALID_THRESHOLD=0
DEFINES += -DAM_PART_APOLLO2

INCLUDES = -I../../../../mcu/apollo2
INCLUDES += -I../../../../utils

VPATH = ../.

SRC = am_hal_adc.c
SRC += am_hal_cachectrl.c
SRC += am_hal_clkgen.c
SRC += am_hal_ctimer.c
SRC += am_hal_debug.c
SRC += am_hal_flash.c
SRC += am_hal_global.c
SRC += am_hal_gpio.c
SRC += am_hal_i2c_bit_bang.c
SRC += am_hal_interrupt.c
SRC += am_hal_iom.c
SRC += am_hal_ios.c
SRC += am_hal_itm.c
SRC += am_hal_mcuctrl.c
SRC += am_hal_otp.c
SRC += am_hal_pdm.c
SRC += am_hal_pwrctrl.c
SRC += am_hal_queue.c
SRC += am_hal_reset.c
SRC += am_hal_rtc.c
SRC += am_hal_stimer.c
SRC += am_hal_sysctrl.c
SRC += am_hal_systick.c
SRC += am_hal_tpiu.c
SRC += am_hal_ttp.c
SRC += am_hal_uart.c
SRC += am_hal_vcomp.c
SRC += am_hal_wdt.c

CSRC = $(filter %.c,$(SRC))
ASRC = $(filter %.s,$(SRC))

OBJS = $(CSRC:%.c=bin/%.o)
OBJS+= $(ASRC:%.s=bin/%.o)

DEPS = $(CSRC:%.c=bin/%.d)
DEPS+= $(ASRC:%.s=bin/%.d)

LIBS = 

CFLAGS = -mthumb -mcpu=$(CPU) -mfpu=$(FPU) -mfloat-abi=$(FABI)
CFLAGS+= -ffunction-sections -fdata-sections
CFLAGS+= -MMD -MP -std=c99 -Wall
# Libraries O3 for production, examples O0 for debug.
CFLAGS+= -O3 -funroll-loops
CFLAGS+= $(DEFINES)
CFLAGS+= $(INCLUDES)
CFLAGS+= 

ODFLAGS = -S

#### Rules ####
.PHONY: all clean
all: bin bin/$(TARGET).a

bin:
	@mkdir -p $@

bin/%.o: %.c bin/%.d
	@+echo " Compiling $(WITHCC) $<" ;\
	$(CC) -c $(CFLAGS) $< -o $@

bin/%.o: %.s bin/%.d
	@+echo " Assembling $(WITHCC) $<" ;\
	$(CC) -c $(CFLAGS) $< -o $@

bin/$(TARGET).a: $(OBJS)
	@+echo " Library $(WITHCC) $@" ;\
	$(AR) rsvc $@ $(OBJS)

clean:
	@+echo "Cleaning..." ;\
	rm -f $(OBJS) $(DEPS) bin/$(TARGET).a

bin/%.d: ;

# Automatically include any generated dependencies
-include $(DEPS)
endif

