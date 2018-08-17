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
  unsigned char usb_buffer_in[64] = {0};   /* 64 byte transfer buffer IN */
  unsigned char usb_buffer_out[64] = {0};  /* 64 byte transfer buffer OUT */
  unsigned long len            = 0;        /* Number of bytes transferred. */

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

	// Fix

	unsigned char rom_buffer_begin[64*9] = {0};
	unsigned char rom_buffer_end[64*8] = {0};

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
	res = libusb_claim_interface(handle, 1);
	 if (res != 0)
  {
    printf("Error claiming interface.\n");
    return 1;}
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

  libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0); // Send Packets to Sega Dumper
  
libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0); // Receive packets from Sega Dumper
/*
printf("\nDisplaying USB IN buffer\n\n");

   for (i = 0; i < 64; i++)
    {
        printf("%02X ",usb_buffer_in[i]);
		j++;
		if (j==16){printf("\n");j=0;}
    }
		
    printf("\nSega Dumper %.*s",6, (char *)usb_buffer_in);
		printf("\n");


  for (i = 0; i < 64; i++)
    {
      usb_buffer_in[i]=0x00;
      usb_buffer_out[i]=0x00;
	}
*/

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
/*
   for (i = 0; i < 64; i++)
    {
        printf("%02X ",usb_buffer_in[i]);
		j++;
		if (j==16){printf("\n");j=0;}
    }
*/
printf("\n%.*s",16, (char *)usb_buffer_in);
printf("\n%.*s",16, (char *)usb_buffer_in+16);
printf("\n%.*s",32, (char *)usb_buffer_in+32);
/*
/////  Fix 1

address=0;
i=0;

while(i<9)
{
usb_buffer_out[0] = READ_MD;
usb_buffer_out[1] = address&0xFF ;
usb_buffer_out[2] = (address&0xFF00)>>8;
usb_buffer_out[3] = 0;
usb_buffer_out[4] = 0;
libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0); 
libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0);
memcpy((unsigned char *)rom_buffer_begin+64*i, usb_buffer_in, 64);
address+=32;
i++;
}

/////  Fix 2

address=(Gamesize -512)/2;
i=0;

while(i<8)
{
usb_buffer_out[0] = READ_MD;
usb_buffer_out[1] = address&0xFF ;
usb_buffer_out[2] = (address&0xFF00)>>8;
usb_buffer_out[3] = (address & 0xFF0000)>>16;
usb_buffer_out[4] = 0;
libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0); 
libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0);
memcpy((unsigned char *)rom_buffer_end+64*i, usb_buffer_in, 64);
address+=32;
i++;
}
/*
j=0;

printf("\nDisplaying Fix info :\n");

   for (i = 0; i < 64*8; i++)
    {
        printf("%02X ",rom_buffer_end[i]);
		j++;
		if (j==16){printf("\n");j=0;}
    }*/


/////*/

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
				BufferROM = (unsigned char*)malloc(Gamesize);
				address=0;
				// Cleaning ROM Buffer
       			 for (i=0; i<Gamesize; i++)
        			{
            			BufferROM[i]=0x00;
					}

						usb_buffer_out[0] = READ_MD;           				
						usb_buffer_out[1]=address & 0xFF;
            			usb_buffer_out[2]=(address & 0xFF00)>>8;
            			usb_buffer_out[3]=(usb_buffer_in[37] +1);
            			usb_buffer_out[4]=1;

		libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
printf("ROM dump in progress...\n"); 
res = libusb_bulk_transfer(handle, 0x82,BufferROM,Gamesize, &numBytes, 60000);
  if (res != 0)
  {
    printf("Error \n");
    return 1;
  }     
 printf("\nDump ROM completed !\n");
         myfile = fopen("dump_smd.bin","wb");
		// Write Fix
		//memcpy((unsigned char *)BufferROM, rom_buffer_begin,64*9);
		//memcpy((unsigned char *)BufferROM+(Gamesize-512), rom_buffer_end,512);
        fwrite(BufferROM, 1,Gamesize, myfile);
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
