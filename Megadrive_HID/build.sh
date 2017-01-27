#!/bin/bash

make
rm megadrive-hid-bootldr.bin
rm megadrive-hid-bootldr.elf
rm megadrive-hid.elf
mv *.bin out
rm megadrive-hid.o
rm megadrive-hid-bootldr.o

