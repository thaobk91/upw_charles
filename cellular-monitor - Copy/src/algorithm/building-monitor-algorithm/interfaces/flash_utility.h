/**
 ******************************************************************************
 * @file    flash_utility.h
 * @author  Charles Fayal
 * @brief   Utility for accessing flash memory
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 20223 NOWI Sensors
 * All rights reserved.</center></h2>
 *
 *
 ******************************************************************************
 */

#ifndef __FLASH_UTILITY_H__
#define __FLASH_UTILITY_H__

#include <stdint.h>
#include <stdbool.h>

// Forward declaration - device_type_t is defined in app.h
typedef enum device_type_e device_type_t;

typedef enum
{
  FlashStatus_Success = 0,
  FlashStatus_Failure = 1,
  FlashStatus_NoData = 2
} FlashStatus_t;

int FlashUtility_save_config(uint32_t app_duty_cycle, uint32_t upload_duty_cycle);
int FlashUtility_pull_config(uint32_t *app_duty_cycle, uint32_t *upload_duty_cycle);

/**
 * @brief Initialize the flash space.
 *
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_init(void);

/**
 * @brief Saves the device ID to flash.
 *
 * @param[in] deviceID           The device ID to save
 * @param[in] deviceIDLength     The length of the device ID
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_SaveDeviceID(char *deviceID, uint8_t deviceIDLength);

/**
 * @brief Loads the device ID from flash.
 *
 * @param[out] deviceID           The device ID to load
 * @param[in] deviceIDLength     The length of the device ID
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_LoadDeviceID(char *deviceID, uint8_t deviceIDLength);

/**
 * @brief Saves half_cycles to memory.
 *
 * @param[in] half_cycles        The variable to save
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_SaveHalfCycles(uint32_t half_cycles);

/**
 * @brief Loads total pulses from flash.
 *
 * @param[in] half_cycles        Variable to load
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_LoadHalfCycles(uint32_t *half_cycles);

/**
 * @brief Saves LowestFlowOverPeriodTracker Data
 *
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_Save_LowestFlowOverPeriodTrackerData(uint16_t numberBuckets, uint16_t bucketDuration);

/**
 * @brief Loads LowestFlowOverPeriodTracker Data
 *
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_Load_LowestFlowOverPeriodTrackerData(uint16_t *numberBuckets, uint16_t *bucketDuration);

/**
 * @brief Erases all lowest flow over period tracker data.
 **/
FlashStatus_t FlashUtility_Erase_LowestFlowOverPeriodTrackerData(void);

/**
 * @brief Loads configuration
 *
 * @retval FlashStatus_t of the operation
 * @note This function will erase the entire flash page before writing the data.
 *      This is done to ensure that the data is written correctly.
 * @note Currently these variables are all globals, which is why there is no parameters
 **/
FlashStatus_t FlashUtility_LoadConfiguration(void);

/**
 * @brief Saves the configuration to flash.
 *
 * @retval FlashStatus_t of the operation
 * @note This function will erase the entire flash page before writing the data.
 *      This is done to ensure that the data is written correctly.
 * @note Currently these variables are all globals, which is why there is no parameters
 **/

FlashStatus_t FlashUtility_SaveConfiguration(void);

FlashStatus_t FlashUtility_SetProvisioning_State(bool provisioned);

FlashStatus_t FlashUtility_GetProvisioning_State(bool *provisioned);

FlashStatus_t FlashUtility_EraseConfiguration(void);

uint32_t FloatToUInt32(float input);

float Uint32ToFloat(uint32_t input);

/**
 * @brief Saves the device state to flash.
 *
 * @param[in] is_enabled    The device state to save (true for enabled, false for disabled)
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_SaveDeviceState(bool is_enabled);

/**
 * @brief Loads the device state from flash.
 *
 * @param[out] is_enabled   Pointer to store the loaded device state
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_LoadDeviceState(bool *is_enabled);

/**
 * @brief Saves the device type configuration to flash.
 *
 * @param[in] device_type    The device type to save (enum value)
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_SaveDeviceType(device_type_t device_type);

/**
 * @brief Loads the device type configuration from flash.
 *
 * @param[out] device_type   Pointer to store the loaded device type
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_LoadDeviceType(device_type_t *device_type);

/**
 * @brief Saves the algorithm logging state to flash.
 *
 * @param[in] logging_enabled    The logging state to save (true for enabled, false for disabled)
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_SaveLoggingState(bool logging_enabled);

/**
 * @brief Loads the algorithm logging state from flash.
 *
 * @param[out] logging_enabled   Pointer to store the loaded logging state
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_LoadLoggingState(bool *logging_enabled);

#endif /* __FLASH_UTILITY_H__ */

/************************ (C) COPYRIGHT NOWI Sensors LLC *****END OF FILE****/
