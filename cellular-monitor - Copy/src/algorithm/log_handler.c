/**
 ******************************************************************************
 * @file    log_handler.c
 * @author  Charles Fayal
 * @brief   Handler for algorithm logging state management
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2024 NOWI Sensors
 * All rights reserved.</center></h2>
 *
 *
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/pm/device.h>
#include "flash_utility.h"
#include "log_handler.h"

LOG_MODULE_REGISTER(log_handler, CONFIG_APP_LOG_LEVEL);

#ifdef CONFIG_PROVISIONING
// default to enabled so we can write information to the device
static bool logging_enabled = true; // Default to enabled
#else
static bool logging_enabled = false; // Default to disabled
#endif
static bool initialized = false;

/**
 * @brief Enable logging and UART
 *
 * @retval int 0 on success, negative error code on failure
 **/
int log_handler_enable_logging_and_uart(void)
{
#if CONFIG_LOG == n
  LOG_INF("Logging and UART disabled in config");
  return 0;
#else

  LOG_INF("Enabling logging and UART");
  // Commented out because it was causing a hard fault on start
  // Enable all logging backends
  // uint32_t count = log_backend_count_get();
  // for (uint32_t i = 0; i < count; i++)
  // {
  //   log_backend_activate(log_backend_get(i), NULL);
  // }

  // Enable log message processing with default level
  // log_filter_set(NULL, 0, LOG_MODULE_DEFAULT, CONFIG_LOG_DEFAULT_LEVEL);

  // Resume UART peripheral
  const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
  if (device_is_ready(uart_dev))
  {
    enum pm_device_state state;
    pm_device_state_get(uart_dev, &state);
    if (state == PM_DEVICE_STATE_SUSPENDED)
    {
      pm_device_action_run(uart_dev, PM_DEVICE_ACTION_RESUME);
    }
    else
    {
      LOG_INF("UART device already enabled");
    }
  }
  else
  {
    LOG_ERR("UART device not ready");
    return -EIO;
  }

  LOG_INF("Logging and UART enabled");
  return 0;
#endif
}

/**p
 * @brief Disable logging and UART
 *
 * @retval int 0 on success, negative error code on failure
 **/
int log_handler_disable_logging_and_uart(void)
{
#if CONFIG_LOG == n
  LOG_INF("Logging and UART disabled in config");
  return 0;
#else

  LOG_INF("Disabling logging and UART");
  // Disable all logging backends
  // uint32_t count = log_backend_count_get();
  // for (uint32_t i = 0; i < count; i++)
  // {
  //   log_backend_deactivate(log_backend_get(i));
  // }

  // Disable log message processing
  // log_filter_set(NULL, 0, LOG_MODULE_DEFAULT, LOG_LEVEL_NONE);

  // Suspend UART peripheral
  const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
  if (device_is_ready(uart_dev))
  {
    enum pm_device_state state;
    pm_device_state_get(uart_dev, &state);
    if (state == PM_DEVICE_STATE_ACTIVE)
    {
      pm_device_action_run(uart_dev, PM_DEVICE_ACTION_SUSPEND);
    }
    else
    {
      LOG_INF("UART device already disabled");
    }
  }

  // Note: We can't log this message since logging is disabled
  return 0;
#endif
}

/**
 * @brief Initialize the log handler
 *
 * @retval int 0 on success, negative error code on failure
 **/
int log_handler_init(void)
{
  if (initialized)
  {
    return 0;
  }

  LOG_INF("Initializing log handler");

  // Load logging state from flash
  FlashStatus_t status = FlashUtility_LoadLoggingState(&logging_enabled);
  if (status == FlashStatus_Success)
  {
    LOG_INF("Logging state loaded from flash: %s", logging_enabled ? "ENABLED" : "DISABLED");
  }
  else if (status == FlashStatus_NoData)
  {
    LOG_INF("No logging state found in flash, using default: %d", logging_enabled);
  }
  else
  {
    LOG_ERR("Failed to load logging state from flash, using default: %d", logging_enabled);
  }

  // Apply the loaded logging state
  if (logging_enabled)
  {
    log_handler_enable_logging_and_uart();
  }
  else
  {
    log_handler_disable_logging_and_uart();
  }

  initialized = true;
  return 0;
}

/**
 * @brief Get the current logging state
 *
 * @retval bool true if logging is enabled, false if disabled
 **/
bool log_handler_is_enabled(void)
{
#if CONFIG_LOG == n
  return false;
#else

  if (!initialized)
  {
    LOG_WRN("Log handler not initialized, returning default state");
    return true; // Default to enabled
  }
  return logging_enabled;
#endif
}

/**
 * @brief Set the logging state
 *
 * @param[in] enabled    The logging state to set (true for enabled, false for disabled)
 * @retval int 0 on success, negative error code on failure
 **/
int log_handler_set_enabled(bool enabled)
{
  if (!initialized)
  {
    LOG_ERR("Log handler not initialized");
    return -EINVAL;
  }

  if (logging_enabled == enabled)
  {
    LOG_INF("Logging state already set to: %s", enabled ? "ENABLED" : "DISABLED");
    return 0;
  }

  FlashStatus_t status = FlashUtility_SaveLoggingState(enabled);
  if (status == FlashStatus_Success)
  {
    logging_enabled = enabled;

    // Apply the new logging state
    if (enabled)
    {
      log_handler_enable_logging_and_uart();
      LOG_INF("Logging state changed to: ENABLED");
    }
    else
    {
      // Disable logging first, then save state
      log_handler_disable_logging_and_uart();
      // Note: Can't log after disabling
    }

    return 0;
  }
  else
  {
    LOG_ERR("Failed to save logging state to flash");
    return -EIO;
  }
}

/**
 * @brief Toggle the logging state
 *
 * @retval int 0 on success, negative error code on failure
 **/
int log_handler_toggle(void)
{
  return log_handler_set_enabled(!logging_enabled);
}

/************************ (C) COPYRIGHT NOWI Sensors LLC *****END OF FILE****/