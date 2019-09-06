/*
USB2 Sega Dumper
BETA
X-death 11/2018
*/

#include <string.h>
#include <stdlib.h>

// include only used LibopenCM3 lib

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>

// Define Dumper Pinout

#define D0 			GPIO9  // PB9
#define D1 			GPIO8  // PB8
#define D2 			GPIO7  // PB7
#define D3 			GPIO6  // PB6
#define D4 			GPIO4  // PB4
#define D5 			GPIO3  // PB3
#define D6 			GPIO15 // PA15
#define D7 			GPIO10 // PA10

#define D8 			GPIO9  // PA9
#define D9 			GPIO8  // PA8
#define D10 		GPIO15 // PB15
#define D11 		GPIO14 // PB14
#define D12 		GPIO13 // PB13
#define D13 		GPIO12 // PB12
#define D14 		GPIO11 // PB11
#define D15			GPIO10 // PB10

#define OE 			GPIO1  // PB1 out act as /RD signal ok
#define SEL 		GPIO0  // PA0 out
#define MIO 		GPIO1  // PA1 out
#define WR      	GPIO2  // PA2 out act as /WR Signal
#define RESET 		GPIO3  // PA3 out act as /RESET Signal 

#define CLK_CLEAR 	GPIO4  // PA4 in Reset for Mux logic
#define CLK1 		GPIO7  // PA7
#define CLK2 		GPIO6  // PA6
#define CLK3 		GPIO5  // PA5

#define IRQ      	GPIO15 // PC15 out act as /WE for Flash ROM
#define LED_PIN 	GPIO13 // PC13

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
#define LOAD_PAGE    	0x20

// USB Specific VAR

static char serial_no[25];
static uint8_t usb_buffer_IN[64];
static uint8_t usb_buffer_OUT[64];
static uint32_t len=0;

// Swan Dumper Specific Var

static const unsigned char stmReady[] = {'R','E','A','D','Y','!'};
static unsigned char dump_running = 0;
static unsigned long address = 0;
static unsigned char read8 = 0;
static unsigned int read16 = 0;
static unsigned char bank = 0;
static unsigned char j = 0;
static unsigned char page_adr = 0;
static unsigned short bufferMX[128] = {0};
static unsigned short Current_Word = 0;
static unsigned long address_max;

//  USB Specific Fonction ///// 

static const struct usb_device_descriptor dev = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0xff,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0x0483,
	.idProduct = 0x5740,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

static const struct usb_endpoint_descriptor data_endp[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x01,
    .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = 64,
    .bInterval = 1,
}, {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x82,
    .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = 64,
    .bInterval = 1,
}, {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x02,
    .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = 64,
    .bInterval = 1,
}};

static const struct usb_interface_descriptor data_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 3,
	.bInterfaceClass = 0xff,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.endpoint = data_endp,
}};

static const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = data_iface,
}};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

static void usb_setup(void)
{
	/* Enable clocks for USB */
	//rcc_usb_prescale_1();
	rcc_periph_clock_enable(RCC_USB);

	// Cleaning USB Buffer

	int i=0;

	    for(i = 0; i < 64; i++)
    {
		usb_buffer_IN[i]=0x00;
		usb_buffer_OUT[i]=0x00;
	}
}

static const char *usb_strings[] = {
    "Ultimate-Consoles",
    "USB2 WonderSwan Dumper",
    serial_no,
};

static char *get_dev_unique_id(char *s)
{
    volatile uint8_t *unique_id = (volatile uint8_t *)0x1FFFF7E8;
    int i;

    // Fetch serial number from chip's unique ID
    for(i = 0; i < 24; i+=2)
    {
        s[i] = ((*unique_id >> 4) & 0xF) + '0';
        s[i+1] = (*unique_id++ & 0xF) + '0';
    }
    for(i = 0; i < 24; i++)
        if(s[i] > '9')
            s[i] += 'A' - '9' - 1;

    return s;
}

//  Sega Dumper Specific Fonction ///// 

void wait(int nb){
    while(nb){ __asm__("nop"); nb--;}
}

void setDataInput(){
    GPIO_CRH(GPIOA) = 0x44444444;
    GPIO_CRL(GPIOB) = 0x44344434;
    GPIO_CRH(GPIOB) = 0x44444444;
}

void setDataOutput(){
    GPIO_CRH(GPIOA) = 0x34444333;
    GPIO_CRL(GPIOB) = 0x33333333;
    GPIO_CRH(GPIOB) = 0x33333333;
}

void buffer_checksum(unsigned char * buffer, unsigned char length){
	unsigned char i = 0;
	unsigned int check = 0;

	while( i < length){
		check += buffer[i];
		i++;
	}
	usb_buffer_OUT[0] = check & 0xff;
	usb_buffer_OUT[1] = (check >> 8)&0xff;
}

//precalc GPIOs states
const unsigned int lut_write8[] = {
0x0,0x200,0x100,0x300,0x80,0x280,0x180,0x380,0x40,0x240,0x140,0x340,0xC0,0x2C0,0x1C0,0x3C0,0x10,0x210,0x110,0x310,0x90,0x290,0x190,0x390,0x50,0x250,0x150,0x350,0xD0,0x2D0,0x1D0,0x3D0,0x8,0x208,0x108,0x308,0x88,0x288,0x188,0x388,0x48,0x248,0x148,0x348,0xC8,0x2C8,0x1C8,0x3C8,0x18,0x218,0x118,0x318,0x98,0x298,0x198,0x398,0x58,0x258,0x158,0x358,0xD8,0x2D8,0x1D8,0x3D8,0x8000,0x8200,0x8100,0x8300,0x8080,0x8280,0x8180,0x8380,0x8040,0x8240,0x8140,0x8340,0x80C0,0x82C0,0x81C0,0x83C0,0x8010,0x8210,0x8110,0x8310,0x8090,0x8290,0x8190,0x8390,0x8050,0x8250,0x8150,0x8350,0x80D0,0x82D0,0x81D0,0x83D0,0x8008,0x8208,0x8108,0x8308,0x8088,0x8288,0x8188,0x8388,0x8048,0x8248,0x8148,0x8348,0x80C8,0x82C8,0x81C8,0x83C8,0x8018,0x8218,0x8118,0x8318,0x8098,0x8298,0x8198,0x8398,0x8058,0x8258,0x8158,0x8358,0x80D8,0x82D8,0x81D8,0x83D8,0x400,0x600,0x500,0x700,0x480,0x680,0x580,0x780,0x440,0x640,0x540,0x740,0x4C0,0x6C0,0x5C0,0x7C0,0x410,0x610,0x510,0x710,0x490,0x690,0x590,0x790,0x450,0x650,0x550,0x750,0x4D0,0x6D0,0x5D0,0x7D0,0x408,0x608,0x508,0x708,0x488,0x688,0x588,0x788,0x448,0x648,0x548,0x748,0x4C8,0x6C8,0x5C8,0x7C8,0x418,0x618,0x518,0x718,0x498,0x698,0x598,0x798,0x458,0x658,0x558,0x758,0x4D8,0x6D8,0x5D8,0x7D8,0x8400,0x8600,0x8500,0x8700,0x8480,0x8680,0x8580,0x8780,0x8440,0x8640,0x8540,0x8740,0x84C0,0x86C0,0x85C0,0x87C0,0x8410,0x8610,0x8510,0x8710,0x8490,0x8690,0x8590,0x8790,0x8450,0x8650,0x8550,0x8750,0x84D0,0x86D0,0x85D0,0x87D0,0x8408,0x8608,0x8508,0x8708,0x8488,0x8688,0x8588,0x8788,0x8448,0x8648,0x8548,0x8748,0x84C8,0x86C8,0x85C8,0x87C8,0x8418,0x8618,0x8518,0x8718,0x8498,0x8698,0x8598,0x8798,0x8458,0x8658,0x8558,0x8758,0x84D8,0x86D8,0x85D8,0x87D8};

void directWrite8(unsigned char val){
	unsigned int invVal = 0;

	invVal = ~lut_write8[val] & 0x3D8;
	GPIOB_BSRR |= lut_write8[val] | (invVal << 16); //set and reset pins GPIOB

	invVal = ~lut_write8[val] & 0x8400;
	GPIOA_BSRR |= lut_write8[val] | (invVal << 16); //set and reset pins GPIOA
}

void directWrite16(unsigned int val){
	unsigned int invVal = 0;

	/*
	#define D0 			GPIO9  // PB9
	#define D1 			GPIO8  // PB8
	#define D2 			GPIO7  // PB7
	#define D3 			GPIO6  // PB6
	#define D4 			GPIO4  // PB4
	#define D5 			GPIO3  // PB3
	#define D10 		GPIO15 // PB15
	#define D11 		GPIO14 // PB14
	#define D12 		GPIO13 // PB13
	#define D13 		GPIO12 // PB12
	#define D14 		GPIO11 // PB11
	#define D15			GPIO10 // PB10
	#define D6 			GPIO15 // PA15
	#define D7 			GPIO10 // PA10
	#define D8 			GPIO9  // PA9
	#define D9 			GPIO8  // PA8
	busB = D0 - D5, D10 - D15 / mask FFD8
	busA = D6 - D9,  / mask 8700
	*/

	unsigned int busB = ((val&0x1)<<9) | ((val&0x2)<<7) | ((val&0x4)<<5) | ((val&0x8)<<3) | (val&0x10) | ((val&0x20)>>2) | ((val&0x400)<<5) | ((val&0x800)<<3) | ((val&0x1000)<<1) | ((val&0x2000)>>1) | ((val&0x4000)>>3) | ((val&0x8000)>>5); //D0 to D5 & D10 to D15
	unsigned int busA = ((val&0x40)<<9) | ((val&0x80)<<3) | ((val&0x100)<<1) | ((val&0x200)>>1);  //D6 to D9

	invVal = ~busB & 0xFFD8;
	GPIOB_BSRR |= busB | (invVal << 16); //set and reset pins GPIOB

	invVal = ~busA & 0x8700;
	GPIOA_BSRR |= busA | (invVal << 16); //set and reset pins GPIOA
}


void directRead8(){
	unsigned int busA = GPIOA_IDR;
	unsigned int busB = GPIOB_IDR;

    read8 = ((busB>>9)&0x1) | ((busB>>7)&0x2) | ((busB>>5)&0x4) | ((busB>>3)&0x8) | (busB&0x10) | ((busB<<2)&0x20) | ((busA>>9)&0x40) | ((busA>>3)&0x80); //Byte D0-7
}

void directRead16(){
	unsigned int busA = GPIOA_IDR;
	unsigned int busB = GPIOB_IDR;

	read16 = ((busB>>9)&0x1) | ((busB>>7)&0x2) | ((busB>>5)&0x4) | ((busB>>3)&0x8) | ((busB&0x10)) | ((busB<<2)&0x20) | ((busA>>9)&0x40) | ((busA>>3)&0x80) | ((busA>>1)&0x100) | ((busA<<1)&0x200) | ((busB>>5)&0x400) | ((busB>>3)&0x800) | ((busB>>1)&0x1000) | ((busB<<1)&0x2000) | ((busB<<3)&0x4000) | ((busB<<5)&0x8000); //Word D0-D15
}


void setAddress(unsigned long adr){

    setDataOutput();

    directWrite8(adr&0xFF);
    GPIOA_BRR  |= CLK1;
    GPIOA_BSRR |= CLK1;

    directWrite8((adr>>8)&0xFF);
    GPIOA_BRR  |= CLK2;
    GPIOA_BSRR |= CLK2;

    directWrite8((adr>>16)&0xFF);
    GPIOA_BRR  |= CLK3;
    GPIOA_BSRR |= CLK3;

}

void cleanBuffer_OUT()
{
unsigned char i=0;
while ( i < 64)
{
usb_buffer_OUT[i]=0x00;
i++;
}

}


void readROM(){ // 0.253 m/s

	unsigned char adr = 0;
	unsigned char adr16 = 0;

    setDataOutput(); // 1,3 µs

	GPIOA_BSRR |= CLK1| CLK2 | CLK3 | (CLK_CLEAR<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;


    while(adr<64){ // 64bytes/32word 8,5 µs * 32

		setAddress(address+adr16+0x20000); //4,96µs
	    setDataInput();

	  // GPIOA_BRR |= CE;
	    GPIOB_BRR |= OE;

	    wait(16);

	    directRead16(); //save into read16 global
	    usb_buffer_OUT[adr+1] = (read16 >> 8)&0xFF; //word to byte
	    usb_buffer_OUT[(adr)] = read16 & 0xFF;

	    GPIOB_BSRR |= OE;
	 //   GPIOA_BSRR |= CE;
	    setAddress(address+adr16+0x10000);

		adr+=2;  //buffer
		adr16+=2; //md adr word
	}

}


void commandMdFlash(unsigned long adr, unsigned int val){

	/*setAddress(adr);
	setDataOutput();
	directWrite16(val);
	GPIOC_BRR |= IRQ;
wait(16);
	GPIOC_BSRR |= IRQ;
	setAddress(0x10000);*/

	setAddress(adr);
	GPIOC_BRR |= IRQ;
	setDataOutput();
	directWrite16(val);
	GPIOC_BSRR |= IRQ;
	setAddress(0x10000);

}

void reset_command(){
	setDataOutput();
    commandMdFlash(0x2AAAA,0xAA);
    commandMdFlash(0x25555,0x55);
    commandMdFlash(0x2AAAA,0xF0);
	wait(16);
}


void infosId()
{
	//seems to be valid only for 29LVxxx ?

	setAddress(0x10000);	
	GPIOC_BSRR |= IRQ;
	GPIOA_BSRR |= CLK1| CLK2 | CLK3;
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;

    /*commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x90);*/

 commandMdFlash(0x2AAAA,0xAA);
    commandMdFlash(0x25555,0x55);
    commandMdFlash(0x2AAAA,0x90);



    setAddress(0x20000);
	setDataInput();
    GPIOB_BRR |= OE;
	wait(16);
    directRead16(); //save into read16 global
		usb_buffer_OUT[0] = 0;
	    usb_buffer_OUT[1] = read16 & 0xFF;
    GPIOB_BSRR |= OE;
    setAddress(0x10000);

		setDataOutput();
    commandMdFlash(0x2AAAA,0xAA);
    commandMdFlash(0x25555,0x55);
    commandMdFlash(0x2AAAA,0xF0);
	wait(16);


 commandMdFlash(0x2AAAA,0xAA);
    commandMdFlash(0x25555,0x55);
    commandMdFlash(0x2AAAA,0x90);



	 setAddress(0x20000+2);
	setDataInput();
    GPIOB_BRR |= OE;
	wait(16);
    directRead16(); //save into read16 global
		usb_buffer_OUT[2] = 1;
	    usb_buffer_OUT[3] = read16 & 0xFF;
    GPIOB_BSRR |= OE;
    setAddress(0x10000);

    reset_command();

	//chipid = (usb_buffer_OUT[1]<<8) | usb_buffer_OUT[3];
}

void EraseFlash()
{
	unsigned char poll_dq7=0;
    commandMdFlash(0x2AAAA,0xAA);
    commandMdFlash(0x25555,0x55);
    commandMdFlash(0x2AAAA,0x80);
    commandMdFlash(0x2AAAA,0xAA);
    commandMdFlash(0x25555,0x55);
    commandMdFlash(0x2AAAA,0x10);


    reset_command();
    setAddress(0);
    setDataInput();
    setAddress(0x20000); // CE '0'
    GPIOB_BRR |= OE;
    wait(16);
   while (!poll_dq7)
    {
        poll_dq7 = (GPIOA_IDR >> 3)&0x80; //test only dq7
    }
    GPIOB_BSRR |= OE;
    setAddress(0);
    reset_command();
}


unsigned char readRegister(unsigned char Register)
{
	unsigned char byte =0;
	GPIOA_BRR |= MIO;
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= WR;
	setAddress(0xC0000+Register); 
	GPIOB_BRR |= OE;
	setDataInput();
	directRead8();
	byte = read8;
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= WR;
	GPIOA_BSRR |= MIO;
	return byte;
	
}

void writeRegister(unsigned char Register,unsigned char value)
{
	GPIOA_BRR |= MIO;
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= WR;
	setAddress(0xC0000+Register); 
	GPIOB_BRR |= OE;
	GPIOA_BRR |= WR;
	setDataOutput();
	directWrite8(value);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= WR;
	GPIOA_BSRR |= MIO;
}

void LoadPage(unsigned char page_offset)
{
   unsigned char byte_count=0;
	j=0;
	while ( j < 16 )
	{
	  Current_Word = (usb_buffer_IN[33+byte_count]<<8) | usb_buffer_IN[32+byte_count];
	  byte_count = byte_count +2;
	  bufferMX[j+page_offset]=Current_Word;
	  j++;
	} 
}

void ClearPage()
{
	j=0;
	while ( j < 128 )
	{
	  Current_Word = 0x0000;
	  bufferMX[j]=Current_Word;
	  j++;
	} 
}

   void WritePage(usbd_device *usbd_dev){
    unsigned char poll_dq7=0;
    unsigned char word=0;
    unsigned char i=0;
    unsigned int adr=0;
    word=0;
    adr=0;

    while(address < address_max){

        //prepare MXbuffer (256 chars / 128 words)
        //read 4 x 64 bytes from PC
        for(i=0;i<4;i++){
            while(usbd_ep_read_packet(usbd_dev, 0x02, usb_buffer_IN, 64)!=64);    //read from PC
            memcpy((unsigned char *)bufferMX +(i*32), (unsigned char *)usb_buffer_IN, 64);
        }
        writeRegister(2,0);

reset_command();
    setDataInput();
    wait(16);
    while (!poll_dq7)
    {
        poll_dq7 = (GPIOA_IDR >> 3)&0x80; //test only dq7
    }

    commandMdFlash(0x2AAAA,0xAA);
    commandMdFlash(0x25555,0x55);
    commandMdFlash(0x2AAAA,0xA0);
        
        wait(60);
            word=0;
        adr=0;
        
        while(word < 128){
            commandMdFlash(address+adr+0x20000,bufferMX[word]);    
            word++;    
            adr+=2;   
        }

        
    
        wait(5000); //sur le code MD c'était 18000 (mais je n'ai pas comparé les fonctions wait(n)
        address += 256;
        setAddress(0x20000);
        GPIOB_BSRR |= OE;
        wait(16);
        reset_command();
        while(!poll_dq7){
            poll_dq7 = (GPIOA_IDR >> 3)&0x80; //test only dq7
        }
        gpio_clear(GPIOC, GPIO13);
        GPIOB_BSRR |= OE;
        reset_command();
    }  
}   


/// USB Specific Function


void SendNextPaquet(usbd_device *usbd_dev, uint8_t ep)
{

	unsigned char adr = 0;
	unsigned char adr16 = 0;

	GPIOA_BSRR |= CLK1| CLK2 | CLK3  | (CLK_CLEAR<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;


    while(adr<64){ // 64bytes/32word

		//setAddress(address+adr16);

    GPIO_CRH(GPIOA) = 0x34444333; // Set Data OUT
    GPIO_CRL(GPIOB) = 0x33333333;
    GPIO_CRH(GPIOB) = 0x33333333;

    directWrite8((address+adr16+0x20000) & 0xFF);
    GPIOA_BRR  |= CLK1;
    GPIOA_BSRR |= CLK1;

    directWrite8(((address+adr16+0x20000)>>8)&0xFF);
    GPIOA_BRR  |= CLK2;
    GPIOA_BSRR |= CLK2;

    directWrite8(((address+adr16+0x20000)>>16)&0xFF);
    GPIOA_BRR  |= CLK3;
    GPIOA_BSRR |= CLK3;

      GPIO_CRH(GPIOA) = 0x44444444; // Set Data IN
    GPIO_CRL(GPIOB) = 0x44344434;
    GPIO_CRH(GPIOB) = 0x44444444;


	  //  GPIOA_BRR |= CE;
	    GPIOB_BRR |= OE;

	    wait(16);

	    directRead16(); //save into read16 global
	    usb_buffer_OUT[adr+1] = (read16 >> 8)&0xFF; //word to byte
	    usb_buffer_OUT[(adr)] = read16 & 0xFF;

	    GPIOB_BSRR |= OE;
	   // GPIOA_BSRR |= CE;

		adr+=2;  //buffer
		adr16+=2; //md adr word
	}       
usbd_ep_write_packet(usbd_dev,ep,usb_buffer_OUT,64);
address += 64;
len +=64;
// Bankswitch//
if ( len == 65536)
	{
		bank++;
		writeRegister(2,bank);	
		len=0;
	    address=0;
	}
}


/*
* This gets called whenever a new IN packet has arrived from PC to STM32
 */

static void usbdev_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	(void)ep;
	(void)usbd_dev;

	usb_buffer_IN[0] = 0;
	cleanBuffer_OUT();
	usbd_ep_read_packet(usbd_dev, 0x01,usb_buffer_IN, 64); // Read Paquets from PC

	address = (usb_buffer_IN[3]<<16) | (usb_buffer_IN[2]<<8) | usb_buffer_IN[1];
	address_max = (usb_buffer_IN[7]<<16) | (usb_buffer_IN[6]<<8) | usb_buffer_IN[5];

	dump_running = usb_buffer_IN[4];
		

	if (usb_buffer_IN[0] == WAKEUP)   // Wake UP
   {
				memcpy((unsigned char *)usb_buffer_OUT, (unsigned char *)stmReady, sizeof(stmReady));
				usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
	}

	if (usb_buffer_IN[0] == READ_ROM && usb_buffer_IN[4] != 1 )   // READ ROM Exchange mode ( Low Speed)
   {
		dump_running = 0;
		GPIOC_BSRR |= IRQ;
		readROM();
		usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
	}

	if (usb_buffer_IN[0] == READ_ROM && usb_buffer_IN[4] == 1 )   // READ MD Streaming mode ( High Speed)
    {
		dump_running = 1;
		SendNextPaquet(usbd_dev,0x82);
	}

	if (usb_buffer_IN[0] == READ_REGISTER)   
    {
				usb_buffer_OUT[0] = readRegister(0);
				usb_buffer_OUT[1] = readRegister(1);
				usb_buffer_OUT[2] = readRegister(2);
				usb_buffer_OUT[3] = readRegister(3);
				usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
	}

	if (usb_buffer_IN[0] == WRITE_REGISTER)   
    {
				writeRegister(0,usb_buffer_IN[1]);
				writeRegister(1,usb_buffer_IN[2]);
				writeRegister(2,usb_buffer_IN[3]);
				writeRegister(3,usb_buffer_IN[4]);
				usb_buffer_OUT[0] = 0xAA;
				usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
	}

	if (usb_buffer_IN[0] == ERASE_FLASH )  // ERASE Flash
   {
		dump_running = 0;
        EraseFlash();
		usb_buffer_OUT[0]=0xFF;
		usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
		usb_buffer_OUT[6]=0x00;
   }

	if (usb_buffer_IN[0] == WRITE_FLASH)   

     {
		  WritePage(usbd_dev);
		  gpio_clear(GPIOC, GPIO13);
	}

	if (usb_buffer_IN[0] == LOAD_PAGE)   

     {
		  page_adr= usb_buffer_IN[1];
		  LoadPage(page_adr);
		  usb_buffer_OUT[0]=0xFF;
		  usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
	 }

	if (usb_buffer_IN[0] == INFOS_ID)   // Chip Information
   {
		dump_running = 0;
		infosId();
		usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
   }

	

}

/*
* This gets called whenever a new OUT packet has been send from STM32 to PC
*/

static void usbdev_data_tx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	(void)ep;
	(void)usbd_dev;


if ( dump_running == 1 ){
//GPIOA_BRR |= WE_FLASH;
SendNextPaquet(usbd_dev,0x82);}
//GPIOA_BSRR |= WE_FLASH;
}


static void usbdev_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	(void)wValue;
	(void)usbd_dev;

	usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, usbdev_data_rx_cb);
	usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, usbdev_data_tx_cb);
	usbd_ep_setup(usbd_dev, 0x02, USB_ENDPOINT_ATTR_BULK, 64, NULL); //read stream no int
}



//  Main Fonction ///// 

uint8_t usbd_control_buffer[128];

int main(void)
{
	int i=0;
	usbd_device *usbd_dev;

    // Init Clock

	rcc_clock_setup_in_hse_8mhz_out_72mhz();
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);

   /* Enable alternate function peripheral clock. */
    rcc_periph_clock_enable(RCC_AFIO);

   // Led ON

   gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
   gpio_clear(GPIOC, GPIO13); // LED on/off

    // Force USB Re-enumeration after bootloader is executed

	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	gpio_clear(GPIOA, GPIO12);
for( i = 0; i < 0x800000; i++){ __asm__("nop"); } //1sec
	get_dev_unique_id(serial_no);

     // Init USB2 Sega Dumper

	usb_setup();
	usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, usb_strings,
			3, usbd_control_buffer, sizeof(usbd_control_buffer));
	usbd_register_set_config_callback(usbd_dev, usbdev_set_config);

	for (i = 0; i < 0x800000; i++)
		__asm__("nop");

	// GPIO Init

	  //Full GPIO
      AFIO_MAPR = AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_OFF;
      GPIO_CRL(GPIOA) = 0x33333313; //always ouput (ce, clear etc) expact MARK 3 
      GPIO_CRH(GPIOA) = 0x34444333;
	  GPIO_CRL(GPIOB) = 0x33333333;
      GPIO_CRH(GPIOB) = 0x33333333;

    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, LED_PIN);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, IRQ);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, CLK_CLEAR);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, SEL);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, MIO);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, WR);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, RESET);

	GPIOB_BSRR |= OE;
    GPIOA_BSRR |= CLK_CLEAR;
	GPIOC_BSRR |= IRQ;
	
    GPIOA_BRR |= WR;
	GPIOA_BRR |= RESET;
	GPIOA_BRR |= MIO;
	GPIOA_BRR |= SEL;
	GPIOC_BSRR |= (LED_PIN<<16); //inhib
	
	gpio_set(GPIOC, GPIO13); // Turn Led OFF

	reset_command();


	dump_running = 0;



	while(1)
	{
        usbd_poll(usbd_dev);

	}

}



