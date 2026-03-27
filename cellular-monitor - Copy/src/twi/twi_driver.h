#ifndef __I2C_DRIVER_H__
#define __I2C_DRIVER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

  int twi_init(void);
  int twi_write(uint8_t i2c_addr, uint8_t reg_add, const uint8_t *data, uint16_t data_len);
  int twi_read(uint8_t i2c_addr, uint8_t reg_add, uint8_t *data, uint16_t data_len);
  int twi_reset(void);

#ifdef __cplusplus
}
#endif

#endif //__I2C_DRIVER_H__