/**
 ******************************************************************************
 * @file    log_handler.h
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

#ifndef __LOG_HANDLER_H__
#define __LOG_HANDLER_H__

#include <stdbool.h>

/**
 * @brief Initialize the log handler
 *
 * @retval int 0 on success, negative error code on failure
 **/
int log_handler_init(void);

/**
 * @brief Get the current logging state
 *
 * @retval bool true if logging is enabled, false if disabled
 **/
bool log_handler_is_enabled(void);

/**
 * @brief Set the logging state
 *
 * @param[in] enabled    The logging state to set (true for enabled, false for disabled)
 * @retval int 0 on success, negative error code on failure
 **/
int log_handler_set_enabled(bool enabled);

/**
 * @brief Toggle the logging state
 *
 * @retval int 0 on success, negative error code on failure
 **/
int log_handler_toggle(void);

/**
 * @brief Enable logging and UART
 *
 * @retval int 0 on success, negative error code on failure
 **/
int log_handler_enable_logging_and_uart(void);

/**
 * @brief Disable logging and UART
 *
 * @retval int 0 on success, negative error code on failure
 **/
int log_handler_disable_logging_and_uart(void);

#endif /* __LOG_HANDLER_H__ */

/************************ (C) COPYRIGHT NOWI Sensors LLC *****END OF FILE****/