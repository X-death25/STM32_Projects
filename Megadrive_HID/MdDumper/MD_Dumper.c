#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <sys/ioctl.h>
#include <termios.h>
#elif defined(OS_WINDOWS)
//#include <conio.h>
#endif

#include "hid.h" // HID Lib

// HID Special Command

#define WakeUP     0x08  // WakeUP for first STM32 Communication
#define Read16     0x09  // Read16 : Read Page (64 byte) in 16 bit mode
#define Read8      0x0A  // Read8  : Read Page (64 byte) in 8 bit mode
#define Erase8     0x0B  // Erase8 : Erase Page (64 byte) in 8 bit mode
#define Erase16    0x0C  // Erase16: Erase Page (64 byte) in 16 bit mode
#define Write8     0x0D  // Write8  :Write Page (32 byte) in 8 bit mode
#define ReadSMS    0x0E  // DumpSMS : Dump SMS cartridge

int main()
{
    // Program Var
    int  r, num;
    unsigned char buf[64];
    int choixMenu=0;
    int ReadOK=1;
    unsigned long address=0;
    unsigned long compteur=0;
    unsigned long i=0;
    unsigned long j=0;
    unsigned char octetActuel=0;

    // HID Command Buffer & Var
    unsigned char HIDCommand [64];
    unsigned char *BufferROM;
    unsigned char *BufferSave;
    unsigned char ReleaseDate[8];
    unsigned char GameName[32];
    unsigned long Gamesize=0;
    unsigned short SaveSize=0;
    unsigned char TMSS_MD[4];
    unsigned char TMSS_SMS[8];
    
    // Output File
    FILE *dump;
    FILE *save;

    printf("-= Sega Megadrive USB Dumper -= \n\n");
    printf("Detecting USB Device ... \n");

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

    printf("found HID Megadrive Dumper ! \n");
    printf("Receiving game info ...\n");

    HIDCommand[0] = WakeUP; // Select WakeUP Command
    HIDCommand[6]=0xCC; // Disable Mapper control for SMS Cartridge
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
            /*printf("\nrecu %d bytes:\n", num);
            for (i=0; i<num; i++) {
            	printf("%02X ", buf[i] & 255);
            	if (i % 16 == 15 && i < num-1) printf("\n");
            }*/
            printf("\n");
            ReadOK=0;
        }
    }
    
    for (i=0; i<4; i++)
    {
        TMSS_MD[i]=buf[51+i];
    }
    
      for (i=0; i<8; i++)
    {
        TMSS_SMS[i]=buf[55+i];
    } 
    
  if (memcmp(TMSS_MD,"SEGA",sizeof(TMSS_MD)) == 0)
   {
     printf("Megadrive/Genesis cartridge Detected !\n");
     
    for (i=0; i<8; i++)
    {
        ReleaseDate[i]=buf[i];
    }

    for (i=0; i<32; i++)
    {
        GameName[i]=buf[i+8];
    }

    Gamesize = ((buf[42]) | (buf[41] << 8) | (buf[40]<<16));
    Gamesize=(Gamesize/1024)+1;
    SaveSize = ((buf[45]) | (buf[46] << 8));
    SaveSize=(SaveSize/1024)+1;

    printf("Game Name : %.*s \n",32,GameName);
    printf("Release Date : %.*s \n",8,ReleaseDate);
    printf("Game Size : %ld Ko \n",Gamesize);
    if (buf[43]!=0x01)
    {
        printf("Save Support : No\n ");
    }
    else
    {
        printf("Save Support : Yes\n ");
        printf("Save Size : %d Ko \n",SaveSize);
        if (buf[44]==0xF8)
        {
            printf("Save Type : SRAM\n ");
        }
        if (buf[44]==0xE8)
        {
            printf("Save Type : EEPROM\n ");
        }
    }
    printf("Region : %c",buf[48]);
    printf("%c",buf[49]);
    printf("%c \n\n",buf[50]);
}
    
     else if (memcmp(TMSS_SMS,"TMR SEGA", sizeof(TMSS_SMS)) == 0)
   {
     printf("Master System/Mark3 Cartridge Detected !\n\n");
   }
   
   else {
          printf("Unknown cartridge or bad connection  ...\n ");
	 }
   


    printf("---Menu---\n\n");
    printf("1.Dump SMD ROM\n");
    printf("2.Dump SMD Save\n");
    printf("3.Write SMD Save\n");
    printf("4.Erase SMD Save\n");
    printf("5.Dump SMS ROM \n");
    printf("8.SMD Hex View \n");
    printf("9.SMS Hex View \n");
    printf("\nWhat do you want ?\n\n");
    scanf("%d", &choixMenu);
    clock_t start = clock();
    clock_t finish = clock();

    switch(choixMenu)
    {
    case 1:  // Dump SMD ROM
      
        choixMenu=0;
	printf("Dump mode : \n");
	printf("1.Auto (Size from header)\n");
	printf("2.Manual (Size from user)\n");
        scanf("%d", &choixMenu);
	switch(choixMenu)
	{
	  case 1:
	          Gamesize=Gamesize; // Gamesize is 
		  break;
	  case 2:
	          printf("Enter number of Ko to dump \n");
	          scanf("%ld", &Gamesize);
	          Gamesize=Gamesize;
		  break;
          default:
                   Gamesize=Gamesize;
		   break;
	}
	printf("Sending command Dump ROM \n");   
        printf("Dumping please wait ...\n");
        HIDCommand[0] = 0x09; // Select Read in 16bit Mode

        address=0;
        i=0;
        j=0;
        BufferROM = (unsigned char*)malloc(1024*Gamesize);
        while (address < ((1024*Gamesize)/2) )

        {

            HIDCommand[1]=address & 0xFF;
            HIDCommand[2]=(address & 0xFF00)>>8;
            HIDCommand[3]=(address & 0xFF0000)>>16;
            HIDCommand[4]=(address & 0xFF000000)>>24;
            rawhid_send(0, HIDCommand, 64, 15);
            rawhid_recv(0,(BufferROM+ (address*2)), 64,15);
            address +=32 ;
        }

        dump=fopen("dump.bin","wb");
        fwrite(BufferROM,1,1024*Gamesize,dump);
        finish = clock();
        printf("Dump completed in %ld ms",(finish - start));
        scanf("%d");
        break;

    case 2:
        printf("Sending command Dump Save \n");
        printf("Dumping please wait ...\n");
        HIDCommand[0] = 0x0A; // Select Read in 8bit Mode

        address=2097153; // Start adress for Save Area
        j=0;
        BufferROM = (unsigned char*)malloc(1024*128);
        BufferSave = (unsigned char*)malloc((1024*128));
        // Cleaning Buffer

        for (i=0; i<(1024*128); i++)
        {
            BufferSave[i]=0xFF;
            BufferROM[i]=0xFF;
        }
        rawhid_send(0, HIDCommand, 64, 15);
        num = rawhid_recv(0,buf, 64,15);
        address = address/2; // 8 bits read
        while (j < (1024*64) )
        {

            HIDCommand[1]=address & 0xFF;
            HIDCommand[2]=(address & 0xFF00)>>8;
            HIDCommand[3]=(address & 0xFF0000)>>16;
            HIDCommand[4]=(address & 0xFF000000)>>24;
            rawhid_send(0, HIDCommand, 64, 15);
            rawhid_recv(0,BufferROM+j, 64,15);
            j +=64;
            address +=64;
        }

        j=0;

        for (i=0; i<(1024*64); i++)
        {
            j=j+1;
            BufferSave[i+j]=BufferROM[i];
        }
        dump=fopen("dump.srm","wb");
        fwrite(BufferSave,1,(1024*64),dump);
        printf("Dump Save OK");
        scanf("%d");
        break;

    case 3:
        printf("Opening save file.. \n");
        BufferSave = (unsigned char*)malloc((1024*64)/2);
        save=fopen("save.srm","rb");
        if (save == NULL)
        {
            printf("file save.srm not found !\n");
            printf("exit application\n");
            scanf("%d");
            exit(0);
        }
        else
        {
            printf("Send save to cartridge please wait ....\n");
            for (i=0; i<(1024*64)/2; i++)
            {
                fread(&octetActuel,1,1,save);
                fread(&octetActuel,1,1,save);
                BufferSave[i]=octetActuel;
            }
            while ( j< (1024*64)/8) // /8 OK
            {
                HIDCommand[0] = 0x0D; // Select Write in 8bit Mode
                for (i=0; i<32; i++)
                {
                    HIDCommand[32+i]=BufferSave[i+j];
                }
                rawhid_send(0,HIDCommand, 64, 15);
                while (buf[0] != 0xAA)
                {
                    num = rawhid_recv(0,buf, 64,15);
                }
                j +=32;
            }
            printf("Save Writted sucessfully ! \n");
            scanf("%d");
        }

        break;

    case 4:
        printf("WARNING ALL SAVED DATA WILL BE LOST CONTINUE ?  \n");
        scanf("%d");
        printf("Sending command Erase Save \n");
        HIDCommand[0] = 0x0B; // Select Erase in 8bit Mode
        rawhid_send(0, HIDCommand, 64, 15);
        printf("Erasing please wait...");
        while (buf[0] != 0xAA)
        {
            num = rawhid_recv(0,buf, 64,15);
        }
        printf("\nErase completed sucessfully ! \n");
        break;

    case 5:
        printf("Enter number of Ko to dump \n");
	scanf("%d", &Gamesize);
        printf("Sending command Dump SMS \n");
        printf("Dumping please wait ...\n");
        HIDCommand[0] = 0x0E; // Select Dump SMS
        BufferROM = (unsigned char*)malloc(1024*Gamesize);
        HIDCommand[1]=address & 0xFF;
        HIDCommand[2]=(address & 0xFF00)>>8;
        HIDCommand[3]=(address & 0xFF0000)>>16;
        HIDCommand[4]=(address & 0xFF000000)>>24;
	HIDCommand[11]=0xCC; // Disable SMS Mapper Control	
        rawhid_send(0, HIDCommand, 64, 15);
        rawhid_recv(0,BufferROM, 64,15);
	
        while (compteur < (1024*Gamesize))
        {
            address = address;
	    if (j== 0xC000) {HIDCommand[11]=0x03;HIDCommand[6]=0x03;address=0x8000;}  // Enable Bank3
	    if (j== 0x10000) {HIDCommand[11]=0x03;HIDCommand[6]=0x04;address=0x8000;} // Enable Bank4
	    if (j== 0x14000) {HIDCommand[11]=0x03;HIDCommand[6]=0x05;address=0x8000;} // Enable Bank5
	    if (j== 0x18000) {HIDCommand[11]=0x03;HIDCommand[6]=0x06;address=0x8000;} // Enable Bank6
	    if (j== 0x1C000) {HIDCommand[11]=0x03;HIDCommand[6]=0x07;address=0x8000;} // Enable Bank7
	    
	    if (j== 0x20000) {HIDCommand[11]=0x03;HIDCommand[6]=0x08;address=0x8000;} // Enable Bank8
	    if (j== 0x24000) {HIDCommand[11]=0x03;HIDCommand[6]=0x09;address=0x8000;} // Enable Bank9
	    if (j== 0x28000) {HIDCommand[11]=0x03;HIDCommand[6]=0x0A;address=0x8000;} // Enable BankA
	    if (j== 0x2C000) {HIDCommand[11]=0x03;HIDCommand[6]=0x0B;address=0x8000;} // Enable BankB 
	    
	    if (j== 0x30000) {HIDCommand[11]=0x03;HIDCommand[6]=0x0C;address=0x8000;} // Enable BankC
	    if (j== 0x34000) {HIDCommand[11]=0x03;HIDCommand[6]=0x0D;address=0x8000;} // Enable BankD
	    if (j== 0x38000) {HIDCommand[11]=0x03;HIDCommand[6]=0x0E;address=0x8000;} // Enable BankE
	    if (j== 0x3C000) {HIDCommand[11]=0x03;HIDCommand[6]=0x0F;address=0x8000;} // Enable BankF 
	    
	    if (j== 0x40000) {HIDCommand[11]=0x03;HIDCommand[6]=0x10;address=0x8000;} // Enable Bank10
	    if (j== 0x44000) {HIDCommand[11]=0x03;HIDCommand[6]=0x11;address=0x8000;} // Enable Bank11
	    if (j== 0x48000) {HIDCommand[11]=0x03;HIDCommand[6]=0x12;address=0x8000;} // Enable Bank12   
	    if (j== 0x4C000) {HIDCommand[11]=0x03;HIDCommand[6]=0x13;address=0x8000;} // Enable Bank13
	    if (j== 0x50000) {HIDCommand[11]=0x03;HIDCommand[6]=0x14;address=0x8000;} // Enable Bank14
	    if (j== 0x54000) {HIDCommand[11]=0x03;HIDCommand[6]=0x15;address=0x8000;} // Enable Bank15	    	    
	    if (j== 0x58000) {HIDCommand[11]=0x03;HIDCommand[6]=0x16;address=0x8000;} // Enable Bank16	    
	    if (j== 0x5C000) {HIDCommand[11]=0x03;HIDCommand[6]=0x17;address=0x8000;} // Enable Bank17	    
	    
	    if (j== 0x60000) {HIDCommand[11]=0x03;HIDCommand[6]=0x18;address=0x8000;} // Enable Bank18
	    if (j== 0x64000) {HIDCommand[11]=0x03;HIDCommand[6]=0x19;address=0x8000;} // Enable Bank19
	    if (j== 0x68000) {HIDCommand[11]=0x03;HIDCommand[6]=0x1A;address=0x8000;} // Enable Bank20   
	    if (j== 0x6C000) {HIDCommand[11]=0x03;HIDCommand[6]=0x1B;address=0x8000;} // Enable Bank21
	    if (j== 0x70000) {HIDCommand[11]=0x03;HIDCommand[6]=0x1C;address=0x8000;} // Enable Bank22
	    if (j== 0x74000) {HIDCommand[11]=0x03;HIDCommand[6]=0x1D;address=0x8000;} // Enable Bank23	    	    
	    if (j== 0x78000) {HIDCommand[11]=0x03;HIDCommand[6]=0x1E;address=0x8000;} // Enable Bank24
	    if (j== 0x7C000) {HIDCommand[11]=0x03;HIDCommand[6]=0x1F;address=0x8000;} // Enable Bank25

	    
            HIDCommand[1]=address & 0xFF;
            HIDCommand[2]=(address & 0xFF00)>>8;
            HIDCommand[3]=(address & 0xFF0000)>>16;
            HIDCommand[4]=(address & 0xFF000000)>>24;
            rawhid_send(0, HIDCommand, 64, 15);
            rawhid_recv(0,BufferROM+j, 64,15);
            j+=64;
            address +=64;
	    compteur +=64;
        }
        dump=fopen("dump.sms","wb");
        fwrite(BufferROM,1,(1024*Gamesize),dump);
        printf("Dump SMS OK");
        scanf("%d");
        break;

    case 8:
        BufferROM = (unsigned char*)malloc(1024*Gamesize);
        HIDCommand[0] = 0x09; // Select Read in 16bit Mode
        while (1)
        {
            printf("\n\nEnter ROM Address ( decimal value) :\n \n");
            scanf("%ld",&address);
            address = address/2; // 16 bits read
            HIDCommand[1]=address & 0xFF;
            HIDCommand[2]=(address & 0xFF00)>>8;
            HIDCommand[3]=(address & 0xFF0000)>>16;
            HIDCommand[4]=(address & 0xFF000000)>>24;
            rawhid_send(0, HIDCommand, 64, 15);
            num = rawhid_recv(0,buf, 64,15);
            if (num > 0)
            {
                printf("\n\n", num);
                for (i=0; i<num; i++)
                {
                    printf("%02X ", buf[i] & 255);
                    if (i % 16 == 15 && i < num-1) printf("\n");
                }
            }

        }

    case 9:
        BufferROM = (unsigned char*)malloc(1024*Gamesize);
        HIDCommand[0] = 0x0A; // Select Read in 8bit Mode
        rawhid_send(0, HIDCommand, 64, 15);
        num = rawhid_recv(0,buf, 64,15);
        while (1)
        {
            printf("\n\nEnter ROM Address ( decimal value) :\n \n");
            scanf("%ld",&address);
           // address = address/2; // 8 bits read
            HIDCommand[1]=address & 0xFF;
            HIDCommand[2]=(address & 0xFF00)>>8;
            HIDCommand[3]=(address & 0xFF0000)>>16;
            HIDCommand[4]=(address & 0xFF000000)>>24;
	    
            rawhid_send(0, HIDCommand, 64, 15);
            num = rawhid_recv(0,buf, 64,15);
            if (num > 0)
            {
                printf("\n\n", num);
                for (i=0; i<num; i++)
                {
                    printf("%02X ", buf[i] & 255);
                    if (i % 16 == 15 && i < num-1) printf("\n");
                }
            }

        }

    default:
        printf("Nice try bye ;)");
	scanf("%d");
    }
}


#if defined(OS_LINUX) || defined(OS_MACOSX)


#endif



