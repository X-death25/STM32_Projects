#!/bin/bash

make
rm snes-hid-bootldr.bin
rm snes-hid-bootldr.elf
rm snes-hid.elf
mv *.bin out
rm snes-hid.o
rm snes-hid-bootldr.o

