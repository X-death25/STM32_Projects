#!/bin/bash

make
rm stm32-tx-hid-bootldr.bin
rm stm32-tx-hid-bootldr.elf
rm stm32-tx-hid.bin
rm stm32-tx-hid.elf
mv *.bin out
rm stm32-tx-hid.o
rm stm32-tx-hid-bootldr.o

