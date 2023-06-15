#include "i2c.h"
#include <stdbool.h>
#include <fx2regs.h>

#define DEFAULT_TIMEOUT 32

#define I2C_WAIT()                            \
    {                                         \
        if (!_i2c_wait_done(DEFAULT_TIMEOUT)) \
        {                                     \
            i2c_stop();                       \
            return false;                     \
        }                                     \
    }

static uint8_t _i2c_wait_done(uint8_t timeout)
{
    while (!(I2CS & _DONE))
    {
        if (--timeout == 0)
        {
            return false;
        }
    }

    if (I2CS & _BERR)
    {
        return false;
    }

    return true;
}

static uint8_t _i2c_wait_stop(uint8_t timeout)
{
    I2CS |= _STOP;
    volatile uint8_t _ = I2DAT;
    while (I2CS & _STOP)
    {
        if (--timeout == 0)
        {
            return false;
        }
    }
    return true;
}

uint8_t i2c_start(uint8_t addr)
{
    I2CS |= _START;
    I2DAT = addr;
    I2C_WAIT();
    if (I2CS & _ACK)
    {
        if (addr & 1) { // read
            volatile uint8_t _ = I2DAT;
        }
        return true;
    }

    i2c_stop();
    return false;
}

void i2c_stop()
{
    _i2c_wait_stop(DEFAULT_TIMEOUT);
}

uint8_t i2c_read(uint8_t *buf, uint8_t sz)
{
    for(uint8_t i = 0; i< sz - 1; i++) {
        I2C_WAIT()
        buf[i] = I2DAT;
    }

    I2CS |= _LASTRD;
    buf[sz - 1] = I2DAT;
    
    return 0;
}

uint8_t i2c_write(uint8_t *buf, uint8_t sz)
{
    for (uint8_t i = 0; i < sz; i++)
    {
        I2DAT = buf[i];
        I2C_WAIT();
        if (!(I2CS & _ACK))
        {
            i2c_stop();
            return false;
        }
    }
    return true;
}