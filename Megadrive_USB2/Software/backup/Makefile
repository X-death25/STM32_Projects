
#OS = LINUX
#OS = MACOSX
OS = WINDOWS

PROG = main

# To set up Ubuntu Linux to cross compile for Windows:
#
#    apt-get install i686-w64-mingw32-gcc
#
# Just edit the variable above for WINDOWS, then use "make" to build rawhid.exe

ifeq ($(OS), LINUX)
TARGET = $(PROG)
CC = gcc
STRIP = strip
CFLAGS = -Wall -O2 -DOS_$(OS)
LIBS = -lusb -lm
else ifeq ($(OS), MACOSX)
TARGET = $(PROG).dmg
SDK = /Developer/SDKs/MacOSX10.5.sdk
ARCH = -mmacosx-version-min=10.5 -arch ppc -arch i386
CC = gcc
STRIP = strip
CFLAGS = -Wall -O2 -DOS_$(OS) -isysroot $(SDK) $(ARCH)
LIBS = $(ARCH) -Wl,-syslibroot,$(SDK) -framework IOKit -framework CoreFoundation
else ifeq ($(OS), WINDOWS)
TARGET = $(PROG).exe
CC=i686-w64-mingw32-gcc
STRIP=i686-w64-mingw32-strip
CFLAGS = -Wall -O2 -DOS_$(OS) -lusb-1.0
LIBS = -libusb-1.0
endif

OBJS = $(PROG).o


all: $(TARGET)

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS) $(LIBS)
	$(STRIP) $(PROG)

$(PROG).exe: $(PROG)
	cp $(PROG) $(PROG).exe

$(PROG).dmg: $(PROG)
	mkdir tmp
	cp $(PROG) tmp

clean:
	rm -f *.o $(PROG) $(PROG).exe $(PROG).dmg
	rm -rf tmp

