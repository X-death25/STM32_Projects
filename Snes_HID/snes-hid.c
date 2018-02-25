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

//  Include Personal Functions
/*
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
*/

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
static unsigned char hid_interrupt = 0;
static const unsigned char stmReady[] = {'R','E','A','D','Y','!'};
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

void sys_tick_handler(void)
{

    if(hid_interrupt == 1)
     {

    	bufferIn[0] = 0;
		usbd_ep_read_packet(usbd_dev, 0x01, bufferIn, 64);

		switch(bufferIn[0])
        {
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

	setDataOutput();   // PD0-7 OUT
    GPIOD_BSRR = 0xFF; // PD0-7 "1"
	//GPIOD_BRR = 0xFF; // PD0-7 "0"

//GPIO_CRH(GPIOB) = 0x33333333;
 //GPIOB_BSRR = 0xFF; // PD0-7 "1"

   
//gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,GPIO8);
// gpio_set(GPIOB, GPIO8);	/* LED on/off */
//GPIOB_BSRR |= GPIO8;
	

     hid_interrupt = 1; // Enable HID Interrupt

    while(1)
     {
        usbd_poll(usbd_dev);
	     if(hid_interrupt!=1)
           {
			switch(hid_interrupt)
              {
				case WAKEUP:
		        	memcpy((unsigned char *)bufferOut, (unsigned char *)stmReady, sizeof(stmReady));
					break;
				}
               while( usbd_ep_write_packet(usbd_dev, 0x81, bufferOut, 64) != 64);
			   hid_interrupt = 1;
            }
      }
}
			
