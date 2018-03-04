/*
 * \file Snes_Dumper.c
 * \brief Snes_Dumper Software for Read/Write Super Nintendo/Famicom Cartridge
 * \author X-death for Ultimate-Consoles forum (http://www.ultimate-consoles.fr)
 * \date 2017/01
 *
 * This source code is based on the very good HID lib RawHid from https://www.pjrc.com/teensy/rawhid.html
 * It uses some USB HID interrupt for communication with the STM32F103
 * Support Sega Megadrive cartridge dump ( max size is 64MB)
 * Support Read & Write save ( 8-32Kb & Bankswitch)
 * Support Sega Master System/Mark3 cartridge dump (max size is 4MB)
 * Please report bug at x-death@ultimate-consoles.fr
 *
 * --------------------------------------------
 *
 * Reprog Ichigo 2017/06 - changed/new features
 * HID full speed (64kB/s) read/dump mode
 * Nearly full speed write mode
 * CFI infos
 * Manufacturer/device ID
 * Write Master System with flash (ichigo's pbc)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>


#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#endif

//#define DEBUG //show debug - comment to hide
#define HEADER //show header - comment to hide

#include "hid.h" // HID Lib

// HID Special Command

#define WakeUP     0x08  // WakeUP for first STM32 Communication
#define Read16     0x09  // Read16 : Read Page (64 byte) in 16 bit mode
#define Read8      0x0A  // Read8  : Read Page (64 byte) in 8 bit mode
#define Erase8     0x0B  // Erase8 : Erase Page (64 byte) in 8 bit mode
#define Erase16    0x0C  // Erase16: Erase Page (64 byte) in 16 bit mode
#define Write8     0x0D  // Write8  :Write Page (32 byte) in 8 bit mode
#define ReadSMS    0x0E  // DumpSFC : Dump SFC cartridge

void pause(char const *message)
{
    int c;
 
    printf("%s", message);
    fflush(stdout);
 
    while ((c = getchar()) != '\n' && c != EOF)
    {
    }
}

int main()
{

    // Program Var
    int  r, num;
    unsigned char buf[64];
    int choixMenu=0;
    int ReadOK=1;
    unsigned long address=0;
    unsigned long i=0;

    
    // Rom Header info
    unsigned char Game_Name[21];
    unsigned char Rom_Type=0;
    unsigned long Rom_Size=0;
    unsigned char Rom_Version=0;

    // HID Command Buffer & Var
    unsigned char HIDCommand [64];
    unsigned char *BufferROM;
    unsigned long Gamesize=0;

 FILE *myfile;

    printf(" ---------------------------------\n");
    printf(" SUPER NINTENDO/FAMICOM USB Dumper\n");
    printf(" ---------------------------------\n");
    printf(" 2018/02 ORIGINAL X-DEATH\n");
  //  printf(" 2017/06 REPROG. ICHIGO\n");
	#if defined(OS_WINDOWS)
	    printf(" WINDOWS");
	#elif defined(OS_MACOSX)
	    printf(" MACOS");
	#elif defined(OS_LINUX)
	    printf(" LINUX");
	#endif
    printf(" ver. 1.0\n");
	printf(" Compiled 2018/02/24\n");
    printf(" www.utlimate-consoles.fr\n\n");

    printf(" Detecting Snes Dumper... \n");

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

    printf(" found HID Snes Dumper ! \n");
    printf(" Receiving header info ...\n\n");

    HIDCommand[0] = WakeUP; // Select WakeUP Command
    rawhid_send(0,HIDCommand,64,15);


    while (ReadOK !=0)
    {

        num = rawhid_recv(0, buf, 64,15);
        if (num < 0)
        {
            printf("\nerror reading, device went offline\n");
            rawhid_close(0);
            return 0;
        }
        if (num > 0)
        {
           // a commenter
            for (i=0; i<num; i++) {
            	printf("%02X ", buf[i] & 255);
            	if (i % 16 == 15 && i < num-1) printf("\n");
            }
            printf("\n");
            ReadOK=0;
        }
    }
    // Backup header info
   
   for (i=0; i<21; i++) {Game_Name[i]=buf[i];} // ROM Name
   Rom_Type=buf[21]; // Cartridge Format
   Rom_Size= (0x400 << buf[23]); // Rom Size
   Rom_Version=buf[60];

   // Display header info

  // printf("\nGame name is : %s",Game_Name);
printf("\nGame name is :  %.*s",21,(char *) Game_Name);
if ( (Rom_Type & 0x01) == 1){printf("\nCartridge format is : HiROM");}
else {printf("\nCartridge format is : LoROM");}
printf("\nGame Size is :  %ld Ko",Rom_Size/1024);
printf("\nGame Version is :  %d ",Rom_Version);
printf("\nHeader Checksum is : %02X%02X ",buf[31],buf[30]);
printf("\nComplement Checksum is : %02X%02X ",buf[29],buf[28]);

   printf("\n\n --- MENU ---\n\n");
    printf(" 1) Dump SFC ROM\n");				//MD
    printf("\nYour choice: \n");
    scanf("%d", &choixMenu);

switch(choixMenu)
{

  case 1:  // READ SFC ROM

        choixMenu=0;
        printf("Enter number of KB to dump: ");
        scanf("%ld", &Gamesize);
        printf("Sending command Dump ROM \n");
        printf("Dumping please wait ...\n");
        HIDCommand[0] = 0x0A; // Select Read in 8 bit Mode
        BufferROM = (unsigned char*)malloc(1024*Gamesize);
        // Cleaning Buffer
        for (i=0; i<1024*Gamesize; i++)
        {
            BufferROM[i]=0x00;
        }

            address=0;
           
       while (address < 1024*Gamesize )

        {

            HIDCommand[1]=address & 0xFF;
            HIDCommand[2]=(address & 0xFF00)>>8;
            HIDCommand[3]=(address & 0xFF0000)>>16;
            HIDCommand[4]=(address & 0xFF000000)>>24;
            rawhid_send(0, HIDCommand, 64, 15);
            rawhid_recv(0,(BufferROM+address), 64,15);
            address +=64 ;
           printf("\rROM dump in progress: %ld%% (adr: 0x%1X)",(100*address)/Gamesize/1024,address);
           fflush(stdout);
        }

        printf("\nDump ROM completed !\n");
         myfile = fopen("dump_sfc.bin","wb");
        fwrite(BufferROM, 1,1024*Gamesize, myfile);
        rawhid_close(0);
        fclose(myfile);
        break;
  

default:
        rawhid_close(0);
   		return 0;
}
 



 
}
