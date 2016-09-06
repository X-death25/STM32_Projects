#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <sys/ioctl.h>
#include <termios.h>
#elif defined(OS_WINDOWS)
#include <conio.h>
#endif

#include "hid.h"

static char get_keystroke(void);

// HID Special Command (only one byte is used in the STM32 Side)

#define WakeUP     0x08  // WakeUP for first STM32 Communication
#define NextPage   0x09  // NextPage : Page is 64 byte

#define TimeTest   0xF0  // Time Test


int main()
{
    int  r, num;
    char buf[64];
    int choixMenu=0;
    int ReadOK=1;
    unsigned long adress=0;
    unsigned long i=0;
    unsigned long j=0;
    
    int time=0;
    int timetest=0;

    // HID Command Buffer
    unsigned char HIDCommand [64];
    char *BufferROM;
    BufferROM = (char*)malloc(128*1024);
    FILE *dump;

    printf("-= Sega Megadrive USB Dumper -= \n\n");
    printf("Detecting USB Device ... \n");

    r = rawhid_open(1, 0x0483, 0x5750,-1, -1);
    if (r <= 0)
    {

        r = rawhid_open(1, 0x0483, 0x5750,-1, -1);
        if (r <= 0)
        {
            printf("No hid device found\n");
            scanf("%d", &choixMenu);
            return -1;
        }
    }

    printf("found HID Megadrive Dumper ! \n\n");
    printf("Receiving game info ...\n");


    /*
    	HIDCommand[0] = WakeUP; // Select WakeUP Command
    	rawhid_send(0,HIDCommand,64,5);

    	while (ReadOK !=0)
    	{

    	  num = rawhid_recv(0, buf, 64,5);
    	  		if (num < 0) {
    			printf("\nerror reading, device went offline\n");
    			rawhid_close(0);
    			return 0;
    		}
    		if (num > 0) {
    			printf("\nrecu %d bytes:\n", num);
    			for (i=0; i<num; i++) {
    				printf("%02X ", buf[i] & 255);
    				if (i % 16 == 15 && i < num-1) printf("\n");
    			}
    			printf("\n\n");
    			ReadOK=0;
    		}


    	}
    	*/



    printf("---Menu---\n\n");
    printf("1.Dump ROM\n");
    printf("2.Time Test\n");
    printf("\nWhat do you want ?\n\n");
    scanf("%d", &choixMenu);

    switch(choixMenu)
    {
    case 1:
        printf("Sending command Dump Header \n");
        printf("Trying to Dump ...\n");
        HIDCommand[0] = 0x09; // Select NextPage Command
        // building adress

        adress=0;
        i=0;
        j=0;

         time=0;
        while (j < 1024*128)
        {
            adress=0+j;
            HIDCommand[1]=adress & 0xFF;
            HIDCommand[2]=(adress & 0xFF00)>>8;
            HIDCommand[3]=(adress & 0xFF0000)>>16;
            HIDCommand[4]=(adress & 0xFF000000)>>24;
            ReadOK=1;
            rawhid_send(0,HIDCommand,64,15);
            num = rawhid_recv(0, buf, 64, 15);
            if (num < 0)
            {
                printf("\nerror reading... \n");
                rawhid_close(0);
                return 0;
            }
            if (num > 0)
            {
                printf("\nrecu %d bytes:\n", num);
                for (i=0; i<num; i++)
                {
                    printf("%02X ", buf[i] & 255);
                    if (i % 16 == 15 && i < num-1) printf("\n");
                }
                printf("\n\n");
                ReadOK=0;
            }

            for (i=0; i<64; i++)
            {
                BufferROM[i+j]=buf[i];
            }

            j=j+64;
        }

        dump=fopen("dump.bin","wb");
        fwrite(BufferROM,1,1024*128,dump);
	time = clock();
	printf("Dump time : = %d ms", time);
	scanf("%d");

    case 2:
        printf("Sending Read Buffer Test \n");
	HIDCommand[0] = WakeUP; // Select WakeUP Command
    	rawhid_send(0,HIDCommand,64,5);

    	while (ReadOK !=0)
    	{

    	  num = rawhid_recv(0, buf, 64,5);
    	  		if (num < 0) {
    			printf("\nerror reading, device went offline\n");
    			rawhid_close(0);
    			return 0;
    		}
    		if (num > 0) {
    			printf("\nrecu %d bytes:\n", num);
    			for (i=0; i<num; i++) {
    				printf("%02X ", buf[i] & 255);
    				if (i % 16 == 15 && i < num-1) printf("\n");
    			}
    			printf("\n\n");
    			ReadOK=0;
    		}


    	}
	time = clock();
	printf("Temps d'execution = %d ms", time);
	scanf("%d");

    default:
        printf("Nice try bye ;)");

    }
}


#if defined(OS_LINUX) || defined(OS_MACOSX)
// Linux (POSIX) implementation of _kbhit().
// Morgan McGuire, morgan@cs.brown.edu
static int _kbhit()
{
    static const int STDIN = 0;
    static int initialized = 0;
    int bytesWaiting;

    if (!initialized)
    {
        // Use termios to turn off line buffering
        struct termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initialized = 1;
    }
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}
static char _getch(void)
{
    char c;
    if (fread(&c, 1, 1, stdin) < 1) return 0;
    return c;
}
#endif



