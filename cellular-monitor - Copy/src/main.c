#include <zephyr/kernel.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/reboot.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "app.h"
#include "cloud.h"
#include "error_handler.h"
#include "hw_id.h"
#include "led_manager.h"
#include "twi_driver.h"
#include "flash_utility.h"
#include "button_manager.h"
#include "buzzer.h"
#include "codec.h"
#include "interaction_manager.h"

#include <zephyr/logging/log.h>
#ifdef CONFIG_SIMULATED_MAG
#include "signal.h"
#endif

#include "log_handler.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

struct app_info_t app_info = {0};
char device_id[CONFIG_MAX_DEV_ID_LEN] = {0};
uint32_t reset_cause = 0;
bool is_provisioned = false;
bool device_enabled = true;

void button_enabled_device(void)
{
	LOG_INF("button_enabled_device");
	if (FlashUtility_SaveDeviceState(true) == FlashStatus_Success)
	{
		LOG_INF("Device state saved successfully");
	}
	else
	{
		LOG_ERR("Failed to save device state");
	}
	// Reboot for a fresh start
	sys_reboot(0);
}

void button_disabled_device(void)
{
	LOG_INF("button_disabled_device");
	if (FlashUtility_SaveDeviceState(false) == FlashStatus_Success)
	{
		LOG_INF("Device state saved successfully");
	}
	else
	{
		LOG_ERR("Failed to save device state");
	}
	// Reboot for a fresh start
	sys_reboot(0);
}

void enable_device(void)
{
	LOG_INF("enable_device");
	int err = 0;
	interaction_manager_device_enabled();
#ifdef CONFIG_TESTING_DISABLE_MODEM
	err = cloud_stop();
	if (err)
	{
		LOG_ERR("cloud_stop() failed, err 0x%x \n", err);
	}
#else
	LOG_INF("cloud_start");
	err = cloud_start();
	if (err)
	{
		LOG_ERR("cloud_start() failed, err 0x%x \n", err);
		led_manager_led_on_for(LED_ERROR_CLOUD_FAILED_DURATION_MS, LED_ERROR);
		k_msleep(5000);
		sys_reboot(0);
	}
#endif
	LOG_INF("app_start");
	app_start();
	device_enabled = true;
	LOG_INF("FlashUtility_SaveDeviceState");
	FlashUtility_SaveDeviceState(true);
	LOG_INF("Device enabled");
}

void disable_device(void)
{
	LOG_INF("disable_device");
	interaction_manager_device_disabled();
	// Send the disabled state to the cloud before disconnecting
	int err = send_device_state_to_cloud(false);
	if (err == -ENOTCONN)
	{
		LOG_WRN("Device not connected to cloud. Proceeding with disable process.");
	}
	else if (err)
	{
		LOG_ERR("Failed to send disable status to cloud, err %d", err);
	}
	else
	{
		LOG_INF("Disable status sent to cloud successfully");
	}

	app_stop();
	err = cloud_stop();
	if (err)
	{
		LOG_ERR("cloud_stop() failed, err %d", err);
	}
	device_enabled = false;
	FlashUtility_SaveDeviceState(false);
	LOG_INF("Device disabled");
}

int main(void)
{
	int err = 0;
	LOG_INF("main started");

	err = hwinfo_get_reset_cause(&reset_cause);
	if (!err)
	{
		LOG_WRN("reset cause 0x%02x", reset_cause);
		app_info.reset_cause = reset_cause;
		Reset_Reason(reset_cause);
	}
	else
	{
		LOG_ERR("hwinfo_get_reset_cause failed, err %d", err);
		Reset_Reason(err);
	}
	hwinfo_clear_reset_cause();

	FlashUtility_init();
	log_handler_init(); // Depends on FlashUtility_init()
	twi_init();
	led_manager_init();
	interaction_manager_init();

	app_init();

	err = FlashUtility_LoadDeviceID(device_id, CONFIG_FW_ID_LEN);
	if (err != FlashStatus_Success)
	{

#ifdef CONFIG_PROVISIONING
		LOG_INF("Disabling device until commissioned with ID");
		app_stop();
		cloud_init(app_info);
		err = cloud_stop();
		led_manager_led_on_for(LED_ERROR_NOT_COMMISSIONED_DURATION_MS, LED_ERROR);
		if (err)
		{
			LOG_ERR("cloud_stop() failed, err %d", err);
			sys_reboot(0); // rebooting caused it to cycle if failed
		}
		return 0;
#endif

		LOG_ERR("FlashUtility_LoadDeviceID failed, err %d", err);

		// Trying to load hw ID for some older devices
		err = hw_id_get(device_id, HW_ID_LEN);
		if (err != 0)
		{
			LOG_ERR("hw_id_get failed, err %d", err);
			led_manager_led_on_for(LED_ERROR_HW_ID_GET_DURATION_MS, LED_ERROR);
			k_msleep(5000);
			sys_reboot(0);
		}
		else
		{
			LOG_INF("Device ID from hw: %s", device_id);
			memcpy(app_info.device_id_buf, device_id, HW_ID_LEN);
			app_info.device_id_len = HW_ID_LEN;
		}
	}
	else
	{
		LOG_INF("Device ID from flash: %s", device_id);
		memcpy(app_info.device_id_buf, device_id, CONFIG_FW_ID_LEN);
		app_info.device_id_len = CONFIG_FW_ID_LEN;
	}

	err = FlashUtility_GetProvisioning_State(&is_provisioned);
	switch (err)
	{
	case FlashStatus_Success:
		break;
	case FlashStatus_NoData:
		LOG_WRN("FlashUtility_GetProvisioning_State no data, defaulting to false");
		is_provisioned = false;
		break;
	case FlashStatus_Failure:
		LOG_ERR("FlashUtility_GetProvisioning_State failed, err %d", err);
		is_provisioned = false;
		break;
	}

	app_info.is_provisioned = is_provisioned;
	cloud_init(app_info);

	if (FlashUtility_LoadDeviceState(&device_enabled) == FlashStatus_Success)
	{
		LOG_INF("Loaded device state: %s", device_enabled ? "ON" : "OFF");
	}
	else
	{
		LOG_WRN("Failed to load device state, using default: ON");
	}

	if (device_enabled)
	{
		enable_device(); // Enable the device on startup if it was enabled when last powered off
	}
	else
	{
		LOG_INF("Device starting in disabled state");
		disable_device();
	}

	button_manager_init(button_enabled_device, button_disabled_device);

#ifdef CONFIG_SIMULATED_MAG
	printk("WARNING: Simulated MAG\n");

	Signal_Init(
			10.0f,	// noiseAmplitudeInput - moderate noise level
			15.0f,	// signalFrequencyInput - 1 Hz base frequency
			160.0f, // signalAmplitudeInput - base signal amplitude
			false,	// pulseEnableInput - start without pulse
			false,	// fluctuateNoiseInput - enable noise fluctuation
			false,	// popUp - disable pop-up events
			1.0f,		// xScaleInput - normal scale for X axis
			1.0f,		// yScaleInput - normal scale for Y axis
			1.0f,		// zScaleInput - normal scale for Z axis
			false		// _offsetBump - disable offset bumping
	);
#endif

	return 0;
}
