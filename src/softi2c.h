#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include <stdint.h>

/**
 * @brief Generate 'Start' impulse and send device address into bus
 * @param addr device address
 * @return 1 - if device acknowledge request, otherwise 0
 */
uint8_t i2c_start(uint8_t addr);

/**
 * @brief Send 'Stop' signal into bus
*/
void i2c_stop();

/**
 * @brief Reads from the bus, if read less than provided sz - returns 0
 * @param buf - output buffer
 * @param sz - buffer size
 * 
 * @return 1 - on ok
*/
uint8_t i2c_read(uint8_t* buf, uint8_t sz);

/**
 * @brief Writes data into bus
 * @param buf - output buffer
 * @param sz - buffer size
 * 
 * @return 0 - if devices send nak before sz is reached, otherwise 1
*/
uint8_t i2c_write(uint8_t* buf, uint8_t sz);

#endif