# Copyright (c) 2012, Mauro Scomparin
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Mauro Scomparin nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY Mauro Scomparin ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL Mauro Scomparin BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# File:			Makefile.
# Author:		Mauro Scomparin <http://scompoprojects.worpress.com>.
# Version:		1.0.0.
# Description:	Sample makefile.

#==============================================================================
#           Cross compiling toolchain / tools specifications
#==============================================================================

# Prefix for the arm-eabi-none toolchain.
# I'm using codesourcery g++ lite compilers available here:
# http://www.mentor.com/embedded-software/sourcery-tools/sourcery-codebench/editions/lite-edition/
PREFIX_ARM = arm-none-eabi

# Microcontroller properties.
PART=LM4F120H5QR
CPU=-mcpu=cortex-m4
FPU=-mfpu=fpv4-sp-d16 -mfloat-abi=softfp

# Stellarisware path
STELLARISWARE_PATH=~/opt/stellaris-launchpad/
DIR_LWIP=$(PWD)/lwip/src

# Program name definition for ARM GNU C compiler.
CC      = ${PREFIX_ARM}-gcc
CXX	= ${PREFIX_ARM}-g++
# Program name definition for ARM GNU Linker.
LD      = ${PREFIX_ARM}-ld
# Program name definition for ARM GNU Object copy.
CP      = ${PREFIX_ARM}-objcopy
# Program name definition for ARM GNU Object dump.
OD      = ${PREFIX_ARM}-objdump

# Option arguments for C compiler.
CFLAGS=-mthumb ${CPU} ${FPU} -Os -ffunction-sections -fdata-sections -MD -std=gnu99 -Wall  -c -g -DUART_BUFFERED
# Library stuff passed as flags!
CFLAGS+= -I ${STELLARISWARE_PATH} -DPART_$(PART) -c -DTARGET_IS_BLIZZARD_RA1
CFLAGS+= -I$(DIR_LWIP)/include -I$(DIR_LWIP)/include/ipv4 -I$(DIR_LWIP)/include/ipv6 -I.
CFLAGS+= -Imcu++-lib/include

CXXFLAGS = $(CFLAGS) -fno-rtti -fno-exceptions

# Flags for LD
LFLAGS  = -Wl,--gc-sections

# Flags for objcopy
CPFLAGS = -Obinary

# flags for objectdump
ODFLAGS = -S

# I want to save the path to libgcc, libc.a and libm.a for linking.
# I can get them from the gcc frontend, using some options.
# See gcc documentation
LIB_GCC_PATH=${shell ${CC} ${CFLAGS} -print-libgcc-file-name}
LIBC_PATH=${shell ${CC} ${CFLAGS} -print-file-name=libc.a}
LIBM_PATH=${shell ${CC} ${CFLAGS} -print-file-name=libm.a}

# Uploader tool path.
# Set a relative or absolute path to the upload tool program.
# I used this project: https://github.com/utzig/lm4tools
FLASHER=~/opt/stellaris-launchpad/lm4flash
# Flags for the uploader program.
FLASHER_FLAGS=

#==============================================================================
#                         Project properties
#==============================================================================

# Project name (W/O .c extension eg. "main")
PROJECT_NAME = main
# Startup file name (W/O .c extension eg. "LM4F_startup")
STARTUP_FILE = LM4F_startup
# Linker file name
LINKER_FILE = LM4F.ld


SRC_LWIP=$(DIR_LWIP)/core/raw.c \
        $(DIR_LWIP)/core/init.c \
        $(DIR_LWIP)/core/tcp.c \
        $(DIR_LWIP)/core/udp.c \
        $(DIR_LWIP)/core/tcp_in.c \
        $(DIR_LWIP)/core/tcp_out.c \
        $(DIR_LWIP)/core/pbuf.c \
        $(DIR_LWIP)/core/sys.c \
        $(DIR_LWIP)/core/def.c \
        $(DIR_LWIP)/core/mem.c \
        $(DIR_LWIP)/core/memp.c \
        $(DIR_LWIP)/core/ipv4/ip4.c \
        $(DIR_LWIP)/core/ipv4/ip4_addr.c \
        $(DIR_LWIP)/core/ipv4/icmp.c \
        $(DIR_LWIP)/core/ipv4/ip_frag.c \
        $(DIR_LWIP)/core/ipv4/igmp.c \
        $(DIR_LWIP)/core/stats.c \
        $(DIR_LWIP)/core/inet_chksum.c \
        $(DIR_LWIP)/core/netif.c \
        $(DIR_LWIP)/core/timers.c \
        $(DIR_LWIP)/core/dhcp.c \
        $(DIR_LWIP)/netif/etharp.c 

SRC = LM4F_startup.c \
      main.cpp \
      dummyfuncs.c \
      enc28j60.cpp \
      enc28j60_stellaris.cpp \
      httpd.cpp \
      TCPConnection.cpp \
      jenkins-api-client.c \
      parser.c \
      json.c \
      $(wildcard led-matrix-lib/*.cpp) \
      ${STELLARISWARE_PATH}/utils/uartstdio.c \
      ${SRC_LWIP}

OBJS = $(subst .c,.o,$(subst .cpp,.o,$(SRC))) 
$(warning $(OBJS))

#==============================================================================
#                      Rules to make the target
#==============================================================================

#make all rule
all: $(OBJS) ${PROJECT_NAME}.axf ${PROJECT_NAME}

%.o: %.c
	@echo
	@echo Compiling $<...
	$(CC) -c $(CFLAGS) ${<} -o ${@}

%.o: %.cpp
	@echo
	@echo Compiling $<...
	$(CXX) -c $(CXXFLAGS) ${<} -o ${@}

${PROJECT_NAME}.axf: $(OBJS)
	@echo
	@echo Making driverlib
	$(MAKE) -C ${STELLARISWARE_PATH}driverlib/
	@echo
	@echo Linking...
	$(CXX) -nodefaultlibs -mcpu=cortex-m4 -mthumb -T $(LINKER_FILE) $(LFLAGS) -o ${PROJECT_NAME}.axf $(OBJS) ${STELLARISWARE_PATH}driverlib/gcc-cm4f/libdriver-cm4f.a -lc -lgcc
#$(LIBM_PATH) $(LIBC_PATH) $(LIB_GCC_PATH)

${PROJECT_NAME}: ${PROJECT_NAME}.axf
	@echo
	@echo Copying...
	$(CP) $(CPFLAGS) ${PROJECT_NAME}.axf ${PROJECT_NAME}.bin
	@echo
	@echo Creating list file...
	$(OD) $(ODFLAGS) ${PROJECT_NAME}.axf > ${PROJECT_NAME}.lst

# make clean rule
clean:
	rm *.bin *.o *.d *.axf *.lst

# Rule to load the project to the board
# I added a sudo because it's needed without a rule.
load: ${PROJECT_NAME}
	${FLASHER} ${PROJECT_NAME}.bin ${FLASHER_FLAGS}
