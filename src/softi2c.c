#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <fx2regs.h>
#include <fx2delay.h>
#include "softi2c.h"


#define I2C_PORT IOA
#define I2C_PIN IOA
#define I2C_DDR OEA

#define I2C_SDA (1 << 2)
#define I2C_SCL (1 << 1)

static bool _init = false;


static void i2c_io_set_sda(uint8_t hi)
{
    if (hi)
    {
        I2C_PORT |= I2C_SDA; // with pullup
        I2C_DDR &= ~I2C_SDA; // high -> input
    }
    else
    {
        I2C_DDR |= I2C_SDA;   // low -> output
        I2C_PORT &= ~I2C_SDA; // drive low
    }
    _NOP1;
}

static uint8_t i2c_io_get_sda(void)
{
    // I2C_DDR &= ~I2C_SDA;
    return (I2C_PIN & I2C_SDA);
}

static void i2c_io_set_scl(uint8_t hi)
{
    I2C_DDR |= I2C_SCL;
    if (hi)
    {
        I2C_PORT |= I2C_SCL; // port is high
        _NOP3;
    }
    else
    {
        I2C_PORT &= ~I2C_SCL; // port is low
        _NOP1;
    }
}

static void i2c_init(void)
{
    if (!_init)
    {
        /* init the sda/scl pins */
        I2C_DDR &= ~I2C_SDA; // port is input
        I2C_PORT |= I2C_SDA; // enable pullup
        I2C_DDR |= I2C_SCL;  // port is output
        _init = true;
    }
}

/* clock HI, delay, then LO */
static void i2c_scl_toggle(void)
{
    i2c_io_set_scl(1);
    i2c_io_set_scl(0);
}

/* i2c stop condition */
void i2c_stop(void)
{
    i2c_io_set_sda(0);
    i2c_io_set_scl(1);
    i2c_io_set_sda(1);
}

static uint8_t i2c_put_u08(uint8_t b)
{
    for (int8_t i = 0; i < 8; i++)
    {
        if (b & (1 << (7 - i)))
        {
            i2c_io_set_sda(1);
        }
        else
        {
            i2c_io_set_sda(0);
        }

        i2c_scl_toggle(); // clock HI, delay, then LO
    }

    i2c_io_set_sda(1); // leave SDL HI
    i2c_io_set_scl(1); // clock back up

    b = i2c_io_get_sda(); // get the ACK bit
    i2c_io_set_scl(0);    // not really ??

    return (b == 0); // return ACK value
}

static uint8_t i2c_get_u08(uint8_t last)
{
    uint8_t c, b = 0;

    i2c_io_set_sda(1); // make sure pullups are activated
    i2c_io_set_scl(0); // clock LOW

    for (uint8_t i = 0; i < 8; i++)
    {
        i2c_io_set_scl(1); // clock HI
        c = i2c_io_get_sda();
        b <<= 1;
        if (c)
        {
            b |= 1;
        }
        i2c_io_set_scl(0); // clock LO
    }

    if (last)
    {
        i2c_io_set_sda(1); // set NAK
    }
    else
    {
        i2c_io_set_sda(0); // set ACK
    }

    i2c_scl_toggle();  // clock pulse
    i2c_io_set_sda(1); // leave with SDL HI

    return b; // return received byte
}

/* i2c start condition */
uint8_t i2c_start(uint8_t addr)
{
    i2c_init();
    i2c_io_set_sda(1);
    i2c_io_set_scl(1);
    i2c_io_set_sda(0);
    i2c_io_set_scl(0);

    return i2c_put_u08(addr);
}

uint8_t i2c_read(uint8_t *buf, uint8_t sz)
{
    for (uint8_t i = 0; i < sz; i++)
    {
        buf[i] = i2c_get_u08(i == (sz - 1));
    }

    return true;
}

uint8_t i2c_write(uint8_t *buf, uint8_t sz)
{
    for (uint8_t i = 0; i < sz; i++)
    {
        if (!i2c_put_u08(buf[i]))
        {
            i2c_stop();
            return false;
        }
    }

    return true;
}