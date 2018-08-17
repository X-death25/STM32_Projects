/*
 * \file main.c
 * \brief Libusb software for communicate with STM32
 * \author X-death for Ultimate-Consoles forum (http://www.ultimate-consoles.fr)
 * \date 2018/06
 *
 * just a simple sample for testing libusb and new code of Sega Dumper
 *
 * --------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#include <libusb.h>
#include <assert.h>

// USB Special Command

#define WAKEUP  0x10  // WakeUP for first STM32 Communication
#define READ_MD 0x11

// Sega Dumper Specific Variable

char * game_rom = NULL;
char * game_name = NULL;
const char unk[] = {"unknown"};

// Sega Dumper Specific Function

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

  // LibUSB var
    
  int res                      = 0;        /* return codes from libusb functions */
  libusb_device_handle* handle = 0;        /* handle for USB device */
  int kernelDriverDetached     = 0;        /* Set to 1 if kernel driver detached */
  int numBytes                 = 0;        /* Actual bytes transferred. */
  int r                        = 0;			/* Command trasfer*/
  unsigned char usb_buffer_in[64] = {0};   /* 64 byte transfer buffer IN */
  unsigned char usb_buffer_out[64] = {0};  /* 64 byte transfer buffer OUT */

   // Dumper Specific Var

    unsigned long i=0;
	unsigned long j=0;
    unsigned char *buffer_rom = NULL;
    unsigned long address=0;
	unsigned long Gamesize=0;
	char dump_name[64];
	char *game_region = NULL;
	int choixMenu=0;
	unsigned char *BufferROM;
	 FILE *myfile;

   // Main Program   

    printf("\n");
    printf(" ---------------------------------\n");
    printf("    Sega Dumper USB2 Software     \n");
    printf(" ---------------------------------\n");

    printf(" \nInit LibUSB... \n");

  /* Initialise libusb. */

  res = libusb_init(0);
  if (res != 0)
  {
    fprintf(stderr, "Error initialising libusb.\n");
    return 1;
  }

    printf(" \nLibUSB Init Sucessfully ! \n");

 printf(" \nDetecting Sega Dumper... \n");

  /* Get the first device with the matching Vendor ID and Product ID. If
   * intending to allow multiple demo boards to be connected at once, you
   * will need to use libusb_get_device_list() instead. Refer to the libusb
   * documentation for details. */

  handle = libusb_open_device_with_vid_pid(0, 0x0483, 0x5740);

  if (!handle)
  {
    fprintf(stderr, "Unable to open device.\n");
    return 1;
  }

  /* Claim interface #0. */
  res = libusb_claim_interface(handle, 0);
  if (res != 0)
  {
    printf("Error claiming interface.\n");
    return 1;
  }

// Clean Buffer


  for (i = 0; i < 64; i++)
    {
      usb_buffer_in[i]=0x00;
      usb_buffer_out[i]=0x00;
	}


 printf(" \nSega Dumper Found ! \n");
 printf("Sending commande Wake Up ! \n");


 // At this step we can try to read the buffer wake up Sega Dumper

  usb_buffer_out[0] = WAKEUP;// Affect request to  WakeUP Command

  libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 100); // Send Packets to Sega Dumper
  
r = libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 100); // Receive packets from Sega Dumper
if (r == 0 && 64 == sizeof(usb_buffer_in)) {
printf("\nDisplaying USB IN buffer\n\n");

   for (i = 0; i < 64; i++)
    {
        printf("%02X ",usb_buffer_in[i]);
		j++;
		if (j==16){printf("\n");j=0;}
    }
		
    printf("\nSega Dumper %.*s",6, (char *)usb_buffer_in);
		printf("\n");
}

 else {
    printf("Error");
}

  for (i = 0; i < 64; i++)
    {
      usb_buffer_in[i]=0x00;
      usb_buffer_out[i]=0x00;
	}


  // Now try to read ROM MD Header

		
		i = 0;
        address = 256/2;

        //	while(i<8){
	   			usb_buffer_out[0] = READ_MD;
	      		usb_buffer_out[1] = address&0xFF ;
	   			usb_buffer_out[2] = (address&0xFF00)>>8;
	   			usb_buffer_out[3] = 0;
	   			usb_buffer_out[4] = 0;

libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0); 
libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0); 

        //	}

printf("\nDisplaying Cartridge info :\n");
j=0;

   for (i = 0; i < 64; i++)
    {
        printf("%02X ",usb_buffer_in[i]);
		j++;
		if (j==16){printf("\n");j=0;}
    }

printf("\n%.*s",16, (char *)usb_buffer_in);
printf("\n%.*s",16, (char *)usb_buffer_in+16);
printf("\n%.*s",32, (char *)usb_buffer_in+32);

// Calculate ROM Size :

address = 384/2;
usb_buffer_out[0] = READ_MD;
usb_buffer_out[1] = address&0xFF ;
usb_buffer_out[2] = (address&0xFF00)>>8;
usb_buffer_out[3] = 0;
usb_buffer_out[4] = 0;

libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0); 
libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0);
Gamesize=(usb_buffer_in[37] +1) * 65536;
printf("\nRom Size : %ld Ko \n",Gamesize/1024); //M

printf("\n\n --- MENU ---\n");
printf(" 1) Dump SMD ROM\n"); //MD

printf("\nYour choice: \n");
    scanf("%d", &choixMenu);

switch(choixMenu)
{

		case 1: // DUMP SMD ROM
				choixMenu=0;
				printf("Sending command Dump ROM \n");
        		printf("Dumping please wait ...\n");
				Gamesize=Gamesize/1024; // Fix
				BufferROM = (unsigned char*)malloc(1024*Gamesize);
				address=0;
				// Cleaning ROM Buffer
       			 for (i=0; i<1024*Gamesize; i++)
        			{
            			BufferROM[i]=0x00;
					}
	
			     while (address < (1024*Gamesize)/2 )
      				{
						usb_buffer_out[0] = READ_MD;           				
						usb_buffer_out[1]=address & 0xFF;
            			usb_buffer_out[2]=(address & 0xFF00)>>8;
            			usb_buffer_out[3]=(address & 0xFF0000)>>16;
            			usb_buffer_out[4]=(address & 0xFF000000)>>24;
						libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes,0); 
						libusb_bulk_transfer(handle, 0x82,(BufferROM+address*2),64, &numBytes,0);
            			address +=32;
           				//printf("\rROM dump in progress: %ld%% (adr: 0x%1X)",(100*address)/Gamesize/1024,address*2);
						printf("\rROM dump in progress: %ld%% (adr: 0x%1X)",(100*address)/Gamesize/512,address*2);
           				fflush(stdout);
        			}

        printf("\nDump ROM completed !\n");
         myfile = fopen("dump_smd.bin","wb");
        fwrite(BufferROM, 1,1024*Gamesize, myfile);
        fclose(myfile);
		break;

}

	




/*

//HEADER MD
    if(memcmp((unsigned char *)buffer_rom,"SEGA",4) == 0)
{
        printf("\n Megadrive/Genesis cartridge detected!\n");

		printf("\n --- HEADER ---\n");
		memcpy((unsigned char *)dump_name, (unsigned char *)buffer_rom +32, 48);
		trim((unsigned char *)dump_name, 0);
		printf(" Domestic: %.*s\n", 48, (char *)game_name);

		memcpy((unsigned char *)dump_name, (unsigned char *)buffer_rom +80, 48);

		trim((unsigned char *)dump_name, 0);
	    printf(" International: %.*s\n", 48, game_name);

//		trim((unsigned char *)dump_name, 1); //save in game_rom for filename output (remove unwanted & lowercase)

		printf(" Release date: %.*s\n", 16, buffer_rom +0x10);
printf(" Version: %.*s\n", 14, buffer_rom +0x80);

}

else
{

        printf(" \n Unknown cartridge type\n (erased flash eprom, Sega Mark III game, bad connection,...)\n");
        game_rom = (char *)malloc(sizeof(unk));
        game_name = (char *)malloc(sizeof(unk));
        game_region = (char *)malloc(4);
        game_region[3] = '\0';
        memcpy((char *)game_rom, (char *)unk, sizeof(unk));
        memcpy((char *)game_name, (char *)unk, sizeof(unk));
        memcpy((char *)game_region, (char *)unk, 3);
}


	/*	
		i = 0;
        address = 256/2;

        //	while(i<8){
	   			usb_buffer_out[0] = READ_MD;
	      		usb_buffer_out[1] = address&0xFF ;
	   			usb_buffer_out[2] = (address&0xFF00)>>8;
	   			usb_buffer_out[3] = 0;
	   			usb_buffer_out[4] = 0;

libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0); 
libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0); 
printf("\n 1 paquets recu \n");

        //	}
*/


   		return 0;

  
}
