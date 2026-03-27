/**
  ******************************************************************************
  * @file    follower_algorithm.c
  * @author  Charles Fayal
  * @brief   Algorithm to count cycles where the offset is adjusted to follow the signal
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 NOWI Sensors
  * All rights reserved.</center></h2>
  *
  *
  ******************************************************************************
  */
#ifdef ALGORITHM_FOLLOWER
#if defined(LIS3MDL)
	LIS3MDL is not compatible with the min/max algorithm because it cannot do offsets before threshold
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <float.h>

#include "algorithm.h"

#include "timeServer.h"
#include "flash_utility.h"
#include "log.h"
#include "error_handler.h"

// Algorithm includes
#include "cycle_counter.h"
#include "flash_utility.h"
#include "follower_algorithm.h"
#include "axis_configuration_utility.h"
#include "lowest_flow_over_period_tracker.h"
#include "so_hpf.h"
#include "noise_monitor.h"
#include "communication_monitor.h"
#include "magnetometer_controller.h"
#include "signal_history.h"

// Algorithm specific configuration
#define CYCLES_PER_PULSE 0.5f
#define PULSES_PER_CYCLE 2
#define MIN_ODR  10.0f // Could decrease to increase battery life, will result in worse accuracy. Note also found in magnetometer_controller

#define FREQUENCY_MULTIPLIER  5.0f // Higher -> More accurate counting of pulses but decreased battery life
#define FREQUENCY_TAPPER_DELAY (1000.0f * CYCLES_PER_PULSE * 2 *  FREQUENCY_MULTIPLIER / MIN_ODR) // Sets it back to the MIN_ODR

#define TRIGGER_DURATION FREQUENCY_TAPPER_DELAY

#if defined(LIS2MDL) || defined(SIMULATED_MAG)
#define MAX_ODR  100.0f
#define MIN_TRIGGER_FREQUENCY  50.0f
#endif

#define MAX_DATARATE_FREQUENCY  MAX_ODR 
#define MIN_DATARATE_FREQUENCY  1.0f

// Configurations
#define ERROR_TIMEOUT 1000*60*5 // Retry timeout
#define ERROR_RETRIES 5
#define CONFIGURATION_TIMEOUT 1000*60*60 // Once an hour now
// READ_DELAY_AFTER_PULSE How long to wait after a pulse before reading and checking configs
// If we lock onto noise this can cause the node to lock onto it and be very hard to get off of it.
#define READ_DELAY_AFTER_PULSE 2000 
#define MAX_CONFIG_DIFFERENCE_mG  5000

#define MIN_NOISE_BUFFER 10 // If noise is small then the buffer will get really small when we use a multiplicative buffer

#define NUMBER_OF_FAILED_COMMS_BEFORE_RESET  5
#define NUMBER_CHANGES_WITHOUT_INCREASE  6 // Arbitrary, if it's too low it can reset too much, too high and it could be hard to hit
#define NUMBER_VALUE_REPEATS_BEFORE_RESET  3

// Add after other defines
#define CONFIGURATION_SELECTION_TIMEOUT 30000 // 30 seconds

// Algorithm Variables
bool addPulsesDuringConfiguration = false;
uint8_t halfCyclesAddedDuringConfiguration = 0;
float percentNoiseBuffer = 0.20f; // Want to make this as small as possible without noise overriding signal
uint16_t minNoise_mG = 20;
uint16_t maxNoise_mG = 600.0f;
uint16_t staticNoiseBuffer = 0;
bool debugAlerts = true;
float maxSignalFrequency = 60.0f;
uint32_t maxTimeSinceLastPulse = 1000 * 60 * 60 * 24 * 7; // irrigation doesn't always happen every day
uint8_t maxNumberOfTimestampsToAverage = 3; // Should be lower so the algorithm adapts quicker and picks up on change
uint8_t maxNumberOfTimestampsToAverageForReset = 6; // Should be higher than other max number of timestamps. 
uint16_t minNoiseChange = 100;
float percentOfNoiseDecreaseForReset = 0.7f;
uint16_t minConfigurationDifference = 50;
bool checkNoNewPulses = true;

// Increasing this improves accuracy, and noise detection but increases reads
float percentSignalForBuffer = 0.8f;

// 1 < 2* percentOfBufferForTarget + percentOfBufferForUpdate or else theres's no ovelap of buffer
// overlap = (2* percentOfBufferForTarget + percentOfBufferForUpdate) - 1
float percentOfBufferForTarget = 0.56f;
// Increasing this improves accuracy, and noise detection but increases reads
float percentOfBufferForUpdate = 0.2f;

extern bool readToCheck;

typedef enum {
  Algorithm_State_Start = 0,
  Algorithm_State_BaselineNoise,
  Algorithm_State_Configuring,
  Algorithm_State_ValidateNoise,
  Algorithm_State_Running,
  Algorithm_State_Error,
  Algorithm_State_Disabled,
} Algorithm_State_t;

typedef enum {
  CREATE_THRESHOLDS_UNKNOWN = 0,
  CREATE_THRESHOLDS_SUCCESS = 1,
  CREATE_THRESHOLDS_OUTER_MINMAX_FAILED = 2,
  CREATE_THRESHOLDS_PEAK_BELOW_VALLEY = 3,
  CREATE_THRESHOLDS_INNER_MINMAX_FAILED = 4,
  CREATE_THRESHOLDS_INNER_PEAK_BELOW_VALLEY = 5,
  CREATE_THRESHOLDS_DIFF_TOO_SMALL = 6,
  CREATE_THRESHOLDS_DIFF_TOO_LARGE = 7,
  CREATE_THRESHOLDS_NOISE_TOO_HIGH = 8
} CreateThresholdsResult_t;

static Algorithm_State_t CurrentState = Algorithm_State_Start;
// Timers
static TimerEvent_t MagnetometerDataTimer;
static TimerEvent_t ConfigurationTimer;
static TimerEvent_t ErrorTimeoutTimer;
static TimerEvent_t DelayedSignalTimer;

static void OnConfigurationTimerEvent( void );
static void OnMagnetometerDataTimerEvent( void );
static void ResetMagnetometerTimer( void );
static void HandleInterruptEvent( void );
static void HandleDelayedCheck( void );
static void OnErrorTimeoutEvent( void );

// Private Functions
static void TransitionTo(Algorithm_State_t nextState);
static bool HandleInterrupt( void );
static void HandleReadConfiguring( void );
static void HandleReadRunning( int16_t magValue );
static bool CalculateFrequency(bool preemptiveTrigger);
static void ResetConfiguring(RESET_REASON_t reason, CreateThresholdsResult_t resultX, CreateThresholdsResult_t resultY, CreateThresholdsResult_t resultZ);
static void ResetAlgorithm(RESET_REASON_t reason, int16_t value);
static void ResetAlgorithmWithData(int16_t *errorData, uint8_t size, uint8_t version);
static CreateThresholdsResult_t CreateThresholds( AxisConfiguration *config, float *snr );
static void UpdateOffset(void);
static void HandleCommunicationError(void);
static void HandleNoiseMonitorError( uint16_t resets );
static void HandleCriticalError(ALGORITHM_ERROR_t error);
static void FinishedMeasuringNoise(void);
static void FinishedValidatingNoise(void);

// Private variables
static AxisConfiguration *chosenConfig; // Configuration we're using

static AxisConfiguration xConfig; // Configuration for X axis
static AxisConfiguration yConfig; // Configuration for Y axis
static AxisConfiguration zConfig; // Configuration for Z axis

static bool volatile InterruptFired = false;
static bool volatile ReadDataFlag = false;
static bool volatile ConfigurationTimeoutFlag = false;
static bool volatile ErrorTimeoutFlag = false;

static int16_t magValue = 0; // Last read mag value
static TimerTime_t lastPreemptiveTriggerTimestamp = 0;

static float samplingFrequency = MAX_DATARATE_FREQUENCY;
static float signalFrequency = 0;
static float desiredDataRateFrequency = MIN_DATARATE_FREQUENCY;

static TimerTime_t lastHalfCycleTime = 0;

ALGORITHM_t Algorithm = {
  Follower_Init,
  Follower_ReadData,
  Follower_Reset,
  Follower_Log_Config,
  Follower_Get_IsConfigured,
  Follower_Update_Variable,
  Follower_Enable,
  Follower_Disable,
  Follower_Set_Variables,
  Follower_Get_Variables
};

// Public Functions
void Follower_Init( void )
{
  PRINTF_TS("Follower_Init\n");
  cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);
  TimerInit( &MagnetometerDataTimer, OnMagnetometerDataTimerEvent );
  TimerInit( &ConfigurationTimer, OnConfigurationTimerEvent );
  TimerInit( &ErrorTimeoutTimer, OnErrorTimeoutEvent );
  TimerInit( &DelayedSignalTimer, HandleDelayedCheck );

  TimerSetValue(&ConfigurationTimer, CONFIGURATION_TIMEOUT);
  TimerSetValue(&ErrorTimeoutTimer, ERROR_TIMEOUT);
  
  FlashUtility_LoadConfiguration();
  chosenConfig = NULL;
  Noise_Monitor_Init(HandleNoiseMonitorError);

  signal_history_reset();

  PRINTF_TS("Follower_Init finished\n");
  magnetometer_controller_init(HandleCommunicationError);
}

void Follower_ReadData( void )
{
  switch(CurrentState)
  {
    case Algorithm_State_Start:
      if(!magnetometer_controller_reset( ))
      {
        PRINTF_TS("ERROR Enable Mag Failed\n\r");
        HandleCriticalError(ALGORITHM_FAILED_TO_INIT);
      } else {
        TransitionTo(Algorithm_State_BaselineNoise);
      }
      break;
    case Algorithm_State_BaselineNoise:{
      if(Noise_Monitor_CheckIn()) {
          FinishedMeasuringNoise();
        }
      }
      break;
    case Algorithm_State_ValidateNoise: {
        if(Noise_Monitor_CheckIn()) {
          FinishedValidatingNoise();
        }
      }
      break;
    case Algorithm_State_Running:
      if (InterruptFired)
      {
        InterruptFired = false;
        if(HandleInterrupt())
        {
          // Error
          return;
        }
      }
      
      if (ReadDataFlag)
      {
        ReadDataFlag = false;
        if (!magnetometer_controller_get_axis(chosenConfig->axis, &chosenConfig->lastValue))
        {
          PRINTF_TS("FAILED Get data\n\r");
          return;
        }

        HandleReadRunning(chosenConfig->lastValue);
        if (checkNoNewPulses)
        {
          uint32_t timeDiff = TimerGetElapsedTime(lastHalfCycleTime);
          if (timeDiff > maxTimeSinceLastPulse)
          {
            PRINTF_TS("No pulses duration exceeded %u max %u\n", timeDiff, maxTimeSinceLastPulse);
            int16_t data[4] = { RESET_REASON_NO_NEW_PULSES, chosenConfig->minSeen, chosenConfig->maxSeen, timeDiff/1000 };
            ResetAlgorithmWithData(data, 4, 0);
            return;
          }
        }
      } 
      break;
    case Algorithm_State_Configuring:
      if (ReadDataFlag)
      {
        ReadDataFlag = false;
        HandleReadConfiguring();
      }

      if (ConfigurationTimeoutFlag)
      {
        TimerReset(&ConfigurationTimer);
        int16_t errorData[4] = { RESET_REASON_TIMEOUT, xConfig.maxSeen - xConfig.minSeen, yConfig.maxSeen - yConfig.minSeen, zConfig.maxSeen - zConfig.minSeen };
        ResetAlgorithmWithData(errorData, 4, 1);
        return;
      }
      break;
    case Algorithm_State_Error:
      if (ErrorTimeoutFlag)
      {
        ErrorTimeoutFlag = false;
        static uint8_t errorTries = 0;
        errorTries++;
        PRINTF_TS("Error retry %d\n", errorTries);
        if (magnetometer_controller_reset())
        {
          errorTries = 0;
          TransitionTo(Algorithm_State_Start);
        } else {
          if (errorTries >= ERROR_RETRIES)
          {
            // Give up reset device
            errorTries = 0;
            Error_Handler(ERROR_CODE_ALGORITHM_TIMEOUT, false);
          }

          TimerReset(&ErrorTimeoutTimer);
        }
      }
    case Algorithm_State_Disabled:
      // Do nothing
      break;
    default:
      break;
  }
  
  magnetometer_controller_check_in();
}


/**
 * @brief Gets or sets the variable based on the type
 * 
 * @param type what variable
 * @param value it's value or what should be set
 * @param get if true then return the value, if false then set the value
 * @return true Continue adding variables
 * @return false Stop adding variables
 */
static bool Get_Or_Set_Variable(uint8_t type, uint32_t *value, bool get)
{
  switch(type)
  {
    case 0:
      if (get) {
        *value = FloatToUInt32(percentSignalForBuffer);
      } else {
        percentSignalForBuffer = Uint32ToFloat(*value);
        PRINTF_BASE_TS("percentSignalForBuffer to %.3f \n", (double)percentSignalForBuffer);
      }
      break;
    case 1:
      // Used to be percentNoiseThreshold
      break;
    case 2:
      if (get) {
        *value = FloatToUInt32(percentNoiseBuffer);
      } else {
        percentNoiseBuffer = Uint32ToFloat(*value);
        PRINTF_BASE_TS("percentNoiseBuffer to %.3f \n", (double)percentNoiseBuffer);
      }
      break;
    case 3:
      if (get) {
        *value = maxNoise_mG;
      } else {
        maxNoise_mG = *value & 0xFFFF;
        PRINTF_BASE_TS("maxNoise_mG to %d\n",maxNoise_mG);
      }
      break;     
    case 4:
      if (get) {
        *value = minNoise_mG;
      } else {
        minNoise_mG = *value & 0xFFFF;
        PRINTF_BASE_TS("minNoise_mG to %d\n", minNoise_mG);
      }
      break;
    case 5:
      // used to be monitorMissedCycles
      break;
    case 6:
      if (get) {
        *value = staticNoiseBuffer;
      } else {
        staticNoiseBuffer = *value & 0xFFFF;
        PRINTF_BASE_TS("staticNoiseBuffer to %d\n", staticNoiseBuffer);
      }
      break;
    case 7:
      if (get) {
        *value = minNoiseChange;
      } else {
        minNoiseChange = *value & 0xFFFF;
        PRINTF_BASE_TS("minNoiseChange to %d\n",minNoiseChange);
      }
      break;
    case 8:
      if (get) {
        *value = FloatToUInt32(percentOfNoiseDecreaseForReset);
      } else {
        percentOfNoiseDecreaseForReset = Uint32ToFloat(*value);
        PRINTF_BASE_TS("percentOfNoiseDecreaseForReset to %.3f\n", (double)percentOfNoiseDecreaseForReset);
      }
      break;
    case 9:
      if (get) {
        *value = maxNumberOfTimestampsToAverage;
      } else {
        maxNumberOfTimestampsToAverage = *value & 0xFF;
        PRINTF_BASE_TS("maxNumberOfTimestampsToAverage to %d\n", maxNumberOfTimestampsToAverage);
      }
      break;
    case 10:
      if (get) {
        *value = maxTimeSinceLastPulse / 1000 / 60;
      } else {
        maxTimeSinceLastPulse = (*value & 0xFFFFFFFF) * 1000 * 60; // in minutes
        PRINTF_BASE_TS("maxTimeSinceLastPulse to %d\n",maxTimeSinceLastPulse);
      }
      break;
    case 11:
      if (get) {
        *value = FloatToUInt32(maxSignalFrequency);
      } else {
        maxSignalFrequency = Uint32ToFloat(*value);
        PRINTF_BASE_TS("maxSignalFrequency to %.3f input %d\n", (double)maxSignalFrequency, *value);
      }
      break;
    case 12:
      if (get) {
        *value = debugAlerts;
      } else {
        debugAlerts = *value & 0x01;
        PRINTF_BASE_TS("debugAlerts to %d\n",debugAlerts);
      }
      break;
    case 13:
      if (get) {
        *value = readToCheck;
      } else {
        readToCheck = *value & 0x01;
        PRINTF_BASE_TS("readToCheck to %d\n",readToCheck);
      }
      break;
    case 14:
      // get algo information
      break;
    case 15:
      if (get) {
        *value = minConfigurationDifference;
      } else {
        minConfigurationDifference = *value & 0xFFFF;
        PRINTF_BASE_TS("minConfigurationDifference to %d\n", minConfigurationDifference);
      }
      break;
    case 16:
      // used to be check config
      break;
    case 17:
      if (get) {
        *value = FloatToUInt32(percentOfBufferForTarget);
      } else {
        percentOfBufferForTarget = Uint32ToFloat(*value);
        PRINTF_BASE_TS("percentOfBufferForTarget to %.3f\n", (double)percentOfBufferForTarget);
      }
      break;
    case 18:
      if (get) {
        *value = FloatToUInt32(percentOfBufferForUpdate);
      } else {
        percentOfBufferForUpdate = Uint32ToFloat(*value);
        PRINTF_BASE_TS("percentOfBufferForUpdate to %.3f\n", (double)percentOfBufferForUpdate);
      }
      break;
    case 19:
      if (get) {
        *value = maxNumberOfTimestampsToAverageForReset;
      } else {
        maxNumberOfTimestampsToAverageForReset = *value & 0xFF;
        PRINTF_BASE_TS("maxNumberOfTimestampsToAverageForReset to %d\n", maxNumberOfTimestampsToAverageForReset);
      }
      break;
    case 20:
      if (get) {
        *value = checkNoNewPulses;
      } else {
        checkNoNewPulses = *value & 0x01;
        PRINTF_BASE_TS("checkNoNewPulses to %d\n", checkNoNewPulses);
      }
    case 21:
      // This stops it from looking at more variables
      return false;
    default:
      PRINTF_BASE_TS("Unhandled type %d\n", type);
      break;
  }
  return true;
}

void Follower_Update_Variable(uint8_t variable, uint32_t value)
{
  // Special cases for getting data
  switch (value)
  {
    case 14:
     {
      PRINTF_BASE_TS("getting data\n");
      uint8_t size = 1;
      int16_t data[4];
      data[0] = CurrentState;
      switch (CurrentState)
      {
        case Algorithm_State_Configuring:
          size = 4;
          switch(value)
          {
            case 0:
              data[1] = xConfig.minSeen;
              data[2] = xConfig.maxSeen;
              data[3] = xConfig.noiseAmplitude;
              break;
            case 1:
              data[1] = yConfig.minSeen;
              data[2] = yConfig.maxSeen;
              data[3] = yConfig.noiseAmplitude;
              break;
            case 2:
              data[1] = zConfig.minSeen;
              data[2] = zConfig.maxSeen;
              data[3] = zConfig.noiseAmplitude;
              break;
            default:
            size = 1;
              break;
          }
          break;
        case Algorithm_State_Running:
          switch (value)
          {
            default:
            case 0:
            size = 4;
            data[1] = chosenConfig->valleyMax;
            data[2] = chosenConfig->peakMin;
            data[3] = chosenConfig->noiseAmplitude;
            break;
            case 1:
            size = 4;
            data[1] = chosenConfig->signalMin;
            data[2] = chosenConfig->signalMax;
            data[3] = magnetometer_controller_get_lpf();
            break;
            case 2:
            size = 3;
            data[1] = chosenConfig->minSeen;
            data[2] = chosenConfig->maxSeen;
            break;
          }
          break;
        default:
          break;
      }
      Alert_Data(ERROR_CODE_NO_ERROR_DATA, 0, data, size);
      return; // Dont' get or set variable after
    }
    default:
      break;
  }

  // Normal case
  PRINTF_BASE_TS("Update Variable %d %d\n", variable, value);
  Get_Or_Set_Variable(variable, &value, false);
  FlashUtility_SaveConfiguration();
}

void Follower_Get_Variables(uint32_t *variables, uint8_t *length)
{
  uint8_t variables_count = 0;
  uint32_t value = 0;
  while(Get_Or_Set_Variable(variables_count, &value, true))
  {
    variables[variables_count++] = value;
    value = 0;
  }
 
  *length = variables_count;
}

void Follower_Set_Variables(uint32_t *variables, uint8_t length)
{
  for (int i = 0; i < length; i++)
  {
    Get_Or_Set_Variable(i, &variables[i], false);
  }
}

void Follower_Log_Config( void )
{
  PRINTF_BASE_TS("%d,%d,%.1f,%.1f\n", magValue, cycle_counter_get_half_cycles(), (double)samplingFrequency, (double)signalFrequency);
  if (chosenConfig != NULL){
    PRINTF_BASE("CONFIG axis %d valleyMin %d valleyMax %d peakMin %d peakMax %d innerDiff %d outerDiff %d n %d lpf %d\n",chosenConfig->axis, chosenConfig->signalMin, chosenConfig->valleyMax, chosenConfig->peakMin, chosenConfig->signalMax, chosenConfig->peakMin - chosenConfig->valleyMax, chosenConfig->signalMax - chosenConfig->signalMin, chosenConfig->noiseAmplitude, magnetometer_controller_get_lpf());   
  } else {
    PRINTF_BASE("CONFIG NULL state:%d\n", CurrentState);
  }
  PRINTF_BASE("Variables");
  PRINTF_BASE("Ps %.3f ", (double)percentSignalForBuffer);
  PRINTF_BASE("Pb %.3f ", (double)percentNoiseBuffer);
  PRINTF_BASE("minNoise %d maxNoise %d ", minNoise_mG, maxNoise_mG);
  PRINTF_BASE("static:%d\n", staticNoiseBuffer);
  PRINTF_BASE("maxTime:%d maxFreq:%.1f\n", maxTimeSinceLastPulse, (double)maxSignalFrequency);
  PRINTF_BASE("percent signal %.3f target %.3f update %.3f\n", (double)percentSignalForBuffer, (double)percentOfBufferForTarget, (double)percentOfBufferForUpdate);
}

void Follower_Reset( void )
{
  PRINTF_TS("Follower_Reset\n");
  magnetometer_controller_set_to_default(true);
  magnetometer_controller_reset();
	ResetAlgorithm(RESET_REASON_NORMAL, 0);
}

bool Follower_Get_IsConfigured( void )
{
  return CurrentState == Algorithm_State_Running;
}

void Follower_Enable( void )
{
  PRINTF_TS("Follower_Enable\n");
  TransitionTo(Algorithm_State_Start);
}

void Follower_Disable( void )
{
  PRINTF_TS("Follower_Disable\n");
  if (CurrentState != Algorithm_State_Disabled) {
    TransitionTo(Algorithm_State_Disabled);
  }
}

// Private Functions

static void TransitionTo(Algorithm_State_t nextState){
    
  PRINTF_TS("TransitionTo %d\n",nextState);
  Algorithm_State_t lastState = CurrentState;
  CurrentState = nextState;
  
  if (CurrentState == lastState)
  {
    PRINTF_TS("TransitionTo already in %d\n", nextState);
  } else {
    // no point in deinit if about to transition into that state
    switch (lastState)
    {
      case Algorithm_State_BaselineNoise:
      case Algorithm_State_ValidateNoise:
        Noise_Monitor_Stop();
        break;
      case Algorithm_State_Configuring:
        TimerStop(&ConfigurationTimer);
        break;
      case Algorithm_State_Error:
        TimerStop(&ErrorTimeoutTimer);
        break;
      case Algorithm_State_Running:
        magnetometer_controller_stop_interrupt();
        magnetometer_controller_set_lpf(false);
        break;
      default:
        break;
    }
  }
  
  switch(nextState)
  {
    case Algorithm_State_Start:
      // Go back to clean state
      samplingFrequency = 1; // To check in
      magnetometer_controller_set_to_default(false);
      ResetMagnetometerTimer();
      break;
    case Algorithm_State_BaselineNoise:
      // Reset configuration when back at baseline.
      ResetConfiguration(&xConfig, SENSOR_AXIS_X);
      ResetConfiguration(&yConfig, SENSOR_AXIS_Y);
      ResetConfiguration(&zConfig, SENSOR_AXIS_Z);
      chosenConfig = NULL;
    case Algorithm_State_ValidateNoise:
      Noise_Monitor_Start(magnetometer_controller_get_lpf());
      samplingFrequency = 0;
      // Trigger read data automatically because there's nothing in noise monitor to trigger it
      Follower_ReadData();
      break;
    case Algorithm_State_Configuring:
      ReadDataFlag = false; // Don't want to read right away or may get bad value
      ConfigurationTimeoutFlag = false;
      halfCyclesAddedDuringConfiguration = 0;
      magnetometer_controller_stop_interrupt(); // Stop to reduce power consumption
      magnetometer_controller_set_operating_mode(MAG_OPERATING_MODE_CONTINUOUS);
      TimerStart(&ConfigurationTimer);
      CalculateFrequency(true);
      samplingFrequency = desiredDataRateFrequency;
      break;
    case Algorithm_State_Running: {
        if (chosenConfig == NULL) {
          PRINTF_TS("ERROR CONFIG NULL\n");
          ResetAlgorithm(RESET_REASON_CONFIG_NULL, 0);
          return;
        }
        lastHalfCycleTime = TimerGetCurrentTime(); // Reset
        magnetometer_controller_set_operating_mode(MAG_OPERATING_MODE_CONTINUOUS);
        magnetometer_controller_set_interrupt_threshold(chosenConfig->threshold / 2);
        if (debugAlerts)
        {
          const uint16_t version = 3;
          int16_t configData[4] = { ( magnetometer_controller_get_lpf() << 8 | chosenConfig->axis) , chosenConfig->peakMin - chosenConfig->valleyMax, chosenConfig->signalMax - chosenConfig->signalMin, chosenConfig->noiseAmplitude };
          Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_SET, version, configData, 4);
        }
        samplingFrequency = MIN_DATARATE_FREQUENCY;
        
        UpdateOffset(); // Triggers update
        // Start off with trigger
        CalculateFrequency(true);
        LowestFlowOverPeriodTracker_Reset();
        if (!magnetometer_controller_start_interrupt_threshold(HandleInterruptEvent, chosenConfig->axis))
        {
          PRINTF_TS("Failed to start interrupt\n");
          HandleCriticalError(ALGORITHM_FAILED_TO_START_INTERRUPT);
          return;
        }
      }
      break;
    case Algorithm_State_Error: {
        if (lastState != Algorithm_State_Error)
        {
          magnetometer_controller_set_operating_mode(MAG_OPERATING_MODE_POWER_DOWN);
          magnetometer_controller_set_lpf(false);
          // Set values for low power consumption
          samplingFrequency = 0;
          TimerStart(&ErrorTimeoutTimer);
        }
      }
      break;
    case Algorithm_State_Disabled:
      samplingFrequency = 0;
      magnetometer_controller_stop_interrupt();
      magnetometer_controller_set_to_default(true);
      magnetometer_controller_reset();
      magnetometer_int_pin_pull_down(); // Don't want to cause an issue with the magnetometer pin in high z
      break;
    default:
      PRINTF_TS("Unhandled state\n");
      break;
  }
  
  // Update to new sampling frequency;
  ResetMagnetometerTimer();
}

static bool DidNoiseChange(AxisConfiguration *configToCheck, uint16_t newNoiseAmplitude)
{
  uint16_t originalNoise = configToCheck->noiseAmplitude;
   // Only using percent noise buffer here because at low noise it can cause false positives
  int16_t noiseChange = originalNoise - newNoiseAmplitude;
  uint16_t noiseChangeAbs = abs(noiseChange);
  if (noiseChangeAbs > minNoiseChange && noiseChangeAbs > originalNoise * percentNoiseBuffer)
  {
    PRINTF_TS("Noise change, old noise %d new %d hc added %d\n", originalNoise, newNoiseAmplitude, halfCyclesAddedDuringConfiguration);
    return true;
  }
  return false;
}

static void FinishedValidatingNoise(void)
{
  uint16_t xNoise, yNoise, zNoise;
  uint16_t noiseOfChosen = 0;
  Noise_Monitor_GetNoise(&xNoise, &yNoise, &zNoise);
  switch ( chosenConfig->axis)
  {
    case SENSOR_AXIS_X:
      noiseOfChosen = xNoise;
      break;
    case SENSOR_AXIS_Y:
      noiseOfChosen = yNoise;
      break;
    case SENSOR_AXIS_Z:
      noiseOfChosen = zNoise;
      break;
  }

    // Look at noise after because we don't want a low SNR axis to cause a reset
  if (DidNoiseChange(chosenConfig, noiseOfChosen))
  {
    if (addPulsesDuringConfiguration)
    {
      cycle_counter_decrement_half_cycles(halfCyclesAddedDuringConfiguration);
    }
    int16_t data[3] = { RESET_REASON_NOISE_INCREASE, chosenConfig->axis, chosenConfig->noiseAmplitude };
    ResetAlgorithmWithData(data, 3, 1);
    return;
  }

  PRINTF_TS("CONFIG Chose axis %d with noise %d thresh %d bufferForPulse %d\n", chosenConfig->axis, noiseOfChosen, chosenConfig->threshold, chosenConfig->bufferForPulse);
  TransitionTo(Algorithm_State_Running);
}

/**
 * @brief 
 * @param config 
 * @param noiseAmplitude 
 * @return Returns false if there was an issue with the thresholds
 */
static bool CheckNoise(AxisConfiguration *configToCheck, uint16_t noiseAmplitude)
{
  configToCheck->noiseAmplitude = noiseAmplitude;
  if (configToCheck->noiseAmplitude < minNoise_mG)
  {
    PRINTF_TS("Noise %d, using min\n", configToCheck->noiseAmplitude);
    configToCheck->noiseAmplitude = minNoise_mG;
  }
   
  configToCheck->noiseBuffer = (configToCheck->noiseAmplitude * percentNoiseBuffer) + staticNoiseBuffer;
  if (configToCheck->noiseBuffer < MIN_NOISE_BUFFER)
  {
    PRINTF_TS("Noise buffer %d min buffer %d\n", configToCheck->noiseBuffer, MIN_NOISE_BUFFER);
    configToCheck->noiseBuffer = MIN_NOISE_BUFFER;
  }

  if (configToCheck->noiseAmplitude > maxNoise_mG && !magnetometer_controller_get_lpf())
  {
    PRINTF_TS("Noise above max noise %d, enabling LPF\n", configToCheck->noiseAmplitude);
    return false;
  }
  return true;
}

static void FinishedMeasuringNoise(void)
{
  uint16_t xNoise, yNoise, zNoise;
  Noise_Monitor_GetNoise(&xNoise, &yNoise, &zNoise);
  if (CheckNoise(&xConfig, xNoise) && CheckNoise(&yConfig, yNoise) && CheckNoise(&zConfig, zNoise))
  {
    TransitionTo(Algorithm_State_Configuring);
  } else {
    // Noise too high, enable LPF
    magnetometer_controller_set_lpf(true);
    TransitionTo(Algorithm_State_BaselineNoise);
  }
}

static bool CheckAxis(AxisConfiguration* configToCheck, int16_t val)
{
  // No longer valid without large default offset
  // if (val == 0)
  // {
  //   PRINTF_TS("CheckAxis 0 value\n");
  //   return false;
  // }

  if(val < configToCheck->minSeen)
  {
    configToCheck->minSeen = val;
  } else if (val > configToCheck->maxSeen)
  {
    configToCheck->maxSeen = val;
  }
 
  bool changedDirection = false;
  
  // Important Calculation: determines how much the signal can change before it is considered a new cycle or a new peak/valley of configuration
  // We laer subtract noise so we have to add it for min buffer
  uint16_t buffer = minConfigurationDifference + configToCheck->noiseAmplitude; 

  uint16_t noiseAndBuffer = configToCheck->noiseBuffer + configToCheck->noiseAmplitude;
  if (noiseAndBuffer > buffer)
  { 
    // Choose which ever buffer is larger
     buffer = noiseAndBuffer;
  }
  
  if (configToCheck->rising) {
    // check if still rising
    if (val < configToCheck->currentMax - buffer)
    {
      LogMag("Max: %d ",configToCheck->currentMax);
      changedDirection = true;
      AddConfiguration(configToCheck, true);
      configToCheck->currentMax = INT16_MIN;
      configToCheck->currentMin = val;
    } else if (val > configToCheck->currentMax)
    {
      configToCheck->currentMax = val;
      LogMag("New Max: %d\n",configToCheck->currentMax);
    }
  } else {
    // check if still falling
    if (val > configToCheck->currentMin + buffer)
    {
      LogMag("Min: %d ",configToCheck->currentMin);
      changedDirection = true;
      AddConfiguration(configToCheck, false);
      configToCheck->currentMin = INT16_MAX;
      configToCheck->currentMax = val;
    } else if (val < configToCheck->currentMin) 
    {
      configToCheck->currentMin = val;
      LogMag("New Min: %d\n",configToCheck->currentMin);
    }
  }

  bool newHalfCycle = false;

  if (changedDirection) {
    configToCheck->rising = !configToCheck->rising;
    newHalfCycle = true;
  } 

  configToCheck->lastValue = val;
  return newHalfCycle;
}

static float bestSNR = FLT_MIN, currentSNR = 0;
static bool xDone = false, yDone = false, zDone = false;
static CreateThresholdsResult_t xResult = CREATE_THRESHOLDS_UNKNOWN, yResult = CREATE_THRESHOLDS_UNKNOWN, zResult = CREATE_THRESHOLDS_UNKNOWN;

static void resetVariables (void) {
  bestSNR = FLT_MIN;
  currentSNR = FLT_MIN;
  xDone = false;
  yDone = false;
  zDone = false;
  xResult = CREATE_THRESHOLDS_UNKNOWN;
  yResult = CREATE_THRESHOLDS_UNKNOWN;
  zResult = CREATE_THRESHOLDS_UNKNOWN;
}

static void HandleReadConfiguring( void )
{
  SensorAxes_t newValue;
  if (!magnetometer_controller_get_axes(&newValue))
  {
      PRINTF_TS("FAILED Get data\n\r");
      return;
  }
  
  bool xNewHalfCycle = CheckAxis(&xConfig, newValue.AXIS_X);
  bool yNewHalfCycle = CheckAxis(&yConfig, newValue.AXIS_Y);
  bool zNewHalfCycle = CheckAxis(&zConfig, newValue.AXIS_Z);
  // Using z axis to be consistent with previous versions, usually Z is the best axis
  if (zNewHalfCycle) //xNewHalfCycle || yNewHalfCycle || zNewHalfCycle)
  {
      halfCyclesAddedDuringConfiguration++;
      cycle_counter_add_half_cycle(addPulsesDuringConfiguration);
  }


  // Check if any axis is ready for threshold creation (normal flow)
  if (!xConfig.needsMoreData && !xDone)
  {
    xResult = CreateThresholds(&xConfig, &currentSNR);
    if (xResult == CREATE_THRESHOLDS_SUCCESS)
    {
      xDone = true;
      if (currentSNR > bestSNR)
      {
        bestSNR = currentSNR;
        chosenConfig = &xConfig;
      }
    } else {
      // Reset config
      int16_t configData[4] = { xConfig.axis, xResult, xConfig.peakMin - xConfig.valleyMax, xConfig.noiseAmplitude };
      Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_AXIS_RESET, 0, configData, 4);
      PRINTF_TS("Reset xConfig, failed with error %d\n", xResult);
      uint16_t noise = xConfig.noiseAmplitude; // Remember noise
      ResetConfiguration(&xConfig, xConfig.axis);
      xConfig.noiseAmplitude = noise;
      xDone = false;
    }
  }
  if (!yConfig.needsMoreData && !yDone)
  {
    yResult = CreateThresholds(&yConfig, &currentSNR);
    if (yResult == CREATE_THRESHOLDS_SUCCESS)
    {
      yDone = true;
      if (currentSNR > bestSNR)
      {
        bestSNR = currentSNR;
        chosenConfig = &yConfig;
      }
    } else {
      // Reset config
      int16_t configData[4] = { yConfig.axis, yResult, yConfig.peakMin - yConfig.valleyMax, yConfig.noiseAmplitude };
      Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_AXIS_RESET, 0, configData, 4);
      PRINTF_TS("Reset yConfig, failed with error %d\n", yResult);
      uint16_t noise = yConfig.noiseAmplitude; // Remember noise
      ResetConfiguration(&yConfig, yConfig.axis);
      yConfig.noiseAmplitude = noise;
      yDone = false;
    }
  }
  if (!zConfig.needsMoreData && !zDone)
  {
    zResult = CreateThresholds(&zConfig, &currentSNR);
    if (zResult == CREATE_THRESHOLDS_SUCCESS)
    {
      zDone = true;
      if (currentSNR > bestSNR)
      {
        bestSNR = currentSNR;
        chosenConfig = &zConfig;
      }
    } else {
      // Reset config
      PRINTF_TS("Reset zConfig, failed with error %d\n", zResult);
      int16_t configData[4] = { zConfig.axis, zResult, zConfig.peakMin - zConfig.valleyMax, zConfig.noiseAmplitude };
      Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_AXIS_RESET, 0, configData, 4);
      uint16_t noise = zConfig.noiseAmplitude; // Remember noise
      ResetConfiguration(&zConfig, zConfig.axis);
      zConfig.noiseAmplitude = noise;
      zDone = false;

      // Only for Z axis since that's what we're using to add pulses.
      if (addPulsesDuringConfiguration)
      {
        cycle_counter_decrement_half_cycles(halfCyclesAddedDuringConfiguration);
        halfCyclesAddedDuringConfiguration = 0;
      }
    }
  }

  // Check if all axes have failed at some point and check noise if so
  if (xResult > CREATE_THRESHOLDS_SUCCESS && yResult > CREATE_THRESHOLDS_SUCCESS && zResult > CREATE_THRESHOLDS_SUCCESS)
  {
    uint16_t xNoise, yNoise, zNoise;
    Noise_Monitor_GetNoise(&xNoise, &yNoise, &zNoise);
    uint16_t maxNoise = xNoise;
    if (yNoise > maxNoise)
    {
      maxNoise = yNoise;
    }
    if (zNoise > maxNoise)
    {
      maxNoise = zNoise;
    }

    // Only for Z axis since that's what we're using to add pulses.
    if (addPulsesDuringConfiguration)
    {
      cycle_counter_decrement_half_cycles(halfCyclesAddedDuringConfiguration);
      halfCyclesAddedDuringConfiguration = 0;
    }

    resetVariables();

    if (!magnetometer_controller_get_lpf() && maxNoise > minNoise_mG) {
      PRINTF_TS("CONFIG none found enabling LPF\n");
      magnetometer_controller_set_lpf(true);
      ResetConfiguring(RESET_REASON_BAD_CONFIGURATION, xResult, yResult, zResult);
    } else {
      PRINTF_TS("CONFIG none found reset config\n\r");
      magnetometer_controller_set_lpf(false); // reset lpf
      ResetConfiguring(RESET_REASON_BAD_CONFIGURATION_WITH_LPF, xResult, yResult, zResult);
    }
    return;
  }

  // Check if any of the axes have completed successfully
  // We used to let this run longer than had a timeout but that resulted in an axis that had a small inner cycle having time to complete by just using the outter cycle.
  if (xDone || yDone || zDone)
  {
    // Try to create thresholds for X axis if not already done regardless of data amount

    if (!xDone && xConfig.peaksIndex > MIN_NUM_CONFIG_DATA_POINTS && xConfig.valleyIndex > MIN_NUM_CONFIG_DATA_POINTS)
    {
      xResult = CreateThresholds(&xConfig, &currentSNR);
      if (xResult == CREATE_THRESHOLDS_SUCCESS)
      {
        xDone = true;
        if (currentSNR > bestSNR)
        {
          bestSNR = currentSNR;
          chosenConfig = &xConfig;
        }
        PRINTF_TS("CONFIG Timeout: X axis succeeded with SNR %.2f (peaks: %d, valleys: %d)\n", currentSNR, xConfig.peaksIndex, xConfig.valleyIndex);
      } else {
        PRINTF_TS("CONFIG Timeout: X axis failed with error %d (peaks: %d, valleys: %d)\n", xResult, xConfig.peaksIndex, xConfig.valleyIndex);
      }
    }
    
    // Try to create thresholds for Y axis if not already done
    if (!yDone && yConfig.peaksIndex > MIN_NUM_CONFIG_DATA_POINTS && yConfig.valleyIndex > MIN_NUM_CONFIG_DATA_POINTS)
    {
      yResult = CreateThresholds(&yConfig, &currentSNR);
      if (yResult == CREATE_THRESHOLDS_SUCCESS)
      {
        yDone = true;
        if (currentSNR > bestSNR)
        {
          bestSNR = currentSNR;
          chosenConfig = &yConfig;
        }
        PRINTF_TS("CONFIG Timeout: Y axis succeeded with SNR %.2f (peaks: %d, valleys: %d)\n", currentSNR, yConfig.peaksIndex, yConfig.valleyIndex);
      } else {
        PRINTF_TS("CONFIG Timeout: Y axis failed with error %d (peaks: %d, valleys: %d)\n", yResult, yConfig.peaksIndex, yConfig.valleyIndex);
      }
    }
    
    // Try to create thresholds for Z axis if not already done
    if (!zDone && zConfig.peaksIndex > MIN_NUM_CONFIG_DATA_POINTS && zConfig.valleyIndex > MIN_NUM_CONFIG_DATA_POINTS)
    {
      zResult = CreateThresholds(&zConfig, &currentSNR);
      if (zResult == CREATE_THRESHOLDS_SUCCESS)
      {
        zDone = true;
        if (currentSNR > bestSNR)
        {
          bestSNR = currentSNR;
          chosenConfig = &zConfig;
        }
        PRINTF_TS("CONFIG Timeout: Z axis succeeded with SNR %.2f (peaks: %d, valleys: %d)\n", currentSNR, zConfig.peaksIndex, zConfig.valleyIndex);
      } else {
        PRINTF_TS("CONFIG Timeout: Z axis failed with error %d (peaks: %d, valleys: %d)\n", zResult, zConfig.peaksIndex, zConfig.valleyIndex);
      }
    }
 
    
    if (xDone) {
      int16_t configData[4] = { xConfig.axis , xConfig.peakMin - xConfig.valleyMax, xConfig.signalMax - xConfig.signalMin, xConfig.noiseAmplitude };
      Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_AXIS_DONE_INFO, 0, configData, 4);
    } else {
      int16_t configData[4] = { xConfig.axis , xConfig.valleyIndex, xConfig.peaksIndex, xConfig.noiseAmplitude };
      Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_AXIS_NOT_DONE_INFO, 0, configData, 4);
    }
    
    if (yDone) {
      int16_t configData[4] = { yConfig.axis , yConfig.peakMin - yConfig.valleyMax, yConfig.signalMax - yConfig.signalMin, yConfig.noiseAmplitude };
      Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_AXIS_DONE_INFO, 0, configData, 4);
    } else {
      int16_t configData[4] = { yConfig.axis , yConfig.valleyIndex, yConfig.peaksIndex, yConfig.noiseAmplitude };
      Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_AXIS_NOT_DONE_INFO, 0, configData, 4);
    }

    if (zDone) {
      int16_t configData[4] = { zConfig.axis , zConfig.peakMin - zConfig.valleyMax, zConfig.signalMax - zConfig.signalMin, zConfig.noiseAmplitude };
      Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_AXIS_DONE_INFO, 0, configData, 4);
    } else {
      int16_t configData[4] = { zConfig.axis , zConfig.valleyIndex, zConfig.peaksIndex, zConfig.noiseAmplitude };
      Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_AXIS_NOT_DONE_INFO, 0, configData, 4);
    }

    PRINTF_TS("CONFIG Chose axis %d with SNR %.2f\n", chosenConfig->axis, bestSNR);
    
    resetVariables();

    TransitionTo(Algorithm_State_ValidateNoise);
    return;
  }

  CalculateFrequency(xNewHalfCycle || yNewHalfCycle || zNewHalfCycle);
  if (samplingFrequency != desiredDataRateFrequency)
  {
      samplingFrequency = desiredDataRateFrequency;
      ResetMagnetometerTimer();
  }
  PRINTF_TS("%d,%d,%d,%d,%.1f\n", newValue.AXIS_X, newValue.AXIS_Y, newValue.AXIS_Z, cycle_counter_get_half_cycles(), samplingFrequency);
}

static void HandleReadRunning(int16_t magValue)
{

  if (magnetometer_controller_get_lpf())
  {
    // only care to check for decreased noise if we're wasting energy on lpf. We should check if the decrease is large enough to warrant turning off lpf
    uint16_t minMaxDiff;
    if(signal_history_add(magValue, &minMaxDiff))
    {
      uint16_t noiseAmplitude = chosenConfig->noiseAmplitude;
      //PRINTF("minMaxDiff %d ", minMaxDiff);
      if (minMaxDiff < noiseAmplitude * percentOfNoiseDecreaseForReset && (noiseAmplitude - minMaxDiff) > minNoiseChange)
      {
        PRINTF_TS("Noise decrease noise: %d minMaxDiff %d\n", noiseAmplitude, minMaxDiff);
        magnetometer_controller_set_lpf(false);
        int16_t data[2] = { RESET_REASON_NOISE_DECREASE, minMaxDiff };
        ResetAlgorithmWithData(data, 2, 0);
        return; // return because it will reset the config
      }
    }
  }

  CalculateFrequency(false);
}

/**
 * @brief Handles an interrupt for the running state
 * 
 * @return true if there's an error
 */
static bool HandleInterrupt( void )
{
  if (CurrentState != Algorithm_State_Running)
  {
    // Only using interrupts in running state
    return true;
  }

  // cheaper to read the pin than mag
  if (!magnetometer_controller_get_pin_status())
  {
    PRINTF("No interrupt\n");
    return false;
  }

  if (!magnetometer_controller_get_axis(chosenConfig->axis, &magValue))
  {
    PRINTF_TS("FAILED Get data\n\r");
    return true;
  }
  
  chosenConfig->lastValue = magValue;

  int16_t trueValue = magValue + magnetometer_controller_get_offset();
  chosenConfig->lastTrueValue = trueValue;
  
  // First check if it's risen further or fallen further than the current max/min
  bool updateMaxOrMin = false;
  if (chosenConfig->rising && trueValue > chosenConfig->currentMax)
  {
    updateMaxOrMin = true;
    chosenConfig->currentMax = trueValue;
  }
  else if (!chosenConfig->rising && trueValue < chosenConfig->currentMin)
  {
    updateMaxOrMin = true;
    chosenConfig->currentMin = trueValue;
  } 
  
  // If it has risen or fallen more, then update the offset
  if (updateMaxOrMin)
  {
    // Update value even if not new cycle, new max/min
    PRINTF("Update mag %d true %d rising %d \n", magValue, trueValue, chosenConfig->rising);
    UpdateOffset();
    return false; // not a real error
  }
  
  // Otherwise it hit the opposite side of the threshold
  static bool innerRising = false;
  static int16_t innerMax = INT16_MIN;
  static int16_t innerMin = INT16_MAX;
  if (chosenConfig->rising){
    PRINTF("rising mag %d trueValue %d max %d\n", magValue, trueValue, chosenConfig->currentMax);
  } else {
    PRINTF("falling mag %d trueValue %d min %d\n", magValue, trueValue, chosenConfig->currentMin);
  }

  if ((chosenConfig->rising && trueValue < chosenConfig->currentMax - chosenConfig->bufferForPulse) ||
      (!chosenConfig->rising && trueValue > chosenConfig->currentMin + chosenConfig->bufferForPulse))
  {
    // Hit target, switch both
    if (chosenConfig->rising) {
      chosenConfig->currentMin = trueValue;
      chosenConfig->currentMax = INT16_MIN;
      innerMin = trueValue;
      innerMax = INT16_MIN;
    } else {
      chosenConfig->currentMin = INT16_MAX;
      chosenConfig->currentMax = trueValue;
      innerMin = INT16_MAX;
      innerMax = trueValue;
    }
    chosenConfig->rising = !chosenConfig->rising;
    innerRising = chosenConfig->rising;
    cycle_counter_add_half_cycle(true);
    lastHalfCycleTime = TimerGetCurrentTime();
    PRINTF("Hit outer cycle now rising %d\n", chosenConfig->rising);
  } 
  else if ((innerRising && trueValue < innerMax - chosenConfig->threshold) ||
    (!innerRising && trueValue > innerMin + chosenConfig->threshold))
  {
    // Inner cycle is for noise detection.
    // Hit inner target, switch inner
    if (innerRising) {
      innerMin = trueValue;
      innerMax = INT16_MIN;
    } else {
      innerMin = INT16_MAX;
      innerMax = trueValue;
    }
    innerRising = !innerRising;
    PRINTF("Hit inner cycle now innerRising %d\n", innerRising);
    // Don't actually increment. We'll increment when we hit the outer cycle.
    cycle_counter_add_half_cycle(false);
  } else {
    // No new cycle
    PRINTF("No new cycle\n");
    // Should we still switch offset? Yes because we to move the offset to follow the signal
  }

  UpdateOffset();

  // Pre-emptive increases reads by about the same percentage as it increases accuracy.
  if (CalculateFrequency(true))
  {
    return true;
  }

  TimerSetValue(&MagnetometerDataTimer, READ_DELAY_AFTER_PULSE);
  TimerStart(&MagnetometerDataTimer);
  ReadDataFlag = false;
  return false;
}
  
static bool CalculateFrequency(bool preemptiveTrigger)
{
  TimerTime_t currentTimestamp = TimerGetCurrentTime();

  if (preemptiveTrigger)
  {
    lastPreemptiveTriggerTimestamp = currentTimestamp;
  }
  
  desiredDataRateFrequency = MIN_DATARATE_FREQUENCY;
  
  uint8_t numberTimestamps = cycle_counter_number_timestamps();
  if (maxNumberOfTimestampsToAverage < numberTimestamps)
  {
    numberTimestamps = maxNumberOfTimestampsToAverage;
  }

  if (numberTimestamps >= 2) 
  {
    // Get half cycle time length for signal frequency calculations
    float timeBetweenPulses;
    TimerTime_t timeSinceLastPulse = currentTimestamp - cycle_counter_get_timestamp(0);
    
    // If it's been a while since a new cycle, then we want to taper off the sampling frequency
    if (timeSinceLastPulse >= FREQUENCY_TAPPER_DELAY)
    { 
      timeBetweenPulses =  timeSinceLastPulse;
    } else {
      // Average over the timestamps
      TimerTime_t total = cycle_counter_get_timestamp(0) - cycle_counter_get_timestamp(numberTimestamps - 1);
      timeBetweenPulses = (float)total / (float)(numberTimestamps - 1);
    }

    if (timeBetweenPulses == 0) {
      LogMag("WARN timeBetweenPulses is 0\n");
      signalFrequency = maxSignalFrequency;
    } else {
      signalFrequency = (1000.0f * CYCLES_PER_PULSE) / timeBetweenPulses; // 1000 for ms
    }

    // Check for max frequency using the new parameter
    uint8_t timestampsForReset = cycle_counter_number_timestamps();
    if (maxNumberOfTimestampsToAverageForReset < timestampsForReset)
    {
      timestampsForReset = maxNumberOfTimestampsToAverageForReset;
    }

    if (timestampsForReset >= 2)
    {
      TimerTime_t totalTimeForReset = cycle_counter_get_timestamp(0) - cycle_counter_get_timestamp(timestampsForReset - 1);
      float averageTimeBetweenPulsesForReset = (float)totalTimeForReset / (float)(timestampsForReset - 1);
      float averageSignalFrequencyForReset = (1000.0f * CYCLES_PER_PULSE) / averageTimeBetweenPulsesForReset;

      if (averageSignalFrequencyForReset > maxSignalFrequency)
      {
        PRINTF_TS("AvgSig=%.2f time=%.2f #ts=%d hit max freq\n", averageSignalFrequencyForReset, averageTimeBetweenPulsesForReset, timestampsForReset);
        if (CurrentState == Algorithm_State_Running)
        {
          cycle_counter_delete_recent_cycles();
        } else if ( CurrentState == Algorithm_State_Configuring && addPulsesDuringConfiguration)
        {
          cycle_counter_decrement_half_cycles(halfCyclesAddedDuringConfiguration);
        }
        cycle_counter_reset_timestamps();
        ResetAlgorithm(RESET_REASON_MAX_FREQUENCY, averageSignalFrequencyForReset);
        return true;
      }
    }

    // Update ODR using signalFrequency
    desiredDataRateFrequency = signalFrequency * FREQUENCY_MULTIPLIER;  // Multiple signal frequency by a scaling factor
    if (desiredDataRateFrequency < MIN_DATARATE_FREQUENCY) {
      desiredDataRateFrequency = MIN_DATARATE_FREQUENCY;
    }
      
    LogMag("Sig=%.2f time=%.2f df=%.1f\n", signalFrequency, timeBetweenPulses, desiredDataRateFrequency);

  } else {
    LogMag("No TS\n");
  }
  
  if (TimerGetElapsedTime(lastPreemptiveTriggerTimestamp) < TRIGGER_DURATION && 
      desiredDataRateFrequency < MIN_TRIGGER_FREQUENCY)
  {
    LogMag("Preempt\n");
    desiredDataRateFrequency = MIN_TRIGGER_FREQUENCY;
  }

  magnetometer_controller_set_odr(desiredDataRateFrequency);
  return false;
}

static void UpdateOffset( void )
{
  int16_t lastValue = chosenConfig->lastTrueValue;

  int newMax = 0;
  int newMin = 0;
  // We have two windows
  // 1. The window that we use to detect an new extreme (min or max)
  // 2. The window that we use to detect a new cycle

  if (chosenConfig->rising)
  {
    int valueToUpdateMax = chosenConfig->currentMax + (chosenConfig->bufferForPulse * percentOfBufferForUpdate);
    int valueToUpdateWindowTowardsNewCycle = valueToUpdateMax - chosenConfig->threshold;
    if (lastValue > valueToUpdateWindowTowardsNewCycle)
    {
      // Move window up
      newMax = valueToUpdateMax;
      newMin = valueToUpdateWindowTowardsNewCycle;
    } else {
      // Move window down
      newMin = chosenConfig->currentMax - chosenConfig->bufferForPulse;
      newMax = newMin + chosenConfig->threshold;
    }
  } else {
    int valueToUpdateMin = chosenConfig->currentMin - (chosenConfig->bufferForPulse * percentOfBufferForUpdate);
    int valueToUpdateWindowTowardsNewCycle = valueToUpdateMin + chosenConfig->threshold;
    if (lastValue < valueToUpdateWindowTowardsNewCycle)
    {
      // Move window down to check if we go to a new min
      // New min should be below the current min
      newMin = valueToUpdateMin;
      newMax = valueToUpdateWindowTowardsNewCycle;
    } else {
      // Move window up to see when we hit a new cycle
      newMax = chosenConfig->currentMin + chosenConfig->bufferForPulse;
      newMin = newMax - chosenConfig->threshold;
    }
  }

  int16_t newOffset = (newMax + newMin) / 2;
  bool changed = magnetometer_controller_set_offset(chosenConfig->axis, newOffset);
  if (changed)
  {
    PRINTF("NewOffset a:%d newMin: %d offset: %d newMax:%d rising:%d\n", chosenConfig->axis, newMin, newOffset, newMax, chosenConfig->rising);

    // Sometimes the magnetometer stays in interrupt and keeps going up or down this makes sure we see that
    float odr = magnetometer_controller_get_odr();
    uint32_t delay = odr > 0 ? (1000 / magnetometer_controller_get_odr()) + 1 : 20; // add 1 so it's definitely after the next read
    TimerSetValue(&DelayedSignalTimer, delay);
    TimerStart(&DelayedSignalTimer);
  }
}

static void ResetConfiguring(RESET_REASON_t reason, CreateThresholdsResult_t resultX, CreateThresholdsResult_t resultY, CreateThresholdsResult_t resultZ)
{
  signal_history_reset();
  if (debugAlerts)
  {
    int16_t errorData[4] = {(uint16_t)reason, (uint16_t)resultX, (uint16_t)resultY, (uint16_t)resultZ};
    Alert_Data(ERROR_CODE_ALGORITHM_CONFIURING_RESET, 0, errorData, 4);
  }
  TransitionTo(Algorithm_State_BaselineNoise); // maintains LPF
}

static void ResetAlgorithm(RESET_REASON_t reason, int16_t value)
{
  int16_t errorData[2] = {(uint16_t)reason, value};
  ResetAlgorithmWithData(errorData, 2, 0);
}

static void ResetAlgorithmWithData(int16_t *errorData, uint8_t size, uint8_t version)
{
  PRINTF_TS("ResetAlgorithmWithData\n\r");
  signal_history_reset();
  if (debugAlerts)
  {
    Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_RESET, version, errorData, size);
  }
  TransitionTo(Algorithm_State_Start);
}

static CreateThresholdsResult_t CreateThresholds(AxisConfiguration *config, float *snr)
{
#ifndef DISABLE_LOGGING
    PRINTF_TS("CONFIG a:%d Peaks: ", config->axis);
    qsort( config->peaks, config->peaksIndex, sizeof(int16_t), compareAscending ); // sort just for logging
    for (uint8_t i = 0; i < config->peaksIndex; i++)
    {
      PRINTF("%d ", config->peaks[i]);
    }
    PRINTF("\n");

    PRINTF_TS("CONFIG a:%d Valleys: ", config->axis);
    qsort( config->valleys, config->valleyIndex, sizeof(int16_t), compareAscending ); // sort just for logging
    for (uint8_t i = 0; i < config->valleyIndex; i++)
    {
      PRINTF("%d ", config->valleys[i]);
    }
    PRINTF("\n");
#endif

    int16_t peakMaximum;
    int16_t valleyMinimum;
    if (GetMinAndMaxConfiguration(config, &valleyMinimum, &peakMaximum, false, config->noiseAmplitude) != 0)
    {
      PRINTF_TS("CONFIG Get Min/Max failed for outer n\r");
      return CREATE_THRESHOLDS_OUTER_MINMAX_FAILED;
    }

    if (peakMaximum <= valleyMinimum)
    {
      PRINTF_TS("CONFIG peakMaximum %d < valleyMinimum %d\n\r", peakMaximum, valleyMinimum);
      return CREATE_THRESHOLDS_PEAK_BELOW_VALLEY;
    }

    config->signalMax = peakMaximum;
    config->signalMin = valleyMinimum;

    int16_t peakMinimum;
    int16_t valleyMaximum;
    if (GetMinAndMaxConfiguration(config, &valleyMaximum, &peakMinimum, true, config->noiseAmplitude) != 0)
    {
      PRINTF_TS("CONFIG Get Min/Max failed for inner n\r");
      return CREATE_THRESHOLDS_INNER_MINMAX_FAILED;
    }

    if (peakMinimum <= valleyMaximum)
    {
      PRINTF_TS("CONFIG peakMinimum %d < valleyMaximum %d\n\r", peakMinimum, valleyMaximum);
      return CREATE_THRESHOLDS_INNER_PEAK_BELOW_VALLEY;
    }

    config->peakMin = peakMinimum;
    config->valleyMax = valleyMaximum;
    int16_t innerCycleSignalDifference = config->peakMin - config->valleyMax - config->noiseAmplitude;

    if (innerCycleSignalDifference < minConfigurationDifference)
    {
      PRINTF_TS("CONFIG signal diff %d too low  min %d max %d n %d", innerCycleSignalDifference, config->valleyMax, config->peakMin, config->noiseAmplitude);
      PRINTF(" totalSig:%d\n\r",config->signalMax - config->signalMin - config->noiseAmplitude);
      return CREATE_THRESHOLDS_DIFF_TOO_SMALL;
    }

    if (innerCycleSignalDifference > MAX_CONFIG_DIFFERENCE_mG)
    {
      PRINTF_TS("CONFIG signal diff %d too high\n\r", innerCycleSignalDifference);
      return CREATE_THRESHOLDS_DIFF_TOO_LARGE;
    }

    config->bufferForPulse = percentSignalForBuffer * innerCycleSignalDifference;
    config->threshold =  (percentOfBufferForTarget + percentOfBufferForUpdate) * config->bufferForPulse;
    
    PRINTF("CONFIG threshold:%d buffer:%d\n\r", config->threshold, config->bufferForPulse);

    if (config->noiseAmplitude + config->noiseBuffer >= (config->threshold * 2)) {
      PRINTF_TS("CONFIG noise %d buff %d too high thresh %d\n\r", config->noiseAmplitude, config->noiseBuffer, config->threshold);
      PRINTF("totalSig:%d\n\r",config->signalMax - config->signalMin - config->noiseAmplitude);
      return CREATE_THRESHOLDS_NOISE_TOO_HIGH;
    }

    *snr = (float)(innerCycleSignalDifference) / config->noiseAmplitude;

    config->needsMoreData = false;
    PRINTF_TS("CONFIG a:%d valleyMin %d valleyMax %d peakMin %d peakMax %d innerDiff %d outerDiff %d n %d lpf %d\n", config->axis, config->signalMin, config->valleyMax, config->peakMin, config->signalMax, config->peakMin - config->valleyMax, config->signalMax - config->signalMin, config->noiseAmplitude, magnetometer_controller_get_lpf());
    return CREATE_THRESHOLDS_SUCCESS;
}

// Event handlers

static void ResetMagnetometerTimer( void )
{
  if (samplingFrequency == 0)
  {
    TimerStop(&MagnetometerDataTimer);
    return;
  }
  uint32_t delay = (uint32_t)((double)1000.0f / (double)samplingFrequency);
  //PRINTF_TS("Delay:%d\n", delay);
  TimerSetValue(&MagnetometerDataTimer, delay);
  TimerStart(&MagnetometerDataTimer);
}

// Reading data occasionally is important to check if out of bounds.
// Can also detect when magnetometer is giving weird data, such as way out of tolerance
static void OnMagnetometerDataTimerEvent( void ) 
{
	ResetMagnetometerTimer();
	ReadDataFlag = true;
}

static void HandleDelayedCheck( void )
{
  if (CurrentState != Algorithm_State_Running)
  {
    return;
  }
  PRINTF_TS("DelayedCheck %d\n", magnetometer_controller_get_pin_status());
  InterruptFired = true;
}

static void HandleInterruptEvent( void )
{
  if (CurrentState != Algorithm_State_Running)
  {
    return;
  }
  PRINTF_TS("InterruptFired %d\n", magnetometer_controller_get_pin_status());
  InterruptFired = true;
}

static void OnConfigurationTimerEvent( void )
{
  if (CurrentState == Algorithm_State_Configuring)
  {
    ConfigurationTimeoutFlag = true;
  }
}

static void OnErrorTimeoutEvent( void )
{
  if (CurrentState == Algorithm_State_Error)
  {
    ErrorTimeoutFlag = true;
  }
}

static void HandleNoiseMonitorError( uint16_t resets )
{
  PRINTF_TS("HandleNoiseMonitorError resets:%d\n", resets);
  if (CurrentState != Algorithm_State_BaselineNoise && CurrentState != Algorithm_State_ValidateNoise)
  {
    PRINTF_TS("ERROR Noise monitor error in wrong state\n");
    return;
  }
  
  if (debugAlerts)
  {
    uint8_t version = 1;
    int16_t data[3] = { RESET_REASON_NOISE_MONITOR_TIMEOUT, magnetometer_controller_get_lpf(), resets };
    Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_RESET, version, data, 3);

    MAGNETOMETER_CONNECTION_t connection = magnetometer_controller_test_connection();
    if (connection != MAGNETOMETER_CONNECTION_NO_ISSUE)
    {
      int16_t connection_value = (int16_t)connection;
      PRINTF_TS("ERROR magnetometer_controller_test_connection failed %d\n", connection);
      Alert_Data(ERROR_CODE_HW_BSP_INTERRUPT_NOT_CONNECTED, 0, &connection_value, 1);
    }
  }

  TransitionTo(Algorithm_State_Error);
}

// Magnetometer and communication functions
static void HandleCriticalError(ALGORITHM_ERROR_t error)
{
  PRINTF_TS("Handle Critical %d\n",error);
  if (debugAlerts) {
    int16_t errorData[1] = { error };
    Alert_Data(ERROR_CODE_ALGORITHM, 0, errorData , 1);
  }
  TransitionTo(Algorithm_State_Error);
}

static void HandleCommunicationError( void )
{
  PRINTF_TS("Handle Communication Error\n");
  HandleCriticalError(ALGORITHM_FAILED_COMMUNICATION);
}

#endif // ALGORITHM

/************************ (C) COPYRIGHT NOWI Sensors *****END OF FILE****/
