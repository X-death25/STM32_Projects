CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
LIBOPENCM3 ?= ./libopencm3

APP_ADDRESS = 0x08002000
APP_OFFSET = $(shell echo $$(($(APP_ADDRESS) - 0x08000000)))

CFLAGS = -Ofast -std=gnu99 -Wall -pedantic -Werror -Istm32/include \
	-mcpu=cortex-m3 -mthumb -DSTM32F1 \
	-I$(LIBOPENCM3)/include -DAPP_ADDRESS=$(APP_ADDRESS) -ggdb3

LDFLAGS = -lopencm3_stm32f1 \
	-Wl,-Tstm32f103.ld -nostartfiles -lc -lnosys \
	-mthumb -mcpu=cortex-m3 -L$(LIBOPENCM3)/lib/ -Wl,-gc-sections

megadrive-hid-bootldr-combined.bin: megadrive-hid-bootldr.bin megadrive-hid.bin
	cp megadrive-hid-bootldr.bin $@
	dd if=megadrive-hid.bin of=$@ seek=1 bs=$(APP_OFFSET)

megadrive-hid.elf: megadrive-hid.o | $(LIBOPENCM3)/lib/libopencm3_stm32f1.a
	$(CC) $^ -o $@ $(LDFLAGS) -Wl,-Ttext=$(APP_ADDRESS)

megadrive-hid-bootldr.elf: megadrive-hid-bootldr.o | $(LIBOPENCM3)/lib/libopencm3_stm32f1.a
	$(CC) $^ -o $@ $(LDFLAGS)

$(LIBOPENCM3)/lib/libopencm3_stm32f1.a:
	$(MAKE) -C $(LIBOPENCM3) TARGETS=stm32/f1

%.bin: %.elf
	$(OBJCOPY) -O binary $^ $@

clean:
	-rm *.elf *.o *.bin
