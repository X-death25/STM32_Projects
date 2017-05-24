#!/bin/bash
make
rm smsdumper_stm32.bin
rm smsdumper_stm32.elf
rm smsdumper_stm32_bootldr.elf
mv *.bin out
rm smsdumper_stm32.o
rm smsdumper_stm32_bootldr.o


