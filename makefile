#
#             LUFA Library
#     Copyright (C) Dean Camera, 2015.
#
#  dean [at] fourwalledcubicle [dot] com
#           www.lufa-lib.org
#
# --------------------------------------
#         LUFA Project Makefile.
# --------------------------------------

# Run "make help" for target help.

MCU          = atmega32u2
ARCH         = AVR8
BOARD        = NONE
F_CPU        = 16000000
F_USB        = $(F_CPU)
OPTIMIZATION = 2
TARGET       = Zeitschaltuhr
C_STANDARD   = gnu1x
SRC          = rules.c $(LUFA_SRC_USB_DEVICE) $(TARGET).c Descriptors.c resources.c bootup.c USB.c Lib/Ethernet.c Lib/UDP.c Lib/ARP.c Lib/ICMP.c Lib/IP.c Lib/SNTP.c
LUFA_PATH    = ../lufa/LUFA
CC_FLAGS     = -DCONFIG="test.h" -DUSE_LUFA_CONFIG_HEADER -IConfig/ -Winline -Wall -Wextra -Wpadded -Wwrite-strings -Wcast-align -Wundef -Wfloat-equal -Wswitch-enum -Wno-long-long -flto # -Warray-bounds=2
LD_FLAGS     = $(CC_FLAGS)

# Default target
all:

# Include LUFA-specific DMBS extension modules
DMBS_LUFA_PATH ?= $(LUFA_PATH)/Build/LUFA
include $(DMBS_LUFA_PATH)/lufa-sources.mk
include $(DMBS_LUFA_PATH)/lufa-gcc.mk

# Include common DMBS build system modules
DMBS_PATH      ?= $(LUFA_PATH)/Build/DMBS/DMBS
include $(DMBS_PATH)/core.mk
include $(DMBS_PATH)/cppcheck.mk
include $(DMBS_PATH)/dfu.mk
include $(DMBS_PATH)/gcc.mk
include $(DMBS_PATH)/avrdude.mk
