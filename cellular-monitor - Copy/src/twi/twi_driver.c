/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @addtogroup t_i2c_basic
 * @{
 * @defgroup t_i2c_read_write test_i2c_read_write
 * @brief TestPurpose: verify I2C master can read and write
 * @}
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <string.h>

#include "twi_driver.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(twi, CONFIG_APP_LOG_LEVEL);


#if DT_NODE_EXISTS(DT_NODELABEL(mag1))
static const struct device *twi_dev = DEVICE_DT_GET(DT_NODELABEL(mag1));
#else
static const struct device *twi_dev = DEVICE_DT_GET(DT_NODELABEL(i2c2));
#endif

int twi_init(void)
{
	int err = 0;

	if (!device_is_ready(twi_dev))
	{
		LOG_ERR("I2C device is not ready");
		return -1;
	}

#if CONFIG_ENABLE_TWI_SCAN
	uint8_t found = 0;
	for (uint8_t i = 4; i <= 0x7F; i++)
	{
		struct i2c_msg msgs[1];
		uint8_t dst = 1;

		msgs[0].buf = &dst;
		msgs[0].len = 1U;
		msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

		int error = i2c_transfer(twi_dev, &msgs[0], 1, i);
		if (error == 0)
		{
			LOG_INF("i2c device found on 0x%2x", i);
			found++;
		}
	}
	LOG_INF("found %d i2c devices", found);
#endif

	return err;
}

int twi_write(uint8_t i2c_addr, uint8_t reg_add, const uint8_t *data, uint16_t data_len)
{
	int err = 0;
	struct i2c_msg msg;

	uint8_t buf[1 + data_len];
	buf[0] = reg_add;
	memcpy(buf + 1, data, data_len);

	msg.buf = buf;
	msg.len = 1 + data_len;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	err = i2c_transfer(twi_dev, &msg, 1, i2c_addr);
	if (err)
	{
		LOG_ERR("twi_write() i2c_transfer() failed, err %d", err);
	}

	return err;
}

int twi_read(uint8_t i2c_addr, uint8_t reg_add, uint8_t *data, uint16_t data_len)
{
	int err = 0;
	struct i2c_msg msg[2];

	msg[0].buf = &reg_add;
	msg[0].len = 1;
	msg[0].flags = I2C_MSG_WRITE;

	msg[1].buf = data;
	msg[1].len = data_len;
	msg[1].flags = I2C_MSG_READ | I2C_MSG_STOP;

	err = i2c_transfer(twi_dev, msg, 2, i2c_addr);
	if (err)
	{
		LOG_ERR("twi_read() i2c_transfer() failed, err %d", err);
	}

	return err;
}

int twi_reset(void)
{
	int err = 0;

	/* First recover the I2C bus */
	err = i2c_recover_bus(twi_dev);
	if (err)
	{
		LOG_ERR("Failed to recover I2C bus, err %d", err);
	}

	/* The I2C driver will automatically configure the pins correctly when resumed */
	LOG_INF("I2C bus reset complete");

	return err;
}
