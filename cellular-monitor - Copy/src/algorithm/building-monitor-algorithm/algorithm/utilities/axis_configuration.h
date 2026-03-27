/**
  ******************************************************************************
  * @file    axis_configuration.h
  * @author  Charles Fayal
  * @brief   Configuration used for each axis
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 NOWI Sensors
  * All rights reserved.</center></h2>
  *
  *
  ******************************************************************************
  */
#ifndef AXIS_CONFIGURATION_H
#define AXIS_CONFIGURATION_H

#include <stdint.h>
#include <stdbool.h>
#include "magnetometer.h"

// Configuration parameters
#define NUM_CONFIG_DATA_POINTS  12 
#define MIN_NUM_CONFIG_DATA_POINTS  9 // This minimum make sure we don't lock on to an axis that just looking at outter cycles
#define NUM_CORRELATIONS  2 

typedef struct
{
  int16_t currentMin;
  int16_t currentMax;
  bool rising;
  int16_t peaks[NUM_CONFIG_DATA_POINTS];
  int16_t valleys[NUM_CONFIG_DATA_POINTS];
  int16_t signalMax;
  int16_t peakMin;
  int16_t signalMin;
  int16_t valleyMax;
  int16_t configuredLowerThreshold; // DEPRECATED
  int16_t configuredUpperThreshold; // DEPRECATED
  uint8_t peaksIndex;
  uint8_t valleyIndex;
  SENSOR_AXIS_t axis;
  int16_t lastValue;
  int16_t lastTrueValue; // Value including offset
  bool needsMoreData;
  bool useUpperThreshold; // DEPRECATED
  int16_t maxSeen;
  int16_t minSeen;
  int16_t minNoise;
  int16_t maxNoise;
  uint16_t noiseAmplitude;
  uint16_t noiseBuffer;
  uint16_t threshold;
  uint16_t bufferForPulse;
} AxisConfiguration;

#endif
/************************ (C) COPYRIGHT NOWI Sensors *****END OF FILE****/
