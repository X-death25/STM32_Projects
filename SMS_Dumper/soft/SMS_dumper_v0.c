/*
	V0a	12/05/2017
	Sega Mapper
	Dump only
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>

#elif defined(OS_WINDOWS)
#include <conio.h>
#endif

#include "hid.h" // HID Lib

// HID Special Command
#define SMS_WAKEUP    		0x11  // WakeUP STM32
#define SMS_READ      		0x12  // Read
#define SMS_WRITE     		0x13  // Write
#define SMS_WRITE_FLASH     0x14  // Write Flash
#define SMS_ERASE_FLASH     0x15  // Erase Flash
//#define SMS_FLASH_ID     	0x16  // Chip ID

//Prototype
unsigned int check_buffer(unsigned long adr, unsigned char * buf, unsigned char length);

int main(){
    // Program Var
   	int connect;
   	int check = 1;
	unsigned int verif = 0; 
    unsigned int choixMenu = 0;
    unsigned int choixMapperType = 0;
    unsigned int choixSousMenu = 0;
    unsigned long int buffer_checksum[0xFF] = {0};
//    unsigned long int buffer_checksum_flash[0xFF] = {0};

    // HID Command Buffer & Var
    unsigned char HIDcommand[64];
    unsigned char buffer_recv[64];
    unsigned char *buffer_rom = NULL;
    unsigned char *buffer_flash = NULL;
    
    char gameName[36];
    unsigned long address = 0;
    unsigned long gameSize = 0;
    unsigned long totalGameSize = 0;
    
    unsigned int slotAdr = 0;
    unsigned int slotSize = 0;
    unsigned int slotRegister = 0;

  	struct timeval time;
    long microsec_start = 0;
    long microsec_end = 0;

	unsigned long int i=0, checksum_rom=0, checksum_flash=0;
	unsigned char page=0;
		
    // Output File
    FILE *myfile = NULL;

    printf("\n*** Sega MASTER SYSTEM,");
    printf("\n*** Mark III and SG1000");
    printf("\n*** USB Dumper");
    printf("\n*** v0a 2017/05/12 - ichigo\n");
    printf("-------------------------\n");
    printf(" Detecting... \n");

    connect = rawhid_open(1, 0x0483, 0x5750, -1, -1);
    if(connect <= 0){
        connect = rawhid_open(1, 0x0483, 0x5750, -1, -1);
        if(connect <= 0){
            printf(" SMS Dumper NOT FOUND!\n Exit...\n\n");
            return 0;
        }
    }
	
    printf(" SMS Dumper detected!\n");
    
    HIDcommand[0] = SMS_WAKEUP;
    rawhid_send(0, HIDcommand, 64, 15);

    while(check){
        connect = rawhid_recv(0, buffer_recv, 64, 15);
        if(buffer_recv[0]!=0xFF){
 		    printf("-------------------------\n");
           printf(" Error reading\n Exit\n\n");
            rawhid_close(0);
            return 0;
        }else{
            printf(" and ready!\n");
           check = 0;
           buffer_recv[0] = 0;
        }
    }

    printf("\n----------MENU-----------\n");
    printf(" 1. Dump ROM\n");
    printf(" 2. Dump S-RAM\n");
    printf(" 3. Write/Erase S-RAM\n");
    printf(" 4. Write FLASH 39SFxxx (max. 512KB)\n");
    printf(" 5. Exit\n");
    printf(" Enter your choice: ");
    scanf("%d", &choixMenu);
    
    switch(choixMenu){
    	
    case 1:  // DUMP ROM
	    printf("\n-------MAPPER TYPE-------\n");
   		printf(" 1. SEGA\n");
	    printf(" 2. Codemasters\n");
	    printf(" 3. Korean\n");
	    printf(" 4. Korean MSX 8kb\n");
	    printf(" 5. Exit\n");
	    printf(" Enter your choice: ");
	    scanf("%d", &choixMapperType);
   	
   		if(choixMapperType!=5){
   		
        printf(" Size to dump in KB: ");
        scanf("%ld", &gameSize);
        printf(" Rom's name (no space/no ext): ");
        scanf("%32s", gameName);

   		printf("-------------------------\n");
        HIDcommand[0] = SMS_READ;
		totalGameSize = gameSize*1024;
		
        buffer_rom = (unsigned char*)malloc(totalGameSize);

	    gettimeofday(&time, NULL);
		microsec_start = ((unsigned long long)time.tv_sec * 1000000) + time.tv_usec;
       
		//only for testing / debug - ONLY SEGA
        switch(choixMapperType){
        	case 1: 
        		strcat(gameName, "_SEGA.sms"); 
        		HIDcommand[7] = 0; //SEGA mapper
 			   	slotSize = 16384; //16ko - 0x4000
	       		break;
        	case 2: 
        		strcat(gameName, "_CODEMASTERS.sms"); 
        		HIDcommand[7] = 1; //Codemasters mapper
  			   	slotSize = 16384; //16ko - 0x4000
	      		break;
        	default: 
        		strcat(gameName, "_UNKNOWN.bin"); 
        		HIDcommand[7] = 0;
        		break;
        }
   	       
        while(address < totalGameSize){
        	/*
        	sega mapper
        	dump 16ko per page = 256 * 64bytes blocs
			max 256 pages (0xFF) 32mbits
        	
        	slot0 16k - 0x0000 to 0x3FFF	A14=0 	A15=0
        	slot1 16k - 0x4000 to 0x7FFF 	A14=1 	A15=0
        	slot2 16k - 0x8000 to 0xBFFF 	A14=0	A15=1
        	*/
       	       	       	
 	        if(choixMapperType == 1 || choixMapperType == 2){ 	//SEGA or Codemasters
 	        	if(address<0x4000){
 	        		slotAdr = address;
 	        		if(choixMapperType==2){
	 	        		slotRegister = 0;		// slot 0 codemasters
 	        			}else{
 	        			slotRegister = 0xFFFD; 	// slot 0 sega
 	        		}
 	        	}else if(address<0x8000){
 	        		slotAdr = address;
 	        		if(choixMapperType==2){
	 	        		slotRegister = 0x4000;	// slot 1 codemasters
 	        			}else{
 	        			slotRegister = 0xFFFE; 	// slot 1 sega
 	        		}
 	        	}else if(address>0x7FFF){
 	        		if(choixMapperType==2){
	 	        		slotRegister = 0x8000;	// slot 2 codemasters	
 	        			}else{
 	        			slotRegister = 0xFFFF; 	// slot 2 sega
 	        		}
		        	slotAdr = 0x8000 + (address & 0x3FFF);     	
		        }
       			HIDcommand[3] = 1; 								//enable MAPPER
		        HIDcommand[4] = (address/slotSize); 			//page MAPPER
		        HIDcommand[5] = (slotRegister & 0xFF); 			//reg MAPPER lo adr
		        HIDcommand[6] = (slotRegister & 0xFF00)>>8; 	//reg MAPPER hi adr
	        
	        }else if(choixMapperType == 3){						//KOREAN
	        	 if(address>0x7FFF){
	 	        	slotRegister = 0xA000;	
		        	slotAdr = 0x8000 + (address & 0x3FFF);     	
       				HIDcommand[3] = 1; 								//enable MAPPER
		        	HIDcommand[4] = (address/slotSize); 			//page MAPPER
		        	HIDcommand[5] = (slotRegister & 0xFF); 			//reg MAPPER lo adr
		        	HIDcommand[6] = (slotRegister & 0xFF00)>>8; 	//reg MAPPER hi adr
	        	}else{
	        		slotAdr = address;	
	        	}
	        }else{
	        	slotAdr = address;								//32Ko
	        }	
 	        
            HIDcommand[1] = slotAdr & 0xFF;			//lo adr
            HIDcommand[2] = (slotAdr & 0xFF00)>>8; 	//hi adr
                    
            rawhid_send(0, HIDcommand, 64, 15);
            rawhid_recv(0, (buffer_rom +address), 64, 15);

            address += 64 ;

            printf("\r ROM dump in progress: %lu%%", ((100 * address)/totalGameSize));
            fflush(stdout);

        }
	 	gettimeofday(&time, NULL);
		microsec_end = ((unsigned long long)time.tv_sec * 1000000) + time.tv_usec;

   		printf("\n-------------------------\n");
        printf(" ROM dump complete! \n");
		myfile = fopen(gameName,"wb"); //extension en fontcion du support ? .sms, .sg, .bin (default)
	    fwrite(buffer_rom, 1, totalGameSize, myfile);
	    fclose(myfile);
   		
   		}else{
   		
   		return 0; //exit	
   		
   		}
        break;	
    
    
    case 2: //DUMP S/F-RAM
    	printf("\n-------DUMP S-RAM--------\n");
       	printf(" Sram's name (no space/no ext): ");
        scanf("%32s", gameName);
   		printf("-------------------------\n");

        HIDcommand[0] = SMS_READ;
       
		totalGameSize = 8192; //set as 8ko
        
        buffer_rom = (unsigned char*)malloc(totalGameSize);
	    gettimeofday(&time, NULL);
		microsec_start = ((unsigned long long)time.tv_sec * 1000000) + time.tv_usec;
       
        strcat(gameName, "_SRAM.bin"); 
 		slotSize = 8192;
 		slotRegister = 0xFFFC; 	// slot 2 sega
   
        while(address < totalGameSize){

		    slotAdr = 0x8000 + (address & 0x3FFF);     	
       		HIDcommand[3] = 1; 								//enable MAPPER
		    HIDcommand[4] = 8;					 			//bit3 = RAM enable ($8000-$bfff)
		    HIDcommand[5] = (slotRegister & 0xFF); 			//reg MAPPER lo adr
		    HIDcommand[6] = (slotRegister & 0xFF00)>>8; 	//reg MAPPER hi adr
 	        
            HIDcommand[1] = slotAdr & 0xFF;					//lo adr
            HIDcommand[2] = (slotAdr & 0xFF00)>>8; 			//hi adr
                    
            rawhid_send(0, HIDcommand, 64, 15);
            rawhid_recv(0, (buffer_rom +address), 64, 15);

            address += 64 ;

            printf("\r SRAM dump in progress: %lu%%", ((100 * address)/totalGameSize));
            fflush(stdout);
        }
        
	 	gettimeofday(&time, NULL);
		microsec_end = ((unsigned long long)time.tv_sec * 1000000) + time.tv_usec;

   		printf("\n-------------------------\n");
        printf(" SRAM dump complete! \n");
		myfile = fopen(gameName,"wb"); //extension en fontcion du support ? .sms, .sg, .bin (default)
	    fwrite(buffer_rom, 1, totalGameSize, myfile);
	    fclose(myfile);
    	break;
 
	case 3: //WRITE S/F-RAM
        HIDcommand[0] = SMS_WRITE;
   		totalGameSize = 8192;
        slotRegister = 0xFFFC;  
        buffer_rom = (unsigned char*)malloc(totalGameSize);
		
     	printf("\n-------WRITE S-RAM--------\n");
   		printf(" 1. Write file\n");
	    printf(" 2. Erase\n");
	    printf(" 3. Exit\n");
	    printf(" Enter your choice: ");
	    scanf("%d", &choixSousMenu);

       	if(choixSousMenu == 1){
	       	printf(" SRAM file : ");
	        scanf("%32s", gameName);
			myfile = fopen(gameName,"rb");
		    if(myfile == NULL){
		    	printf(" FLASH file %s not found !\n Exit\n\n", gameName);
		        return 0;
		    }
		    fread(buffer_rom, 1, totalGameSize, myfile);
			fclose(myfile);
       	}else if(choixSousMenu == 2){
			for(unsigned int i=0; i<totalGameSize; i++){
				buffer_rom[i] = 0xFF;
			}
      	   	printf(" Erase SRAM\n");
        }else{
        printf("\n");
		return 0;        	
        }
        printf("-------------------------\n");


	    gettimeofday(&time, NULL);
		microsec_start = ((unsigned long long)time.tv_sec * 1000000) + time.tv_usec;
       
   		//0 to 0x4000 - 256 packets 32bytes to send
        while(address < totalGameSize){
        	
		    slotAdr = 0x8000 +address;     	
            HIDcommand[1] = slotAdr & 0xFF;					//lo adr
            HIDcommand[2] = (slotAdr & 0xFF00)>>8; 			//hi adr
 		    HIDcommand[4] = 8;					 			//bit3 = RAM enable ($8000-$bfff)
		    HIDcommand[5] = (slotRegister & 0xFF); 			//reg MAPPER lo adr
		    HIDcommand[6] = (slotRegister & 0xFF00)>>8; 	//reg MAPPER hi adr
			memcpy((unsigned char *)HIDcommand +7, (unsigned char *)buffer_rom +address, 32);
             
            rawhid_send(0, HIDcommand, 64, 15);
            rawhid_recv(0, buffer_recv, 64, 15); //wait to continue
            
            address += 32;

            printf("\r SRAM write in progress: %lu%%", ((100 * address)/totalGameSize));
            fflush(stdout);
        }
        
	 	gettimeofday(&time, NULL);
		microsec_end = ((unsigned long long)time.tv_sec * 1000000) + time.tv_usec;

   		printf("\n-------------------------\n");
        printf(" SRAM write complete! \n");

    	break;

	case 4: //WRITE FLASH
		
        printf("\n-------WRITE FLASH-------\n");
	    printf(" FILE to write: ");
	    scanf("%32s", gameName);

		myfile = fopen(gameName,"rb");

	    if(myfile == NULL){
	    	printf(" FLASH file %s not found !\n Exit\n\n", gameName);
	        return 0;
	    }
	    fseek(myfile, 0, SEEK_END);
	    totalGameSize = ftell(myfile);
	    rewind(myfile);
	    
	    if(totalGameSize>524288){
	    	printf(" FLASH file too big!\n Max 524288 bytes (file : %lu)\nExit\n\n", totalGameSize);
	        return 0;	    	
	    }
	    
	    buffer_flash = (unsigned char*)malloc(totalGameSize);
	    
	    fread(buffer_flash, 1, totalGameSize, myfile);
		fclose(myfile);
	    
	    printf(" FILESIZE to write: %luKB\n", (totalGameSize/1024));
		
		//1st erase chip !
		//ERASE CHIP (2 TIMES) 
		printf(" FLASH erase:");
		fflush(stdout);
		verif = 0;
	    buffer_rom = (unsigned char*)malloc(64); //temp for verif

 		while(verif != 0x3FC0){     	
	      	HIDcommand[0] = SMS_READ; //set mapper page 0
	        HIDcommand[1] = 0;
	        HIDcommand[2] = 0;
	      	HIDcommand[3] = 1;
		    HIDcommand[4] = 0;
		    HIDcommand[5] = 0xFD;
		    HIDcommand[6] = 0xFF;
		    verif = 0;
	        rawhid_send(0, HIDcommand, 64, 15);
	        rawhid_recv(0, buffer_rom, 64, 15);
			verif = check_buffer(0, (unsigned char *)buffer_rom, 64);
			
	      	HIDcommand[0] = SMS_READ; //set mapper page 1
	        HIDcommand[1] = 0x00;
	        HIDcommand[2] = 0x40;
	      	HIDcommand[3] = 1;
		    HIDcommand[4] = 1;
		    HIDcommand[5] = 0xFE;
		    HIDcommand[6] = 0xFF;
		    verif = 0;
	        rawhid_send(0, HIDcommand, 64, 15);
	        rawhid_recv(0, buffer_rom, 64, 15);
			verif = check_buffer(0, (unsigned char *)buffer_rom, 64);
			
	      	HIDcommand[0] = SMS_READ; //set mapper page 2
	        HIDcommand[1] = 0x00;
	        HIDcommand[2] = 0x80;
	      	HIDcommand[3] = 1;
		    HIDcommand[4] = 2;
		    HIDcommand[5] = 0xFF;
		    HIDcommand[6] = 0xFF;
		    verif = 0;
	        rawhid_send(0, HIDcommand, 64, 15);
	        rawhid_recv(0, buffer_rom, 64, 15);
			verif = check_buffer(0, (unsigned char *)buffer_rom, 64);
	
		    connect = 0;
			HIDcommand[0] = SMS_ERASE_FLASH;
	        rawhid_send(0, HIDcommand, 64, 15);
	        while(connect < 0xFF){
		    	rawhid_recv(0, buffer_recv, 64, 15);
		    	connect = buffer_recv[0];
	        }
 		}
	    printf(" 100%%\n");
        fflush(stdout);
		free(buffer_rom);

		HIDcommand[0] = SMS_WRITE_FLASH;
		HIDcommand[3] = 1; //enable MAPPER			        
		slotSize = 16384; //16ko - 0x4000
		address = 0;
		verif = 0;    

	    gettimeofday(&time, NULL);
		microsec_start = ((unsigned long long)time.tv_sec * 1000000) + time.tv_usec;
		
		while(address<totalGameSize){
        	
        	verif = check_buffer((unsigned long)address, (unsigned char *)buffer_flash, 32);
        	
        	if(check<0x1FE0){ //skip if 32 bytes full of 0xFF
        	
	        	if(address < 0x4000){
	       			slotRegister = 0xFFFD; 	// slot 0 sega
	        		slotAdr = address;     	       		
	        	}else if(address < 0x8000){
	       			slotRegister = 0xFFFE; 	// slot 1 sega
	        		slotAdr = address;     	       		
	        	}else{
	        		//sup à 0x8000
	       			slotRegister = 0xFFFF; 	// slot 2 sega
	        		slotAdr = 0x8000 + (address & 0x3FFF);     	
	        	}

	            HIDcommand[1] = slotAdr & 0xFF;					//lo adr
	            HIDcommand[2] = (slotAdr & 0xFF00)>>8; 			//hi adr
		        HIDcommand[4] = (address/slotSize); 			//page MAPPER
		       	HIDcommand[5] = (slotRegister & 0xFF); 			//reg MAPPER lo adr
		        HIDcommand[6] = (slotRegister & 0xFF00)>>8; 	//reg MAPPER hi adr
		        	
				memcpy((unsigned char *)HIDcommand +7, (unsigned char *)buffer_flash +address, 32);
	             
	            rawhid_send(0, HIDcommand, 64, 15);
	            rawhid_recv(0, buffer_recv, 64, 15);
        	}
            address += 32;
            printf("\r FLASH write: %lu%%", ((100 * address)/totalGameSize));
            fflush(stdout);
        }

        printf("\n");

		buffer_rom = (unsigned char*)malloc(totalGameSize);			        
      	HIDcommand[0] = SMS_READ;
      	HIDcommand[3] = 1; //enable MAPPER
		address = 0;

		while(address<totalGameSize){

        	if(address < 0x4000){
       			slotRegister = 0xFFFD; 	// slot 0 sega
        		slotAdr = address;     	       		
        	}else if(address < 0x8000){
       			slotRegister = 0xFFFE; 	// slot 1 sega
        		slotAdr = address;     	       		
        	}else{
        		//sup à 0x7FFF
       			slotRegister = 0xFFFF; 	// slot 2 sega
        		slotAdr = 0x8000 + (address & 0x3FFF);     	
        	}
        		
            HIDcommand[1] = slotAdr & 0xFF;			//lo adr
            HIDcommand[2] = (slotAdr & 0xFF00)>>8; 	//hi adr
	        HIDcommand[4] = (address/slotSize); 			//page MAPPER
	       	HIDcommand[5] = (slotRegister & 0xFF); 			//reg MAPPER lo adr
	        HIDcommand[6] = (slotRegister & 0xFF00)>>8; 	//reg MAPPER hi adr

            rawhid_send(0, HIDcommand, 64, 15);
            rawhid_recv(0, (buffer_rom +address), 64, 15);
            address += 64 ;
            printf("\r FLASH verif: %lu%%", ((100 * address)/totalGameSize));
            fflush(stdout);
        }
        
	 	gettimeofday(&time, NULL);
		microsec_end = ((unsigned long long)time.tv_sec * 1000000) + time.tv_usec;

		printf("\n-------------------------\n");

		while(page<(totalGameSize/slotSize)){
			checksum_rom = 0;
			checksum_flash = 0;
			while(i<(slotSize*(page+1))){
				checksum_rom += buffer_rom[i];
				checksum_flash += buffer_flash[i];
				i++;
			}
			printf(" Page %03d: ", page);
			printf("file 0x%06lX", checksum_flash);
			printf(" - flash 0x%06lX", checksum_rom);
			if(checksum_rom!= checksum_flash){ printf(" !BAD!\n");}else{printf(" (good)\n");}
			page++;
		}
		printf("-------------------------\n");
	 	
    	break;

/*		
	case 6: //CHIP ID
        printf("\n--------FLASH ID----------\n");
		HIDcommand[0] = SMS_FLASH_ID;
	    buffer_rom = (unsigned char*)malloc(64);
        rawhid_send(0, HIDcommand, 64, 15);
       	rawhid_recv(0, buffer_rom, 64, 15); //ok
	    printf(" Chip ID[0]: 0x%02X", buffer_rom[0]);
	    printf(" ID[1]: 0x%02X", buffer_rom[1]);
	    printf(" ID[2]: 0x%02X\n\n", buffer_rom[2]);
	    rawhid_close(0);
	    return 0;
	break;
*/    
    default:
	    rawhid_close(0);
        return 0; 
    }
    
    rawhid_close(0);
    printf(" Time: %lds", (microsec_end - microsec_start)/1000000);
    printf(" (%ldms)\n", (microsec_end - microsec_start)/1000);
	printf("-------------------------\n");
	/*
	checksum each page 
	*/
	if(choixMenu<3){
		unsigned long int i=0, checksum_rom=0;
		unsigned char page=0, j=0, k=0, l=0;
		
		while(page<(totalGameSize/slotSize)){
			checksum_rom = 0;
			while(i<(slotSize*(page+1))){
				checksum_rom += buffer_rom[i];
				i++;
			}
			buffer_checksum[page] = checksum_rom;
			printf(" Page %03d: ", page);
			printf("0x%06lX", checksum_rom);
			
			if(page>0){ //skip 1st page...
				k=0;
				for(j=0; j<page; j++){
		        	if(buffer_checksum[j] == checksum_rom){
		        		k=1;
		        	}
		    	}
		    	if(checksum_rom == 0x27E000|| checksum_rom == 0x3FC000){k=1;} //overdump 32k or my mapper
			}
			if(k){
				if(l){
					printf(" (overdump? %dKB?)\n", (l*(slotSize/1024)));
				}
				}else{
				printf("\n");
				l++;
			}
			page++;
		}
		printf("-------------------------\n\n");
	}
    return 0;
}


unsigned int check_buffer(unsigned long adr, unsigned char * buf, unsigned char length){
	unsigned int c=0;
	unsigned char i=0;
	while(i < length){
		c += buf[(adr +i)];
		i++;
	}
	return c;	
}