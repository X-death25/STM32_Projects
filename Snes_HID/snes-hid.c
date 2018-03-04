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
 *  \file Snes-hid.C
 *  \brief ARM code for the STM32 part of Snes_Dumper
 *  \author X-death for Ultimate-Consoles forum ( http://www.ultimate-consoles.fr/index)
 *  \created on 02/2018
 *
 * USB HID code is based on libopencm3 & paulfertser/stm32-tx-hid ( https://github.com/paulfertser/stm32-tx-hid)
 * i have added OUT endpoint for bi-directionnal communication and some modification in the descriptor
 * IN & OUT size is 64 bytesrd
 * Support Super Nintendo/Famicom cartridge dump ( max size is 64MB)
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

//  Snes I/O <> STM32 Definition

#define D0 			GPIO0  // PD0   Snes Data lines
#define D1 			GPIO1  // PD1
#define D2 			GPIO2  // PD2
#define D3 			GPIO3  // PD3
#define D4 			GPIO4  // PD4
#define D5 			GPIO5  // PD5
#define D6 			GPIO6  // PD6
#define D7 			GPIO7  // PD7

#define PA0 		GPIO8  // PD8   Snes EXP Address lines
#define PA1 		GPIO9  // PD9
#define PA2 		GPIO10 // PD10
#define PA3 		GPIO11 // PD11
#define PA4 		GPIO12 // PD12
#define PA5 		GPIO13 // PD13
#define PA6 		GPIO14 // PD14
#define PA7			GPIO15 // PD15

#define A0 			GPIO0  // PE0   Snes Address lines
#define A1 			GPIO1  // PE1
#define A2 		    GPIO2  // PE2
#define A3 		    GPIO3  // PE3
#define A4 		    GPIO4  // PE4
#define A5 		    GPIO5  // PE5
#define A6 		    GPIO6  // PE6
#define A7			GPIO7  // PE7
#define A8			GPIO8  // PE8
#define A9			GPIO9  // PE9
#define A10			GPIO10 // PE10
#define A11			GPIO11 // PE11
#define A12			GPIO12 // PE12
#define A13			GPIO13 // PE13
#define A14			GPIO14 // PE14
#define A15			GPIO15 // PE15

#define OE			GPIO8  // PB8   Snes Control lines
#define CE			GPIO9  // PB9
#define WE			GPIO10 // PB10

#define LED_PIN 	GPIO13 // PC13  STM32 Specific


// HID Special Command

#define WAKEUP     		0x10
#define READ_MD     	0x11
#define READ_SFC_ROM  	0x12
#define WRITE_MD_SAVE 	0x13
#define WRITE_MD_FLASH 	0x14
#define ERASE_MD_FLASH 	0x15
#define READ_SMS   		0x16

#define CFI_MODE   		0x17
#define INFOS_ID   		0x18

// We need a special large control buffer for this device:

unsigned char usbd_control_buffer[5*64];


static uint8_t hid_buffer_IN[64];
static uint8_t hid_buffer_OUT[64];
static uint8_t temp_buffer[64];

static uint8_t hid_interrupt=0;
static unsigned long address = 0;
static usbd_device *usbd_dev;


/// Specific Dumper Function ///

void wait(int nb){
    while(nb){ __asm__("nop"); nb--;}
}


void setDataInput(){
    GPIO_CRL(GPIOD) = 0x44444444;
}

void setDataOutput(){
    GPIO_CRL(GPIOD) = 0x33333333;
}

void directWrite8(unsigned char val)
{
  GPIOD_BSRR = val & 0xFF;
}

unsigned char directRead8()
{
  return GPIOD_IDR & 0xFF;
}

static void gpio_setup(void)
{
  // OUT From STM32 to Snes
  gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,OE | CE | WE);
  gpio_set_mode(GPIOE, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,A0 | A1 | A2 | A3 | A4 | A5 | A6 | A7 );
  gpio_set_mode(GPIOE, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,A8 | A9 | A10 | A11 | A12 | A13 | A14 | A15 );
  // Set Control lines "1" and Adress Lines "0"
  GPIOB_BSRR |= OE | CE | WE; // "1"
  GPIOE_BRR = 0xFFFF; // "0"
  setDataOutput();
  directWrite8(0x00);
  setDataInput();
}

void SetAddress(unsigned long adr)
{
GPIOE_BRR = 0xFFFF; // "0"
GPIOE_BSRR = adr;
}

void WriteFlashByte(unsigned long address,unsigned char val)
{
GPIOB_BSRR |= CE;
GPIOB_BSRR |= WE;
SetAddress(address);
wait(1);
GPIOB_BRR |= CE;
GPIOB_BRR |= WE;
setDataOutput();
directWrite8(val);
wait(1);
GPIOB_BSRR |= WE;
GPIOB_BSRR |= CE;
setDataInput();
}
/*
unsigned char ReadFlash(unsigned long address)
{
unsigned char byte = 0;
GPIOB_BSRR |= CE;
GPIOB_BSRR |= OE;
	SetAddress(address);
	wait(4);
	GPIOB_BRR |= CE;
	GPIOB_BRR |= OE;
	wait(4);
	setDataInput();
	wait(4);
	byte = directRead8();
	wait(4);
	GPIOB_BSRR |= CE;
	wait(4);
	GPIOB_BSRR |= OE;
    return byte;
}*/

void ReadFlash()
{
unsigned char adr = 0;
GPIOB_BSRR |= CE;
GPIOB_BSRR |= OE;
while (adr<64)
 {
	SetAddress(address+adr);
    setDataInput();
	GPIOB_BRR |= CE;
	GPIOB_BRR |= OE;
	wait(16);
    directRead8();
	temp_buffer[adr] = directRead8();
	GPIOB_BSRR |= CE;
	GPIOB_BSRR |= OE;
    adr++;
 }
}

void ReadSFCHeader(void)
{
     // HID OUT Buffer clean

    for (unsigned int i = 0; i < 64; i++)
    {
        hid_buffer_OUT[i]=0x00;
        temp_buffer[i]=0x00;
    }

address=32704; // fix adress offset
ReadFlash();
}




/// Specific USB Function ///


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
    "HID Snes Dumper",
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


/// Interrupt Function Called Every Ticks ///

// Function Called Every Ticks

void sys_tick_handler(void)
{
    if (hid_interrupt ==1)
    {
        usbd_ep_read_packet(usbd_dev,0x01,hid_buffer_IN,sizeof(hid_buffer_IN));

        if (hid_buffer_IN[0] == 0x08 ) // HID Command Send SFC Header
        {
            hid_interrupt = 0x08;
        }

        if (hid_buffer_IN[0] == 0x09  ) // HID Command Read16
        {
            hid_interrupt = 0x09;
        }

        if (hid_buffer_IN[0] == 0x0A  ) // HID Command Read8
        {
            hid_interrupt = 0x0A;
        }

        if (hid_buffer_IN[0] == 0x0B  ) // HID Command Erase8
        {
            hid_interrupt = 0x0B;
        }

        if (hid_buffer_IN[0] == 0x0D  ) // HID Command Write8
        {
            hid_interrupt = 0x0D;
        }

        if (hid_buffer_IN[0] == 0x0E  ) // HID Command DumpSMS
        {
            hid_interrupt = 0x0E;
        }
    }

}



/// Main Program Function  ///

int main(){

    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_GPIOD);
	rcc_periph_clock_enable(RCC_GPIOE);

    /* Enable alternate function peripheral clock. */
    rcc_periph_clock_enable(RCC_AFIO);

    /* Interrupt for USB */
    exti_set_trigger(EXTI18, EXTI_TRIGGER_RISING);
    exti_enable_request(EXTI18);
    nvic_enable_irq(NVIC_USB_WAKEUP_IRQ);

    /*Setup Programming mode */
    gpio_set_mode(GPIOC, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO12);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

	// Locking Bootloader for stay in programming mode if GPIO14 is set


    if(GPIOC_IDR & GPIO12){
        while(1){
            AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_FULL_SWJ_NO_JNTRST;
            gpio_toggle(GPIOC, GPIO13);	// LED on/off 
            for(unsigned long i = 0; i < 0x100000; i++){ __asm__("nop"); }
        }
    }


	/*
	 * if not GPIO14 Switch to USB Snes HID
	 * (need at least 2.5us to trigger usb disconnect) physically _drag_ d+ low
	 */

	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	gpio_clear(GPIOA, GPIO12);
for(unsigned long i = 0; i < 0x800000; i++){ __asm__("nop"); } //1sec
	get_dev_unique_id(serial_no);

    usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config, usb_strings, 4,           usbd_control_buffer, sizeof(usbd_control_buffer));
    usbd_register_set_config_callback(usbd_dev, hid_set_config);
    usbd_register_suspend_callback(usbd_dev, usb_suspend_callback);

	//Full GPIO
	AFIO_MAPR = AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_OFF;

	// GPIO Test LED

    // gpio_set(GPIOB, GPIO8);	/* LED on/off */

	gpio_setup();

    // HID OUT Buffer clean

    ReadSFCHeader(); // Read some cool stuff
    hid_interrupt = 1; // Enable HID Interrupt

    while(1)
    {
        usbd_poll(usbd_dev);


        if (hid_interrupt == 0x08)  // Send SFC Header
        {
            hid_interrupt=0; // Disable HID Interrupt
            usbd_ep_write_packet(usbd_dev, 0x81,temp_buffer, sizeof(temp_buffer));
            hid_buffer_IN[0]=0;
            hid_interrupt=1; // Enable HID Interrupt
        }

        if (hid_interrupt == 0x0A)   // Read in 8 bit mode
        {
            hid_interrupt=0;  // Disable HID Interrupt
            address = hid_buffer_IN[1] |  (hid_buffer_IN[2] << 8 ) |  (hid_buffer_IN[3]<< 16) | (hid_buffer_IN[4]  << 24 );
      	    ReadFlash();
            usbd_ep_write_packet(usbd_dev, 0x81,temp_buffer, sizeof(temp_buffer));
            hid_buffer_IN[0]=0;
            hid_interrupt=1; // Enable HID Interrupt
        }
    }
}
			
