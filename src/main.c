#include <fx2lib.h>
#include <fx2delay.h>
#include <fx2debug.h>
#include <fx2usb.h>
#include <usbcdc.h>
#include <ctype.h>

#include <stdio.h>
#include <string.h>

#include "i2c.h"
#include "misc.h"

const uint32_t i2c_linux_functionality = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL; // 0x8eff0001
static uint8_t status = STATUS_IDLE;

usb_desc_device_c usb_device = {
    .bLength = sizeof(struct usb_desc_device),
    .bDescriptorType = USB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = USB_DEV_CLASS_VENDOR,
    .bDeviceSubClass = USB_DEV_SUBCLASS_PER_INTERFACE,
    .bDeviceProtocol = USB_DEV_PROTOCOL_PER_INTERFACE,
    .bMaxPacketSize0 = 64,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0000,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 0,
    .bNumConfigurations = 1,
};

usb_desc_interface_c usb_interface = {
    .bLength = sizeof(struct usb_desc_interface),
    .bDescriptorType = USB_DESC_INTERFACE,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 0,
    .bInterfaceClass = USB_IFACE_CLASS_VENDOR,
    .bInterfaceSubClass = USB_IFACE_SUBCLASS_VENDOR,
    .bInterfaceProtocol = USB_IFACE_PROTOCOL_VENDOR,
    .iInterface = 0,
};

usb_configuration_c usb_config = {
    {
        .bLength = sizeof(struct usb_desc_configuration),
        .bDescriptorType = USB_DESC_CONFIGURATION,
        .bNumInterfaces = 1,
        .bConfigurationValue = 1,
        .iConfiguration = 0,
        .bmAttributes = USB_ATTR_RESERVED_1,
        .bMaxPower = 50,
    },
    {
        {.interface = &usb_interface},
        {0},
    },
};

usb_configuration_set_c usb_configs[] = {
    &usb_config,
};

usb_ascii_string_c usb_strings[] = {
    [0] = "intx82@gmail.com",
    [1] = "FX2 i2c-tiny-usb",
};

usb_descriptor_set_c usb_descriptor_set = {
    .device = &usb_device,
    .config_count = ARRAYSIZE(usb_configs),
    .configs = usb_configs,
    .string_count = ARRAYSIZE(usb_strings),
    .strings = usb_strings,
};

struct i2c_cmd
{
    uint8_t type;
    uint8_t cmd;
    uint16_t flags;
    uint16_t addr;
    uint16_t len;
};

static uint8_t _i2c_start(uint8_t addr)
{
    (void)addr;
    // delay_us(100);
    // return true;
    return i2c_start(addr);
}

static void _i2c_stop()
{
    // delay_us(100);
    i2c_stop();
}

static uint8_t _i2c_read(uint8_t *buf, uint8_t sz)
{
    // memset(buf, 0xcc, sz);
    // delay_us(100);
    // return true;
    return i2c_read(buf, sz);
}

static uint8_t _i2c_write(uint8_t *buf, uint8_t sz)
{
    (void)buf;
    (void)sz;
    // delay_us(100);
    // return true;
    return i2c_write(buf, sz);
}

static uint8_t _pending_flag;
static struct i2c_cmd _pending_req;

static void i2c_do(struct i2c_cmd *cmd)
{
    uint8_t len = cmd->len;
    uint16_t addr = (cmd->addr << 1);

    if (len > sizeof(EP0BUF))
    {
        STALL_EP0();
        return;
    }

    if (cmd->flags & I2C_M_RD)
    {
        addr |= 1;
    }

    if (cmd->cmd & CMD_I2C_BEGIN)
    {
        if (_i2c_start(addr))
        {
            status = STATUS_ADDRESS_ACK;
        }
        else
        {
            status = STATUS_ADDRESS_NAK;
            SETUP_EP0_BUF(0);
            return;
        }
    }

    if (cmd->flags & I2C_M_RD)
    {
        _i2c_start(addr); // repeated start
        while (EP0CS & _BUSY)
            ;
        _i2c_read(EP0BUF, len);
        SETUP_EP0_BUF(len);
    }
    else
    {
        _i2c_write(scratch, len);
        ACK_EP0();
    }

    if (cmd->cmd & CMD_I2C_END)
    {
        _i2c_stop();
    }
}

void handle_usb_setup(__xdata struct usb_req_setup *req)
{
    if (req->bRequest == CMD_ECHO)
    {
        while (EP0CS & _BUSY)
            ;
        ((uint8_t *)EP0BUF)[0] = req->wValue & 0xff;
        ((uint8_t *)EP0BUF)[1] = (req->wValue >> 8) & 0xff;
        SETUP_EP0_BUF(2);
        return;
    }

    if (req->bRequest == CMD_GET_FUNC) // req->bmRequestType: 0xc1
    {
        while (EP0CS & _BUSY)
            ;
        ((uint8_t *)EP0BUF)[0] = i2c_linux_functionality & 0xff;
        ((uint8_t *)EP0BUF)[1] = (i2c_linux_functionality >> 8) & 0xff;
        ((uint8_t *)EP0BUF)[2] = (i2c_linux_functionality >> 16) & 0xff;
        ((uint8_t *)EP0BUF)[3] = (i2c_linux_functionality >> 24) & 0xff;
        SETUP_EP0_BUF(sizeof(i2c_linux_functionality));
        return;
    }

    if (req->bRequest == CMD_SET_DELAY) // req->bmRequestType: 0x41
    {
        ACK_EP0();
        return;
    }

    if (req->bRequest == CMD_GET_STATUS)
    {
        if (!_pending_flag)
        {
            _pending_flag = 2;
            return;
        }
        // return;
    }

    if (req->bRequest >= CMD_I2C_IO && req->bRequest <= (CMD_I2C_IO | CMD_I2C_BEGIN | CMD_I2C_END))
    {
        if (!_pending_flag)
        {
            if (!req->wValue & 1)
            {
                _pending_flag = 0xff;
                SETUP_EP0_BUF(0);
                while (EP0CS & _BUSY)
                    ;
                xmemcpy(scratch, EP0BUF, req->wLength);
            }
            _pending_flag = 1;
            return;
        }
    }

    STALL_EP0();
}

int main()
{
    CPUCS = _CLKSPD1;
    usb_init(true);
    while (1)
    {
        if (_pending_flag)
        {
            if (_pending_flag == 1)
            {
                _pending_flag = 0;
                i2c_do((struct i2c_cmd *)SETUPDAT);
            }
            else if (_pending_flag == 2)
            {
                while (EP0CS & _BUSY)
                    ;
                ((uint8_t *)EP0BUF)[0] = status;
                SETUP_EP0_BUF(sizeof(status));
                _pending_flag = 0;
            }
        }
    }
}
