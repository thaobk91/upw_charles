/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include "at_handler.h"
#include <stdio.h>
#include <string.h>
#include <modem/at_cmd_custom.h>
#include "building-monitor-algorithm/interfaces/flash_utility.h"
#include "app.h"
#include <zephyr/logging/log.h>
#include "log_handler.h"

LOG_MODULE_REGISTER(at_handler, CONFIG_APP_LOG_LEVEL);

#define ID_LEN 16

static int my_command_callback(char *buf, size_t len, char *at_cmd);
static int provisioned_callback(char *buf, size_t len, char *at_cmd);
static int pulse_callback(char *buf, size_t len, char *at_cmd);
static int devicetype_callback(char *buf, size_t len, char *at_cmd);
static int disable_callback(char *buf, size_t len, char *at_cmd);
static int logging_callback(char *buf, size_t len, char *at_cmd);

AT_CMD_CUSTOM(my_command_filter, "AT+DEVICEID", my_command_callback);
AT_CMD_CUSTOM(provisioned_command_filter, "AT+PROVISIONED", provisioned_callback);
AT_CMD_CUSTOM(pulse_command_filter, "AT+PULSE", pulse_callback);
AT_CMD_CUSTOM(devicetype_command_filter, "AT+DEVICETYPE", devicetype_callback);
AT_CMD_CUSTOM(disable_command_filter, "AT+DISABLE", disable_callback);
AT_CMD_CUSTOM(logging_command_filter, "AT+LOGGING", logging_callback);

static int my_command_callback(char *buf, size_t len, char *at_cmd)
{
	FlashStatus_t flash_status;
	LOG_INF("at_cmd=%s buf=%s len=%d strlen=%d \n", at_cmd, buf, len, strlen(buf));

	/* test */
	if (strncmp("AT+DEVICEID=?", at_cmd, strlen("AT+DEVICEID=?")) == 0)
	{
		char device_id_saved[ID_LEN];
		flash_status = FlashUtility_LoadDeviceID(device_id_saved, ID_LEN);
		if (flash_status == FlashStatus_Success)
		{
			LOG_INF("device_id_saved=%.*s \n", ID_LEN, device_id_saved);
		}
		else
		{
			LOG_ERR("FlashUtility_LoadDeviceID failed status=%d \n", flash_status);
		}

		return at_cmd_custom_respond(buf, len, "\r\nOK\r\n");
	}

	/* set */
	// AT+DEVICEID=<id>
	if (strncmp("AT+DEVICEID=", at_cmd, strlen("AT+DEVICEID=")) == 0)
	{
		// Get the device ID from the buf
		char *id = buf + strlen("AT+DEVICEID=");
		size_t id_size = strlen(buf) - strlen("AT+DEVICEID=");

		if (id_size > ID_LEN)
		{
			LOG_ERR("id_size=%d \n", id_size);
			return at_cmd_custom_respond(buf, len, "+CME ERROR: ID wrong size must be %d \r\n", 161);
		}

		printk("id=%.*s \n", id_size, id);

		flash_status = FlashUtility_SaveDeviceID(id, id_size);
		if (flash_status == FlashStatus_Success)
		{
			return at_cmd_custom_respond(buf, len, "OK\r\n");
		}
		else
		{
			LOG_ERR("FlashUtility_SaveDeviceID failed status=%d \n", flash_status);
			return at_cmd_custom_respond(buf, len, "ERROR\r\n");
		}
	}

	/* read */
	if (strncmp("AT+DEVICEID?", at_cmd, strlen("AT+DEVICEID?")) == 0)
	{
		return at_cmd_custom_respond(buf, len, "Set and read the device ID\r\n");
	}

	return -1;
}

static int provisioned_callback(char *buf, size_t len, char *at_cmd)
{
	FlashStatus_t flash_status;
	LOG_INF("at_cmd=%s buf=%s len=%d strlen=%d \n", at_cmd, buf, len, strlen(buf));
	bool provisioned = false;
	/* test */
	if (strncmp("AT+PROVISIONED=?", at_cmd, strlen("AT+PROVISIONED=?")) == 0)
	{
		flash_status = FlashUtility_GetProvisioning_State(&provisioned);
		if (flash_status == FlashStatus_Success)
		{
			LOG_INF("provisioned=%d \n", provisioned);
		}
		else
		{
			LOG_ERR("FlashUtility_GetProvisioning_State failed status=%d \n", flash_status);
		}

		return at_cmd_custom_respond(buf, len, "\r\nOK\r\n");
	}

	/* set */
	// AT+DEVICEID=<id>
	if (strncmp("AT+PROVISIONED=", at_cmd, strlen("AT+PROVISIONED=")) == 0)
	{
		// Get bool 0 or 1 from the buf
		if (strlen(buf) - strlen("AT+PROVISIONED=") != 1)
		{
			LOG_ERR("buf=%s \n", buf);
			return at_cmd_custom_respond(buf, len, "+CME ERROR: ID wrong size must be 1 \r\n");
		}
		else if (buf[strlen("AT+PROVISIONED=")] == '0')
		{
			provisioned = false;
		}
		else if (buf[strlen("AT+PROVISIONED=")] == '1')
		{
			provisioned = true;
		}
		else
		{
			LOG_ERR("buf=%s \n", buf);
			return at_cmd_custom_respond(buf, len, "+CME ERROR: ID wrong value must be 0 or 1 \r\n");
		}

		LOG_INF("provisioned=%d \n", provisioned);

		flash_status = FlashUtility_SetProvisioning_State(provisioned);
		if (flash_status == FlashStatus_Success)
		{
			return at_cmd_custom_respond(buf, len, "OK\r\n");
		}
		else
		{
			LOG_ERR("FlashUtility_SetProvisioning_State failed status=%d \n", flash_status);
			return at_cmd_custom_respond(buf, len, "ERROR\r\n");
		}
	}

	/* read */
	if (strncmp("AT+PROVISIONED?", at_cmd, strlen("AT+PROVISIONED?")) == 0)
	{
		return at_cmd_custom_respond(buf, len, "Set and read if the device is provisioned\r\n");
	}

	return -1;
}

static int pulse_callback(char *buf, size_t len, char *at_cmd)
{
	FlashStatus_t flash_status;
	LOG_INF("at_cmd=%s buf=%s len=%d strlen=%d \n", at_cmd, buf, len, strlen(buf));
	bool is_pulse_tracker = false;

	/* test */
	if (strncmp("AT+PULSE=?", at_cmd, strlen("AT+PULSE=?")) == 0)
	{
		device_type_t device_type;
		flash_status = FlashUtility_LoadDeviceType(&device_type);
		if (flash_status == FlashStatus_Success || flash_status == FlashStatus_NoData)
		{
			is_pulse_tracker = (device_type == DEVICE_TYPE_PULSE_TRACKER);
			LOG_INF("is_pulse_tracker=%d \n", is_pulse_tracker);
		}
		else
		{
			LOG_ERR("FlashUtility_LoadDeviceType failed status=%d \n", flash_status);
		}

		return at_cmd_custom_respond(buf, len, "\r\nOK\r\n");
	}

	/* set */
	if (strncmp("AT+PULSE=", at_cmd, strlen("AT+PULSE=")) == 0)
	{
		// Get y or n from the buf
		if (strlen(buf) - strlen("AT+PULSE=") != 1)
		{
			LOG_ERR("buf=%s \n", buf);
			return at_cmd_custom_respond(buf, len, "+CME ERROR: answer wrong size must be 1 \r\n");
		}
		else if (buf[strlen("AT+PULSE=")] == 'y' || buf[strlen("AT+PULSE=")] == 'Y')
		{
			is_pulse_tracker = true;
		}
		else if (buf[strlen("AT+PULSE=")] == 'n' || buf[strlen("AT+PULSE=")] == 'N')
		{
			is_pulse_tracker = false;
		}
		else
		{
			LOG_ERR("buf=%s \n", buf);
			return at_cmd_custom_respond(buf, len, "+CME ERROR: Value must be y or n \r\n");
		}

		LOG_INF("is_pulse_tracker=%d \n", is_pulse_tracker);

		device_type_t device_type = is_pulse_tracker ? DEVICE_TYPE_PULSE_TRACKER : DEVICE_TYPE_MAGNETOMETER;
		flash_status = FlashUtility_SaveDeviceType(device_type);
		if (flash_status == FlashStatus_Success)
		{
			return at_cmd_custom_respond(buf, len, "OK\r\n");
		}
		else
		{
			LOG_ERR("FlashUtility_SaveDeviceType failed status=%d \n", flash_status);
			return at_cmd_custom_respond(buf, len, "ERROR\r\n");
		}
	}

	/* read */
	if (strncmp("AT+PULSE?", at_cmd, strlen("AT+PULSE?")) == 0)
	{
		return at_cmd_custom_respond(buf, len, "Set (y/n)and read the device type (pulse tracker or magnetometer)\r\n");
	}

	return -1;
}

static int devicetype_callback(char *buf, size_t len, char *at_cmd)
{
	LOG_INF("at_cmd=%s buf=%s len=%d strlen=%d \n", at_cmd, buf, len, strlen(buf));
	device_type_t device_type;
	FlashStatus_t flash_status;

	/* test - query current device type */
	if (strncmp("AT+DEVICETYPE=?", at_cmd, strlen("AT+DEVICETYPE=?")) == 0)
	{
		flash_status = FlashUtility_LoadDeviceType(&device_type);
		if (flash_status == FlashStatus_Success || flash_status == FlashStatus_NoData)
		{
			LOG_INF("current device_type=%d \n", device_type);
			return at_cmd_custom_respond(buf, len, "+DEVICETYPE: %d\r\nOK\r\n", device_type);
		}
		else
		{
			LOG_ERR("FlashUtility_LoadDeviceType failed status=%d \n", flash_status);
			return at_cmd_custom_respond(buf, len, "ERROR\r\n");
		}
	}

	/* set device type */
	if (strncmp("AT+DEVICETYPE=", at_cmd, strlen("AT+DEVICETYPE=")) == 0)
	{
		// Get device type value from the buf
		if (strlen(buf) - strlen("AT+DEVICETYPE=") != 1)
		{
			LOG_ERR("buf=%s \n", buf);
			return at_cmd_custom_respond(buf, len, "+CME ERROR: Value wrong size must be 1 digit\r\n");
		}

		char type_char = buf[strlen("AT+DEVICETYPE=")];
		if (type_char == '0')
		{
			device_type = DEVICE_TYPE_MAGNETOMETER;
		}
		else if (type_char == '1')
		{
			device_type = DEVICE_TYPE_PULSE_TRACKER;
		}
		else if (type_char == '2')
		{
			device_type = DEVICE_TYPE_SENSUS_PROTOCOL;
		}
		else
		{
			LOG_ERR("buf=%s \n", buf);
			return at_cmd_custom_respond(buf, len, "+CME ERROR: Value must be 0 (Magnetometer), 1 (Pulse Tracker), or 2 (Sensus Protocol)\r\n");
		}

		LOG_INF("Setting device_type=%d \n", device_type);

		// Set the device type (this will reboot the device)
		app_set_device_type(device_type);

		// Note: Device will reboot, so this response may not be seen
		return at_cmd_custom_respond(buf, len, "OK - Rebooting to apply device type\r\n");
	}

	/* help */
	if (strncmp("AT+DEVICETYPE?", at_cmd, strlen("AT+DEVICETYPE?")) == 0)
	{
		return at_cmd_custom_respond(buf, len,
																 "Set and read device type:\r\n"
																 "0 = Magnetometer\r\n"
																 "1 = Pulse Tracker\r\n"
																 "2 = Sensus Protocol\r\n"
																 "Usage: AT+DEVICETYPE=<0|1|2>\r\n"
																 "Query: AT+DEVICETYPE=?\r\n");
	}

	return -1;
}

static int disable_callback(char *buf, size_t len, char *at_cmd)
{
	LOG_INF("at_cmd=%s buf=%s len=%d strlen=%d \n", at_cmd, buf, len, strlen(buf));

	/* test - query current disable state */
	if (strncmp("AT+DISABLE=?", at_cmd, strlen("AT+DISABLE=?")) == 0)
	{
		// You can implement a function to check if device is disabled
		// For now, returning a placeholder response
		return at_cmd_custom_respond(buf, len, "+DISABLE: 0\r\nOK\r\n");
	}

	/* set disable state */
	if (strncmp("AT+DISABLE=", at_cmd, strlen("AT+DISABLE=")) == 0)
	{
		// Get disable value from the buf
		if (strlen(buf) - strlen("AT+DISABLE=") != 1)
		{
			LOG_ERR("buf=%s \n", buf);
			return at_cmd_custom_respond(buf, len, "+CME ERROR: Value wrong size must be 1 digit\r\n");
		}

		char disable_char = buf[strlen("AT+DISABLE=")];
		if (disable_char == '0')
		{
			LOG_INF("Enabling device\r\n");
			// Add your enable logic here
			// This could involve starting sensors, enabling communications, etc.
			return at_cmd_custom_respond(buf, len, "OK - Device enabled\r\n");
		}
		else if (disable_char == '1')
		{
			LOG_INF("Disabling device\r\n");
			// Add your disable logic here
			// This could involve stopping sensors, disabling communications, etc.
			return at_cmd_custom_respond(buf, len, "OK - Device disabled\r\n");
		}
		else
		{
			LOG_ERR("buf=%s \n", buf);
			return at_cmd_custom_respond(buf, len, "+CME ERROR: Value must be 0 (enable) or 1 (disable)\r\n");
		}
	}

	/* help */
	if (strncmp("AT+DISABLE?", at_cmd, strlen("AT+DISABLE?")) == 0)
	{
		return at_cmd_custom_respond(buf, len,
																 "Enable or disable the device:\r\n"
																 "0 = Enable device\r\n"
																 "1 = Disable device\r\n"
																 "Usage: AT+DISABLE=<0|1>\r\n"
																 "Query: AT+DISABLE=?\r\n");
	}

	return -1;
}

static int logging_callback(char *buf, size_t len, char *at_cmd)
{
	LOG_INF("at_cmd=%s buf=%s len=%d strlen=%d \n", at_cmd, buf, len, strlen(buf));

	/* test - query current logging state */
	if (strncmp("AT+LOGGING=?", at_cmd, strlen("AT+LOGGING=?")) == 0)
	{
		bool logging_enabled = log_handler_is_enabled();
		LOG_INF("current logging_enabled=%d \n", logging_enabled);
		return at_cmd_custom_respond(buf, len, "+LOGGING: %d\r\nOK\r\n", logging_enabled ? 1 : 0);
	}

	/* set logging state */
	if (strncmp("AT+LOGGING=", at_cmd, strlen("AT+LOGGING=")) == 0)
	{
		// Get logging value from the buf
		if (strlen(buf) - strlen("AT+LOGGING=") != 1)
		{
			LOG_ERR("buf=%s \n", buf);
			return at_cmd_custom_respond(buf, len, "+CME ERROR: Value wrong size must be 1 digit\r\n");
		}

		char logging_char = buf[strlen("AT+LOGGING=")];
		bool enabled;
		if (logging_char == '0')
		{
			enabled = false;
		}
		else if (logging_char == '1')
		{
			enabled = true;
		}
		else
		{
			LOG_ERR("buf=%s \n", buf);
			return at_cmd_custom_respond(buf, len, "+CME ERROR: Value must be 0 (disable) or 1 (enable)\r\n");
		}

		// Important log line for provisioning script, the OK later will not go through if UART is disabled
		LOG_INF("OK - Setting logging_enabled=%d \n", enabled);

		int result = log_handler_set_enabled(enabled);
		if (result == 0)
		{
			if (enabled)
			{
				return at_cmd_custom_respond(buf, len, "OK - Logging enabled\r\n");
			}
			else
			{
				return at_cmd_custom_respond(buf, len, "OK - Logging disabled\r\n");
			}
		}
		else
		{
			return at_cmd_custom_respond(buf, len, "ERROR - Failed to set logging state\r\n");
		}
	}

	/* help */
	if (strncmp("AT+LOGGING?", at_cmd, strlen("AT+LOGGING?")) == 0)
	{
		return at_cmd_custom_respond(buf, len,
																 "Enable or disable algorithm logging:\r\n"
																 "0 = Disable logging (saves power)\r\n"
																 "1 = Enable logging\r\n"
																 "Usage: AT+LOGGING=<0|1>\r\n"
																 "Query: AT+LOGGING=?\r\n");
	}

	return -1;
}
