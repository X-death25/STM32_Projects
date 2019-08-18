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

// Specific Win32 lib ( only used for debug )

#include <conio.h>

// USB Special Command

#define WAKEUP  0x10  // WakeUP for first STM32 Communication
#define READ_GB 0x11
#define READ_GB_SAVE 0x12
#define WRITE_GB_SAVE 0x13
#define WRITE_GB_FLASH 	0x14
#define ERASE_GB_FLASH 0x15
#define INFOS_ID 0x18
#define DEBUG_MODE 0x19



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

  // LibUSB Specific Var
    
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
	int choixMenu=0;
	unsigned long address=0;
	unsigned char *BufferROM;
	int game_size=0;
	FILE *myfile;

  // Rom Header info

    unsigned char Game_Name[16];
	unsigned char Rom_Type=0;
    unsigned long Rom_Size=0;
	unsigned long Ram_Size=0;
	unsigned char CGB=0;
	unsigned char SGB=0;
   

   // Main Program   

    printf("\n");
    printf(" ---------------------------------\n");
    printf("    GB Dumper USB2 Software     \n");
    printf(" ---------------------------------\n");
	printf("\n");

    printf("Init LibUSB... \n");

  /* Initialise libusb. */

  res = libusb_init(0);
  if (res != 0)
  {
    fprintf(stderr, "Error initialising libusb.\n");
    return 1;
  }

    printf("LibUSB Init Sucessfully ! \n");


 printf("Detecting GameBoy Dumper... \n");

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


 printf("GameBoy Dumper Found ! \n");
 printf("Reading cartridge...\n");


 // At this step we can try to read the buffer wake up Sega Dumper

  usb_buffer_out[0] = WAKEUP;// Affect request to  WakeUP Command

  libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0); // Send Packets to Sega Dumper
  
libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0); // Receive packets from Sega Dumper

		
    printf("\nGB Dumper %.*s",6, (char *)usb_buffer_in);
		printf("\n");


  for (i = 0; i < 64; i++)
    {
      usb_buffer_in[i]=0x00;
      usb_buffer_out[i]=0x00;
	}


// Now try to read ROM GB Header


address=308;
game_size=308+64;
usb_buffer_out[0] = READ_GB;
usb_buffer_out[1] = address & 0xFF ;
usb_buffer_out[2] = (address & 0xFF00)>>8;
usb_buffer_out[3]=(address & 0xFF0000)>>16;
usb_buffer_out[5]=game_size & 0xFF;
usb_buffer_out[6]=(game_size & 0xFF00)>>8;
usb_buffer_out[7]=(game_size & 0xFF0000)>>16;
usb_buffer_out[4] = 0; // Slow Mode
libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
libusb_bulk_transfer(handle, 0x82,usb_buffer_in,64, &numBytes, 60000);


printf("\nDisplaying USB IN buffer\n\n");

   for (i = 0; i < 64; i++)
    {
        printf("%02X ",usb_buffer_in[i]);
		j++;
		if (j==16){printf("\n");j=0;}
    }

printf("\nReading ROM Header...  ");
printf("\nGame Title : ");
for (i=0; i<16; i++) {Game_Name[i]=usb_buffer_in[i];} // ROM Name
printf("%.*s",16,(char *) Game_Name);
Rom_Type = usb_buffer_in[19];
printf("\nCartridge Type : %02X ",Rom_Type);
if ( Rom_Type == 0x00){ printf("(ROM ONLY)");}
if ( Rom_Type == 0x01){ printf("(MBC1)");}
if ( Rom_Type == 0x02){ printf("(MBC1+RAM)");}
if ( Rom_Type == 0x03){ printf("(MBC1+RAM+BATTERY)");}
if ( Rom_Type == 0x05){ printf("(MBC2)");}
if ( Rom_Type == 0x06){ printf("(MBC2+BATTERY)");}
if ( Rom_Type == 0x08){ printf("(ROM+RAM)");}
if ( Rom_Type == 0x09){ printf("(ROM+RAM+BATTERY)");}
if ( Rom_Type == 0x0B){ printf("(MMM01)");}
if ( Rom_Type == 0x0C){ printf("(MMM01+RAM)");}
if ( Rom_Type == 0x0D){ printf("(MMM01+RAM+BATTERY)");}
if ( Rom_Type == 0x0F){ printf("(MBC3+TIMER+BATTERY)");}
if ( Rom_Type == 0x10){ printf("(MBC3+TIMER+RAM+BATTERY)");}
if ( Rom_Type == 0x11){ printf("(MBC3)");}
if ( Rom_Type == 0x12){ printf("(MBC3+RAM)");}
if ( Rom_Type == 0x13){ printf("(MBC3+RAM+BATTERY)");}
if ( Rom_Type == 0x19){ printf("(MBC5)");}
if ( Rom_Type == 0x1A){ printf("(MBC5+RAM)");}
if ( Rom_Type == 0x1B){ printf("(MBC5+RAM+BATTERY)");}
if ( Rom_Type == 0x1C){ printf("(MBC5+RUMBLE)");}
if ( Rom_Type == 0x1D){ printf("(MBC5+RUMBLE+RAM)");}
if ( Rom_Type == 0x1E){ printf("(MBC5+RUMBLE+RAM+BATTERY)");}
if ( Rom_Type == 0x20){ printf("(MBC6)");}
if ( Rom_Type == 0x22){ printf("(POCKET CAMERA)");}
if ( Rom_Type == 0xFC){ printf("(MBC7+SENSOR+RUMBLE+RAM+BATTERY)");}
if ( Rom_Type == 0xFD){ printf("(BANDAI TAMA5)");}
if ( Rom_Type == 0xFE){ printf("(HuC3)");}
if ( Rom_Type == 0xFF){ printf("(HuC1+RAM+BATTERY)");}
Rom_Size= (32768 << usb_buffer_in[20]); // Rom Size
printf("\nGame Size is :  %ld Ko",Rom_Size/1024);
Ram_Size = usb_buffer_in[21];
printf("\nRam Size : ");
if ( Ram_Size == 0x00){ printf("None");}
if ( Ram_Size == 0x01){ printf("2 Ko");}
if ( Ram_Size == 0x02){ printf("8 Ko");}
if ( Ram_Size == 0x03){ printf("32 Ko ");}
if ( Ram_Size == 0x04){ printf("128 Ko");}
if ( Ram_Size == 0x05){ printf("64 Ko");}
CGB = usb_buffer_in[15];
if ( CGB  == 0xC0){ printf("\nGame only works on GameBoy Color");}
else { printf("\nGameBoy / GameBoy Color compatible game");}
SGB = usb_buffer_in[18];
if ( SGB  == 0x00){ printf("\nNo Super GameBoy enhancements");}
if ( SGB  == 0x03){ printf("\nGame have Super GameBoy function");}
	
printf("\n\n --- MENU ---\n");
printf(" 1) Dump GB ROM\n"); 
printf(" 2) Debug\n"); 


printf("\nYour choice: \n");
scanf("%d", &choixMenu);

switch(choixMenu)
{

case 1: // READ GB ROM
 
    	choixMenu=0;
				printf(" 1) Auto (from header)\n");
        		printf(" 2) Manual\n");
				printf(" Your choice: ");
        		scanf("%d", &choixMenu);
					if(choixMenu!=1)
					{
            			printf(" Enter number of KB to dump: ");
            			scanf("%d", &game_size);
						game_size *= 1024;
					}
					else {game_size = Rom_Size; }
						    
				printf("Sending command Dump ROM \n");
        		printf("Dumping please wait ...\n");
				address=0;
				printf("\nRom Size : %ld Ko \n",game_size/1024);
				BufferROM = (unsigned char*)malloc(game_size);
        // Cleaning Buffer
   
// Cleaning ROM Buffer
address=0;
BufferROM = (unsigned char*)malloc(game_size); 
     for (i=0; i<game_size; i++)
        {
            BufferROM[i]=0x00;
        }			

						usb_buffer_out[0] = READ_GB;           				
						usb_buffer_out[1]=address & 0xFF;
            			usb_buffer_out[2]=(address & 0xFF00)>>8;
            			usb_buffer_out[3]=(address & 0xFF0000)>>16;
            			usb_buffer_out[4]=0;
						usb_buffer_out[5]=game_size & 0xFF;
            			usb_buffer_out[6]=(game_size & 0xFF00)>>8;
            			usb_buffer_out[7]=(game_size & 0xFF0000)>>16;
						usb_buffer_out[8]=Rom_Type;
						usb_buffer_out[9]=usb_buffer_in[20]; // for calculate number of rom bank
						usb_buffer_out[10]=usb_buffer_in[21];
/*
						usb_buffer_out[6]=usb_buffer_in[20]; // for calculate number of rom bank
						usb_buffer_out[7]=usb_buffer_in[21]; // for calculate number of ram bank*/

						libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
						printf("ROM dump in progress...\n"); 
						res = libusb_bulk_transfer(handle, 0x82,BufferROM,game_size, &numBytes, 60000);
  							if (res != 0)
  								{
    								printf("Error \n");
    								return 1;
  								}     
 						printf("\nDump ROM completed !\n");
						if ( CGB  == 0xC0){myfile = fopen("dump_gbc.gbc","wb");}
         				else {myfile = fopen("dump_gb.gb","wb");}
        				fwrite(BufferROM, 1,game_size, myfile);
       					fclose(myfile);
 break;

case 2:  // DEBUG

        while (1)
        {
            printf("\n\nEnter ROM Address ( decimal value) :\n \n");
            scanf("%ld",&address);
			usb_buffer_out[0] = READ_GB;
			usb_buffer_out[1] = address & 0xFF ;
			usb_buffer_out[2] = (address & 0xFF00)>>8;
			usb_buffer_out[3]=(address & 0xFF0000)>>16;
			usb_buffer_out[4] = 0; // Slow Mode

           libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
libusb_bulk_transfer(handle, 0x82,usb_buffer_in,64, &numBytes, 60000);


printf("\nDisplaying USB IN buffer\n\n");

   for (i = 0; i < 64; i++)
    {
        printf("%02X ",usb_buffer_in[i]);
		j++;
		if (j==16){printf("\n");j=0;}
    }
        }
  

}

		
return 0;

}





