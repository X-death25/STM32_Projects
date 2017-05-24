/**
 *  \file MD_Dumper.C
 *  \brief MD_Dumper Software for Read/Write Sega Megadrive Cartridge
 *  \author X-death for Ultimate-Consoles forum ( http://www.ultimate-consoles.fr/index)
 *  \date 01/2017
 *
 * This source code is based on the very good HID lib RawHid from https://www.pjrc.com/teensy/rawhid.html
 * It uses some USB HID interrupt for communication with the STM32F103
 * Support Sega Megadrive cartridge dump ( max size is 64MB)
 * Support Read & Write save ( 8-32Kb & Bankswitch)
 * Support Sega MasterSystem/Mark3 cartridge dump ( max size is 4MB)
 * Please report bug at X-death@ultimate-consoles.fr
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "hid.h" // HID Lib

// HID Special Command
#define SMSwakeup    0x08  // WakeUP for first STM32 Communication utilité ?
#define SMSread      0x0A  // Read
#define SMSerase     0x0B  // Erase
#define SMSwrite     0x0D  // Write

int main(){
    // Program Var
    int  r, num;
    unsigned char choixMenu = 0;
    unsigned char choixSousMenu = 0;
    int ReadOK=1;
    
    unsigned long i=0;
    unsigned long j=0;

    // HID Command Buffer & Var
    unsigned char HIDCommand [64];
    unsigned char *bufferRom;
    unsigned char *bufferSave;
    
    unsigned long address = 0;
    unsigned long gameSize = 0;
    unsigned short saveSize = 0;
    
    unsigned int slotAdr = 0;
    unsigned int slotSize = 0;
    unsigned int slotRegister = 0;

    // Output File
    FILE *dump;
    FILE *save;

    printf("*** Sega MASTER SYSTEM/Mark III/SG1000 USB Dumper ***\n\n");
    printf("Detecting... \n");

    r = rawhid_open(1, 0x0483, 0x5750,-1, -1);
    if (r <= 0){
        r = rawhid_open(1, 0x0483, 0x5750,-1, -1);
        if (r <= 0){
            printf("SMS Dumper NOT FOUND!\n");
            return 0;
        }
    }

    printf("SMS Dumper detected! \n");

    HIDCommand[0] = SMSwakeup;
    rawhid_send(0,HIDCommand,64,15);

    while(ReadOK){
        num = rawhid_recv(0, buf, 64,15);
        if(num < 0){
            printf("\nError reading, device went offline\n");
            rawhid_close(0);
            return 0;
        }
        if(num){
            printf("\n");
            ReadOK=0;
        }
    }

    printf("---ACTION---\n\n");
    printf(" 1. Dump ROM\n");
    printf(" 2. Dump SRAM\n");
    printf(" 3. Write SRAM\n");
    printf(" 4. Exit\n");
    printf("\nEnter your choice ?\n\n");
    scanf("%d", &choixMenu);
    clock_t start = clock();
    clock_t finish = clock();

    switch(choixMenu){
    case 1:  // Dump SMD ROM

/*
Read header ?
0x7ff0 read 16 bytes
$1ff0, $3ff0 or $7ff0
*/


//        printf("---MAPPER---\n\n");
//		printf(" 1. Sega\n");					//0xFFFF
//		printf(" 2. Codemasters\n");			//0x8000
//		printf(" 3. Korean MSX 8Kb\n");		//0x	
//		printf(" 4. Korean \n");				//0xA000
//		printf(" 5. Manual (size/registre)\n");
//    	printf("\nEnter your choice ?\n\n");
//   		scanf("%d", &choixSousMenu);      
        
        slotSize = 16384; //default

//        switch(choixSousMenu){
//	        case 1: slot_registre = 0xFFFF; break;
//	        case 2:	slot_registre = 0x8000; break;
//	        case 3: slot_size = 8192; slot_registre = 0x4000; break;
//	        case 4: slot_registre = 0xA000; break;
//        }
                 
        printf("Enter number of Ko to dump : \n");
        scanf("%ld", &gameSize);
        (void)gameSize = gameSize*1024;

        printf("Dump in progress! \n");
        printf("Please wait...\n");
        HIDCommand[0] = SMSread;

        bufferRom = (unsigned char*)malloc(gameSize);
        while(address < gameSize){
        	/*
        	dump 16ko per page = 256 * 64bytes blocs
			max 256 pages (0xFF) 32mbits
        	
        	slot0 16k - 0x0000 to 0x3FFF	A14=0 	A15=0
        	slot1 16k - 0x4000 to 0x7FFF 	A14=1 	A15=0
        	slot2 16k - 0x8000 to 0xBFFF 	A14=0	A15=1
        	*/       	
        	if(address > 0xBFFF){
        		//slot 2
        		slotAdr = 0x8000 + (address & 0xBFFF);     	
        		}else{
        		slotAdr = address;	
        	}
        	
        	/*
        	Mapper Sega			slot2	
        	Mapper Codemaster 	slot2
        	Mapper Coreen MSX	*** 8kb
        	Mapper Corren A000 	slot2	
        	*/
        	
            HIDCommand[1] = slotAdr & 0xFF;					//lower
            HIDCommand[2] = (slotAdr & 0xFF00)>>8;			//upper
            HIDCommand[3] = (address/slotSize); 			//num page for MAPPER
            HIDCommand[4] = slotRegister & 0xFF; 			//reg for MAPPER
            HIDCommand[5] = (slotRegister & 0xFF00) >>8; 	//reg for MAPPER
            
            rawhid_send(0, HIDCommand, 64, 15);
            rawhid_recv(0, bufferRom +address, 64, 15);
            address +=64 ;
        }
        finish = clock();
        printf("Dump complete! \n(%ld ms) \n",(finish - start));

        dump = fopen("sms_dump.bin","wb");
        fwrite(bufferRom, 1, gameSize, dump);
        break;

    case 2:
//        choixMenu=0;
//        printf("Sending command Dump Save \n");
//        printf(" 1.Auto (Save Size from header)\n");
//        printf(" 2.Manual 8 KB (32Kb)\n");
//        printf(" 3.Manual 32 KB (256Kb)\n");
//        scanf("%d", &choixMenu);
//        printf("Dumping save please wait...\n");
//        HIDCommand[0] = 0x0A; // Select Read in 8bit Mode
//        j=0;
//
//        BufferROM = (unsigned char*)malloc(1024*128);
//        BufferSave = (unsigned char*)malloc((1024*128));
//        // Cleaning Buffer
//
//        for (i=0; i<(1024*64); i++)
//        {
//            BufferSave[i]=0xFF;
//            BufferROM[i]=0xFF;
//        }
//        rawhid_send(0, HIDCommand, 64, 15);
//        num = rawhid_recv(0,buf, 64,15);
//        address = address/2; // 8 bits read
//        while (j < (1024*64) )
//        {
//
//            HIDCommand[1]=address & 0xFF;
//            HIDCommand[2]=(address & 0xFF00)>>8;
//            HIDCommand[3]=(address & 0xFF0000)>>16;
//            HIDCommand[4]=(address & 0xFF000000)>>24;
//            rawhid_send(0, HIDCommand, 64, 15);
//            rawhid_recv(0,BufferROM+j, 64,15);
//            j +=64;
//            address +=64;
//        }
//
//        j=0;
//        if (choixMenu==1)
//        {
//            (void)SaveSize;
//        }
//        if (choixMenu==2)
//        {
//            SaveSize=16;
//        }
//        if (choixMenu==3)
//        {
//            SaveSize=64;
//        }
//
//        for (i=0; i<(1024*64); i++)
//        {
//            j=j+1;
//            BufferSave[i+j]=BufferROM[i];
//        }
//
//        if (SaveSize!=64) // Remove mirrored data for game with save checksum
//        {
//            for (i=0; i<49152; i++)
//            {
//                BufferSave[16384+i]=0;
//            }
//        }
//
//        dump=fopen("dump.srm","wb");
//        fwrite(BufferSave,1,(1024*64),dump);
//        printf("Dump Save OK\n");
        break;

    case 3:
//        printf("Select cartridge save size\n");
//        printf(" 1.8 KB (32Kb)\n");
//        printf(" 2.32 KB (256Kb)\n");
//        scanf("%d", &choixMenu);
//        printf("Opening save file... \n");
//        BufferSave = (unsigned char*)malloc((1024*64)/2);
//        save=fopen("save.srm","rb");
//        if (save == NULL){
//            printf("file save.srm not found !\n");
//            printf("exit application\n");
//        }
//        else
//        {
//            printf("Send save to cartridge please wait...\n");
//            for (i=0; i<(1024*64)/2; i++)
//            {
//                fread(&octetActuel,1,1,save);
//                fread(&octetActuel,1,1,save);
//                BufferSave[i]=octetActuel;
//            }
//
//            dump=fopen("temp.srm","wb");
//            fwrite(BufferSave,1,(1024*64)/2,dump);
//
//
//            if (choixMenu ==1){
//                while ( j< (1024*64)/8) // /8 OK
//                {
//                    HIDCommand[0] = 0x0D; // Select Write in 8bit Mode
//                    for (i=0; i<32; i++){
//                        HIDCommand[32+i]=BufferSave[i+j];
//                    }
//                    rawhid_send(0,HIDCommand, 64, 15);
//                    while (buf[0] != 0xAA){
//                        num = rawhid_recv(0,buf, 64,15);
//                    }
//                    j +=32;
//                }
//            }
//
//            j=0;
//            if (choixMenu !=1){
//                while ( j< (1024*64)/2) // /8 OK
//                {
//                    HIDCommand[0] = 0x0D; // Select Write in 8bit Mode
//                    for (i=0; i<32; i++){
//                        HIDCommand[32+i]=BufferSave[i+j];
//                    }
//                    rawhid_send(0,HIDCommand, 64, 25);
//                    while (buf[0] != 0xAA)
//                    {
//                        num = rawhid_recv(0,buf, 64,25);
//                    }
//                    j=j+32;
//                }
//                j=0;
//                i=0;
//                while ( j< (1024*64)/2) // Need to make it twice , stupid timing bug...
//                {
//                    HIDCommand[0] = 0x0D; // Select Write in 8bit Mode
//                    for (i=0; i<32; i++){
//                        HIDCommand[32+i]=BufferSave[i+j];
//                    }
//                    rawhid_send(0,HIDCommand, 64, 25);
//                    while (buf[0] != 0xAA){
//                        num = rawhid_recv(0,buf, 64,25);
//                    }
//                    j=j+32;
//                }
//            }
//            printf("Save Writted sucessfully ! \n");
//        }

        break;

//         case 7:
//			printf("Opening game file.. \n");
//        	save = fopen("game.bin","rb");
//        	fseek(save,0,SEEK_END); // on amène le curseur à la fin du fichier
//        	Taille_Roms = ftell(save); // on récupère la  taille du fichier 
//        	BufferSave= (unsigned char*)malloc(Taille_Roms);
//        	fseek(save,0,SEEK_SET); // On repositionne le curseur à 0
//
//        if (save == NULL){
//            printf("file game.bin not found !\n");
//            printf("exit application\n");
//            return 0;
//        }else{
//            // Cleaning Buffer Save
//
//           printf("Cleaning Buffer...\n");
//            for (i=0; i<Taille_Roms; i++){
//                BufferSave[i]=0x00;
//            }
//
//            // Sending Game to buffer
//
//            printf("Sending Game to Flash...\n");
//            for (i=0; i<Taille_Roms; i++){
//               fread(&octetActuel,1,1, save); 
//               BufferSave[i]=octetActuel;  
//            }
// // Sending Game to Chip
//
//                while( j<  Taille_Roms){ // /8 OK
//                    HIDCommand[0] = 0x0F; // Select CFI Write
//                    for (i=0; i<32; i++){
//                        HIDCommand[32+i]=BufferSave[i+j];
//                    }
//
//                    rawhid_send(0,HIDCommand, 64, 15);
//                                    
//                    while (buf[0] != 0xAA){ // Wait until interrupt
//                        num = rawhid_recv(0,buf, 64,25);
//                    }
//                    j=j+32;                   
//               }
//             }
//
//            j=0;
//            finish = clock();
//            printf("Write completed in %ld ms\n",(finish - start));
//        break;		
        
    default:
    break;
    }
    return 0;
}


