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
OPTIMIZATION = s
TARGET       = Zeitschaltuhr
C_STANDARD   = gnu1x
SRC          = rules.c $(LUFA_SRC_USB_DEVICE) $(TARGET).c Descriptors.c resources.c bootup.c USB.c Lib/Ethernet.c Lib/UDP.c Lib/ARP.c Lib/ICMP.c Lib/IP.c Lib/SNTP.c Lib/ASCII.c
LUFA_PATH    = ../lufa/LUFA
CC_FLAGS     = -DUSE_LUFA_CONFIG_HEADER -IConfig/ -Winline -Wall -Wextra -Wpadded -Wwrite-strings -Wcast-align -Wundef -Wfloat-equal -Wswitch-enum -Wno-long-long -flto -Warray-bounds=2
LD_FLAGS     = $(CC_FLAGS)

# Default target
all:

# Include LUFA build script makefiles
include $(LUFA_PATH)/Build/lufa_core.mk
include $(LUFA_PATH)/Build/lufa_sources.mk
include $(LUFA_PATH)/Build/lufa_build.mk
include $(LUFA_PATH)/Build/lufa_dfu.mk
