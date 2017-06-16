/*
 * \file MD_Dumper.c
 * \brief MD_Dumper Software for Read/Write Sega Megadrive Cartridge
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

const char * save_msg[] = {	"WRITE SMD save",  //0
							"ERASE SMD save"}; //1

const char * flash_msg[] = {"WRITE SMD flash",  //0
							"ERASE SMD flash"}; //1

const char * menu_msg[] = {	"\n --- DUMP ROM MODE --- \n",
							"\n --- DUMP SAVE MODE ---\n",
							"\n --- WRITE SMD SAVE ---\n",
							"\n --- ERASE SMD SAVE ---\n",
							"\n --- WRITE SMD FLASH ---\n",
							"\n --- ERASE SMD FLASH ---\n",
							"\n --- DUMP SMS ROM ---\n",
							"\n --- CFI MODE ---\n",
							"\n --- INFOS ID ---\n"};


const int eeprom_save_val[] = { 0x1234, 0xFF,  0x03, 0x200001, 0, 0x200001, 0x1, 0x200001, 1, //nba jam
								0x4567, 0xFF,  0x03, 0x200001, 0, 0x200001, 0x0, 0x200000, 0, //nba jam te
								0xACBD, 0xFF,  0x03, 0x200001, 0, 0x200001, 0x0, 0x200000, 0, //nfl quaterback club
								0x1A2B, 0x7FF, 0x07, 0x200001, 0, 0x200001, 0x0, 0x200000, 0, //nfl quaterback club 96
								0x9D79, 0x7F,  0x03, 0x200001, 0, 0x200001, 0x0, 0x200001, 1, //Wonder boy in monster world
								0x0001, 0x7FF, 0x07, 0x200001, 0, 0x200001, 0x0, 0x200000, 0  //nfl quaterback club 96
								}; //need to be completed


const char * eeprom_save_text[] = {	"MODE 2","24C02", //nba jam
									"MODE 2","24C02", //nba jam te
									"MODE 2","24C02", //nfl quaterback club
									"MODE 2","24C02", //nfl quaterback club 96
									"MODE 1","24C01", //Wonder boy in monster world
									"MODE 2","24C02"  //nfl quaterback club 96
									};
								
								
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


int main(){

    int check;
    int choixMenu = 0;
    int connection = 1;
    long i=0, j=0;
    unsigned long address=0;

    unsigned char hid_command[64] = {0};
    unsigned char *buffer_rom = NULL;
    unsigned char *buffer_save = NULL;
    char *game_region = NULL;

    char dump_name[64];
    unsigned char region[5];
    int game_size = 0;
    int checksum_header = 0;
    int checksum_calculated = 0;
    int save_size = 0;
    unsigned long save_address = 0;

    FILE *myfile;

	time_t rawtime;
	time(&rawtime);
	struct tm * timeinfo;
	timeinfo = localtime(&rawtime);	

    printf(" ---------------------------------\n");
    printf(" SEGA MEGADRIVE/GENESIS USB Dumper\n");
    printf(" ---------------------------------\n");
    printf(" 2017/01 ORIGINAL X-DEATH\n");
    printf(" 2017/06 REPROG. ICHIGO\n");
	#if defined(OS_WINDOWS)
	    printf(" WINDOWS");
	#elif defined(OS_MACOSX)
	    printf(" MACOS");
	#elif defined(OS_LINUX)
	    printf(" LINUX");
	#endif
    printf(" ver. 2.0a\n");
	printf(" Compiled %04d/%02d/%02d\n", timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday);
    printf(" www.utlimate-consoles.fr\n\n");
    
    printf(" Detecting MD Dumper... \n");

    check = rawhid_open(1, 0x0483, 0x5750, -1, -1);
    if (check <= 0){
        check = rawhid_open(1, 0x0483, 0x5750, -1, -1);
        if (check <= 0){
            printf(" MD Dumper not found!\n\n");
            return 0;
        }
    }


	j=0;

    printf(" MD Dumper found! \n");

	buffer_rom = (unsigned char *)malloc(0x200);
    hid_command[0] = WAKEUP;
    rawhid_send(0, hid_command, 64, 30);
	
    while(connection){
        check = rawhid_recv(0, buffer_rom, 64, 30);
		printf(" %.*s\n", 6, buffer_rom);
		
        if(check < 0){
            printf("\n Error reading, device went offline\n\n");
            rawhid_close(0);
            free(buffer_rom);
            return 0;
        }else{
        	i = 0;
        	address = 0x80;
        	while(i<8){
	   			hid_command[0] = READ_MD;
	      		hid_command[1] = address&0xFF ;
	   			hid_command[2] = (address&0xFF00)>>8;
	   			hid_command[3] = 0;
	   			hid_command[4] = 0;

		   		rawhid_send(0, hid_command, 64, 100);  
    			rawhid_recv(0, buffer_rom +(64*i), 64, 100);   

   				address+=32; //word
   				i++;
        	}    	
            connection=0;
        }
    }
   	
	//HEADER MD
    if(memcmp((unsigned char *)buffer_rom,"SEGA",4) == 0){
        printf("\n Megadrive/Genesis cartridge detected!\n");

		#if defined(HEADER)
		    for(i=0; i<(256/16); i++){
		        printf("\n");
				printf(" %03lX", 0x100+(i*16));
				for(j=0; j<16; j++){
					printf(" %02X", buffer_rom[j+(i*16)]);
				}
		        printf(" %.*s", 16, buffer_rom +(i*16));
		    }
	        printf("\n");
		#endif

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
	
		memcpy((unsigned char *)region, (unsigned char *)buffer_rom +0xF0, 4);
		
		for(i=0;i<4;i++){
			if(region[i]==0x20){
				game_region = (char *)malloc(i); 
				memcpy((unsigned char *)game_region, (unsigned char *)buffer_rom +0xF0, i);
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
		
	    checksum_header = (buffer_rom[0x8E]<<8) | buffer_rom[0x8F];
	    printf(" Checksum: %X\n", checksum_header);

	    game_size = 1 + ((buffer_rom[0xA4]<<24) | (buffer_rom[0xA5]<<16) | (buffer_rom[0xA6]<<8) | buffer_rom[0xA7])/1024;
	    printf(" Game size: %dKB\n", game_size);

       if((buffer_rom[0xB0] + buffer_rom[0xB1])!=0x93){
            printf(" Save support: No\n");
        }else{
            printf(" Save support: Yes\n");
            switch(buffer_rom[0xB2]){
            	case 0xF8: printf(" Save type: SRAM\n"); break;
            	case 0xE8: printf(" Save type: EEPROM\n"); break;
            }
       		save_size = 1 + ((buffer_rom[0xBA] << 8) | buffer_rom[0xBB])/1024;
        	save_address = (buffer_rom[0xB4]<<24) | (buffer_rom[0xB5]<<16) | (buffer_rom[0xB6] << 8) | buffer_rom[0xB7];
            printf(" Save size: %dKb\n", save_size); save_size /= 8;
            printf(" Save address: %lX\n", save_address);

			if(buffer_rom[0xB2]==0xE8){
				//search checksum
				int search = array_search(checksum_header, eeprom_save_val, 9, sizeof(eeprom_save_val)); 
				if(search){
					printf(" Mode: %s\n", eeprom_save_text[(search/9)*2]);
					printf(" Size mask: %X (%s)\n", eeprom_save_val[search +1], eeprom_save_text[(search/9)*2 +1]);
					printf(" Page mask: %X \n", eeprom_save_val[search +2]);
					printf(" SDA in: %X (bit %d)\n", eeprom_save_val[search +3], eeprom_save_val[search +4]);
					printf(" SDA out: %X (bit %d)\n", eeprom_save_val[search +5], eeprom_save_val[search +6]);
					printf(" SCL: %X (bit %d)\n", eeprom_save_val[search +7], eeprom_save_val[search +8]);
				}else{
					printf(" No information on this game!\n");
				}	
				
			}

        }

//        printf("Region: %c%c%c\n\n", buffer_rom[48], buffer_rom[49], buffer_rom[50]);
//    }else if (memcmp(TMSS_SMS,"TMR SEGA", sizeof(TMSS_SMS)) == 0){
//        printf("Master System/Mark3 cartridge detected !\n");
    }else{
    	//try read at 0x7FF0 for SMS header (on euro/us cart and some japanese)
    	
        
        //try read CFI or manufacturer
        
        printf(" \n Unknown cartridge type\n (erased flash eprom, Sega Mark III game, bad connection,...)\n");
        game_rom = (char *)malloc(sizeof(unk)); 
        game_name = (char *)malloc(sizeof(unk)); 
        game_region = (char *)malloc(4); 
        game_region[3] = '\0';
        memcpy((char *)game_rom, (char *)unk, sizeof(unk));
        memcpy((char *)game_name, (char *)unk, sizeof(unk));
        memcpy((char *)game_region, (char *)unk, 3);
    }


    printf("\n --- MENU ---\n");
    printf(" 1) Dump SMD ROM\n");				//MD
    printf(" 2) Dump SMD Save\n");				//MD
    printf(" 3) Write SMD Save\n");				//MD
    printf(" 4) Erase SMD Save\n");				//MD
    printf(" 5) Write SMD Flash (16bits)\n");	//MD
    printf(" 6) Erase SMD Flash\n");			//MD   
    printf(" 7) Dump SMS ROM (SEGA mapper only)\n");				//SMS
    printf(" 8) CFI infos\n");					//CHIP
    printf(" 9) Vendor/ID infos\n");			//CHIP
    printf(" 10) Exit\n");
    printf(" Your choice: ");
    scanf("%d", &choixMenu);
    
    if(choixMenu < 1 || choixMenu >9){
    	choixMenu = 0xFF; //free buffer etc.	
    	}else{    
    	printf("%s", menu_msg[(choixMenu-1)]); //menu's name
    }
    switch(choixMenu){
    
    case 1:  // READ SMD ROM

        choixMenu=0;
        printf(" 1) Auto (from header)\n");
        printf(" 2) Manual\n");
		printf(" Your choice: ");
        scanf("%d", &choixMenu);
		if(choixMenu==2){
            printf(" Enter number of KB to dump: ");
            scanf("%d", &game_size);
        }
        if(choixMenu>2){
        	printf(" Wrong number! \n\n");
        	free(game_name);
        	free(game_rom);
        	free(game_region);
        	rawhid_close(0);
	        return 0;	
        }

        address=0;
        game_size *= 1024;       
        buffer_rom = (unsigned char*)malloc(game_size);
	
		
     	hid_command[0] = READ_MD;
        hid_command[1] = address&0xFF; //start @ 0x0
        hid_command[2] = (address & 0xFF00)>>8;
        hid_command[3] = (address & 0xFF0000)>>16;
        hid_command[4] = 1;

		timer_start();
		
	    rawhid_send(0, hid_command, 64, 60);
	    
      	while(address < (game_size/2)){     		

  			rawhid_recv(0, (buffer_rom +(address*2)), 64, 60);  
 			address += 32;
 			
  	       	printf("\r ROM dump in progress: %ld%% (adr: 0x%lX)", ((100 * address)/(game_size/2)), (address*2));
           	fflush(stdout);
      	} 
      	
      	hid_command[0] = 0xFF; //stop dumping
	    rawhid_send(0, hid_command, 64, 60);
  		rawhid_recv(0, hid_command, 64, 60);  

		check = 1;
   		for(i=0x200; i<game_size; i++){ // declared rom size
   			if(check){
   				checksum_calculated += buffer_rom[i]*256;
            	check = 0;
   				}else{
            	checksum_calculated += buffer_rom[i];
            	check = 1;
   			}
   		}
   		
   		printf("\n Checksum (recalc. from dump): %X", (checksum_calculated & 0xFFFF));
   		if(checksum_header == (checksum_calculated&0xFFFF)){ printf(" (good)");}else{ printf(" (!BAD!)");}	

        rawhid_close(0);
//        sprintf(game_rom, "%s_(%s).bin", game_rom, game_region); // problem on PC, files too long seems to hang?
        myfile = fopen("dump_smd.bin","wb");
        fwrite(buffer_rom, 1, game_size, myfile);
        fclose(myfile);
        break;

    case 2:  // READ SMD SRAM
           
        choixMenu=0;
        printf(" 1) Auto (from header)\n");
        printf(" 2) Manual 64kb/8KB\n");
        printf(" 3) Manual 256kb/32KB\n");
        printf(" Your choice: ");
        scanf("%d", &choixMenu);

        if(choixMenu>3){
        	printf(" Wrong number!\n\n");
        	free(game_name);
        	free(game_rom);
        	free(game_region);
        	rawhid_close(0);
	        return 0;	
        }  

		if(choixMenu>1){
        	printf(" Enter save address (ex. 200001): ");
        	scanf("%lX", &save_address);
		}
                    
		switch(choixMenu){
			case 1:  save_size *= 1024;  break;
			case 2:  save_size = 8192;  break;
			case 3:  save_size = 32768; break;
			default: save_size = 8192;  
		}

        buffer_rom = (unsigned char*)malloc(0x10000); //default SRM size for emu 

		timer_start();
		
        hid_command[0] = READ_MD_SAVE; // Select Read in 8bit Mode
		address = (save_address/2);	
		i=0;

        while(i<save_size){

            hid_command[1] = address & 0xFF;
            hid_command[2] = (address & 0xFF00)>>8;
            hid_command[3] = (address & 0xFF0000)>>16;
            
            rawhid_send(0, hid_command, 64, 30);
            rawhid_recv(0, (buffer_rom+i), 64, 30);
            address +=64; //next adr
            i+=64;
            
            printf("\r SAVE dump in progress: %ld%%", ((100 * i)/save_size));
            fflush(stdout);
        }
        for(i=save_size;i<0x10000;i++){
        	buffer_rom[i] = 0xFF;	
        }
        
        rawhid_close(0);
//        sprintf(game_rom, "%s_(%s).srm", game_rom, game_region);
        myfile = fopen("dump_smd.srm","wb");
        fwrite(buffer_rom, 1, 0x10000, myfile);
        fclose(myfile);
        break;
    
    case 3:  // WRITE SRAM
    case 4:  // ERASE SRAM
		//write -> load file
		//erase full of 0xFF
		save_size *= 1024; //in KB

        buffer_save = (unsigned char*)malloc(save_size); //default SRM size
		
      	if(choixMenu == 3){
	       	printf(" Save file: ");
	        scanf("%60s", dump_name);
			myfile = fopen(dump_name,"rb");
			
		    if(myfile == NULL){
		    	printf(" Save file %s not found!\n Exit\n\n", dump_name);
		       	free(game_name);
		       	free(game_rom);
		       	free(game_region);
		       	free(buffer_save);
		        rawhid_close(0);
		        return 0;
		    }
		    
		    fread(buffer_save, 1, save_size, myfile);
			fclose(myfile);
			
       	}else{		//clean buffer with 0xFF (erase)
			for(i=0;i<save_size;i++){
				buffer_save[i] = 0xFF;
			}
        }

		checksum_header = 0;
		for(i=0;i<save_size;i++){
			checksum_header += buffer_save[i]; //checksum
		}
       	
		//1st BACKUP SRAM (just in case...)
        buffer_rom = (unsigned char*)malloc(0x10000);
		address = (save_address/2);	
     		
		i=0;
        while(i<save_size){
			
			hid_command[0] = READ_MD_SAVE; // Select Read in 8bit Mode
            hid_command[1] = address & 0xFF;
            hid_command[2] = (address & 0xFF00)>>8;
            hid_command[3] = (address & 0xFF0000)>>16;
            
            rawhid_send(0, hid_command, 64, 30);
            rawhid_recv(0, (buffer_rom+i), 64, 30);
            
            address +=64; //next adr
            i+=64;
            
            printf("\r BACKUP save in progress: %ld%%", ((100 * i)/save_size));
            fflush(stdout);
        }

		while(i<0x10000){ buffer_rom[i++] = 0xFF; } //fill with 0xFF until 64KB
         
//        sprintf(game_rom, "%s_(%s).srm.bak", game_rom, game_region);
        myfile = fopen("dump_smd.srm.bak","wb");
        fwrite(buffer_rom, 1, 0x10000, myfile);
        fclose(myfile);       	

		timer_start();

      	printf("\n");
 		address = (save_address/2);	
 		
		int checksum_send = 0;
		int checksum_recv = 0;

		i=0;
  	   	while(i<save_size){

	     	hid_command[0] = WRITE_MD_SAVE; // Select write in 8bit Mode
	  	    hid_command[1] = address & 0xFF;
			hid_command[2] = (address & 0xFF00)>>8;
	  	    hid_command[3] = (address & 0xFF0000)>>16;

	  	    if((save_size - i)<59){
	 	    	hid_command[4] = (save_size - i); //last packet	    
	  	    	}else{
	  	    	hid_command[4] = 59; //48 bytes
	  	    }

			memcpy((unsigned char *)hid_command +5, (unsigned char *)buffer_save +i, hid_command[4]);
            checksum_send += checksum(buffer_save, i, hid_command[4]); //ok
            
            rawhid_send(0, hid_command, 64, 60); //send write
            rawhid_recv(0, buffer_rom, 64, 60);  //return read
            checksum_recv += ((buffer_rom[1]<<8) | buffer_rom[0]); //ko
                      
            if(checksum_send != checksum_recv){
            	//msg error
            	printf("\n ERROR while programming!\n Address: 0x%lX send:%X recv:%x", address, checksum_send, checksum_recv);
   	            fflush(stdout);
            	break;
            }

            i += hid_command[4];
            address += hid_command[4];
                       
            printf("\r %s in progress: %ld%%", save_msg[(choixMenu - 3)], ((100 * i)/save_size));
            fflush(stdout);
        }
        
        free(buffer_save);
        
        break;
        
        
 	case 5: //WRITE FLASH x16
 	
       	printf(" ROM file: ");
        scanf("%60s", dump_name);
		myfile = fopen(dump_name,"rb");
	    
	    if(myfile == NULL){
	    	printf(" ROM file %s not found!\n Exit\n\n", dump_name);
	    	fclose(myfile);
	       	free(game_name);
	       	free(game_rom);
	       	free(game_region);
	        rawhid_close(0);
	        return 0;
	    }
	    
  		fseek(myfile,0,SEEK_END); 
   		game_size = ftell(myfile); 
	    buffer_rom = (unsigned char*)malloc(game_size); 
	    buffer_save = (unsigned char*)malloc(64); 
  		rewind(myfile); 
	    fread(buffer_rom, 1, game_size, myfile);
		fclose(myfile);

		i=0;
		address = 0;
		
		timer_start();

  	   	while(i<game_size){

	     	hid_command[0] = WRITE_MD_FLASH; // Select write in 16bits Mode
	  	    hid_command[1] = address & 0xFF;
			hid_command[2] = (address & 0xFF00)>>8;
	  	    hid_command[3] = (address & 0xFF0000)>>16;

	  	    if((game_size - i)<58){
	 	    	hid_command[4] = (game_size - i); //adjust last packet	    
	  	    	}else{
	  	    	hid_command[4] = 58; //max 58 bytes - must by pair (word)
	  	    }

			memcpy((unsigned char *)hid_command +5, (unsigned char *)buffer_rom +i, hid_command[4]);
            rawhid_send(0, hid_command, 64, 60); //send write
	
	        i += hid_command[4];
            address += (hid_command[4]>>1);

            printf("\r WRITE SMD flash in progress: %ld%%", ((100 * i)/game_size));
            fflush(stdout);
            
         }
        break;
 	

 	case 6: //ERASE FLASH x16

		    buffer_rom = (unsigned char*)malloc(64);
	     	hid_command[0] = ERASE_MD_FLASH; // Select write in 8bit Mode

			timer_start();
			 
            rawhid_send(0, hid_command, 64, 60); //send write
            
            i=0;
            while(buffer_rom[0]!=0xFF){
            	rawhid_recv(0, buffer_rom, 64, 100);  //wait status
		        printf("\r ERASE SMD flash in progress: %s ", wheel[i]);
		        fflush(stdout);
		        i++;
		        if(i==4){i=0;}
            }
                       
            printf("\r ERASE SMD flash in progress: 100%%");
            fflush(stdout);

		break;
		
		
	case 7: //SMS READ
        printf(" Enter number of KB to dump: ");
        scanf("%d", &game_size);

        address=0;
        game_size *= 1024;       
        buffer_rom = (unsigned char*)malloc(game_size);		

		/*
		only SEGA MAPPER !
		0xFFFD slot0
		0xFFFE slot1
		0xFFFF slot2
		*/
        hid_command[0] = READ_SMS;
	    hid_command[4] = 1; //dump_running

		timer_start();
		
	    rawhid_send(0, hid_command, 64, 60);
		
		while(address<game_size){
        	       	       	
	        	/*
	        	if(address < 0x4000){
	       			slotRegister = 0xFFFD; 	// slot 0 sega
	        		slotAdr = address;     	       		
	        	}else if(address < 0x8000){
	       			slotRegister = 0xFFFE; 	// slot 1 sega
	        		slotAdr = address;     	       		
	        	}else{
	       			slotRegister = 0xFFFF; 	// slot 2 sega
	        		slotAdr = 0x8000 + (address & 0x3FFF);     	
	        	}
	        	*/        	             
	            rawhid_recv(0, (buffer_rom +address), 64, 100);
           		address += 64;

           	 	printf("\r ROM Dump in progress: %lu%%", ((100 * address)/game_size));
            	fflush(stdout);
        }
        
      	hid_command[0] = 0xFF; //stop dumping
	    rawhid_send(0, hid_command, 64, 60);
  		rawhid_recv(0, hid_command, 64, 60); 
  		 
  		rawhid_close(0);
        myfile = fopen("dump_sms.sms","wb");
        fwrite(buffer_rom, 1, game_size, myfile);
        fclose(myfile); 
  		 		
		break;
		
		
	case 8: //CFI mode
			buffer_rom = (unsigned char*)malloc(64);
	    	hid_command[0] = CFI_MODE; 

			timer_start();

	    	rawhid_send(0, hid_command, 64, 60);
          	rawhid_recv(0, buffer_rom, 64, 60);

			if(memcmp((unsigned char *)buffer_rom,"QRY",3) == 0){
				printf(" CFI Compatible chip found!\n");	
				printf(" Size: %dKB\n", (int)pow(2, buffer_rom[5])/1024);	
				printf(" Type: %s\n", flash_device_description[buffer_rom[10]]);	

				int search = array_search(buffer_rom[3], flash_algo, 1, sizeof(flash_algo));	
				printf(" Algorithm command set: %s\n", flash_algo_msg[search]);	

				printf(" VCC min: %d.%dv\n", (buffer_rom[6]>>4), (buffer_rom[6]&0xF));	
				printf(" VCC max: %d.%dv\n", (buffer_rom[7]>>4), (buffer_rom[7]&0xF));	
				printf(" Timeout block erase: %ds\n", (int)pow(2, buffer_rom[8])/1000);	
				if(buffer_rom[9]!=0){
					printf(" Timeout chip erase: %ds\n", (int)pow(2, buffer_rom[9])/1000);	
					}else{
					printf(" Timeout chip erase: not indicate\n");	
				}
			}else{
				printf(" CFI mode not supported\n");	
			}
		break;
		
	 case 9:
	 //ID/manu
	 		buffer_rom = (unsigned char*)malloc(64);
	    	hid_command[0] = INFOS_ID;

			timer_start();

	    	rawhid_send(0, hid_command, 64, 60);
          	rawhid_recv(0, buffer_rom, 64, 60);
			printf(" Manufacturer: %02Xh\n", buffer_rom[1]);	
			printf(" Device ID: %02Xh\n",buffer_rom[3]);	
			
			/*
			man: 01 = AMD
			dev: 49 = 29LV160DB
			
			SEGA 	00h 00h
			SEGA 	FFh 00h
			SEGA 	FFh C0h
			WM	 	FFh 00h	
			epromUV	FFh 00h	
			
			man FF = false
			man 00 & id=00 = false
			
			*/

//			if(buffer_rom[9]!=0){
//				printf(" Manufacturer: %Xh\n", buffer_rom[0]);	
//				}else{
//				printf(" Device ID: %Xh\n",buffer_rom[1]);	
//			}

	 break;

 	
	default:
       	free(game_name);
       	free(game_rom);
       	free(game_region);
        rawhid_close(0);
   		return 0;
    }
    
	#if defined(OS_LINUX) || defined(OS_MACOSX)
		gettimeofday(&ostime, NULL);
		microsec_end = ((unsigned long long)ostime.tv_sec * 1000000) + ostime.tv_usec;
		printf("\n Elapsed time: %lds", (microsec_end - microsec_start)/1000000);
		printf(" (%ldms)\n\n", (microsec_end - microsec_start)/1000);
		#elif defined(OS_WINDOWS)
		microsec_end = clock();
		printf("\n Elapsed time: %lds", (microsec_end - microsec_start)/1000);
		printf(" (%ldms)\n\n", (microsec_end - microsec_start));
	#endif


	#if defined(DEBUG)
	    for(i=0; i<(64/16); i++){
	        printf("\n");
			for(j=0; j<16; j++){
				printf(" %02X", buffer_rom[j+(i*16)]);
			}
	        printf(" %.*s", 16, buffer_rom +(i*16));
	    }
		printf("\n");
	#endif

    free(game_name);
    free(game_rom);
    free(game_region);
    free(buffer_rom);
    return 0;
}




