#ifdef ALGORITHM_POLL_AND_INT

#include <stdlib.h>
#include "axis_configuration.h"
#include "polling_algorithm.h"
#include "signal.h"
#include "timeServer.h"
#include "log.h"
#include "communication_monitor.h"
#include "cycle_counter.h"
#include "axis_configuration_utility.h"
#include "magnetometer_controller.h"
#include "signal_history.h"

// #define Log_Mag PRINTF_BASE
#define Log_Mag PRINTF_TS
// #define Log_Mag(...)

// Algorithm variables
#define MIN_CONFIG_DIFFERENCE_mG 40
#define HALFCYCLES_PER_PULSE  1
#define PULSES_PER_CYCLE 2

extern uint16_t noiseDecreaseMin;
extern float percentOfNoiseDecreaseForReset;
extern uint8_t maxNumberOfTimestampsToAverage;
extern float percentNoiseBuffer;
extern uint16_t staticNoiseBuffer;
extern uint16_t maxNoise_mG;
extern uint32_t maxTimeSinceLastPulse;
extern uint16_t maxSignalFrequency;
extern float percentOfLastMinMaxBuffer;

static float samplingFrequency;
static float minDataRateFrequency = 10.0f; // Remember minimum ODR is 10HZ so decimals won't matter

static const float triggerFrequency = 50.0f;
static const float triggerDuration = 200; // ms

static float frequencyMultiplier = 4.0f; // Higher -> More accurate counting of pulses but decreased battery life
static float changeInFrequencyMultiplier = 2.0f;

static float pollingFrequency = 1.0f;
static float signalFrequency = 0;

static float tapperDelay = 200.0f; // (500.0f * HALFCYCLES_PER_PULSE)/ minDataRateFrequency )

static bool initialized = false; 
static bool flagRead = false;
static bool flagInterrupt = false;
static uint32_t noiseAmplitude = 0;
static AxisConfiguration zConfig = {0};
static TimerTime_t lastHalfCycleTime = 0;

static TimerEvent_t readTimer;

static AlgorithmErrorCallback errorCallback = NULL;

static void ReadTimerEvent( void );
static void HandleInterruptEvent( void );
static void CalculateFrequency( void );
static bool CheckAxis(AxisConfiguration* config, int16_t val, bool* valueChanged);

void Polling_Algorithm_Init( uint16_t inputNoiseAmplitude, AlgorithmErrorCallback inputCallback )
{
  PRINTF("Polling_Algorithm_Init initialized %d noise %d\n", initialized, inputNoiseAmplitude);
  if (!initialized)
  {
    initialized = true;
    TimerInit( &readTimer, ReadTimerEvent );
  }

  noiseAmplitude = inputNoiseAmplitude;
  errorCallback = inputCallback;

  samplingFrequency = minDataRateFrequency;
  TimerSetValue( &readTimer, 1000 / pollingFrequency );
  TimerStart( &readTimer );

  magnetometer_controller_set_operating_mode(MAG_OPERATING_MODE_CONTINUOUS);
  magnetometer_controller_start_interrupt_threshold(HandleInterruptEvent, SENSOR_AXIS_Z);

  signal_history_reset();
  ResetConfiguration(&zConfig);
  lastHalfCycleTime = TimerGetCurrentTime(); // Need starting point
  // Get it started
  flagRead = true;
}

void Polling_Algorithm_DeInit( void )
{
  PRINTF("Polling_Algorithm_DeInit\n");
  TimerStop( &readTimer );
  magnetometer_controller_stop_interrupt();
}

void Polling_Algorithm_CheckIn()
{
  if (flagInterrupt)
  {
    flagInterrupt = false;
    flagRead = true;
  }

  if (flagRead)
  {
    if (TimerGetElapsedTime(lastHalfCycleTime) > maxTimeSinceLastPulse)
    {
      Log_Mag("No pulses duration exceeded\n");
      const uint8_t size = 3;
      int16_t data[size] = { RESET_REASON_NO_NEW_PULSES, zConfig.minSeen, zConfig.maxSeen };
      errorCallback(data, size);
      return;
    }

    int16_t value;
    if (!magnetometer_controller_get_axis(SENSOR_AXIS_Z, &value))
    {
      return;
    }

    uint16_t minMaxDiff;
    if(signal_history_add(value, &minMaxDiff))
    {
      if (minMaxDiff < noiseAmplitude * percentOfNoiseDecreaseForReset && (noiseAmplitude - minMaxDiff) > noiseDecreaseMin)
      {
        Log_Mag ("Noise decrease noise: %d minMaxDiff %d\n", noiseAmplitude, minMaxDiff);
        const uint8_t size = 2;
        int16_t data[size] = { RESET_REASON_NOISE_DECREASE, minMaxDiff };
        errorCallback(data, size);
        return;
      } else 
      { 
        Log_Mag("minMaxDiff %d ", minMaxDiff);
      }
    }
  
    flagRead = false;
    bool valueChanged = false;
    bool newHalfCycle = CheckAxis(&zConfig, value, &valueChanged);
    if (newHalfCycle)
    {
      lastHalfCycleTime = TimerGetCurrentTime();
      cycle_counter_add_half_cycle(true);
      Log_Mag("value:%d hc:%d\n", value, cycle_counter_get_half_cycles());
    } else {
      Log_Mag("value:%d\n", value);
    }
    
    CalculateFrequency();

    TimerSetValue( &readTimer, 1000 / pollingFrequency );
    TimerStart( &readTimer );
    magnetometer_controller_set_odr(samplingFrequency);
  // Log_Mag("samp: %.f sig: %.2f\n", samplingFrequency, signalFrequency);
  }

}

void Polling_Algorithm_Log( void )
{
  PRINTF_BASE("Min: %d, Max: %d, Diff: %d, Noise: %d, Last: %d, SF: %.2f, Samp: %.2f\n", 
    zConfig.minSeen, zConfig.maxSeen, zConfig.maxSeen - zConfig.minSeen, noiseAmplitude, zConfig.lastValue, signalFrequency, samplingFrequency);
}

void Polling_Algorithm_Get_Data(int16_t data[], uint8_t *size)
{
  //data[0] = 0; // State
  data[1] = zConfig.minSeen;
  data[2] = zConfig.maxSeen;
  data[3] = noiseAmplitude;
  *size = 4;

  // Reset data
  zConfig.minSeen = INT16_MAX;
  zConfig.maxSeen = INT16_MIN;
}

static bool CheckAxis(AxisConfiguration* config, int16_t val, bool* valueChanged)
{
  if(val < config->minSeen)
  {
    config->minSeen = val;
  } else if (val > config->maxSeen)
  {
    config->maxSeen = val;
  }
 
  bool changedDirection = false;
  
  // Choose buffer by what's largest
  uint16_t buffer = MIN_CONFIG_DIFFERENCE_mG;
  uint16_t noiseAndBuffer = noiseAmplitude + (noiseAmplitude * percentNoiseBuffer) + staticNoiseBuffer;
  if (noiseAndBuffer > buffer)
  { 
     buffer = noiseAndBuffer;
  }

  uint16_t minMax;
  if (signal_history_get_min_max_diff(&minMax))
  {  
    uint16_t lastMinMaxBuffer = minMax * percentOfLastMinMaxBuffer;
    if (lastMinMaxBuffer > buffer)
    {
      Log_Mag("minMaxBuffer %d\n", lastMinMaxBuffer);
      buffer = lastMinMaxBuffer;
    }
}
  
  if (config->rising) {
    // check if still rising
    if (val < config->currentMax - buffer)
    {
      Log_Mag("Max: %d ",config->currentMax);
      changedDirection = true;
      config->currentMax = INT16_MIN;
      config->currentMin = val;
      magnetometer_controller_set_interrupt_threshold(config->currentMin + buffer);

    } else if (val > config->currentMax)
    {
      config->currentMax = val;
      magnetometer_controller_set_interrupt_threshold(config->currentMax - buffer);
    }
  } else {
    // check if still falling
    if (val > config->currentMin + buffer)
    {
      Log_Mag("Min: %d ",config->currentMin);
      changedDirection = true;
      config->currentMin = INT16_MAX;
      config->currentMax = val;
      magnetometer_controller_set_interrupt_threshold(config->currentMax - buffer);
    } else if (val < config->currentMin) 
    {
      config->currentMin = val;
      magnetometer_controller_set_interrupt_threshold(config->currentMin + buffer);
    }
  }

  bool newHalfCycle = false;

  if (changedDirection) {
    config->rising = !config->rising;
    // First min or max is just getting setup and not a real half cycle.
    if(config->configurationState)
    {
      config->configurationState = false;
    } else {
      newHalfCycle = true;
    }
  } 
  
  if (abs(config->lastValue - val) > config->noiseAmplitude)
  {
    *valueChanged = true;
  } else {
    *valueChanged = false;
  }

  config->lastValue = val;
  return newHalfCycle;
}

static void CalculateFrequency()
{
  TimerTime_t currentTimestamp = TimerGetCurrentTime();
  float newFrequency = minDataRateFrequency;
 
  uint8_t numTimestamps = cycle_counter_number_timestamps();

  if (maxNumberOfTimestampsToAverage < numTimestamps)
  {
    numTimestamps = maxNumberOfTimestampsToAverage;
  }

  if (numTimestamps > 2) 
  {
    // Get half cycle time length for signal frequency calculations
    float timeBetweenPulses;
    TimerTime_t timeSinceLastPulse = currentTimestamp - cycle_counter_get_timestamp(0);
    
    // Average over the timestamps
    TimerTime_t total = cycle_counter_get_timestamp(0) - cycle_counter_get_timestamp(numTimestamps - 1);
    timeBetweenPulses = total / (numTimestamps - 1);
  
    float changeSum = 0.0f;

    if (timeSinceLastPulse < triggerDuration)
    {
      newFrequency = triggerFrequency;
    }

    // If it's been a while since the last pulse, go back to the minimum data rate
    if (timeSinceLastPulse >= tapperDelay )
    { 
      Log_Mag("tapper %d ", timeSinceLastPulse);
    } else  {
      if (timeBetweenPulses == 0) {
        PRINTF_TS("WARN timeBetweenPulses is 0\n");
        signalFrequency = maxSignalFrequency;
      } else {
        signalFrequency = (500.0f * HALFCYCLES_PER_PULSE) / timeBetweenPulses; // 500.0f because we're looking at half cycles and we're trying to get frequency.
      }

      // May want to limit this
      if (numTimestamps > 3)
      {
        uint8_t maxBasedOnTimestamps = numTimestamps - 2;
        uint8_t maxConfigured = 3;
        uint8_t max = maxBasedOnTimestamps < maxConfigured ? maxBasedOnTimestamps : maxConfigured;

        for (size_t i = 0; i < max; i++)
        {
          float firstTimeBetweenPulses = cycle_counter_get_timestamp(i) -  cycle_counter_get_timestamp(i + 1);
          
          float secondTimeBetweenPulses = cycle_counter_get_timestamp(i + 1) -  cycle_counter_get_timestamp(i + 2);
          
          if (firstTimeBetweenPulses <= 0 || secondTimeBetweenPulses  <= 0) {
            PRINTF_TS("WARN timeBetweenPulses is 0\n");
            continue;
          }
          float frequency = (500.0f * HALFCYCLES_PER_PULSE) / firstTimeBetweenPulses;
          float frequencyBefore = (500.0f * HALFCYCLES_PER_PULSE) / secondTimeBetweenPulses;
          float  changeInFrequency = frequency - frequencyBefore;
          // Only add positive changes, better to over sample than under sample
          if (changeInFrequency < 0) {
            changeInFrequency = 0;
          }
          changeSum += changeInFrequency;
        }
      }

      if (signalFrequency > maxSignalFrequency)
      {
        PRINTF_TS("Sig=%.2f time=%.2f #ts=%d hit max freq\n", signalFrequency, timeBetweenPulses, numTimestamps);
        cycle_counter_delete_recent_cycles();
        const uint8_t size = 2;
        int16_t data[size] = { RESET_REASON_MAX_FREQUENCY, signalFrequency };
        errorCallback(data, size);
      }

      float calculatedFrequency = (signalFrequency * frequencyMultiplier) + (changeSum * changeInFrequencyMultiplier);  // Multiple signal frequency by a scaling factor
      if (calculatedFrequency > newFrequency)
      {
        // Only update if higher
        newFrequency = calculatedFrequency;
      }
    }

    if (newFrequency > magnetometer_max_datarate)
    {
      newFrequency = magnetometer_max_datarate;
    } else if (newFrequency < minDataRateFrequency)
    {
      newFrequency = minDataRateFrequency;
    }
    
    samplingFrequency = newFrequency;
    
    Log_Mag("sig=%.2f time=%.2f samp=%.1f cf=%.2f\n", signalFrequency, timeBetweenPulses, samplingFrequency, changeSum);
  } else {
    Log_Mag("No TS\n");
  }
}

static void ReadTimerEvent( void )
{
  flagRead = true;
}

static void HandleInterruptEvent( void )
{
  flagInterrupt = true;
}

#endif // POLL_ALGORITHM
