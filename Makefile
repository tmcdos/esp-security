#
# Makefile for esp-link - https://github.com/jeelabs/esp-link
#
# Makefile heavily adapted to esp-link and wireless flashing by Thorsten von Eicken
# Lots of work, in particular to support windows, by brunnels
# Original from esphttpd and others...
#VERBOSE=1

# Start by setting the directories for the toolchain a few lines down
# the default target will build the firmware images
# `make flash` will flash the esp serially
# `make wiflash` will flash the esp over wifi
# `VERBOSE=1 make ...` will print debug info
# `ESP_HOSTNAME=my.esp.example.com make wiflash` is an easy way to override a variable
#

include make_wifi.mak
include make_tool.mak
include make_chip.mak
include make_http.mak

# --------------- esp-link version        ---------------

# This queries git to produce a version string like "esp-link v0.9.0 2015-06-01 34bc76"
# If you don't have a proper git checkout or are on windows, then simply swap for the constant
# Steps to release: create release on github, git pull, git describe --tags to verify you're
# on the release tag, make release, upload esp-link.tgz into the release files
#VERSION ?= "esp-link "
DATE    := $(shell date '+%F')
TIME    := $(shell date '+%T')
#BRANCH  ?= $(shell if git diff --quiet HEAD; then git describe --tags; \
#                   else git symbolic-ref --short HEAD; fi)
#SHA     := $(shell if git diff --quiet HEAD; then git rev-parse --short HEAD | cut -d"/" -f 3; \
#                   else echo "development"; fi)

BUILD_NUMBER_FILE = $(abspath ./build-number.txt)
BUILD_NUMBER_SRC = $(abspath ./user/buildnum.c)
BUILD_NUMBER := $(shell if ! test -f $(BUILD_NUMBER_FILE); then echo 1 > $(BUILD_NUMBER_FILE); fi; cat $(BUILD_NUMBER_FILE))
VERSION ?= esp-link $(DATE) $(TIME)

HTML_PATH = $(abspath ./html)/

# make_vars should be AFTER definition of BUILD_NUMBER

include make_vars.mak

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS)  -c $$< -o $$@
endef

.PHONY: all checkdirs clean webpages.espfs wiflash

all: echo_version checkdirs $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin

echo_version:
	@echo VERSION: $(VERSION)
# Create an auto-incrementing build number.
	@echo "#define VER_STR2(V) #V" > $(BUILD_NUMBER_SRC)
	@echo "#define VER_STR(V) VER_STR2(V)" >> $(BUILD_NUMBER_SRC)
	@echo "char esp_link_version[] = VER_STR(VERSION);" >> $(BUILD_NUMBER_SRC)
	@echo "char esp_link_date[] = VER_STR(BUILD_DATE);" >> $(BUILD_NUMBER_SRC)
	@echo "char esp_link_time[] = VER_STR(BUILD_TIME);" >> $(BUILD_NUMBER_SRC)
	@echo "char esp_link_build[] = VER_STR(BUILD_NUM);" >> $(BUILD_NUMBER_SRC)

# Build number file.  Increment if firmware file changes
$(BUILD_NUMBER_FILE): $(USER1_OUT)
	@if ! test -f $(BUILD_NUMBER_FILE); then echo 0 > $(BUILD_NUMBER_FILE); fi
	@echo $$(($$(cat $(BUILD_NUMBER_FILE)) + 1)) > $(BUILD_NUMBER_FILE)

$(USER1_OUT): $(APP_AR) $(LD_SCRIPT1)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT1) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@
	$(Q) $(OBJDP) -S $(USER1_OUT) > $(addprefix $(BUILD_BASE)/,$(TARGET).dump)

$(USER2_OUT): $(APP_AR) $(LD_SCRIPT2)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT2) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@

$(FW_BASE): 
	$(vecho) "FW $@"
	$(Q) mkdir -p $@

$(FW_BASE)/user1.bin: $(USER1_OUT) $(FW_BASE) $(BUILD_NUMBER_FILE)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER1_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER1_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER1_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER1_OUT) eagle.app.v6.irom0text.bin
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER1_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE) 0
	$(Q) rm -f eagle.app.v6.*.bin
	$(Q) mv eagle.app.flash.bin $@
	@echo "** user1.bin uses $$(stat -c '%s' $@) bytes of" $(ESP_FLASH_MAX) "available =" $(shell expr $$(stat -c '%s' $@) \* 100 / $(ESP_FLASH_MAX) ) "%"
	$(Q) if [ $$(stat -c '%s' $@) -gt $$(( $(ESP_FLASH_MAX) )) ]; then echo "$@ too big!"; false; fi

$(FW_BASE)/user2.bin: $(USER2_OUT) $(FW_BASE)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER2_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER2_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER2_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER2_OUT) eagle.app.v6.irom0text.bin
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER2_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE) 0
	$(Q) rm -f eagle.app.v6.*.bin
	$(Q) mv eagle.app.flash.bin $@
	$(Q) if [ $$(stat -c '%s' $@) -gt $$(( $(ESP_FLASH_MAX) )) ]; then echo "$@ too big!"; false; fi

$(APP_AR): $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	$(Q) mkdir -p $@

wiflash: all
	./wiflash $(ESP_HOSTNAME) $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin

baseflash: all
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash 0x01000 $(FW_BASE)/user1.bin

flash: all
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash -fs $(ET_FS) -ff $(ET_FF) \
	  0x00000 "$(SDK_BASE)/bin/boot_v1.5.bin" 0x01000 $(FW_BASE)/user1.bin \
	  $(ET_BLANK) $(SDK_BASE)/bin/blank.bin

yui/$(HTML_COMPRESSOR):
	$(Q) mkdir -p yui;
	$(Q) if ! test -f yui/$(YUI_COMPRESSOR); then wget https://github.com/yui/yuicompressor/releases/download/v2.4.8/$(YUI_COMPRESSOR) -O yui/$(YUI_COMPRESSOR); fi
	$(Q) if ! test -f yui/$(HTML_COMPRESSOR); then wget https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/htmlcompressor/$(HTML_COMPRESSOR) -O yui/$(HTML_COMPRESSOR); fi

ifeq ("$(COMPRESS_W_HTMLCOMPRESSOR)","yes")
$(BUILD_BASE)/espfs_img.o: yui/$(HTML_COMPRESSOR)
endif

$(BUILD_BASE)/espfs_img.o: $(shell find html) espfs/mkespfsimage/mkespfsimage
	$(Q) rm -rf html_compressed; mkdir -p html_compressed; cp -r html/* html_compressed;
ifeq ("$(COMPRESS_W_HTMLCOMPRESSOR)","yes")
	$(Q) echo "Compressing HTML with htmlcompressor. This is fast..."
	$(Q) for file in `find html_compressed -type f -name "*.html"`; do \
			java -jar yui/$(HTML_COMPRESSOR) \
			-t html --remove-surrounding-spaces max --remove-quotes --remove-intertag-spaces \
			-o $$file $$file; \
		done
	$(Q) echo "Compressing JS/CSS with yui-compressor. This may take a while..."
	$(Q) for file in `find html_compressed -type f -name "*.js"`; do \
			java -jar yui/$(YUI_COMPRESSOR) $$file --line-break 0 -o $$file; \
		done
	$(Q) for file in `find html_compressed -type f -name "*.css"`; do \
			java -jar yui/$(YUI_COMPRESSOR) $$file -o $$file; \
		done
endif
ifeq (,$(findstring mqtt,$(MODULES)))
	$(Q) rm -rf html_compressed/mqtt.html
	$(Q) rm -rf html_compressed/mqtt.js
endif
	$(Q) echo "Now building espFS ..."
	$(Q) cd html_compressed; find . \! -name \*- | ../espfs/mkespfsimage/mkespfsimage > ../build/espfs.img; cd ..;
	@echo "espFS image is $$(stat -c '%s' build/espfs.img) bytes =" $(shell expr $$(stat -c '%s' build/espfs.img) \* 100 / $$(du -sb html_compressed | { read first _ ; echo $$first; })) "% of uncompressed originals"
	$(Q) cd build; $(OBJCP) -I binary -O elf32-xtensa-le -B xtensa --rename-section .data=.espfs \
			espfs.img espfs_img.o; cd ..

# edit the loader script to add the espfs section to the end of irom with a 4 byte alignment.
# we also adjust the sizes of the segments 'cause we need more irom0
# in the end the only thing that matters wrt size is that the whole shebang fits into the
# 236KB available (in a 512KB flash)
ifeq ("$(FLASH_SIZE)","512KB")
build/eagle.esphttpd1.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.512.app1.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/2B000/38000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.512.app1.ld >$@
build/eagle.esphttpd2.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.512.app2.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/2B000/38000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.512.app2.ld >$@
else
build/eagle.esphttpd1.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.1024.app1.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/6B000/7C000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.1024.app1.ld >$@
build/eagle.esphttpd2.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.1024.app2.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/6B000/7C000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.1024.app2.ld >$@
endif

espfs/mkespfsimage/mkespfsimage: espfs/mkespfsimage/*.c
	$(Q) $(MAKE) -C espfs/mkespfsimage USE_HEATSHRINK="$(USE_HEATSHRINK)" GZIP_COMPRESSION="$(GZIP_COMPRESSION)"

release: all
	$(Q) rm -rf release; mkdir -p release/esp-link
	$(Q) egrep -a 'esp-link [a-z0-9.]+ - 201' $(FW_BASE)/user1.bin | cut -b 1-80
	$(Q) egrep -a 'esp-link [a-z0-9.]+ - 201' $(FW_BASE)/user2.bin | cut -b 1-80
	$(Q) cp $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin $(SDK_BASE)/bin/blank.bin \
		   "$(SDK_BASE)/bin/boot_v1.5.bin" wiflash release/esp-link
	$(Q) tar zcf esp-link.tgz -C release esp-link
	$(Q) rm -rf release

clean:
	$(Q) rm -f $(APP_AR)
	$(Q) rm -f $(TARGET_OUT)
	$(Q) find $(BUILD_BASE) -type f | xargs rm -f
	$(Q) make -C espfs/mkespfsimage/ clean
	$(Q) rm -rf $(FW_BASE)
	$(Q) rm -f webpages.espfs
ifeq ("$(COMPRESS_W_HTMLCOMPRESSOR)","yes")
	$(Q) rm -rf html_compressed
endif

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
