/*
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.

 *
 *  \file Megadrive-hid.C
 *  \brief ARM code for the STM32 part of MD_Dumper
 *  \author X-death for Ultimate-Consoles forum ( http://www.ultimate-consoles.fr/index)
 *  \created on 01/2017
 *
 * USB HID code is based on libopencm3 & paulfertser/stm32-tx-hid ( https://github.com/paulfertser/stm32-tx-hid)
 * i have added OUT endpoint for bi-directionnal communication and some modification in the descriptor
 * IN & OUT size is 64 bytes
 * This project is based on the ultra low cost STM32F103 minimum developpement board
 * Support Sega Megadrive cartridge dump ( max size is 64MB)
 * Support Read & Write save ( 8-32Kb & Bankswitch)
 * Support Sega MasterSystem/Mark3 cartridge dump ( max size is 4MB)
 * Please report bug at X-death@ultimate-consoles.fr and respect GPL liscence
 */


/*  Include General Functions
 */

#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/hid.h>
#include <libopencm3/usb/dfu.h>
#include <libopencmsis/core_cm3.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>

//  Include Personal Functions

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

#define OE 			GPIO1  // PB1
#define CE 			GPIO0  // PA0
#define MARK3 		GPIO1  // PA1
#define WE_FLASH	GPIO2  // PA2 	/ASEL B26
#define TIME 		GPIO3  // PA3	/TIME B31

#define CLK_CLEAR 	GPIO4  // PA4
#define CLK1 		GPIO7  // PA7
#define CLK2 		GPIO6  // PA6
#define CLK3 		GPIO5  // PA5

#define WE_SRAM 	GPIO15 // PC15	/LWR B28
#define LED_PIN 	GPIO13 // PC13


// HID Special Command
#define WAKEUP     		0x10
#define READ_MD     	0x11
#define READ_MD_SAVE  	0x12
#define WRITE_MD_SAVE 	0x13
#define WRITE_MD_FLASH 	0x14
#define ERASE_MD_FLASH 	0x15
#define READ_SMS   		0x16

#define CFI_MODE   		0x17
#define INFOS_ID   		0x18




// We need a special large control buffer for this device:

unsigned char usbd_control_buffer[5*64];

static unsigned char bufferIn[64] = {0};
static unsigned char bufferOut[64] = {0};
static unsigned char bufferZeroed[64] = {0};
static unsigned char bufferMX_256bytes[256] = {0};

static unsigned char hid_interrupt = 0;
static unsigned long address = 0;
static unsigned long write_address = 0;

static unsigned char dump_running = 0;
static unsigned char dump_running_ms = 0;

static unsigned char read8 = 0;
static unsigned int read16 = 0;

static unsigned int chipid = 0;
static unsigned int bufferSize = 0; //for MX29L3211, MX29F1610...
static unsigned int timeoutMx = 0; //for MX29L3211, MX29F1610...

static unsigned int slotRegister = 0; //for SMS
static unsigned int slotAdr = 0;


static const unsigned char stmReady[] = {'R','E','A','D','Y','!'};

static usbd_device *usbd_dev;

const struct usb_device_descriptor dev_descr =
{
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x0483,
    .idProduct = 0x5750,
    .bcdDevice = 0x0200,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const uint8_t hid_report_descriptor[] =
{

    0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,        // Usage (0x01)
    0xA1, 0x01,        // Collection (Application)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x40,        //   Usage Maximum (0x40)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x40,        //   Report Count (64)
    0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x40,        //   Usage Maximum (0x40)
    0x91, 0x00,        //   Output (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              // End Collection
};

static const struct
{
    struct usb_hid_descriptor hid_descriptor;
    struct
    {
        uint8_t bReportDescriptorType;
        uint16_t wDescriptorLength;
    } __attribute__((packed)) hid_report;
} __attribute__((packed)) hid_function =
{
    .hid_descriptor = {
        .bLength = sizeof(hid_function),
        .bDescriptorType = USB_DT_HID,
        .bcdHID = 0x0100,
        .bCountryCode = 0,
        .bNumDescriptors = 1,
    },
    .hid_report = {
        .bReportDescriptorType = USB_DT_REPORT,
        .wDescriptorLength = sizeof(hid_report_descriptor),
    }
};

const struct usb_endpoint_descriptor hid_endpoint[] =
{
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x81,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = 64,
        .bInterval = 0x01,
    },
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x01,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = 64,
        .bInterval = 0x01,
    }

};

const struct usb_interface_descriptor hid_iface =
{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = 0, // boot
    .bInterfaceProtocol = 0, // mouse
    .iInterface = 0,

    .endpoint = hid_endpoint, // fix 2

    .extra = &hid_function,
    .extralen = sizeof(hid_function),
};

const struct usb_dfu_descriptor dfu_function =
{
    .bLength = sizeof(struct usb_dfu_descriptor),
    .bDescriptorType = DFU_FUNCTIONAL,
    .bmAttributes = USB_DFU_CAN_DOWNLOAD | USB_DFU_WILL_DETACH,
    .wDetachTimeout = 255,
    .wTransferSize = 1024,
    .bcdDFUVersion = 0x011A,
};

const struct usb_interface_descriptor dfu_iface =
{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 1,
    .bAlternateSetting = 0,
    .bNumEndpoints = 0,
    .bInterfaceClass = 0xFE,
    .bInterfaceSubClass = 1,
    .bInterfaceProtocol = 1,
    // The ST Microelectronics DfuSe application needs this string.
    // The format isn't documented...
    .iInterface = 4,

    .extra = &dfu_function,
    .extralen = sizeof(dfu_function),
};

const struct usb_interface ifaces[] = {{
        .num_altsetting = 1,
        .altsetting = &hid_iface,
    }, {
        .num_altsetting = 1,
        .altsetting = &dfu_iface,
    }
};

const struct usb_config_descriptor config =
{
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 2,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0xC0,
    .bMaxPower = 0x32,

    .interface = ifaces,
};

static int hid_control_request(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
                               void (**complete)(usbd_device *dev, struct usb_setup_data *req))
{
    (void)complete;
    (void)dev;

    if((req->bmRequestType != 0x81) ||
            (req->bRequest != USB_REQ_GET_DESCRIPTOR) ||
            (req->wValue != 0x2200))
        return 0;

    // Handle the HID report descriptor.
    *buf = (uint8_t *)hid_report_descriptor;
    *len = sizeof(hid_report_descriptor);

    return 1;
}

static void dfu_detach_complete(usbd_device *dev, struct usb_setup_data *req)
{
    (void)req;
    (void)dev;

    scb_reset_core();
}

static int dfu_control_request(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
                               void (**complete)(usbd_device *dev, struct usb_setup_data *req))
{
    (void)buf;
    (void)len;
    (void)dev;

    if ((req->bmRequestType != 0x21) || (req->bRequest != DFU_DETACH))
        return 0; // Only accept class request.

    *complete = dfu_detach_complete;

    return 1;
}

static void hid_set_config(usbd_device *dev, uint16_t wValue)
{
    (void)wValue;

    usbd_ep_setup(dev, 0x81, USB_ENDPOINT_ATTR_INTERRUPT, 64, NULL);
    usbd_ep_setup(dev, 0x01, USB_ENDPOINT_ATTR_INTERRUPT, 64, NULL);

    usbd_register_control_callback(
        dev,
        USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
        USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
        hid_control_request);
    usbd_register_control_callback(
        dev,
        USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
        USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
        dfu_control_request);

    //systick configuration

    // 72MHz / 8 => 9000000 counts per second : 9 Mhz
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    // Systick interrupt period 1ms -> 9MHz / 1kHz = 9000
    systick_set_reload(9000 - 1);  //  1 ms interrupt
    systick_interrupt_enable();
    systick_counter_enable();

}

static char serial_no[25];

static const char *usb_strings[] =
{
    "libopencm3",
    "HID Megadrive Dumper",
    serial_no,
    // This string is used by ST Microelectronics' DfuSe utility.
    "@Internal Flash   /0x08000000/8*001Ka,56*001Kg",
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
	bufferOut[0] = check & 0xff;
	bufferOut[1] = (check >> 8)&0xff;
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

void readMd(){

	unsigned char adr = 0;
	unsigned char adr16 = 0;

    setDataOutput();

	GPIOA_BSRR |= CE | CLK1| CLK2 | CLK3 | TIME | WE_FLASH | (CLK_CLEAR<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;


    while(adr<64){ // 64bytes/32word

		setAddress(address+adr16);
	    setDataInput();

	    GPIOA_BRR |= CE;
	    GPIOB_BRR |= OE;

	    wait(16);

	    directRead16(); //save into read16 global
	    bufferOut[adr] = (read16 >> 8)&0xFF; //word to byte
	    bufferOut[(adr+1)] = read16 & 0xFF;

	    GPIOB_BSRR |= OE;
	    GPIOA_BSRR |= CE;

		adr+=2;  //buffer
		adr16++; //md adr word
	}

}


void readMdSave(){

	unsigned char adr = 0;

    setDataOutput();

	GPIOA_BSRR |= CE | CLK1| CLK2 | CLK3 | TIME | WE_FLASH | (CLK_CLEAR<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;

    while(adr<64){

		setAddress((address+adr));

        GPIOB_BSRR |= D0;
    	GPIOA_BRR  |= TIME;
        GPIOA_BSRR  |= TIME;

	    setDataInput();

   		GPIOA_BRR  |= CE;
	    GPIOB_BRR  |= OE;

	    wait(16); //utile ?
	    directRead8(); //save into read8 global
	    bufferOut[adr] = read8;

	 	//inhib OE
	    GPIOA_BSRR |= CE;
	    GPIOB_BSRR |= OE;

		adr++;
	}
}

void writeMdSave(){

	unsigned char adr = 0;

    setDataOutput();

	GPIOA_BSRR |= CE | CLK1| CLK2 | CLK3 | TIME | WE_FLASH | (CLK_CLEAR<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;

	// SRAM rom > 16Mbit
	GPIOB_BSRR |= D0;
	GPIOA_BRR  |= TIME;
	GPIOA_BSRR |= TIME;

	while(adr < bufferIn[4]){
		setAddress((address+adr));
		GPIOA_BRR  |= CE;
	    GPIOC_BRR  |= WE_SRAM;
		// wait(16); //utile ?
		directWrite8(bufferIn[(5+adr)]);
		GPIOC_BSRR |= WE_SRAM;
		GPIOA_BSRR |= CE;
		adr++;
	}
}


void commandMdFlash(unsigned long adr, unsigned int val){

	setAddress(adr);
	GPIOA_BRR  |= CE;
	GPIOA_BRR  |= WE_FLASH;
	directWrite16(val);
	GPIOA_BSRR |= WE_FLASH;
	GPIOA_BSRR |= CE;
}


void reset_command(){
	setDataOutput();
	commandMdFlash(0x5555, 0xAA);
	commandMdFlash(0x2AAA, 0x55);
	commandMdFlash(0x5555, 0xF0);
	wait(16);
}


void eraseMdFlash(){

	unsigned char poll_dq7=0;

    setDataOutput();

	GPIOA_BSRR |= CE | CLK1| CLK2 | CLK3 | TIME | WE_FLASH | (CLK_CLEAR<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;

    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x80);
    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x10);

	if(((chipid&0xFF00)>>8) == 0xBF){
		//SST-MICROCHIP
		wait(2400000);

		}else{

		reset_command();

		setAddress(0);
		setDataInput();
		GPIOA_BRR |= CE;
		GPIOB_BRR |= OE;
		wait(16);
		while(!poll_dq7){
			poll_dq7 = (GPIOA_IDR >> 3)&0x80; //test only dq7
		}
		GPIOB_BSRR |= OE;
		GPIOA_BSRR |= CE;
	}

	reset_command();
	bufferOut[0] = 0xFF;
}



void eraseMdFlash_29L3211(){

	unsigned char poll_dq7=0;

    setDataOutput();

	GPIOA_BSRR |= CE | CLK1| CLK2 | CLK3 | TIME | WE_FLASH | (CLK_CLEAR<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;

    commandMdFlash(0x5555, 0xAA); //erase command
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x80);
    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x10);

	wait(30);
	GPIOA_BRR |= CE;
	wait(10);
	GPIOB_BRR |= OE;

	reset_command();

	setDataInput();
    while(poll_dq7!=0x80){
    	directRead16();
	    poll_dq7 = (read16 & 0x80);
    }

	GPIOB_BSRR |= OE;
	wait(10);
	GPIOA_BSRR |= CE;

	reset_command();
	bufferOut[0] = 0xFF;
}

void writeMdFlash(){
	/*
	compatible
	29LV160 (amd)
	29LV320 (amd)
	29LV640 (amd)
	29W320 (st)
	29F800 (hynix)

	*/

	//write in WORD
	unsigned char adr16=0;
	unsigned char j=5;
	unsigned char poll_dq7=0;
	unsigned char true_dq7=0;
	unsigned int val16=0;

    setDataOutput();

	GPIOA_BSRR |= CE | CLK1| CLK2 | CLK3 | TIME | WE_FLASH | (CLK_CLEAR<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;

    while(adr16 < (bufferIn[4]>>1)){

		val16 = ((bufferIn[j])<<8) | bufferIn[(j+1)];
		true_dq7 = (val16 & 0x80);
		poll_dq7 = ~true_dq7;

		if(val16!=0xFFFF){
		    setDataOutput();

		    commandMdFlash(0x5555, 0xAA);
	    	commandMdFlash(0x2AAA, 0x55);
	    	commandMdFlash(0x5555, 0xA0);
			commandMdFlash((address+adr16), val16);

			if(((chipid&0xFF00)>>8) == 0xBF){
				wait(160); //SST Microchip
			}else{

                reset_command();

				GPIOA_BRR |= CE;
				GPIOB_BRR |= OE;
				setAddress((address+adr16));
				setDataInput();
			    while(poll_dq7 != true_dq7){
				    poll_dq7 = (GPIOA_IDR&0x400)>>3;
			    }
				GPIOB_BSRR |= OE;
				GPIOA_BSRR |= CE;
			}
		}
		j+=2;
		adr16++;
    }

}



void writeMdFlash_29L3211(){
	// works for 29L3211
	// write in WORD - buffer 128 word/256 bytes
	unsigned char adr16=0;
	unsigned char poll_dq7=0;
	unsigned int val16=0;
	unsigned int j=0;

    setDataOutput();

	GPIOA_BSRR |= CE | CLK1| CLK2 | CLK3 | TIME | WE_FLASH | (CLK_CLEAR<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;

    //automatique page 128 word/256 bytes
	commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0xA0);
	wait(60);

    while(adr16 < (bufferSize/2)){

		val16 = ((bufferMX_256bytes[j])<<8) | bufferMX_256bytes[(j+1)];

		setAddress(write_address +adr16);
		GPIOA_BRR |= CE;
		GPIOA_BRR |= WE_FLASH;
	    directWrite16(val16);
		wait(60);
	    GPIOA_BSRR |= WE_FLASH;
	    GPIOA_BSRR |= CE;
		wait(60);

		j+=2;
		adr16++;
    }

	wait(timeoutMx);

	GPIOA_BRR |= CE;
	wait(10);
	GPIOB_BRR |= OE;
	wait(30);

	reset_command();

	setDataInput();
    while(poll_dq7!=0x80){
    	directRead16();
	    poll_dq7 = (read16 & 0x80);
    }

	GPIOB_BSRR |= OE;
	wait(10);
	GPIOA_BRR |= CE;

	reset_command();

}

void requestCfiAdr(unsigned int adr, unsigned char buf_index){

	setAddress(adr); //dataout in this function

    setDataInput();
    GPIOA_BRR |= CE;
    GPIOB_BRR |= OE;
	wait(16);
    directRead16();
    bufferOut[buf_index] = read16&0xFF;
    GPIOB_BSRR |= OE;
    GPIOA_BSRR |= CE;

}

void cfiMode(){

    setDataOutput();

	GPIOA_BSRR |= CE | CLK1| CLK2 | CLK3 | TIME | WE_FLASH | (CLK_CLEAR<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;

    commandMdFlash(0x55, 0x98);

    requestCfiAdr(0x10, 0); //Q
    requestCfiAdr(0x11, 1); //R
    requestCfiAdr(0x12, 2); //Y
    requestCfiAdr(0x13, 3); //algo command
    requestCfiAdr(0x14, 4); //algo command
    requestCfiAdr(0x27, 5); //size 2n Bytes
    requestCfiAdr(0x1B, 6); //VCC min
    requestCfiAdr(0x1C, 7); //VCC max
    requestCfiAdr(0x21, 8); //timeout block erase (typ)
    requestCfiAdr(0x22, 9); //timeout chip erase (typ)
    requestCfiAdr(0x28, 10); //Flash Device Descr. 0
    requestCfiAdr(0x29, 11); //Flash Device Descr. 1

	reset_command();

}

void infosId(){
	//seems to be valid only for 29LVxxx ?
    setDataOutput();

	GPIOA_BSRR |= CE | CLK1| CLK2 | CLK3 | TIME | WE_FLASH | (CLK_CLEAR<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;

    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x90);

	setAddress(0); //Manufacturer
    setDataInput();

    GPIOA_BRR |= CE;
    GPIOB_BRR |= OE;
	wait(16);
    directRead16();
    bufferOut[0] = 0;
    bufferOut[1] = read16&0xFF;
    GPIOB_BSRR |= OE;
    GPIOA_BSRR |= CE;

	reset_command();

    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x90);

	setAddress(1); //Flash id
    setDataInput();
    GPIOA_BRR |= CE;
    GPIOB_BRR |= OE;
	wait(16);
    directRead16();
    bufferOut[2] = 1;
    bufferOut[3] = read16&0xFF;
    GPIOB_BSRR |= OE;
    GPIOA_BSRR |= CE;

    reset_command();

	chipid = (bufferOut[1]<<8) | bufferOut[3];
}


void sms_mapper_register(unsigned char slot){

    setDataOutput();

    setAddress(slotRegister);
 	GPIOA_BSRR |= CE; //A15 hi
   	GPIOB_BRR  |= OE;
    GPIOC_BRR  |= WE_SRAM;

    directWrite8(slot); //slot 0..2

    GPIOC_BSRR |= WE_SRAM;
   	GPIOB_BSRR |= OE;
	GPIOA_BRR  |= CE; //A15 low

}

void readSms(){

	unsigned char adr = 0;
	unsigned char pageNum = 0;

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, MARK3); //Mark as output (low)
	GPIOA_BSRR |= CLK1| CLK2 | CLK3 | TIME | WE_FLASH | ((CE | MARK3 | CLK_CLEAR)<<16);
	GPIOB_BSRR |= OE;
	GPIOA_BSRR |= CLK_CLEAR;

   	if(address < 0x4000){
  		slotRegister = 0xFFFD; 	// slot 0 sega
  		pageNum = 0;
   		slotAdr = address;
   	}else if(address < 0x8000){
  		slotRegister = 0xFFFE; 	// slot 1 sega
  		pageNum = 1;
   		slotAdr = address;
   	}else{
  		slotRegister = 0xFFFF; 	// slot 2 sega
  		pageNum = (address/0x4000); //page num max 0xFF - 32mbits
   		slotAdr = 0x8000 + (address & 0x3FFF);
   	}

	sms_mapper_register(pageNum);

	if(slotAdr > 0x7FFF){GPIOA_BSRR |= CE;} //CE md == A15 in SMS mode !

    while(adr<64){

		setAddress(slotAdr +adr);

	    setDataInput();
	    GPIOB_BRR |= OE;

	    wait(16);

	    directRead8(); //save into read8 global
	    bufferOut[adr] = read8;

	    GPIOB_BSRR |= OE;
		adr++;

	}

}



static void usb_suspend_callback(void){
    *USB_CNTR_REG |= USB_CNTR_FSUSP;
    *USB_CNTR_REG |= USB_CNTR_LP_MODE;
    SCB_SCR |= SCB_SCR_SLEEPDEEP;
    __WFI();
}


void usb_wakeup_isr(void){
    exti_reset_request(EXTI18);
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    *USB_CNTR_REG &= ~USB_CNTR_FSUSP;
}


void sys_tick_handler(void){

    if(hid_interrupt == 1){

    	bufferIn[0] = 0;
		usbd_ep_read_packet(usbd_dev, 0x01, bufferIn, 64);

		if(dump_running){
			if(bufferIn[0]==0xFF){
				dump_running = 0; //stop
				}else{
				bufferIn[0] = READ_MD;
			}
		}else if(dump_running_ms){
			if(bufferIn[0]==0xFF){
				dump_running_ms = 0; //stop
			    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_INPUT_FLOAT, MARK3); //inhib MK3 pin
				}else{
				bufferIn[0] = READ_SMS;
			}
		}else{
			address = (bufferIn[3]<<16) | (bufferIn[2]<<8) | bufferIn[1];
		}

 		switch(bufferIn[0]){
	 		case WAKEUP:
	 		case READ_MD:
	 		case READ_MD_SAVE:
	 		case WRITE_MD_SAVE:
	 		case WRITE_MD_FLASH:
	 		case ERASE_MD_FLASH:
	 		case READ_SMS:
	 		case CFI_MODE:
			case INFOS_ID:
 				hid_interrupt = bufferIn[0];
 				break;

 			default:
 				hid_interrupt = 1;
				break;
		}

    }

}

static unsigned int buffer256bytes[3] = {0}; //offset, restant, a copier

int main(){

    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);

    /* Enable alternate function peripheral clock. */
    rcc_periph_clock_enable(RCC_AFIO);

    /* Interrupt for USB */
    exti_set_trigger(EXTI18, EXTI_TRIGGER_RISING);
    exti_enable_request(EXTI18);
    nvic_enable_irq(NVIC_USB_WAKEUP_IRQ);

    /*Setup Programming mode */
    gpio_set_mode(GPIOC, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO14);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
	//led clignotante programming mode
    if(GPIOC_IDR & GPIO14){
        while(1){
            AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_FULL_SWJ_NO_JNTRST;
            gpio_toggle(GPIOC, GPIO13);	/* LED on/off */
            for(unsigned long i = 0; i < 0x100000; i++){ __asm__("nop"); }
        }
    }

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
    gpio_clear(GPIOA, GPIO12);

    for(unsigned long i = 0; i < 0x800000; i++){ __asm__("nop"); } //1sec

	get_dev_unique_id(serial_no);

    usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config, usb_strings, 4, usbd_control_buffer, sizeof(usbd_control_buffer));
    usbd_register_set_config_callback(usbd_dev, hid_set_config);
    usbd_register_suspend_callback(usbd_dev, usb_suspend_callback);

	//Full GPIO
	AFIO_MAPR = AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_OFF;

 	GPIO_CRL(GPIOA) = 0x33333313; //always ouput (ce, clear etc) expact MARK 3 which is connected to GND on carttridge MD>SMS

    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, LED_PIN);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, WE_SRAM);
    GPIOC_BSRR |= WE_SRAM | (LED_PIN<<16); //inhib

	dump_running = 0;
	dump_running_ms = 0;
    hid_interrupt = 1; // Enable HID Interrupt

    while(1){

        usbd_poll(usbd_dev);

		if(hid_interrupt!=1){

			switch(hid_interrupt){
				case WAKEUP:
		        	memcpy((unsigned char *)bufferOut, (unsigned char *)stmReady, sizeof(stmReady));
					break;

				case READ_MD: 		//read 16bits
					if(dump_running){ address += 32; }
					dump_running = bufferIn[4];
					readMd();
					break;

				case READ_MD_SAVE:	//read 8bits
					readMdSave();
		            break;

				case WRITE_MD_SAVE: //write/erase SRAM
					writeMdSave();

					readMdSave(); // make a checksum/verif this part can be removed as write is stable
					buffer_checksum((unsigned char *)bufferOut, bufferIn[4]);
					memcpy((unsigned char *)bufferOut +2, (unsigned char *)bufferZeroed, 62); //keep checksum only
		            break;

		       	case WRITE_MD_FLASH:

					switch(chipid){
						case 0xC2F9:
						case 0xC2F1:
	       					//special 29F1610 - buffer 128 bytes (64 words)
	       					//special 29L3211 - buffer 256 bytes (128 words)

							if((buffer256bytes[0]+bufferIn[4]) < bufferSize){

			       				memcpy((unsigned char *)bufferMX_256bytes +buffer256bytes[0], (unsigned char *)bufferIn +5, bufferIn[4]);
								buffer256bytes[0] += bufferIn[4];

							}else{

			       				buffer256bytes[1] = ((buffer256bytes[0]+bufferIn[4]) - bufferSize); //save next loop
			       				buffer256bytes[2] = (bufferIn[4] - buffer256bytes[1]); //to copy
		       					memcpy((unsigned char *)bufferMX_256bytes +(bufferSize - buffer256bytes[2]), (unsigned char *)bufferIn +5, buffer256bytes[2]);

								writeMdFlash_29L3211();

			       				if(buffer256bytes[2]){
			       					memcpy((unsigned char *)bufferMX_256bytes, (unsigned char *)bufferIn +(5+buffer256bytes[2]), buffer256bytes[1]);
			       					buffer256bytes[0] = buffer256bytes[1]; //save next offset
			       					}else{
			       					buffer256bytes[0] = 0;
			       				}
			       				buffer256bytes[1] = 0; //raz
			       				buffer256bytes[2] = 0; //raz
								write_address += (bufferSize/2); //next adr

							}
							break;

						default:
							writeMdFlash();
					}

			       	break;

		       	case ERASE_MD_FLASH:
		       		infosId();
					switch(chipid){
						case 0xC2F1: //29F1610
							eraseMdFlash_29L3211();
	       					bufferSize = 128; //value in bytes
	       					timeoutMx = 22000;
	       					break;

						case 0xC2F9: //29L3211
							eraseMdFlash_29L3211();
	       					bufferSize = 256;
	       					timeoutMx = 18000;
							break;

						default:
							eraseMdFlash();
					}
					write_address = 0; //raz
			       	break;

				case READ_SMS:
					if(dump_running_ms){ address += 64; }else{address = 0;}
					dump_running_ms = bufferIn[4];
					readSms();
					break;

		       	case CFI_MODE:
					memcpy((unsigned char *)bufferOut, (unsigned char *)bufferZeroed, 64);
					cfiMode();
			       	break;

		       	case INFOS_ID:
		       		memcpy((unsigned char *)bufferOut, (unsigned char *)bufferZeroed, 64);
					infosId();
			       	break;



			}
			while( usbd_ep_write_packet(usbd_dev, 0x81, bufferOut, 64) != 64);
			hid_interrupt = 1;
		}

	}

}
