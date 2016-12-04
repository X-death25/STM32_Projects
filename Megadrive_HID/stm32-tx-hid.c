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

/* We need a special large control buffer for this device: */

uint8_t usbd_control_buffer[5*64];

static uint8_t hid_buffer_IN[64];
static uint8_t hid_buffer_OUT[64];

static uint8_t header_buffer[64];
static uint8_t temp_buffer[64];

static uint32_t adress=0;

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

    /* 72MHz / 8 => 9000000 counts per second : 9 Mhz */
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

void wait()
{
    for(unsigned char tmp=0; tmp<4; tmp++) __asm__("nop"); //wait ~1,5us
}

void Clean_IO(void)
{
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,D0|D1|D2|D3|D4|D5|D6|D7);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,D8|D9|D10|D11|D12|D13|D14|D15);

    gpio_clear(GPIOB,D0|D1|D2|D3|D4|D5|D6|D7);
    gpio_clear(GPIOA,D8|D9|D10|D11|D12|D13|D14|D15);

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Time);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Asel);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Mark3);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Sram_WE);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Clear);

    gpio_clear(GPIOA,Asel);
    gpio_set(GPIOA,Mark3);
    gpio_set(GPIOC,Sram_WE);

}

void Init_Memory(void)
{

    //Setup Pins value

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Clk1);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Clk2);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Clk3);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,Clear);

    gpio_set(GPIOA,Clear); // Disable Reset for enabling Adress mode

    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,OE);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,CE);
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,WE);

    gpio_clear(GPIOA,Clk1);
    gpio_clear(GPIOA,Clk2);
    gpio_clear(GPIOA,Clk3);
    wait(); //wait ~1,5us
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

    GPIO_CRL(GPIOA) = 0x66666666;
    GPIO_CRH(GPIOA) = 0x44446444;
    GPIO_CRL(GPIOB) = 0x44144414;
    GPIO_CRH(GPIOB) = 0x44444444;
}

void SetData_OUTPUT(void)
{
    GPIO_CRL(GPIOA) = 0x66666663;
    GPIO_CRH(GPIOA) = 0x34446366;
    GPIO_CRL(GPIOB) = 0x33333333;
    GPIO_CRH(GPIOB) = 0x33333333;
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

    if(val&1)
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) | (1 << 9);
    }
    else
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) & ~(1 << 9);
    }
    if((val>>1)&1)
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) | (1 << 8);
    }
    else
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) & ~(1 << 8);
    }
    if((val>>2)&1)
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) | (1 << 7);
    }
    else
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) & ~(1 << 7);
    }
    if((val>>3)&1)
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) | (1 << 6);
    }
    else
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) & ~(1 << 6);
    }
    if((val>>4)&1)
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) | (1 << 4);
    }
    else
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) & ~(1 << 4);
    }
    if((val>>5)&1)
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) | (1 << 3);
    }
    else
    {
        GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) & ~(1 << 3);
    }
    if((val>>6)&1)
    {
        GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) | (1 << 15);
    }
    else
    {
        GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) & ~(1 << 15);
    }
    if((val>>7)&1)
    {
        GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) | (1 << 10);
    }
    else
    {
        GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) & ~(1 << 10);
    }

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

unsigned int DirectRead16()
{
    unsigned int result=0;
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
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) | (1 << 4);  // Clear 1 // Disable Reset for enabling Adress mode
    DirectWrite8(address & 0xff);
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) & ~(1 << 7); // Clk1 0
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) | (1 << 7);  // Clk1 1
    DirectWrite8((address >> 8) & 0xff);
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) & ~(1 << 6); // Clk2 0
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) | (1 << 6);  // Clk2 1
    DirectWrite8((address >> 16) & 0xff);
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) & ~(1 << 5); // Clk3 0
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) | (1 << 5);  // Clk3 1
    SetData_Input();
}

unsigned char ReadFlash8(unsigned long address)
{
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) | (1 << 0);  // CE 1
    GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) | (1 << 1);  // OE 1
    SetAddress(address);
    wait();
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) & ~(1 << 0); // CE 0
    GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) & ~(1 << 1); // OE 0
    wait();
    SetData_Input();
    wait();
    unsigned char result = DirectRead8();
    wait();
    GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) | (1 << 1);  // OE 1
    wait();
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) | (1 << 0);  // CE 1
    return result;
}

unsigned int ReadFlash16(unsigned long address)
{
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) | (1 << 0);  // CE 1
    GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) | (1 << 1);  // OE 1
    SetAddress(address);
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) & ~(1 << 0); // CE 0
    GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) & ~(1 << 1); // OE 0
    SetData_Input();
    wait();
    unsigned int result = DirectRead16();
    wait();
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) | (1 << 0);  // CE 1
    GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) | (1 << 1);  // OE 1
    return result;
}

void writeFlash8(int address, int byte)
{
    SetAddress(address);
    SetData_OUTPUT();
    DirectWrite8(byte);
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) & ~(1 << 0); // CE 0
    GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) & ~(1 << 5); // WE 0
    GPIO_ODR(GPIOB) = GPIO_ODR(GPIOB) | (1 << 5);  // OW 1
    GPIO_ODR(GPIOA) = GPIO_ODR(GPIOA) | (1 << 0);  // CE 1
    SetData_Input();
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

void ByteProgramFlash(int adress, int byte)
{
    writeFlash8(0x5555,0xAA);
    writeFlash8(0x2AAA,0x55);
    writeFlash8(0x5555,0xA0);
    writeFlash8(adress,byte);
    wait();
    wait();
    wait();
}

void ReadMDHeader(void)
{
    // Clean buffer

    temp_buffer[0]=0;
    /*
     for(unsigned char i = 0; i < 32; i++)
       {
          unsigned int read16 = ReadFlash16(i); //on le lit une seule fois
    //header_buffer[(i*2)] = (read16 >> 8) & 0xFF;
    header_buffer[(i*2)] = (read16 & 0xFF00) >> 8;
    header_buffer[(i*2)+1] = read16 & 0xFF;

      }*/


    for(unsigned char i = 0; i < 32; i++)
    {
        unsigned int read16 = ReadFlash16(i+128); //on le lit une seule fois
        temp_buffer[(i*2)+1] = read16 & 0xFF;
        temp_buffer[(i*2)] = (read16 >> 8) & 0xFF;
    }

    // // Write TMSS for SMD

    for (unsigned int i = 0; i < 4; i++)
    {
        header_buffer[i+51]=temp_buffer[i];
    }


    // Write Release Date

    for (unsigned int i = 0; i < 8; i++)
    {
        header_buffer[i]=temp_buffer[24+i];
    }

    // Write Name

    for (unsigned int i = 0; i < 32; i++)
    {
        header_buffer[i+8]=temp_buffer[32+i];
    }

    for(unsigned char i = 0; i < 32; i++)
    {
        unsigned int read16 = ReadFlash16(i+208); //on le lit une seule fois
        temp_buffer[(i*2)+1] = read16 & 0xFF;
        temp_buffer[(i*2)] = (read16 >> 8) & 0xFF;
    }

    // Write Game Size

    header_buffer[40]=temp_buffer[5];
    header_buffer[41]=temp_buffer[6];
    header_buffer[42]=temp_buffer[7];

    // Write Save info

    header_buffer[43]=temp_buffer[23]; // Save Support
    header_buffer[44]=temp_buffer[18]; // Save Type
    header_buffer[45]=temp_buffer[26]; // Save Size
    header_buffer[46]=temp_buffer[27];

    for(unsigned char i = 0; i < 32; i++)
    {
        unsigned int read16 = ReadFlash16(i+224); //on le lit une seule fois
        temp_buffer[(i*2)+1] = read16 & 0xFF;
        temp_buffer[(i*2)] = (read16 >> 8) & 0xFF;
    }

    header_buffer[48]=temp_buffer[48];
    header_buffer[49]=temp_buffer[49];
    header_buffer[50]=temp_buffer[50];

    for(unsigned char i = 0; i < 4; i++)
    {
        unsigned int read16 = ReadFlash16(i+128); //on le lit une seule fois
        temp_buffer[(i*2)+1] = read16 & 0xFF;
        temp_buffer[(i*2)] = (read16 >> 8) & 0xFF;
    }

    // Write TMSS for SMS

    for (unsigned int i = 0; i < 8; i++)
    {
        header_buffer[i+55]=ReadFlash8(32752+i);
        header_buffer[i+55]=ReadFlash8(32752+i);
    }
  /*  
     // Init SMS Mapper
	
	       gpio_clear(GPIOC,Sram_WE);
	       SetAddress(0xFFFC);SetData_OUTPUT();DirectWrite8(0x00);
	       SetAddress(0xFFFD);SetData_OUTPUT();DirectWrite8(0x00);
	       SetAddress(0xFFFE);SetData_OUTPUT();DirectWrite8(0x01);
	       SetAddress(0xFFFF);SetData_OUTPUT();DirectWrite8(0x02);
	       gpio_set(GPIOC,Sram_WE);
	       SetData_Input();*/
	  

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

        if (hid_buffer_IN[0] == 0x08 ) // HID Command Send MD Header
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
    ReadMDHeader(); // Read some cool stuff
    hid_interrupt=1; // Enable HID Interrupt

    while(1)
    {
        usbd_poll(usbd_dev);

        if (hid_interrupt == 0x08)  // Send MD Header
        {
            hid_interrupt=0; // Disable HID Interrupt
            usbd_ep_write_packet(usbd_dev, 0x81,header_buffer, sizeof(header_buffer));
            hid_buffer_IN[0]=0;
            hid_interrupt=1; // Enable HID Interrupt
        }

        if (hid_interrupt == 0x09)  // Read Page 16 bit mode
        {
            hid_interrupt=0;  // Disable HID Interrupt
            adress = hid_buffer_IN[1] |  (hid_buffer_IN[2] << 8 ) |  (hid_buffer_IN[3]<< 16) | (hid_buffer_IN[4]  << 24 );

            for(unsigned char i = 0; i < 32; i++)
            {
                unsigned int read16 = ReadFlash16(adress+i); //on le lit une seule fois
                hid_buffer_OUT[(i*2)+1] = read16 & 0xFF;
                hid_buffer_OUT[(i*2)] = (read16 >> 8) & 0xFF;
                hid_buffer_OUT[(i*2)+1] = read16 & 0xFF;
                hid_buffer_OUT[(i*2)] = (read16 >> 8) & 0xFF;
            }


            usbd_ep_write_packet(usbd_dev, 0x81,hid_buffer_OUT, sizeof(hid_buffer_OUT));
            hid_buffer_IN[0]=0;
            hid_interrupt=1; // Enable HID Interrupt
        }

         if (hid_interrupt == 0x0A)   // Read Page 8 bit mode
	  {
	     hid_interrupt=0;  // Disable HID Interrupt     
	     gpio_set(GPIOC,Sram_WE); // Disable SRAM Write
	       DirectWrite8(0x01); // D0 must be SET before access TIME
	     gpio_clear(GPIOA,Time); // Enable Bankswitch with TIME
	         adress = hid_buffer_IN[1] |  (hid_buffer_IN[2] << 8 ) |  (hid_buffer_IN[3]<< 16) | (hid_buffer_IN[4]  << 24 );
	       for (unsigned int i = 0; i < 64; i++)
	         {
		   hid_buffer_OUT[i]=ReadFlash8(adress+i);
		   hid_buffer_OUT[i]=ReadFlash8(adress+i);
	         }
	     usbd_ep_write_packet(usbd_dev, 0x81,hid_buffer_OUT, sizeof(hid_buffer_OUT));
	     hid_buffer_IN[0]=0;
	     gpio_set(GPIOA,Time); // Disable Bankswitch
	     hid_interrupt=1; // Enable HID Interrupt

	  }

     if (hid_interrupt == 0x0B)   // Erase Page 8 bit mode
	  {
	     hid_interrupt=0;  // Disable HID Interrupt
	     gpio_clear(GPIOC,Sram_WE); // Enable SRAM Write
	      DirectWrite8(0x01); // D0 must be SET before access TIME
	     gpio_clear(GPIOA,Time); // Enable Bankswitch with TIME
	     for (unsigned int i = 0; i < 1024*64; i++) // Erase SRAM
	         {
		   writeFlash8(1048576+i,0xFF);
		 }
	     gpio_set(GPIOC,Sram_WE); // Disable SRAM R/W
	     gpio_set(GPIOA,Time); // Disable Bankswitch
	     hid_buffer_OUT[0]=0xAA;
	     usbd_ep_write_packet(usbd_dev, 0x81,hid_buffer_OUT, sizeof(hid_buffer_OUT));
	     hid_buffer_IN[0]=0; 
	     hid_interrupt=1; // Enable HID Interrupt
	  }

       if (hid_interrupt == 0x0D)   // Write Page 8 bit mode
	  {
	     hid_interrupt=0;  // Disable HID Interrupt
	     gpio_clear(GPIOC,Sram_WE); // Enable SRAM Write
	     DirectWrite8(0x01); // D0 must be SET before access TIME
	     gpio_clear(GPIOA,Time); // Enable Bankswitch with TIME
	     for (unsigned int i = 0; i < 32; i++) // Erase SRAM
	         {
		   writeFlash8(1048576+i+adress,hid_buffer_IN[32+i]);
		 }
	     gpio_set(GPIOC,Sram_WE); // Disable SRAM R/W
	     gpio_set(GPIOA,Time); // Disable Bankswitch
	     adress +=32;
	     hid_buffer_OUT[0]=0xAA;
	     usbd_ep_write_packet(usbd_dev, 0x81,hid_buffer_OUT, sizeof(hid_buffer_OUT));
	     hid_buffer_IN[0]=0; 
	     hid_interrupt=1; // Enable HID Interrupt	
	  }
	  
	  if (hid_interrupt == 0x0E)   // Dump SMS Cartridge
	  {
	     hid_interrupt=0;  // Disable HID Interrupt
	     adress = hid_buffer_IN[1] |  (hid_buffer_IN[2] << 8 ) |  (hid_buffer_IN[3]<< 16) | (hid_buffer_IN[4]  << 24 );

	     if (hid_buffer_IN[11] != 0xCC) // Enable Mapper Control
	     {
	      gpio_clear(GPIOC, GPIO13); /* LED /off */ 
             gpio_clear(GPIOC,Sram_WE);
	     SetAddress(0xFFFF);
	      SetData_OUTPUT();
	      DirectWrite8(hid_buffer_IN[6]);
	    gpio_set(GPIOC,Sram_WE);
	     }	    
	       for (unsigned int i = 0; i < 64; i++)
	         {
		   hid_buffer_OUT[i]=ReadFlash8(adress+i);
		   hid_buffer_OUT[i]=ReadFlash8(adress+i);
	         }
	    
	     usbd_ep_write_packet(usbd_dev, 0x81,hid_buffer_OUT, sizeof(hid_buffer_OUT));
	     hid_buffer_IN[0]=0;
	     hid_interrupt=1; // Enable HID Interrupt
	  }


        //  else{usbd_poll(usbd_dev);}
    }


}

