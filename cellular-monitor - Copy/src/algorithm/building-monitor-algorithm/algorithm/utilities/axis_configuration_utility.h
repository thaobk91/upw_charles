/**
  ******************************************************************************
  * @file    axis_configuration_utility.h
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

#include <stdint.h>
#include "axis_configuration.h"
#include "timeServer.h"
#include "algorithm.h"

/*
  Resets a configuration back to blank
*/
void ResetConfiguration(AxisConfiguration* config, SENSOR_AXIS_t axis);

bool AddConfiguration( AxisConfiguration* config, bool isMax);

RESET_REASON_t GetMinAndMaxConfiguration (AxisConfiguration* config, int16_t *min, int16_t *max, bool getInnerValues, uint16_t correlation_mG);

int compareDescending( const void* a, const void* b);
int compareAscending( const void* a, const void* b);

/************************ (C) COPYRIGHT NOWI Sensors *****END OF FILE****/
