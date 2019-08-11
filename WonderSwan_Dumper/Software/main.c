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

#define WAKEUP     		0x10
#define READ_ROM        0x11
#define READ_SRAM   	0x12
#define WRITE_SRAM  	0x13
#define WRITE_FLASH 	0x14
#define ERASE_FLASH 	0x15
#define READ_REGISTER	0x16
#define WRITE_REGISTER	0x17
#define INFOS_ID 		0x18
#define DEBUG_MODE 		0x19

// WonderSwan Dumper Specific Variable

char * game_rom = NULL;
char * game_name = NULL;
const char unk[] = {"unknown"};
const char * save_msg[] = {	"WRITE SMD save",  //0
							"ERASE SMD save"}; //1
const char * wheel[] = { "-","\\","|","/"}; //erase wheel

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

int array_search(unsigned int find, const int * tab, int inc, int tab_size){
	int i=0;
	for(i=0; i<tab_size; (i+=inc)){
		if(tab[i] == find){
			#if defined(DEBUG)
				printf("\n tab:%X find:%X, i:%d, i/inc:%d", tab[i], find, i, (i/inc));
			#endif
			return i;
		}
	}
	return -1; //nothing found
}

	unsigned int trim(unsigned char * buf, unsigned char is_out){
    unsigned char i=0, j=0;
	unsigned char tmp[49] = {0}; //max
	unsigned char tmp2[49] = {0}; //max
	unsigned char next = 1;

	/*check ascii remove unwanted ones and transform upper to lowercase*/
	if(is_out){
		while(i<48){
			if(buf[i]<0x30 || buf[i]>0x7A || (buf[i]>0x29 && buf[i]<0x41) || (buf[i]>0x5A && buf[i]<0x61)) buf[i] = 0x20; //remove shiiit
			if(buf[i]>0x40 && buf[i]<0x5B) buf[i] += 0x20; //to lower case A => a
		i++;
		}
		i=0;
	}

    while(i<48){
	    if(buf[i]!=0x20){
	       	if(buf[i]==0x2F) buf[i] = '-';
	       	tmp[j]=buf[i];
	       	tmp2[j]=buf[i];
	       	next = 1;
	        j++;
		}else{
	       	if(next){
				tmp[j]=0x20;
				tmp2[j]='_';
				next = 0;
		       	j++;
	       	}
	    }
	 	i++;
     }

     next=0;
     if(tmp2[0]=='_'){ next=1; } //offset
     if(tmp[(j-1)]==0x20){ tmp[(j-1)] = tmp2[(j-1)]='\0'; }else{ tmp[j] = tmp2[j]='\0'; }

	 if(is_out){ //+4 for extension
	 	game_rom = (char *)malloc(j-next +4);
		memcpy((unsigned char *)game_rom, (unsigned char *)tmp2 +next, j-next); //stringed file
	 }

	 game_name = (char *)malloc(j-next);
	 memcpy((unsigned char *)game_name, (unsigned char *)tmp +next, j-next); //trimmed
     return 0;
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
    unsigned long address=0;
	unsigned long save_address = 0;
	unsigned char *buffer_rom = NULL;
	unsigned char *buffer_save = NULL;	
	unsigned char *buffer_header = NULL;
	unsigned char region[5];
	unsigned char odd=0;
	unsigned long Game_size=0;
    unsigned char Game_type=0;
	unsigned char bank=0;
	char *game_region = NULL;
	int choixMenu=0;
	int checksum_header = 0;
	unsigned char manufacturer_id=0;
	unsigned char chip_id=0;

	// File manipulation Specific Var	 
	
	FILE *myfile;
	unsigned char *BufferROM;
	unsigned char octetActuel=0;
	int game_size=0;
	unsigned long save_size1 = 0;
	unsigned long save_size2 = 0;
	unsigned long save_size = 0;
	char dump_name[64];

	// Debug Specific Var

	unsigned char DisplayDebug =0;
	unsigned char Control_Data =0;
	unsigned char Debug_Time =1;
	unsigned char Debug_Asel =1;
	unsigned char Debug_LWR =1;
	char DebugCommand[0];	
	unsigned long DebugAddress = 0;
	unsigned long CurrentAddress = 0;
	unsigned short CurrentData = 0xF;


   // Main Program   

    printf("\n");
    printf(" ---------------------------------\n");
    printf("    WS Dumper USB2 Software     \n");
    printf(" ---------------------------------\n");

    printf("Init LibUSB... \n");

  /* Initialise libusb. */

  res = libusb_init(0);
  if (res != 0)
  {
    fprintf(stderr, "Error initialising libusb.\n");
    return 1;
  }

    printf("LibUSB Init Sucessfully ! \n");


 printf("Detecting WonderSwan Dumper... \n");

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


 printf("WonderSwan Dumper Found ! \n");
 printf("Try to init communication...\n");


 // At this step we can try to read the buffer wake up Sega Dumper

  usb_buffer_out[0] = WAKEUP;// Affect request to  WakeUP Command

  libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0); // Send Packets to Sega Dumper
  
libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0); // Receive packets from Sega Dumper
		
    printf("\nWonderSwan Dumper %.*s",6, (char *)usb_buffer_in);
	printf("\n");

  for (i = 0; i < 64; i++)
    {
      usb_buffer_in[i]=0x00;
      usb_buffer_out[i]=0x00;
	}

 usb_buffer_out[0] = READ_REGISTER;// Affect request to  WakeUP Command
 libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0);  
 libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0); 
 printf("Reading Bandai ROM Register :\n");
 printf("Register C0 is 0x%02x\n",usb_buffer_in[0]);
 printf("Register C1 is 0x%02x\n",usb_buffer_in[1]);
 printf("Register C2 is 0x%02x\n",usb_buffer_in[2]);
 printf("Register C3 is 0x%02x\n",usb_buffer_in[3]);

 for (i = 0; i < 64; i++)
    {
      usb_buffer_in[i]=0x00;
      usb_buffer_out[i]=0x00;
	}

 printf("\nDetecting Bandai chip...");

 //scanf("%ld",&address);
 address=0xFFC0;
 usb_buffer_out[0] = READ_ROM;// Affect request to  ReadROM Command
 usb_buffer_out[1] = address&0xFF ;
 usb_buffer_out[2] = (address&0xFF00)>>8;
 usb_buffer_out[3]=(address & 0xFF0000)>>16;
 usb_buffer_out[4] = 0; // Slow Mode
 
 libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0);  
 libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0);

 if (usb_buffer_in[53] ==0x00) // this byte is always 00 => Direct ROM access => Bandai 2001
 {
	printf("\nCartridge have a Bandai 2001");
 }

 else
    {
        printf("\nCartridge have a Bandai 2003"); // Must be unlocked before ROM access
	    printf ("\nNot yet supported sry..\n");
	    scanf("%d");
        return 0;
    }

 printf("\nReading ROM Header :  ");

  for (i=54; i< 64; i++)
    {
        printf("%02hhX ",usb_buffer_in[i]);
    }

  printf("\nPublisher ID : %02hhX",usb_buffer_in[54]);
  if (usb_buffer_in[55] != 0)
    {
        printf("\nGame is for WS Color/Crystal Only!");
		Game_type=1;
    }
    else
    {
        printf("\nGame compatible with all Wonderswan");
    }

  if (usb_buffer_in[58] ==0x06)
    {
        printf("\nGame size is 32 Mbit");
        game_size=1024*4096;
    }
    else if (usb_buffer_in[58] ==0x04)
    {
        printf("\nGame size is 16 Mbit");
        game_size=1024*2048;
    }
    else if (usb_buffer_in[58] ==0x03)
    {
        printf("\nGame size is 8 Mbit");
        game_size=1024*1024;
    }
    else if (usb_buffer_in[58] ==0x02)
    {
        printf("\nGame size is 4 Mbit");
        game_size=1024*512;
    }

 if (usb_buffer_in[59] ==0x00)
    {
        printf("\nGame don't have SRAM/EEPROM\n");
    }
    else
    {
        printf("\nGame have backup Memory type : ");
        if (usb_buffer_in[59] ==0x10)
        {
            printf("EEPROM 1K\n");
        }
        else if (usb_buffer_in[59] ==0x20)
        {
            printf("EEPROM 8K\n");
        }
        else if (usb_buffer_in[59] ==0x50)
        {
            printf("EEPROM 16K\n");
        }
        else if (usb_buffer_in[59] ==0x01)
        {
            printf("SRAM 64K\n");
        }
        else if (usb_buffer_in[59] ==0x02)
        {
            printf("SRAM 256K\n");
        }
        else if (usb_buffer_in[59] ==0x03)
        {
            printf("SRAM 1M\n");
        }
        else if (usb_buffer_in[59] ==0x04)
        {
            printf("SRAM 2M\n");
        }
    }

    if (usb_buffer_in[61] ==0x00)
    {
        printf("Game don't have RTC");
    }
    else
    {
        printf("\nReal Time Clock supported ! ");
    }	

// Clean Register //

 usb_buffer_out[0] = WRITE_REGISTER;// Affect request to  WakeUP Command
 usb_buffer_out[1] = 0x00;
 usb_buffer_out[2] = 0x00;
 usb_buffer_out[3] = 0x00;
 usb_buffer_out[4] = 0xFF;
 libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0);  
 libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0);


printf("\n\n --- MENU ---\n");
printf(" 1) Dump WS/WSC ROM\n");
printf(" 9) Debug Mode \n");  
/*printf(" 2) Dump MD Save\n");
printf(" 3) Write MD Save\n");
printf(" 4) Erase MD Save\n");
printf(" 5) Write MD Flash\n");
printf(" 6) Erase MD Flash\n");
printf(" 7) Master System Mode\n");
printf(" 8) Flash Memory Detection \n");
printf(" 9) Debug Mode \n"); */

printf("\nYour choice: \n");
    scanf("%d", &choixMenu);

switch(choixMenu)
{

		case 1: // DUMP WS ROM
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
				printf("Sending command Dump ROM \n");
        		printf("Dumping please wait ...\n");
				address=0;
				printf("\nRom Size : %ld Ko \n",game_size/1024);
				BufferROM = (unsigned char*)malloc(game_size);
				// Cleaning ROM Buffer
       			 for (i=0; i<game_size; i++)
        			{
            			BufferROM[i]=0x00;
					}

						usb_buffer_out[0] = READ_ROM;           				
						usb_buffer_out[1]=address & 0xFF;
            			usb_buffer_out[2]=(address & 0xFF00)>>8;
            			usb_buffer_out[3]=(address & 0xFF0000)>>16;
            			usb_buffer_out[4]=1;

						libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
						printf("ROM dump in progress...\n"); 
						res = libusb_bulk_transfer(handle, 0x82,BufferROM,game_size, &numBytes, 60000);
  							if (res != 0)
  								{
    								printf("Error \n");
    								return 1;
  								}     
 						printf("\nDump ROM completed !\n");
						if ( Game_type == 0){myfile = fopen("dump.ws","wb");}
						else {myfile = fopen("dump.wsc","wb");}
         			//	myfile = fopen("dump_smd.bin","wb");
        				fwrite(BufferROM, 1,game_size, myfile);
       					fclose(myfile);
						break;

		case 9:  // DEBUG

 while (1)
        {
			printf("\n\nEnter Bank Value \n");
			scanf("%d",&bank);
			usb_buffer_out[0] = WRITE_REGISTER;// Affect request to  WakeUP Command
			usb_buffer_out[3]=bank;
			 libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0);  
 libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0);  
 usb_buffer_out[0] = READ_REGISTER;// Affect request to  WakeUP Command
 libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 0);  
 libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 0); 
 printf("Reading Bandai ROM Register :\n");
 printf("Register C0 is 0x%02x\n",usb_buffer_in[0]);
 printf("Register C1 is 0x%02x\n",usb_buffer_in[1]);
 printf("Register C2 is 0x%02x\n",usb_buffer_in[2]);
 printf("Register C3 is 0x%02x\n",usb_buffer_in[3]);          
			printf("\n\nEnter ROM Address ( decimal value) :\n \n");
            scanf("%ld",&address);
			usb_buffer_out[0] = READ_ROM;
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





