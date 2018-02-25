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

#define WAKEUP     		0x10
#define READ_MD     	0x11
#define READ_MD_SAVE  	0x12
#define WRITE_MD_SAVE 	0x13
#define WRITE_MD_FLASH 	0x14
#define ERASE_MD_FLASH 	0x15
#define READ_SMS   		0x16
#define CFI_MODE	 	0x17
#define INFOS_ID	 	0x18

const int flash_algo[] = {	0,
							1,
							2,
							3,
							4,
							256,
							257,
							258};

const char * flash_algo_msg[] ={"no Alternate Vendor specified",
								"Intel/Sharp Extended Command Set",
								"AMD/Fujitsu Standard Command Set",
								"Intel Standard Command Set",
								"AMD/Fujitsu Extended Command Set",
								"Mitsubishi Standard Command Set",
								"Mitsubishi Extended Command Set",
								"SST Page Write Command Set"};

const char * flash_device_description[] = {	"x8-only asynchronous",
											"x16-only asynchronous",
											"x8, x16 asynchronous",
											"x32-only asynchronous",
											"x16, x32 asynchronous"};

const char * wheel[] = { "-","\\","|","/"}; //erase wheel

const char * save_msg[] = {	"WRITE SFC save",  //0
							"ERASE SFC save"}; //1

const char * flash_msg[] = {"WRITE SFC flash",  //0
							"ERASE SFC flash"}; //1

const char * menu_msg[] = {	"\n --- DUMP ROM MODE --- \n",
							"\n --- DUMP SAVE MODE ---\n",
							"\n --- WRITE SFC SAVE ---\n",
							"\n --- ERASE SFC SAVE ---\n",
							"\n --- WRITE SFC FLASH ---\n",
							"\n --- ERASE SFC FLASH ---\n",
							"\n --- DUMP SMS ROM ---\n",
							"\n --- CFI MODE ---\n",
							"\n --- INFOS ID ---\n"};


char * game_rom = NULL;
char * game_name = NULL;

const char unk[] = {"unknown"};

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


int checksum(unsigned char * buf, int offset, unsigned char length){
	int i = 0;
	int check = 0;
	for(i=0;i<length;i++){
		check += buf[i+offset];
	}
	return check;
}




#if defined(OS_LINUX) || defined(OS_MACOSX)
	struct timeval ostime;
	long microsec_start = 0;
	long microsec_end = 0;
#elif defined(OS_WINDOWS)
	clock_t microsec_start;
	clock_t microsec_end;
#endif


void timer_start(){

	#if defined(OS_LINUX) || defined(OS_MACOSX)
		gettimeofday(&ostime, NULL);
		microsec_start = ((unsigned long long)ostime.tv_sec * 1000000) + ostime.tv_usec;
	#elif defined(OS_WINDOWS)
		microsec_start = clock();
	#endif

}


int main()
{
    int connection = 1;
    int check;
    unsigned char hid_command[64] = {0};
    unsigned char *buffer_rom = NULL;
	int i,j=0;

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

    check = rawhid_open(1, 0x0483, 0x5750, -1, -1);
    if (check <= 0){
        check = rawhid_open(1, 0x0483, 0x5750, -1, -1);
        if (check <= 0){
            printf(" Snes Dumper not found!\n\n");
            return 0;
        }
    }


	j=0;

    printf(" Snes Dumper found! \n");

	buffer_rom = (unsigned char *)malloc(0x200);
    hid_command[0] = WAKEUP;
    rawhid_send(0, hid_command, 64, 30);

  while(connection)
   {
        check = rawhid_recv(0, buffer_rom, 64, 30);
		printf(" %.*s\n", 6, buffer_rom);

        if(check < 0){
            printf("\n Error reading, device went offline\n\n");
            rawhid_close(0);
            free(buffer_rom);
            return 0;
            }
        else{     	
            connection=0;
            }
    }
  
}
