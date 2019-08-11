/*
USB2 GameBoy Dumper
X-death 05/2019
*/

#include <string.h>
#include <stdlib.h>

// include only used LibopenCM3 lib

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>

// Define Dumper Pinout

#define D0 GPIO8  // PB8   GB Data lines
#define D1 GPIO9  // PB9
#define D2 GPIO10 // PB10
#define D3 GPIO11 // PB11
#define D4 GPIO12 // PB12
#define D5 GPIO13 // PB13
#define D6 GPIO14 // PB14
#define D7 GPIO15 // PB15

#define OE GPIO9  //PA9    GB Control lines
#define WE GPIO10 //PA10
#define CE GPIO8  //PA8

#define AUDIO_IN	GPIO4  // PB4  GB Extra lines
#define CPU_CLK 	GPIO7  // PB7
#define RESET		GPIO6  // PB6

#define CLK_CLEAR 	GPIO0  // PA0  Dumper MUX
#define CLK1 		GPIO3  // PA3
#define CLK2 		GPIO4  // PA4

#define LED_PIN 	GPIO13 // PC13  STM32 Led

// USB Special Command

#define WAKEUP     		0x10
#define READ_GB         0x11
#define READ_GB_SAVE  	0x12
#define WRITE_GB_SAVE 	0x13
#define WRITE_GB_FLASH 	0x14
#define ERASE_GB_FLASH 	0x15
#define CFI_MODE   		0x17
#define INFOS_ID 		0x18
#define DEBUG_MODE 		0x19

// USB Specific VAR

static char serial_no[25];
static uint8_t usb_buffer_IN[64];
static uint8_t usb_buffer_OUT[64];

// GB Dumper Specific Var

static const unsigned char stmReady[] = {'R','E','A','D','Y','!'};
static unsigned char dump_running = 0;
static unsigned long address = 0;
static unsigned char ROM_Bank = 0; // Number of ROM bank
static unsigned char RAM_Bank = 0; // Number of RAM bank
static unsigned char Mapper = 0; // Mapper Type

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
}};

static const struct usb_interface_descriptor data_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
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
    "USB2 Yes Another GB reader",
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

//  GB Dumper Specific Fonction ///// 

void wait(long nb){
    while(nb){ __asm__("nop"); nb--;}
}

void setDataInput(){
 GPIO_CRH(GPIOB) = 0x44444444; //set pb8-15 as data IN;

}

void setDataOutput(){
	GPIO_CRH(GPIOB) = 0x33333333;
	//gpio_set_mode(GPIOD, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,D0|D1|D2|D3|D4|D5|D6|D7);
}

void directWrite8(unsigned char val)
{
    GPIO_ODR(GPIOB) = (val << 8);
}

unsigned char directRead8()
{
unsigned char byte=0;
byte = (GPIO_IDR(GPIOB) & 0xFF00) >> 8; 
return byte;
 	
}


void SetAddress(unsigned short adr)
{

		setDataOutput();
        GPIO_ODR(GPIOB) = ((adr) & 0xFF) << 8; //address LSB
	    GPIO_BRR(GPIOA) |= CLK1; //low
	    GPIO_BSRR(GPIOA) |= CLK1; //high 
        //adr A8-A15
        GPIO_ODR(GPIOB) = (adr) & 0xFF00; //address MSB
    	GPIO_BRR(GPIOA) |= CLK2; //low
    	GPIO_BSRR(GPIOA) |= CLK2; //high
}



void readGB()
{

  
}

void readGBSave()
{

	
}

void writeGBSave()
{

	
}


void ResetFlash(void)
{
    
}


void EraseFlash()
{
	
}

void writeGBFlash()
{
	
}
void infosId()
{
	
}

/// USB Specific Function


void SendNextPaquet(usbd_device *usbd_dev, uint8_t ep)
{
readGB();
usbd_ep_write_packet(usbd_dev,ep,usb_buffer_OUT,64);
address += 64;
}


/*
* This gets called whenever a new IN packet has arrived from PC to STM32
 */

static void usbdev_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	(void)ep;
	(void)usbd_dev;

	usb_buffer_IN[0] = 0;
	usbd_ep_read_packet(usbd_dev, 0x01,usb_buffer_IN, 64); // Read Paquets from PC

	address = (usb_buffer_IN[3]<<16) | (usb_buffer_IN[2]<<8) | usb_buffer_IN[1];
	Mapper =  usb_buffer_IN[5];
	ROM_Bank = 2 << usb_buffer_IN[6];
	RAM_Bank = usb_buffer_IN[7];
			

	if (usb_buffer_IN[0] == WAKEUP)   // Wake UP
   {
				memcpy((unsigned char *)usb_buffer_OUT, (unsigned char *)stmReady, sizeof(stmReady));
				usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
				int i=0;
				 for(i = 0; i < 64; i++)
    {
		usb_buffer_IN[i]=0x00;
		usb_buffer_OUT[i]=0x00;
	}
	}

	if (usb_buffer_IN[0] == READ_GB && usb_buffer_IN[4] != 1 )   // READ GB Exchange mode ( Low Speed)
   {
		dump_running = 0;
		readGB();
		usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
	}

	if (usb_buffer_IN[0] == READ_GB && usb_buffer_IN[4] == 1 )   // READ GB Streaming mode ( High Speed)
   {
		dump_running = 1;
		SendNextPaquet(usbd_dev,0x82);
	}

}

/*
* This gets called whenever a new OUT packet has been send from STM32 to PC
*/

static void usbdev_data_tx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	(void)ep;
	(void)usbd_dev;
if ( dump_running == 1 )
{
SendNextPaquet(usbd_dev,0x82);
}

}


static void usbdev_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	(void)wValue;
	(void)usbd_dev;

	usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, usbdev_data_rx_cb);
	usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, usbdev_data_tx_cb);
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

     // Init USB2 GB Dumper

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
	 
	 /* gpio_clear(GPIOD,D0|D1|D2|D3|D4|D5|D6|D7);
 gpio_set_mode(GPIOE, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,A0 | A1 | A2 | A3 | A4 | A5 | A6 | A7 );
  gpio_set_mode(GPIOE, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,A8 | A9 | A10 | A11 | A12 | A13 | A14 | CE );

	GPIOB_BSRR |= WE | RESET | OE | AUDIO_IN | CPU_CLK; // "1"
	GPIOE_BSRR |= CE;*/
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, LED_PIN);
	gpio_set(GPIOC, GPIO13); // Turn Led OFF

	

	dump_running = 0;


	while(1)
	{
        usbd_poll(usbd_dev);

	}

}



