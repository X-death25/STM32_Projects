#!/bin/bash

gcc -o rawhid_test rawhid_test.c -I/usr/include/libusb-1.0/ -Ihid.h -lusb-1.0
