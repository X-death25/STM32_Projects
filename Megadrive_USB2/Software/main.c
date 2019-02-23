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
#define READ_MD_SAVE 0x12
#define WRITE_MD_SAVE 0x13
#define WRITE_MD_FLASH 	0x14
#define ERASE_MD_FLASH 0x15
#define INFOS_ID 0x18

// Sega Dumper Specific Variable

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
    unsigned long address=0;
	unsigned long save_address = 0;
	unsigned char *buffer_rom = NULL;
	unsigned char *buffer_save = NULL;	
	unsigned char *buffer_header = NULL;
	unsigned char octetActuel=0;
	unsigned char region[5];
	unsigned char odd=0;
	unsigned char *BufferROM;
	char dump_name[64];
	char *game_region = NULL;

	int choixMenu=0;
	int game_size=0;
	unsigned long save_size1 = 0;
	unsigned long save_size2 = 0;
	unsigned long save_size = 0;
	int checksum_header = 0;

	unsigned char manufacturer_id=0;
	unsigned char chip_id=0;	 
	
	FILE *myfile;

	// Fix

	unsigned char rom_buffer_begin[64*9] = {0};
	unsigned char rom_buffer_end[64*8] = {0};

   // Main Program   

    printf("\n");
    printf(" ---------------------------------\n");
    printf("    Sega Dumper USB2 Software     \n");
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


 printf("Detecting Sega Dumper... \n");

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


 printf("Sega Dumper Found ! \n");
 printf("Reading cartridge...\n");


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

		buffer_header = (unsigned char *)malloc(0x200);
		i = 0;
        address = 0x80;

			// Cleaning header Buffer
       			 for (i=0; i<512; i++)
        			{
            			buffer_header[i]=0x00;
					}
				i = 0;

while (i<8)
{

	   			usb_buffer_out[0] = READ_MD;
	      		usb_buffer_out[1] = address&0xFF ;
	   			usb_buffer_out[2] = (address&0xFF00)>>8;
	   			usb_buffer_out[3]=(address & 0xFF0000)>>16;
	   			usb_buffer_out[4] = 0; // Slow Mode

libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
libusb_bulk_transfer(handle, 0x82,buffer_header+(64*i),64, &numBytes, 60000);
address+=32;
i++;
 } 		
		

if(memcmp((unsigned char *)buffer_header,"SEGA",4) == 0)
{
printf("\nMegadrive/Genesis/32X cartridge detected!\n");
 for(i=0; i<(256/16); i++){
		        printf("\n");
				printf(" %03lX", 0x100+(i*16));
				for(j=0; j<16; j++){
					printf(" %02X", buffer_header[j+(i*16)]);
				}
		        printf(" %.*s", 16, buffer_header +(i*16));
		    }
printf("\n");

printf("\n --- HEADER ---\n");
memcpy((unsigned char *)dump_name, (unsigned char *)buffer_header+32, 48);
trim((unsigned char *)dump_name, 0);
printf(" Domestic: %.*s\n", 48, (char *)game_name);
memcpy((unsigned char *)dump_name, (unsigned char *)buffer_header+80, 48);
trim((unsigned char *)dump_name, 0);

printf(" International: %.*s\n", 48, game_name);
printf(" Release date: %.*s\n", 16, buffer_header+0x10);
printf(" Version: %.*s\n", 14, buffer_header+0x80);
memcpy((unsigned char *)region, (unsigned char *)buffer_header +0xF0, 4);
		for(i=0;i<4;i++){
			if(region[i]==0x20){
				game_region = (char *)malloc(i);
				memcpy((unsigned char *)game_region, (unsigned char *)buffer_header +0xF0, i);
				game_region[i] = '\0';
				break;
			}
		}

		if(game_region[0]=='0'){
			game_region = (char *)malloc(4);
			memcpy((char *)game_region, (char *)unk, 3);
			game_region[3] = '\0';
		}

printf(" Region: %s\n", game_region);

checksum_header = (buffer_header[0x8E]<<8) | buffer_header[0x8F];
printf(" Checksum: %X\n", checksum_header);

game_size = 1 + ((buffer_header[0xA4]<<24) | (buffer_header[0xA5]<<16) | (buffer_header[0xA6]<<8) | buffer_header[0xA7])/1024;
printf(" Game size: %dKB\n", game_size);



if((buffer_header[0xB0] + buffer_header[0xB1])!=0x93){
            printf(" Extra Memory : No\n");
        }else{
            printf(" Extra Memory : Yes ");
            switch(buffer_header[0xB2]){
				case 0xF0: printf(" 8bit backup SRAM (even addressing)\n"); break;
            	case 0xF8: printf(" 8bit backup SRAM (odd addressing)\n"); break;
				case 0xB8: printf(" 8bit volatile SRAM (odd addressing)\n"); break;
				case 0xB0: printf(" 8bit volatile SRAM (even addressing)\n"); break;
				case 0xE0: printf(" 16bit backup SRAM\n"); break;
				case 0xA0: printf(" 16bit volatile SRAM\n"); break;
            	case 0xE8: printf(" Serial EEPROM\n"); break;
            }
			if ( buffer_header[0xB2] != 0xE0 | buffer_header[0xB2] != 0xA0 ) // 8 bit SRAM
			{
				save_size2 = (buffer_header[0xB8]<<24) | (buffer_header[0xB9]<<16) | (buffer_header[0xBA] << 8) | buffer_header[0xBB];
				save_size1 = (buffer_header[0xB4]<<24) | (buffer_header[0xB5]<<16) | (buffer_header[0xB6] << 8) | buffer_header[0xB7];

				save_size = save_size2 - save_size1;
				save_size = (save_size/1024); // Kb format
				save_size=(save_size/2) + 1; // 8bit size 
			}
        	save_address = (buffer_header[0xB4]<<24) | (buffer_header[0xB5]<<16) | (buffer_header[0xB6] << 8) | buffer_header[0xB7];           
printf(" Save size: %dKb\n", save_size); 
printf(" Save address: %lX\n", save_address);

if(usb_buffer_in[0xB2]==0xE8) // EEPROM Game
{
   printf(" No information on this game!\n");
}
}

} // No Sega Megadrive cartridge Detected

else {

 printf(" \nUnknown cartridge type\n(erased flash eprom, Sega Mark III game, bad connection,...)\n");
}




printf("\n\n --- MENU ---\n");
printf(" 1) Dump MD ROM\n"); 
printf(" 2) Dump MD Save\n");
printf(" 3) Write MD Save\n");
printf(" 4) Erase MD Save\n");
printf(" 5) Write MD Flash\n");
printf(" 6) Erase MD Flash\n");
printf(" 9) Manufacturer / Identification Chip \n"); 

printf("\nYour choice: \n");
    scanf("%d", &choixMenu);

switch(choixMenu)
{

		case 1: // DUMP MD ROM
				choixMenu=0;
				printf(" 1) Auto (from header)\n");
        		printf(" 2) Manual\n");
				printf(" Your choice: ");
        		scanf("%d", &choixMenu);
					if(choixMenu==2)
					{
            			printf(" Enter number of KB to dump: ");
            			scanf("%d", &game_size);
					}		    
				printf("Sending command Dump ROM \n");
        		printf("Dumping please wait ...\n");
				address=0;
			    game_size *= 1024;
				printf("\nRom Size : %ld Ko \n",game_size/1024);
				BufferROM = (unsigned char*)malloc(game_size);
				// Cleaning ROM Buffer
       			 for (i=0; i<game_size; i++)
        			{
            			BufferROM[i]=0x00;
					}

						usb_buffer_out[0] = READ_MD;           				
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
         				myfile = fopen("dump_smd.bin","wb");
        				fwrite(BufferROM, 1,game_size, myfile);
       					fclose(myfile);
						break;

		case 2: // DUMP MD Save
				choixMenu=0;
				printf(" 1) Auto (from header)\n");
        		printf(" 2) Manual 64kb/8KB\n");
        		printf(" 3) Manual 256kb/32KB\n");
        		printf(" Your choice: ");
				scanf("%d", &choixMenu);
				
				 if(choixMenu>3)
				  {
        			printf(" Wrong number!\n\n");
	        		return 0;
				   }

				switch(choixMenu)
				{
					case 1:  save_size *= 1024;  break;
					case 2:  save_size = 8192;  break;
					case 3:  save_size = 32768; break;
					default: save_size = 8192;
				}
			
				buffer_rom = (unsigned char*)malloc(save_size); // raw buffer
				buffer_save = (unsigned char*)malloc((save_size*2)); // raw in 16bit format

				 for (i=0; i<(save_size*2); i++)
        {
            buffer_save[i]=0x00;
        }

				usb_buffer_out[0] = READ_MD_SAVE;
				address=(save_address/2);
				i=0;
				while ( i< save_size)
			{
							          				
					usb_buffer_out[1]=address & 0xFF;           		
					usb_buffer_out[2]=(address & 0xFF00)>>8;
            		usb_buffer_out[3]=(address & 0xFF0000)>>16;
            		usb_buffer_out[4]=0;
				    libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
					libusb_bulk_transfer(handle, 0x82,(buffer_rom+i),64, &numBytes, 60000);
					address +=64; //next adr
					i+=64;

            		printf("\r SAVE dump in progress: %ld%%", ((100 * i)/save_size));
					fflush(stdout);
				}
					i=0;
					j=0;
					myfile = fopen("raw.srm","wb");
        			fwrite(buffer_rom,1,save_size, myfile);

				for (i=0; i<save_size; i++)
       			 {
           			 j=j+1;
            		buffer_save[i+j]=buffer_rom[i];
        		}


				myfile = fopen("dump_smd.srm","wb");
        		fwrite(buffer_save,1,save_size*2, myfile);
       		    fclose(myfile);
				break;

	
		case 3:  // WRITE SRAM
    	case 4:  // ERASE SRAM
					//write -> load file
					//erase full of 0xFF
				save_size *= 1024; //in KB
        		buffer_save = (unsigned char*)malloc(save_size);

				// Clean Buffer
  for (i = 0; i < 64; i++)
    {
      usb_buffer_in[i]=0x00;
      usb_buffer_out[i]=0x00;
	}

      			if(choixMenu == 3)
				{
	       			printf(" Save file: ");
	        		scanf("%60s", dump_name);
					myfile = fopen(dump_name,"rb");

		    			if(myfile == NULL)
						  {
		    				printf(" Save file %s not found!\n Exit\n\n", dump_name);
		       				free(game_name);
		       				free(game_rom);
		       				free(game_region);
		       				free(buffer_save);
		        			return 0;
		    			  }
						save_size=save_size/2;
						i=0;
						while ( i < save_size) // Read the save file
       			 		{
           					 fread(&octetActuel,1,1,myfile);
                			 buffer_save[i]=octetActuel;
							 i++;
        				}

 					//fread(buffer_save, 1, save_size, myfile);
					//fclose(myfile);

		    				

       			}else{		//clean buffer with 0xFF (erase)
						for(i=0;i<save_size;i++)
							{
								buffer_save[i] = 0xFF;
							}
        			}

					

					//1st BACKUP SRAM (just in case...)


        			buffer_rom = (unsigned char*)malloc(0x10000);
					address = (save_address/2);
					i=0;
       					 
						while(i<save_size)
							{

								usb_buffer_out[0] = READ_MD_SAVE;								
								usb_buffer_out[1]=address & 0xFF;           		
								usb_buffer_out[2]=(address & 0xFF00)>>8;
            					usb_buffer_out[3]=(address & 0xFF0000)>>16;
            					usb_buffer_out[4]=0;
				    			libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
								libusb_bulk_transfer(handle, 0x82,(buffer_rom+i),64, &numBytes, 60000);
								address +=64; //next adr
								i+=64;

            					printf("\r SAVE dump in progress: %ld%%", ((100 * i)/save_size));
								fflush(stdout);
        					}

						while(i<0x10000){ buffer_rom[i++] = 0xFF; } //fill with 0xFF until 64KB
        				myfile = fopen("dump_smd.srm.bak","wb");
        				fwrite(buffer_rom, 1, 0x10000, myfile);
        				fclose(myfile);

////////////////////

      					printf("\n");
 						address = (save_address/2);
						i=0;
						j=0;
						printf(" Save size: %d (0x%X)\n", save_size, save_size);
						fflush(stdout);
  	   					while(i<save_size/8)
							{
	     						usb_buffer_out[0] = WRITE_MD_SAVE; // Select write in 8bit Mode
								usb_buffer_out[1]=address & 0xFF;           		
								usb_buffer_out[2]=(address & 0xFF00)>>8;
            					usb_buffer_out[3]=(address & 0xFF0000)>>16;

								while (j<32)
                    			{
                        			usb_buffer_out[32+j]=buffer_save[i+j];
									j++;
                    			}
								j=0;
						
					


            					libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
								while ( usb_buffer_in[6] != 0xAA)
								{
									libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 6000); 
								}
            					i +=32;
            					address +=32;
           						printf("\r %s in progress: %ld%%", save_msg[(choixMenu - 3)], ((100 * i)/save_size/8));
           						fflush(stdout);
       						 }
						
        				free(buffer_save);
						break;


		case 5: // Write MD Flash

						printf(" ALL DATAS WILL BE ERASED BEFORE ANY WRITE!\n");
						printf(" ROM file: ");
       					scanf("%60s", dump_name);
						myfile = fopen(dump_name,"rb");
						fseek(myfile,0,SEEK_END);
   						game_size = ftell(myfile);
						buffer_rom = (unsigned char*)malloc(game_size);
						rewind(myfile);
	   					fread(buffer_rom, 1, game_size, myfile);
						fclose(myfile);
						i=0;
						address = 0;

						// First Erase Flash Memory

						usb_buffer_out[0] = ERASE_MD_FLASH;
           libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
            i=0;
            while(usb_buffer_in[0]!=0xFF){
            	libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 6000);   //wait status
										}

						printf("\r ERASE SMD flash completed\n");
						i=0;
						address = 0;

						while(i<game_size)
			{
								
						usb_buffer_out[0] = WRITE_MD_FLASH; // Select write in 16bit Mode
						usb_buffer_out[1] = address & 0xFF;
						usb_buffer_out[2] = (address & 0xFF00)>>8;
						usb_buffer_out[3] = (address & 0xFF0000)>>16;

						  if((game_size - i)<54){
	 	    	usb_buffer_out[4] = (game_size - i); //adjust last packet
	  	    	}else{
	  	    	usb_buffer_out[4] = 54; //max 58 bytes - must by pair (word)
					}

					memcpy((unsigned char *)usb_buffer_out +5, (unsigned char *)buffer_rom +i, usb_buffer_out[4]);

						libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
 i += usb_buffer_out[4];
address += (usb_buffer_out[4]>>1);
printf("\r WRITE SMD flash in progress: %ld%%", ((100 * i)/game_size));
fflush(stdout);
}

						printf("\r SMD flash completed\n");
						free(buffer_save);
						break;

	case 6: //ERASE FLASH

		    buffer_rom = (unsigned char*)malloc(64);
	     	usb_buffer_out[0] = ERASE_MD_FLASH;
           libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
            i=0;
            while(usb_buffer_in[0]!=0xFF){
            	libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 6000);   //wait status
		        printf("\r ERASE SMD flash in progress: %s ", wheel[i]);
		        fflush(stdout);
		        i++;
		        if(i==4){i=0;}
            }

            printf("\r ERASE SMD flash in progress: 100%%");
            fflush(stdout);

break;

	case 9: // Vendor / ID Info

						printf("Detecting Flash...\n");
						usb_buffer_out[0] = INFOS_ID;
						libusb_bulk_transfer(handle, 0x01,usb_buffer_out, sizeof(usb_buffer_out), &numBytes, 60000);
						libusb_bulk_transfer(handle, 0x82, usb_buffer_in, sizeof(usb_buffer_in), &numBytes, 6000); 

manufacturer_id = usb_buffer_in[1];
chip_id = usb_buffer_in[3];

printf("Manufacturer ID : %02X \n",usb_buffer_in[1]);
printf("Chip ID : %02X \n",usb_buffer_in[3]);

scanf("%d");
								
						break; 

			

	}
		
return 0;

}





