/*
 * Copyright (C) 2016 Paul Fertser <fercerpav@gmail.com>
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 *
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
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/hid.h>
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencmsis/core_cm3.h>

/*  Include Personal Functions
 */

#include "pinout.h"
#include "UC_Prog.h"

/* We need a special large control buffer for this device: */

uint8_t usbd_control_buffer[5*64];

static uint8_t hid_buffer_IN[64];
static uint8_t hid_buffer_OUT[64];

 static long adress=0;

static uint8_t hid_interrupt=0;


// HID Special Command 

#define ReadBuffer     0x08  // Read HID Buffer
#define ReadPage       0x09  // Read Page
#define TimeTest       0xF0  // Time Test

#include <libopencm3/cm3/scb.h>
#include <libopencm3/usb/dfu.h>

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
        .bInterval = 0x05,
    },
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x01,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = 64,
        .bInterval = 0x05,
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
    .bInterfaceSubClass = 0, /* boot */
    .bInterfaceProtocol = 0, /* mouse */
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
    /* The ST Microelectronics DfuSe application needs this string.
     * The format isn't documented... */
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

    /* Handle the HID report descriptor. */
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
        return 0; /* Only accept class request. */

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
    
    /* 72MHz / 8 => 9000000 counts per second */
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    /* 9000000/9000 = 1000 overflows per second - every 1ms one interrupt */
    /* SysTick interrupt every N clock pulses: set reload to N-1 */
    systick_set_reload(8999);  //  1 ms interrupt
    systick_interrupt_enable();
    systick_counter_enable();

}

static char serial_no[25];

static const char *usb_strings[] =
{
    "libopencm3",
    "HID Megadrive Dumper",
    serial_no,
    /* This string is used by ST Microelectronics' DfuSe utility. */
    "@Internal Flash   /0x08000000/8*001Ka,56*001Kg",
};

static char *get_dev_unique_id(char *s)
{
    volatile uint8_t *unique_id = (volatile uint8_t *)0x1FFFF7E8;
    int i;

    /* Fetch serial number from chip's unique ID */
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

void delayMicroseconds(unsigned char Microseconds)
{
          for (unsigned char i = 0; i < 4*Microseconds; i++)
        __asm__("nop"); 
}

void Clean_IO(void)
{
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,D0|D1|D2|D3|D4|D5|D6|D7);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,D8|D9|D10|D11|D12|D13|D14|D15);

    gpio_clear(GPIOB,D0|D1|D2|D3|D4|D5|D6|D7);
    gpio_clear(GPIOA,D8|D9|D10|D11|D12|D13|D14|D15);

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Time);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Asel);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Mark3);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Sram_CE);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Clear);

    gpio_clear(GPIOA,Asel);
    gpio_set(GPIOA,Mark3);
    gpio_set(GPIOC,Sram_CE);

}

void Init_Memory(void)
{

    //Setup Pins value

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Clk1);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Clk2);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Clk3);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Clear);

    gpio_set(GPIOA,Clear); // Disable Reset for enabling Adress mode

    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,OE);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,CE);
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,WE);

    gpio_clear(GPIOA,Clk1);
    gpio_clear(GPIOA,Clk2);
    gpio_clear(GPIOA,Clk3);
    delayMicroseconds(1);
    gpio_set(GPIOA,Clk1);
    gpio_set(GPIOA,Clk2);
    gpio_set(GPIOA,Clk3);

    gpio_set(GPIOB,OE);
    gpio_set(GPIOA,CE);
    gpio_set(GPIOB,WE);
}

void SetData_Input(void)
{
    /* Directy write Register is faster than call gpio set_mode ( see STM Reference manual P166)
     * CRL & CRH is 32 bit register Low for 0..7 High for 8..15 
     CNF = 01 for Input Float  & MODE = 00 for INPUT Mode -*/ 
    
  //  gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT,D6|D7|D8|D9);
  //  gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT,D0|D1|D2|D3|D4|D5|D10|D11|D12|D13|D14|D15);
 
      GPIO_CRL(GPIOA) = 0x66666666;
      GPIO_CRH(GPIOA) = 0x44446444;
      GPIO_CRL(GPIOB) = 0x44144414;
      GPIO_CRH(GPIOB) = 0x44444444;
}

void SetData_OUTPUT(void)
{
    GPIO_CRL(GPIOA) = 0x66666666;
    GPIO_CRH(GPIOA) = 0x64446666;
    GPIO_CRL(GPIOB) = 0x66166111;
    GPIO_CRH(GPIOB) = 0x66666666;
}

void SetFlashCE(unsigned char state)
{
    if (state==1)
    {
        gpio_set(GPIOA,CE);
    }
    else
    {
        gpio_clear(GPIOA,CE);
    }
}

void SetFlashOE(unsigned char state)
{
    if (state==1)
    {
        gpio_set(GPIOB,OE);
    }
    else
    {
        gpio_clear(GPIOB,OE);
    }
}

void SetFlashWE(unsigned char state)
{
    if (state==1)
    {
        gpio_set(GPIOB,WE);
    }
    else
    {
        gpio_clear(GPIOB,WE);
    }
}


void DirectWrite8(unsigned char val)
{
 /* 
  #define D0 GPIO9  // PB9
#define D1 GPIO8  // PB8
#define D2 GPIO7  // PB7
#define D3 GPIO6  // PB6
#define D4 GPIO4  // PB4 
#define D5 GPIO3  // PB3 
#define D6 GPIO15 //PA15  
#define D7 GPIO10 //PA10 */
 
 GPIO_ODR(GPIOB) =  GPIO_ODR(GPIOB) | ((val & 0x01) << 9); // D0
 GPIO_ODR(GPIOB) =  GPIO_ODR(GPIOB) | ((val & 0x02) << 8); // D1
 GPIO_ODR(GPIOB) =  GPIO_ODR(GPIOB) | ((val & 0x04) << 7); // D2
 GPIO_ODR(GPIOB) =  GPIO_ODR(GPIOB) | ((val & 0x08) << 6); // D3
 GPIO_ODR(GPIOB) =  GPIO_ODR(GPIOB) | ((val & 0x10) << 4); // D4
 GPIO_ODR(GPIOB) =  GPIO_ODR(GPIOB) | ((val & 0x20) << 3); // D5
 GPIO_ODR(GPIOB) =  GPIO_ODR(GPIOA) | ((val & 0x40) << 15); // D6
 GPIO_ODR(GPIOB) =  GPIO_ODR(GPIOA) | ((val & 0x80) << 10); // D7
 /*
 GPIO_ODR(GPIOB) =  (GPIO_ODR(GPIOB) & 0x100) & ( val & 0x02); // D1
 GPIO_ODR(GPIOB) =  (GPIO_ODR(GPIOB) & 0x80)  & ( val & 0x04); // D2
 GPIO_ODR(GPIOB) =  (GPIO_ODR(GPIOB) & 0x40)  & ( val & 0x08); // D3
 GPIO_ODR(GPIOB) =  (GPIO_ODR(GPIOB) & 0x10)  & ( val & 0x10); // D4
 GPIO_ODR(GPIOB) =  (GPIO_ODR(GPIOB) & 0x08)  & ( val & 0x20); // D5
 GPIO_ODR(GPIOB) =  (GPIO_ODR(GPIOA) & 0x8000)& ( val & 0x40); // D6
 GPIO_ODR(GPIOB) =  (GPIO_ODR(GPIOA) & 0x400) & ( val & 0x80); // D7*/

 


  
 // GPIO_ODR(GPIOC)
   /* if(val&1)
    {
        gpio_set(GPIOB,D0);
    }
    else
    {
        gpio_clear(GPIOB,D0);
    }
    if((val>>1)&1)
    {
        gpio_set(GPIOB,D1);
    }
    else
    {
        gpio_clear(GPIOB,D1);
    }
    if((val>>2)&1)
    {
        gpio_set(GPIOB,D2);
    }
    else
    {
        gpio_clear(GPIOB,D2);
    }
    if((val>>3)&1)
    {
        gpio_set(GPIOB,D3);
    }
    else
    {
        gpio_clear(GPIOB,D3);
    }
    if((val>>4)&1)
    {
        gpio_set(GPIOB,D4);
    }
    else
    {
        gpio_clear(GPIOB,D4);
    }
    if((val>>5)&1)
    {
        gpio_set(GPIOB,D5);
    }
    else
    {
        gpio_clear(GPIOB,D5);
    }
    if((val>>6)&1)
    {
        gpio_set(GPIOA,D6);
    }
    else
    {
        gpio_clear(GPIOA,D6);
    }
    if((val>>7)&1)
    {
        gpio_set(GPIOA,D7);
    }
    else
    {
        gpio_clear(GPIOA,D7);
    }*/
}

void DirectWrite16(unsigned short val)
{
    if(val&1)
    {
        gpio_set(GPIOB,D0);
    }
    else
    {
        gpio_clear(GPIOB,D0);
    }
    if((val>>1)&1)
    {
        gpio_set(GPIOB,D1);
    }
    else
    {
        gpio_clear(GPIOB,D1);
    }
    if((val>>2)&1)
    {
        gpio_set(GPIOB,D2);
    }
    else
    {
        gpio_clear(GPIOB,D2);
    }
    if((val>>3)&1)
    {
        gpio_set(GPIOB,D3);
    }
    else
    {
        gpio_clear(GPIOB,D3);
    }
    if((val>>4)&1)
    {
        gpio_set(GPIOB,D4);
    }
    else
    {
        gpio_clear(GPIOB,D4);
    }
    if((val>>5)&1)
    {
        gpio_set(GPIOB,D5);
    }
    else
    {
        gpio_clear(GPIOB,D5);
    }
    if((val>>6)&1)
    {
        gpio_set(GPIOA,D6);
    }
    else
    {
        gpio_clear(GPIOA,D6);
    }
    if((val>>7)&1)
    {
        gpio_set(GPIOA,D7);
    }
    else
    {
        gpio_clear(GPIOA,D7);
    }
    if((val>>8)&1)
    {
        gpio_set(GPIOA,D8);
    }
    else
    {
        gpio_clear(GPIOA,D8);
    }
    if((val>>9)&1)
    {
        gpio_set(GPIOA,D9);
    }
    else
    {
        gpio_clear(GPIOA,D9);
    }
    if((val>>10)&1)
    {
        gpio_set(GPIOB,D10);
    }
    else
    {
        gpio_clear(GPIOB,D10);
    }
    if((val>>11)&1)
    {
        gpio_set(GPIOB,D11);
    }
    else
    {
        gpio_clear(GPIOB,D11);
    }
    if((val>>12)&1)
    {
        gpio_set(GPIOB,D12);
    }
    else
    {
        gpio_clear(GPIOB,D12);
    }
    if((val>>13)&1)
    {
        gpio_set(GPIOB,D13);
    }
    else
    {
        gpio_clear(GPIOB,D13);
    }
    if((val>>14)&1)
    {
        gpio_set(GPIOB,D14);
    }
    else
    {
        gpio_clear(GPIOB,D14);
    }
    if((val>>15)&1)
    {
        gpio_set(GPIOB,D15);
    }
    else
    {
        gpio_clear(GPIOB,D15);
    }
}

unsigned char DirectRead8()
{
    unsigned char result=0;
    if (GPIOB_IDR & D0) result |= 1;
    if (GPIOB_IDR & D1) result |= 2;
    if (GPIOB_IDR & D2) result |= 4;
    if (GPIOB_IDR & D3) result |= 8;
    if (GPIOB_IDR & D4) result |= 16;
    if (GPIOB_IDR & D5) result |= 32;
    if (GPIOA_IDR & D6) result |= 64;
    if (GPIOA_IDR & D7) result |= 128;
    return result;
 
}

unsigned short DirectRead16()
{
    unsigned short result=0;
    if (GPIOB_IDR & D0) result |= 1;
    if (GPIOB_IDR & D1) result |= 2;
    if (GPIOB_IDR & D2) result |= 4;
    if (GPIOB_IDR & D3) result |= 8;
    if (GPIOB_IDR & D4) result |= 16;
    if (GPIOB_IDR & D5) result |= 32;
    if (GPIOA_IDR & D6) result |= 64;
    if (GPIOA_IDR & D7) result |= 128;
    if (GPIOA_IDR & D8) result |= 256;
    if (GPIOA_IDR & D9) result |= 512;
    if (GPIOB_IDR & D10) result |= 1024;
    if (GPIOB_IDR & D11) result |= 2048;
    if (GPIOB_IDR & D12) result |= 4096;
    if (GPIOB_IDR & D13) result |= 8192;
    if (GPIOB_IDR & D14) result |= 16384;
    if (GPIOB_IDR & D15) result |= 32768;
    return result;
}

void SetAddress (unsigned long address)
{
    SetData_OUTPUT();
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Clear);
    gpio_set(GPIOA,Clear); // Disable Reset for enabling Adress mode
    DirectWrite8(address & 0xff);
    gpio_clear(GPIOA,Clk1);
    gpio_set(GPIOA,Clk1);
    DirectWrite8((address >> 8) & 0xff);
    gpio_clear(GPIOA,Clk2);
    gpio_set(GPIOA,Clk2);
    DirectWrite8((address >> 16) & 0xff);
    gpio_clear(GPIOA,Clk3);
    gpio_set(GPIOA,Clk3);
    SetData_Input();
}

int ReadFlash8(int address)
{
    SetFlashCE(1);
    SetFlashOE(1);
    SetAddress(address);
    delayMicroseconds(1);
    SetFlashCE(0);
    SetFlashOE(0);
    delayMicroseconds(1);
    SetData_Input();
 //   delayMicroseconds(1);
    char result = DirectRead8();
 //   delayMicroseconds(1);
    SetFlashCE(1);
 //   delayMicroseconds(1);
    SetFlashOE(1);
    return result;
}


int ReadFlash16(int address)
{
    SetFlashCE(1);
    SetFlashOE(1);
    SetAddress(address);
    delayMicroseconds(1);
    SetFlashCE(0);
    SetFlashOE(0);
    delayMicroseconds(1);
    SetData_Input();
    delayMicroseconds(1);
    short result = DirectRead16();
    delayMicroseconds(1);
    SetFlashCE(1);
    delayMicroseconds(1);
    SetFlashOE(1);
    return result;
}

void writeFlash8(int address, int byte)
{
	SetAddress(address);
	delayMicroseconds(1);
	SetData_OUTPUT();
	DirectWrite8(byte);
	SetFlashCE(0);
	SetFlashWE(0);
	delayMicroseconds(1);
	SetFlashWE(1);
	SetFlashCE(1);
	SetData_Input();
	delayMicroseconds(1);
}

void ResetFlash(void)
{
    writeFlash8(0x5555,0xAA);
    writeFlash8(0x2AAA,0x55);
    writeFlash8(0x5555,0xF0);
}

void ReadFlashID (void)

{
    writeFlash8(0x5555,0xAA);
    writeFlash8(0x2AAA,0x55);
    writeFlash8(0x5555,0x90);
}

void EraseFlash()
{

    writeFlash8(0x5555, 0xAA);
    writeFlash8(0x2AAA, 0x55);
    writeFlash8(0x5555, 0x80);
    writeFlash8(0x5555, 0xAA);
    writeFlash8(0x2AAA, 0x55);
    writeFlash8(0x5555, 0x10);
    
}

void ByteProgramFlash(int adress , int byte)
{
    writeFlash8(0x5555,0xAA);
    writeFlash8(0x2AAA,0x55);
    writeFlash8(0x5555,0xA0);
    writeFlash8(adress,byte);
    delayMicroseconds(5);
}



static void gpio_setup(void)
{
    /* Pull down for nSRST */
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, NSRST_PIN);
    gpio_clear(GPIOB, NSRST_PIN);

    /* Turn Jtag & SW-D to GPIO */

    AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_OFF;

    Clean_IO();
}


static void usb_suspend_callback(void)
{
    gpio_set(GPIOB, NSRST_PIN);
    *USB_CNTR_REG |= USB_CNTR_FSUSP;
    *USB_CNTR_REG |= USB_CNTR_LP_MODE;
    SCB_SCR |= SCB_SCR_SLEEPDEEP;
    __WFI();
}

void usb_wakeup_isr(void)
{
    exti_reset_request(EXTI18);
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    *USB_CNTR_REG &= ~USB_CNTR_FSUSP;
    gpio_clear(GPIOB, NSRST_PIN);
}

/// Function Called Every Ticks///

void sys_tick_handler(void)
{
  if (hid_interrupt ==1)
  {
     usbd_ep_read_packet(usbd_dev,0x01,hid_buffer_IN,sizeof(hid_buffer_IN));
     
      	    if (hid_buffer_IN[0] == 0x08 ) // HID Command Send Current OUT Buffer
	      {
		hid_interrupt=0; // Disable HID Interrupt
		SetData_OUTPUT();
	
              hid_buffer_OUT[0]=GPIO_CRL(GPIOB) & 0xFF;
              hid_buffer_OUT[1]=(GPIO_CRL(GPIOB) & 0xFF00)>>8;
              hid_buffer_OUT[2]=(GPIO_CRL(GPIOB) & 0xFF0000)>>16;
              hid_buffer_OUT[3]=(GPIO_CRL(GPIOB) & 0xFF000000)>>24;
	    
	      hid_buffer_OUT[4]=GPIO_CRH(GPIOB) & 0xFF;
              hid_buffer_OUT[5]=(GPIO_CRH(GPIOB) & 0xFF00)>>8;
              hid_buffer_OUT[6]=(GPIO_CRH(GPIOB) & 0xFF0000)>>16;
              hid_buffer_OUT[7]=(GPIO_CRH(GPIOB) & 0xFF000000)>>24;
	    
		usbd_ep_write_packet(usbd_dev, 0x81,hid_buffer_OUT, sizeof(hid_buffer_OUT));
		gpio_clear(GPIOC, GPIO13);
	      }
	      
	     if (hid_buffer_IN[0] == 0x09  ) // HID Command Read Page ( 64 bytes)
	      {
                hid_interrupt = 0x09;
	      }
	      
  }

}

/// Main Program ///
int main(void)
{
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
    /* Set GPIO14 (in GPIO port C) to 'input float */
    gpio_set_mode(GPIOC, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO14);
    /* Set GPIO13 ( Led in GPIO port C) to 'output push-pull'. */
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

    if (GPIOC_IDR & GPIO14) // if value !=0
    {
        // Turn GPIO to SW-D for Enable Programming Mode
        while (1)
        {
            AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_FULL_SWJ_NO_JNTRST;


            /* Using API function gpio_toggle(): */
            int i=0;
            gpio_toggle(GPIOC, GPIO13);	/* LED on/off */
            for (i = 0; i < 800000; i++)	/* Wait a bit. */
                __asm__("nop");
        }
    }


    /*
     * Vile hack to reenumerate, physically _drag_ d+ low.
     * (need at least 2.5us to trigger usb disconnect)
     */
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
    gpio_clear(GPIOA, GPIO12);
    for (unsigned int i = 0; i < 800000; i++)
        __asm__("nop");

    get_dev_unique_id(serial_no);

    usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config,
                         usb_strings, 4,
                         usbd_control_buffer, sizeof(usbd_control_buffer));
    usbd_register_set_config_callback(usbd_dev, hid_set_config);
    usbd_register_suspend_callback(usbd_dev, usb_suspend_callback);

    // Hid OUT Buffer clean

    for (unsigned int i = 0; i < 64; i++)
    {
        hid_buffer_OUT[i]=0x00;
    }
    
  
    gpio_setup();  // if we are not in programming mode we can acess Full GPIO
    Init_Memory();
    gpio_set(GPIOC, GPIO13); /* LED /off */
    ResetFlash();
    hid_interrupt=1; // Enable HID Interrupt
  /*  
    systick_interrupt_disable();
    systick_counter_disable();
    adress=0;
    gpio_clear(GPIOA,Time);
writeFlash8(0x5555,0xAA);
    gpio_set(GPIOA,Time);
    
    */
   

    while(1)
    {
        usbd_poll(usbd_dev);
	
	  if (hid_interrupt == 0x09)  // Read Page
	  {
	     hid_interrupt=0;  // Disable HID Interrupt
	     adress = hid_buffer_IN[1] |  (hid_buffer_IN[2] << 8 ) |  (hid_buffer_IN[3]<< 16) | (hid_buffer_IN[4]  << 24 );
	     for (unsigned int i = 0; i < 64; i++)
	      {
		hid_buffer_OUT[i]=ReadFlash8(adress+i);
	      }
	     usbd_ep_write_packet(usbd_dev, 0x81,hid_buffer_OUT, sizeof(hid_buffer_OUT));
	     hid_buffer_IN[0]=0; 
	     hid_interrupt=1; // Enable HID Interrupt
	  }
	  
	  else{usbd_poll(usbd_dev);} 
    }


}

