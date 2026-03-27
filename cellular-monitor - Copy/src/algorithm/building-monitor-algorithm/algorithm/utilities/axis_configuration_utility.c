/**
  ******************************************************************************
  * @file    axis_configuration_utility.c
  * @author  Charles Fayal
  * @brief   Utility functions for cycle tracker algorithms 
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 NOWI Sensors
  * All rights reserved.</center></h2>
  *
  *
  ******************************************************************************
  */
  
#include <stdlib.h>
#include "axis_configuration_utility.h"
#include "log.h"
 
int compareDescending( const void* a, const void* b)
{
  int16_t int_a = * ( (int16_t*) a );
  int16_t int_b = * ( (int16_t*) b );

  if ( int_a == int_b ) return 0;
  else if ( int_a > int_b ) return -1;
  else return 1;
}

int compareAscending( const void* a, const void* b)
{
  int16_t int_a = * ( (int16_t*) a );
  int16_t int_b = * ( (int16_t*) b );

  if ( int_a == int_b ) return 0;
  else if ( int_a < int_b ) return -1;
  else return 1;
}

bool findValueWithCorrelations(int16_t *values, uint8_t size, uint16_t tolerance, uint8_t numCorrelations, int16_t *foundValue, bool getMinimum)
{
    if ((size/2) < numCorrelations + 1)
    {
        // Not enough to even correlate
        return false;
    }

    if (getMinimum)
    {
        qsort( values, size, sizeof(int16_t), compareAscending );
    } else {
        qsort( values, size, sizeof(int16_t), compareDescending );
    }
    
    // look at only half because if there's an inner and outer cycle, half should be from the inner one.
    for( uint8_t i = 0; i < ((size/2) - numCorrelations); i++)
    {
        // since they're sorted we can just look the # of correlations apart and make sure it's still within tolerance
        if(abs(values[i] - values[i+numCorrelations]) < tolerance )
        {
            *foundValue = values[i];
            return true;
        }
    }
    return false;
}

/** 
 * \brief Finds the extreme values
 * \param [IN] getInnerValues if true it gets the minimum of the peaks and maximum of the valleys, if false it gets the maximum of the peaks and minimum of the valleys
 * 
*/
RESET_REASON_t GetMinAndMaxConfiguration (AxisConfiguration* config, int16_t *min, int16_t *max, bool getInnerValues, uint16_t correlation_mG)
{
    if (!findValueWithCorrelations(config->peaks, config->peaksIndex, correlation_mG, NUM_CORRELATIONS, max, getInnerValues))
    {
      PRINTF("Config - Couldn't correlate peaks\n");
      return RESET_REASON_BAD_PEAK;
    }
    
    if (!findValueWithCorrelations(config->valleys, config->valleyIndex, correlation_mG, NUM_CORRELATIONS, min, !getInnerValues))
    {
      PRINTF("Config - Couldn't correlate valleys\n");
      return RESET_REASON_BAD_VALLEY;
    }
    return RESET_REASON_NORMAL;
}

bool AddConfiguration( AxisConfiguration* config, bool isMax) 
{
  if (!config->needsMoreData)
  {
    return true;
  }
  int16_t value;
  if (isMax) 
  {
    value = config->currentMax;
    // 0 is probably just a bad value
    if (value == 0) {
      return false;
    }
    if (config->peaksIndex < NUM_CONFIG_DATA_POINTS)
    {
      PRINTF("Add p a %d %d\n", config->axis, value);
      config->peaks[config->peaksIndex++] = value;
    }
  } else {
    value = config->currentMin;
    // 0 is probably just a bad value
    if (value == 0) {
      return false;
    }
    if (config->valleyIndex < NUM_CONFIG_DATA_POINTS)
    {
      PRINTF("Add v a %d %d\n", config->axis, value);
      config->valleys[config->valleyIndex++] = value;
    }
  }
  if (config->peaksIndex >= NUM_CONFIG_DATA_POINTS && config->valleyIndex >= NUM_CONFIG_DATA_POINTS)
  {    
    config->needsMoreData = false;
    return true;
  }
  return false;
}

void ResetConfiguration(AxisConfiguration* config, SENSOR_AXIS_t axis )
{
  config->axis = axis;
  config->needsMoreData = true;
  config->peaksIndex = 0;
  config->valleyIndex = 0;
  config->signalMax = INT16_MIN;
  config->signalMin = INT16_MAX;
  config->peakMin = INT16_MAX;
  config->valleyMax = INT16_MIN;
  config->maxSeen = INT16_MIN;
  config->minSeen = INT16_MAX;
  config->maxNoise = INT16_MIN;
  config->minNoise = INT16_MAX; 
  config->noiseAmplitude = 0;
  config->lastValue = 0;
  config->threshold = 0;
}

/************************ (C) COPYRIGHT NOWI Sensors *****END OF FILE****/
