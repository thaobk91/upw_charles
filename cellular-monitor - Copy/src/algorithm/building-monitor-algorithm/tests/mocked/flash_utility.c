/**
  ******************************************************************************
  * @file    flash_utility.h
  * @author  Charles Fayal
  * @brief   Utility for accessing flash memory
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 NOWi Sensors
  * All rights reserved.</center></h2>
  *
  *
  ******************************************************************************
  */
 
#include "flash_utility.h"

FlashStatus_t FlashUtility_init( void )
{
  return FlashStatus_Success;
}

/**
 * @brief Saves half_cycles to memory.
 *
 * @param[in] half_cycles        The variable to save
 * @retval FlashStatus_t of the operation
**/
FlashStatus_t FlashUtility_SaveHalfCycles(uint32_t half_cycles)
{
  return FlashStatus_Success;
}

/**
 * @brief Loads total pulses from flash.
 *
 * @param[in] half_cycles        Variable to load
 * @retval FlashStatus_t of the operation
**/
FlashStatus_t FlashUtility_LoadHalfCycles( uint32_t *half_cycles ){
  *half_cycles = 0;
  return FlashStatus_Success;
}

/**
 * @brief Saves LowestFlowOverPeriodTracker Data
 *
 * @retval FlashStatus_t of the operation
**/
FlashStatus_t FlashUtility_Save_LowestFlowOverPeriodTrackerData(uint16_t numberBuckets, uint16_t bucketDuration)
{
  return FlashStatus_Success;
}

/**
 * @brief Loads LowestFlowOverPeriodTracker Data
 *
 * @retval FlashStatus_t of the operation
**/
FlashStatus_t FlashUtility_Load_LowestFlowOverPeriodTrackerData(uint16_t* numberBuckets, uint16_t* bucketDuration)
{
  #define DEFAULT_NUMBER_BUCKETS  10*60// 10 min @ 1 second per bucket
  #define DEFAULT_BUCKET_DURATION_SECONDS 1
  *numberBuckets = DEFAULT_NUMBER_BUCKETS;
  *bucketDuration = DEFAULT_BUCKET_DURATION_SECONDS;
  return FlashStatus_Success;
}

FlashStatus_t FlashUtility_Erase_LowestFlowOverPeriodTrackerData ( void )
{
  return FlashStatus_Success;
}

FlashStatus_t FlashUtility_LoadConfiguration( void )
{
  return FlashStatus_Success;
}

FlashStatus_t FlashUtility_SaveConfiguration( void )
{
  return FlashStatus_Success;
}

uint32_t FloatToUInt32(float input)
{
  return input * 1000.0f;
}

float Uint32ToFloat(uint32_t input)
{
  return input / 1000.0f;
}

static bool saved_device_state = true;  // Default to enabled

FlashStatus_t FlashUtility_SaveDeviceState(bool is_enabled)
{
    saved_device_state = is_enabled;
    return FlashStatus_Success;
}

FlashStatus_t FlashUtility_LoadDeviceState(bool *is_enabled)
{
    *is_enabled = saved_device_state;
    return FlashStatus_Success;
}

/************************ (C) COPYRIGHT NOWI Sensors LLC *****END OF FILE****/
