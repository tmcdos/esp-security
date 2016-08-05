# Output directors to store intermediate compiled files
# relative to the project directory
BUILD_BASE	= build
FW_BASE		= firmware

# name for the target project
TARGET		= httpd

# espressif tool to concatenate sections for OTA upload using bootloader v1.2+
APPGEN_TOOL	?= gen_appbin.py

# Optional modules (mqtt rest syslog)
MODULES ?= syslog

CFLAGS=

# set defines for optional modules
ifneq (,$(findstring mqtt,$(MODULES)))
	CFLAGS		+= -DMQTT
endif

ifneq (,$(findstring rest,$(MODULES)))
	CFLAGS		+= -DREST
endif

ifneq (,$(findstring syslog,$(MODULES)))
	CFLAGS		+= -DSYSLOG
endif

# which modules (subdirectories) of the project to include in compiling
LIBRARIES_DIR 	= libraries
MODULES		+= espfs httpd user serial
MODULES		+= $(foreach sdir,$(LIBRARIES_DIR),$(wildcard $(sdir)/*))
EXTRA_INCDIR	= include . # lib/heatshrink/

# libraries used in this project, mainly provided by the SDK (pwm ssl)
LIBS		= c gcc hal phy pp net80211 wpa main lwip crypto ssl

# compiler flags using during compilation of source files
CFLAGS		= -Os -std=c99 -Wno-unused-value -Wpointer-arith -Wundef -Wall -Wl,-EL -fno-inline-functions \
		-nostdlib -mlongcalls -mtext-section-literals -ffunction-sections -fdata-sections \
		-D__ets__ -DICACHE_FLASH -D_STDINT_H -Wno-address -DFIRMWARE_SIZE=$(ESP_FLASH_MAX) \
		-DVERSION="$(VERSION)" -DBUILD_DATE="$(DATE)" -DBUILD_TIME="$(TIME)" -DBUILD_NUM="$(BUILD_NUMBER)"

# linker flags used to generate the main object file
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static -Wl,--gc-sections

# linker script used for the above linker step
LD_SCRIPT 	:= build/eagle.esphttpd.v6.ld
LD_SCRIPT1	:= build/eagle.esphttpd1.v6.ld
LD_SCRIPT2	:= build/eagle.esphttpd2.v6.ld

# various paths from the SDK used in this project
SDK_LIBDIR		= lib
SDK_LDDIR			= ld
SDK_INCDIR		= include include/json
SDK_TOOLSDIR	= tools

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
OBJCP := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objcopy
OBJDP := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objdump


####
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_LDDIR 	:= $(addprefix $(SDK_BASE)/,$(SDK_LDDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))
SDK_TOOLS		:= $(addprefix $(SDK_BASE)/,$(SDK_TOOLSDIR))
APPGEN_TOOL	:= $(addprefix $(SDK_TOOLS)/,$(APPGEN_TOOL))

SRC		:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ		:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC)) $(BUILD_BASE)/espfs_img.o
LIBS		:= $(addprefix -l,$(LIBS))
APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
USER1_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user1.out)
USER2_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user2.out)

INCDIR	:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

ifneq ($(strip $(STA_SSID)),)
CFLAGS		+= -DSTA_SSID="$(STA_SSID)"
endif

ifneq ($(strip $(STA_PASS)),)
CFLAGS		+= -DSTA_PASS="$(STA_PASS)"
endif

ifneq ($(strip $(AP_SSID)),)
CFLAGS		+= -DAP_SSID="$(AP_SSID)"
endif

ifneq ($(strip $(AP_PASS)),)
CFLAGS		+= -DAP_PASS="$(AP_PASS)"
endif

ifneq ($(strip $(AP_AUTH_MODE)),)
CFLAGS		+= -DAP_AUTH_MODE="$(AP_AUTH_MODE)"
endif

ifneq ($(strip $(AP_SSID_HIDDEN)),)
CFLAGS		+= -DAP_SSID_HIDDEN="$(AP_SSID_HIDDEN)"
endif

ifneq ($(strip $(AP_MAX_CONN)),)
CFLAGS		+= -DAP_MAX_CONN="$(AP_MAX_CONN)"
endif

ifneq ($(strip $(AP_BEACON_INTERVAL)),)
CFLAGS		+= -DAP_BEACON_INTERVAL="$(AP_BEACON_INTERVAL)"
endif

ifeq ("$(GZIP_COMPRESSION)","yes")
CFLAGS		+= -DGZIP_COMPRESSION
endif

ifeq ("$(USE_HEATSHRINK)","yes")
CFLAGS		+= -DESPFS_HEATSHRINK
endif

ifeq ("$(CHANGE_TO_STA)","yes")
CFLAGS          += -DCHANGE_TO_STA
endif
