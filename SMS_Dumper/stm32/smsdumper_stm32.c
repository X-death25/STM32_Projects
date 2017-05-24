/*  
SMS dumper v0
STM32F103CT8
*/

#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/hid.h>
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencmsis/core_cm3.h>

#include <libopencm3/cm3/scb.h>
#include <libopencm3/usb/dfu.h>

#define D0 GPIO8  // PB8
#define D1 GPIO9  // PB9
#define D2 GPIO10 // PB10
#define D3 GPIO11 // PB11
#define D4 GPIO12 // PB12
#define D5 GPIO13 // PB13
#define D6 GPIO14 // PB14
#define D7 GPIO15 // PB15

#define CE GPIO8  //PA8
#define OE GPIO9  //PA9
#define WE GPIO10 //PA10

#define CLOCK GPIO3 //PB4  - only for codemasters
#define MREQ GPIO4 //PB3   - only for codemasters and MK3

#define CLEAR_273 GPIO0  // PA0
#define CLK_273N1 GPIO3  // PA3 
#define CLK_273N2 GPIO4  // PA4

#define SMS_WAKEUP    		0x11  // WakeUP STM32
#define SMS_READ      		0x12  // Read
#define SMS_WRITE     		0x13  // Write
#define SMS_WRITE_FLASH     0x14  // Write Flash
#define SMS_ERASE_FLASH     0x15  // Write Flash
#define SMS_FLASH_ID   	  	0x16  // Chip ID

#define CLEAR_ALL_PINS	0x44444444

// We need a special large control buffer for this device:
unsigned char usbd_control_buffer[5*64];

unsigned char buffer_in[64];
unsigned char buffer_out[64];
const unsigned char buffer_zeroed[64] = {0}; 
unsigned char hid_interrupt = 0;
unsigned int address = 0; //0xFFFF max

static usbd_device *usbd_dev;
static char serial_no[25];

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
    0x19, 0x01,        // Usage Minimum (0x01)
    0x29, 0x40,        // Usage Maximum (0x40)
    0x15, 0x00,        // Logical Minimum (0)
    0x26, 0xFF, 0x00,  // Logical Maximum (255)
    0x75, 0x08,        // Report Size (8)
    0x95, 0x40,        // Report Count (64)
    0x81, 0x00,        // Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x19, 0x01,        // Usage Minimum (0x01)
    0x29, 0x40,        // Usage Maximum (0x40)
    0x91, 0x00,        // Output (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
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
    STK_CVR = 0;
    systick_set_reload(8999);
    systick_interrupt_enable();
    systick_counter_enable();
}

static const char *usb_strings[] =
{
    "libopencm3",
    "SMS Dumper",
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



void wait(unsigned char n){
    while(n){ 
    	__asm__("nop"); 
    	n--;
    }
}


void enable_cart(){
	GPIO_BRR(GPIOA) |= (CE | OE);
}

void disable_cart(){
	GPIO_BSRR(GPIOA) |= (CE | OE);
}


void mapper_write_register(){
	
    //set adr MAPPER register
    GPIO_CRH(GPIOB) = 0x33333333; //set pb8-15 as data OUT
  
    GPIO_ODR(GPIOB) = (buffer_in[5]<<8); //MAPPER adr low
    GPIO_BRR(GPIOA)  |= CLK_273N1;
    GPIO_BSRR(GPIOA) |= CLK_273N1;

    GPIO_ODR(GPIOB) = (buffer_in[6]<<8); //MAPPER adr hi
   	GPIO_BRR(GPIOA)  |= CLK_273N2;
   	GPIO_BSRR(GPIOA) |= CLK_273N2;
   	
   	GPIO_ODR(GPIOB) = 0;
   	
   	enable_cart();
    GPIO_ODR(GPIOB) = (buffer_in[4]<<8); //MAPPER page D0-D7
	GPIO_BRR(GPIOA) |= WE;
	//only used by codemasters
	if(buffer_in[7] && hid_interrupt==SMS_READ){ 
		GPIO_BRR(GPIOB) |= CLOCK;
		GPIO_BSRR(GPIOB) |= CLOCK;
	}
    __asm__("nop");
	GPIO_BSRR(GPIOA) |= WE;
	disable_cart();
}




void read_cartridge(){
  	unsigned char adr = 0;

    GPIO_CRL(GPIOA) = 0x44433443; //out Clk1-3, et MRclear
    GPIO_CRH(GPIOA) = 0x44444333; //all out CE, etc. 50mhz
	GPIO_CRL(GPIOB) = 0x44433444;  //set clock/mreq for codemasters mapper
  	GPIO_BSRR(GPIOB) |= CLOCK | (MREQ << 16);

	//Clk273n1, Clk273n2, Clk273n3, WE = High - Clear273
  	GPIO_BSRR(GPIOA) |= (CLK_273N1 | CLK_273N2 | WE | CE | OE) | (CLEAR_273 << 16);
    GPIO_BSRR(GPIOA) |= CLEAR_273; //active 273.
	
	if(buffer_in[3]){ mapper_write_register(); }
  	
  	while(adr < 64){
        GPIO_CRH(GPIOB) = 0x33333333; //set pb8-15 as data OUT

        //adr A0-A7
        GPIO_ODR(GPIOB) = ((address +adr) & 0xFF) << 8; //address LSB
	    GPIO_BRR(GPIOA) |= CLK_273N1; //low
	    GPIO_BSRR(GPIOA) |= CLK_273N1; //high
        
        //adr A8-A15
        GPIO_ODR(GPIOB) = (address +adr) & 0xFF00; //address MSB
    	GPIO_BRR(GPIOA) |= CLK_273N2; //low
    	GPIO_BSRR(GPIOA) |= CLK_273N2; //high
		
        GPIO_CRH(GPIOB) = 0x44444444; //set pb8-15 as data IN
		
		enable_cart();
        wait(16);      
        buffer_out[adr] = (GPIO_IDR(GPIOB) & 0xFF00) >> 8; //save into buffer
		disable_cart();
		adr++; //next adr/byte
  	}
  	//clear all IO
    GPIO_CRL(GPIOA) = CLEAR_ALL_PINS;
    GPIO_CRH(GPIOA) = CLEAR_ALL_PINS;
	GPIO_CRH(GPIOB) = CLEAR_ALL_PINS;

}

void write_sram(){
  	unsigned char adr = 0;

    GPIO_CRL(GPIOA) = 0x44433443; //out Clk1-3, et MRclear
    GPIO_CRH(GPIOA) = 0x44444333; //all out CE, etc. 50mhz

  	GPIO_BSRR(GPIOA) |= (CLK_273N1 | CLK_273N2 | WE | CE | OE) | (CLEAR_273 << 16);
    GPIO_BSRR(GPIOA) |= CLEAR_273; //active 273.
	
	mapper_write_register(); //place & activ SRAM   	
   	
   	while(adr < 32){
		
		//PLACE ADR to WRITE
      	GPIO_ODR(GPIOB) = ((address +adr) & 0xFF) << 8; //address LSB
	    GPIO_BRR(GPIOA) |= CLK_273N1; //low
	    GPIO_BSRR(GPIOA) |= CLK_273N1; //high
	           
        GPIO_ODR(GPIOB) = (address +adr) & 0xFF00; //address MSB
    	GPIO_BRR(GPIOA) |= CLK_273N2; //low
    	GPIO_BSRR(GPIOA) |= CLK_273N2; //high
   	
   		GPIO_ODR(GPIOB) = 0;
   		
		//PLACE BYTE to WRITE
		enable_cart();
		GPIO_BRR(GPIOA) |= WE;
	    GPIO_ODR(GPIOB) = (buffer_in[(adr +7)]<<8);
		GPIO_BSRR(GPIOA) |= WE;
		disable_cart();
	   	adr++; //next adr/byte
	}
  	//clear all IO
    GPIO_CRL(GPIOA) = CLEAR_ALL_PINS;
    GPIO_CRH(GPIOA) = CLEAR_ALL_PINS;
	GPIO_CRH(GPIOB) = CLEAR_ALL_PINS;

}


void command_write_flash(unsigned int flash_adr, unsigned char flash_val){

	GPIO_ODR(GPIOB) = ((flash_adr & 0xFF)<<8); //adr flash lo
    GPIO_BRR(GPIOA) |= CLK_273N1;
    GPIO_BSRR(GPIOA) |= CLK_273N1;

    GPIO_ODR(GPIOB) = (flash_adr & 0xFF00); //adr flash hi
   	GPIO_BRR(GPIOA) |= CLK_273N2;
   	GPIO_BSRR(GPIOA) |= CLK_273N2;
	
	GPIO_ODR(GPIOB) = (flash_val << 8);
	GPIO_BRR(GPIOA) |= WE;	//set adr
	wait(16);
	GPIO_BSRR(GPIOA) |= WE; //set data
}


void erase_flash(){
	unsigned long timeout = 0x400000;
	
    GPIO_CRL(GPIOA) = 0x44433443; //out Clk1-3, et MRclear
    GPIO_CRH(GPIOA) = 0x44444333; //all out CE, etc. 50mhz

  	GPIO_BSRR(GPIOA) |= (CLK_273N1 | CLK_273N2 | WE | CE) | (CLEAR_273 << 16);
    GPIO_BSRR(GPIOA) |= CLEAR_273; //active 273.

//	buffer_in[4] = 0;
//	buffer_in[5] = 0xFF;
//	buffer_in[6] = 0xFD; 
//	mapper_write_register();
//	wait(16);
//
//	buffer_in[4] = 1;
//	buffer_in[5] = 0xFF;
//	buffer_in[6] = 0xFE; 
//	mapper_write_register();
    
    GPIO_CRH(GPIOB) = 0x33333333; //set pb8-15 as data OUT
	wait(16);
	
	command_write_flash(0x5555, 0xAA); 
	command_write_flash(0x2AAA, 0x55); 
	command_write_flash(0x5555, 0x80); 
	command_write_flash(0x5555, 0xAA); 
	command_write_flash(0x2AAA, 0x55); 
	command_write_flash(0x5555, 0x10); 
	
    while(timeout){ __asm__("nop"); timeout--; }

    GPIO_CRL(GPIOA) = CLEAR_ALL_PINS;
    GPIO_CRH(GPIOA) = CLEAR_ALL_PINS;
	GPIO_CRH(GPIOB) = CLEAR_ALL_PINS;

}


void write_flash(){
  	unsigned char adr = 0; //start @7
	unsigned char prg_val = 0;
	unsigned int prg_adr = 0;
	
    GPIO_CRL(GPIOA) = 0x44433443; //out Clk1-3, et MRclear
    GPIO_CRH(GPIOA) = 0x44444333; //all out CE, etc. 50mhz

  	GPIO_BSRR(GPIOA) |= (CLK_273N1 | CLK_273N2 | WE | CE ) | (CLEAR_273 << 16);
    GPIO_BSRR(GPIOA) |= CLEAR_273; //active 273.
		
	mapper_write_register();

   	while(adr < 32){
   		prg_adr = address+adr;
   		prg_val = buffer_in[(adr +7)];

  		if(prg_val!=0xFF){ //skip 0xFF to boost
			command_write_flash(0x5555, 0xAA); 
			command_write_flash(0x2AAA, 0x55); 
			command_write_flash(0x5555, 0xA0); 
			command_write_flash(prg_adr, prg_val); 
		    for(unsigned int timeout=0; timeout<0x140; timeout++){ __asm__("nop"); }
  		}	
		adr++; //next adr/byte
	}
	
  	//clear all IO
    GPIO_CRL(GPIOA) = CLEAR_ALL_PINS;
    GPIO_CRH(GPIOA) = CLEAR_ALL_PINS;
	GPIO_CRH(GPIOB) = CLEAR_ALL_PINS;
 
}

/*
void flash_id(){
	
    GPIO_CRL(GPIOA) = 0x44433443; //out Clk1-3, et MRclear
    GPIO_CRH(GPIOA) = 0x44444333; //all out CE, etc. 50mhz

  	GPIO_BSRR(GPIOA) |= (CLK_273N1 | CLK_273N2 | WE | CE ) | (CLEAR_273 << 16);
    GPIO_BSRR(GPIOA) |= CLEAR_273; //active 273.

	//ENTRY chip ID
    GPIO_CRH(GPIOB) = 0x33333333; //as out
	command_write_flash(0x5555, 0xAA);
	command_write_flash(0x2AAA, 0x55);
	command_write_flash(0x5555, 0x90);
   	
   	wait(150);
   		
    GPIO_ODR(GPIOB) = 0;
	GPIO_BRR(GPIOA) |= CLK_273N1;
    GPIO_BSRR(GPIOA) |= CLK_273N1;
   	GPIO_BRR(GPIOA) |= CLK_273N2;
   	GPIO_BSRR(GPIOA) |= CLK_273N2; 
   			
	GPIO_CRH(GPIOB) = 0x44444444; //as in
 	GPIO_BRR(GPIOA) |= CE;
    buffer_out[0] = (GPIO_IDR(GPIOB) & 0xFF00) >> 8; //save buffer
 	GPIO_BSRR(GPIOA) |= CE;
//
//    GPIO_CRH(GPIOB) = 0x33333333; //as out
//    GPIO_ODR(GPIOB) = 1;
//	GPIO_BRR(GPIOA) |= CLK_273N1;
//    GPIO_BSRR(GPIOA) |= CLK_273N1;
//   	GPIO_BRR(GPIOA) |= CLK_273N2;
//   	GPIO_BSRR(GPIOA) |= CLK_273N2;	
//	GPIO_CRH(GPIOB) = 0x44444444; //as in
//	
// 	GPIO_BRR(GPIOA) |= CE;
//    buffer_out[1] = (GPIO_IDR(GPIOB) & 0xFF00) >> 8; //save buffer
// 	GPIO_BSRR(GPIOA) |= CE;
//
    buffer_out[2] = 0xFF; //save into buffer

	//EXIT chip ID
    GPIO_CRH(GPIOB) = 0x33333333; //as out
	command_write_flash(0x5555, 0xAA); 
	command_write_flash(0x2AAA, 0x55); 
	command_write_flash(0x5555, 0xF0); 
		
    GPIO_CRL(GPIOA) = CLEAR_ALL_PINS;
    GPIO_CRH(GPIOA) = CLEAR_ALL_PINS;
	GPIO_CRH(GPIOB) = CLEAR_ALL_PINS;	

}
*/

static void usb_suspend_callback(){
    *USB_CNTR_REG |= USB_CNTR_FSUSP;
    *USB_CNTR_REG |= USB_CNTR_LP_MODE;
    SCB_SCR |= SCB_SCR_SLEEPDEEP;
    __WFI();
}

void usb_wakeup_isr(){
    exti_reset_request(EXTI18);
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    *USB_CNTR_REG &= ~USB_CNTR_FSUSP;
}


// Function Called Every Ticks

void sys_tick_handler(void){
    
    if(hid_interrupt == 1){
		buffer_in[0] = 0;
        usbd_ep_read_packet(usbd_dev, 0x01, buffer_in, sizeof(buffer_in));
        
        switch(buffer_in[0]){
	        case SMS_WAKEUP:
	        case SMS_READ:
	        case SMS_WRITE:
	        case SMS_WRITE_FLASH:
	        case SMS_ERASE_FLASH:
//	        case SMS_FLASH_ID: 	
	        	hid_interrupt = buffer_in[0]; break;
	        	
	        default: 
	        	memcpy((unsigned char *)buffer_in, (unsigned char *)buffer_zeroed, sizeof(buffer_in));
				hid_interrupt = 1;	        	
        }
    }
}


int main(void){
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
    if (GPIOC_IDR & GPIO14){
        while(1){
            AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_FULL_SWJ_NO_JNTRST;
            gpio_toggle(GPIOC, GPIO13);	/* LED on/off */
            for(unsigned long i = 0; i < 0x100000; i++){ __asm__("nop"); }
        }
    }

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
    gpio_clear(GPIOA, GPIO12);

    for(unsigned long i = 0; i < 0x800000; i++){ __asm__("nop"); } //1sec
	gpio_clear(GPIOC, GPIO13);

    get_dev_unique_id(serial_no);

    usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config, usb_strings, 4, usbd_control_buffer, sizeof(usbd_control_buffer));
    usbd_register_set_config_callback(usbd_dev, hid_set_config);
    usbd_register_suspend_callback(usbd_dev, usb_suspend_callback);

	//Full GPIO
	AFIO_MAPR = AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_OFF;

    //clear pins
    GPIO_CRL(GPIOA) = CLEAR_ALL_PINS;
    GPIO_CRH(GPIOA) = CLEAR_ALL_PINS;
    GPIO_CRL(GPIOB) = CLEAR_ALL_PINS;
    GPIO_CRH(GPIOB) = CLEAR_ALL_PINS;
	
	hid_interrupt = 1;
	
    while(1){
       
        usbd_poll(usbd_dev);
   		
   		//WAKEUP
   		switch(hid_interrupt){
   			
	   		case SMS_WAKEUP:
				buffer_out[0] = 0xFF;
				usbd_ep_write_packet(usbd_dev, 0x81, buffer_out, sizeof(buffer_out));
				buffer_out[0] = 0x0;
				hid_interrupt = 1;
				break;
				
        	case SMS_READ:
        		address = buffer_in[1] | (buffer_in[2]<<8);
				read_cartridge();
				usbd_ep_write_packet(usbd_dev, 0x81, buffer_out, 64);
				memcpy((unsigned char *)buffer_out, (unsigned char *)buffer_zeroed, 64);
				hid_interrupt = 1;
				break;			   		
   		
   			case SMS_WRITE:
	        	address = buffer_in[1] | (buffer_in[2]<<8);
				write_sram();
				buffer_out[0] = 0xFF;
				usbd_ep_write_packet(usbd_dev, 0x81, buffer_out, 64);
				buffer_out[0] = 0x0;
	        	hid_interrupt = 1;
				break;
			
			case SMS_WRITE_FLASH:
				address = buffer_in[1] | (buffer_in[2]<<8);
				write_flash();
				buffer_out[0] = 0xFF;
				usbd_ep_write_packet(usbd_dev, 0x81, buffer_out, 64);
				buffer_out[0] = 0x0;
	        	hid_interrupt = 1;
	        	break;
        	
        	case SMS_ERASE_FLASH:
	        	erase_flash();
				buffer_out[0] = 0xFF;
				usbd_ep_write_packet(usbd_dev, 0x81, buffer_out, 64);
				buffer_out[0] = 0;
	        	hid_interrupt = 1;
	        	break;
        	/*
        	case SMS_FLASH_ID:
	        	flash_id();
				usbd_ep_write_packet(usbd_dev, 0x81, buffer_out, 64);
				memcpy((unsigned char *)buffer_out, (unsigned char *)buffer_zeroed, 64);
	        	hid_interrupt = 1;
	        	break;
	        */
   		
   		}

    }
}

