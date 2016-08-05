# --------------- toolchain configuration ---------------

# Base directory for the compiler. Needs a / at the end.
# Typically you'll install https://github.com/pfalcon/esp-open-sdk
XTENSA_TOOLS_ROOT ?= /opt/esp8266/crosstool-NG/builds/xtensa-lx106-elf/bin/

# Firmware version 
# WARNING: if you change this expect to make code adjustments elsewhere, don't expect
# that esp-link will magically work with a different version of the SDK!!!
SDK_VERS ?= esp_iot_sdk_v1.5.2

# Base directory of the ESP8266 SDK package, absolute
# Typically you'll download from Espressif's BBS, http://bbs.espressif.com/viewforum.php?f=5
SDK_BASE	?= /opt/esp8266/SDK

# Try to find the firmware manually extracted, e.g. after downloading from Espressif's BBS,
# http://bbs.espressif.com/viewforum.php?f=46
##SDK_BASE ?= $(wildcard ../$(SDK_VERS))

# If the firmware isn't there, see whether it got downloaded as part of esp-open-sdk
##ifeq ($(SDK_BASE),)
##SDK_BASE := $(wildcard $(XTENSA_TOOLS_ROOT)/../../$(SDK_VERS))
##endif

# Clean up SDK path
##SDK_BASE := $(abspath $(SDK_BASE))
##$(warning Using SDK from $(SDK_BASE))

# Path to bootloader file
BOOTFILE	?= $(SDK_BASE/bin/boot_v1.5.bin)

# Esptool.py path and port, only used for 1-time serial flashing
# Typically you'll use https://github.com/themadinventor/esptool
# Windows users use the com port i.e: ESPPORT ?= com3
ESPTOOL		?= /opt/esp8266/esptool-py/esptool.py
ESPPORT		?= /dev/ttyUSB0
ESPBAUD		?= 115200
