.PHONY: build
build: $(addsuffix .bin,$(addprefix bin/nodemcu-boot-, 9600 74880 115200 silent nobaudchange))

CC=xtensa-lx106-elf-cc
ESPTOOL?=esptool.py

SDKDIR=../sdk/esp_iot_sdk_v1.5.1

CFLAGS=-std=c99 -Os -Wall -Wextra -ffunction-sections -fdata-sections -mtext-section-literals -fomit-frame-pointer -mlongcalls -I$(SDKDIR)/include

LDFLAGS=-nostdlib -T ld/nodemcu-boot.ld -Wl,-EL -Wl,--no-check-sections -static -Wl,--cref -Wl,-Map=$@.map -L$(SDKDIR)/ld

bin/nodemcu-boot-%.bin: obj/nodemcu-boot-%.elf
	$(ESPTOOL) elf2image -o $(dir $@) $<
	mv bin/0x00000.bin $@
	@-rm bin/0x01000.bin

.PRECIOUS: obj/nodemcu-boot-%.elf
obj/nodemcu-boot-%.elf: src/main.c
	$(CC) $(CFLAGS) -DVERBOSE=1 -DBAUD_RATE=$* $^ $(LDFLAGS) -o $@

obj/nodemcu-boot-nobaudchange.elf: src/main.c
	$(CC) $(CFLAGS) -DVERBOSE=1 -DBAUD_RATE=0 $^ $(LDFLAGS) -o $@

obj/nodemcu-boot-silent.elf: src/main.c
	$(CC) $(CFLAGS) -DVERBOSE=0 $^ $(LDFLAGS) -o $@

.PHONY: clean
clean:
	-rm -f obj/* bin/*

.NOTPARALLEL: # esptool.py can't run in parallel, due to objcopy tmpfile
