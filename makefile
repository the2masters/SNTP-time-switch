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

MCU          = at90usb1287
#MCU          = atmega32u2
ARCH         = AVR8
BOARD        = NONE
F_CPU        = 8000000
F_USB        = 16000000
OPTIMIZATION = s
TARGET       = Zeitschaltuhr
C_STANDARD   = gnu1x
SRC          = $(LUFA_SRC_USB_DEVICE) $(TARGET).c Descriptors.c bootup.c USB.c PacketBuffer.c
LUFA_PATH    = ../lufa/LUFA
CC_FLAGS     = -DCONFIG="test.h" -DUSE_LUFA_CONFIG_HEADER -IConfig/ -Winline -Wall -Wextra -Wpadded -Wwrite-strings -Wcast-align -Wundef -Wfloat-equal -Wswitch-enum -Wno-long-long -flto -Warray-bounds=2
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
