// The state machine and manager for the algorithm

#if defined(ALGORITHM_POLL_AND_INT)

#include <stdlib.h>
#include "error_handler.h"
#include "signal_tracker.h"
#include "log.h"
#include "axis_configuration.h"
#include "noise_monitor.h"
#include "polling_algorithm.h"
#include "magnetometer_controller.h"
#include "lowest_flow_over_period_tracker.h"
#include "signal.h"
#include "communication_monitor.h"
#include "cycle_counter.h"
#include "flash_utility.h"

ALGORITHM_t Algorithm = {
  signal_tracker_init,
  signal_tracker_check_in,
  signal_tracker_reset,
  signal_tracker_log,
  signal_tracker_is_configured,
  signal_tracker_update_variable,
  signal_tracker_enable,
  signal_tracker_disable
};

static TimerEvent_t ErrorTimeoutTimer;
static void OnErrorTimeoutEvent( void );
static TimerTime_t lastErrorTime = 0;
static uint32_t errorTimeout = 0;

bool debugAlerts = true;
float percentNoiseBuffer = 0.20f;
uint16_t staticNoiseBuffer = 40;
uint16_t maxNoise_mG = 300.0f;
uint16_t noiseDecreaseMin = 25;
float percentOfNoiseDecreaseForReset = 0.7f;
uint8_t maxNumberOfTimestampsToAverage = 5;
uint32_t maxTimeSinceLastPulse = 1000 * 60 * 60 * 12; // 12 hours
// EU 50 Hz is the power frequency, USA 60Hz. 13 GPM @ 113ppg = 24.48 Hz
uint16_t maxSignalFrequency = 45.0f; // EU is 50 Hz, when fully pumping a meter can get up to 30+ Hz
float percentOfLastMinMaxBuffer = 0.4f; // jitters are about 0.2 of output but minMax isn't always full signal value

static uint16_t noise;

static void BaselineNoise_CheckIn( void );

typedef enum {
  State_Start = 0,
  State_BaselineNoise,
  State_Running,
  State_Error,
  State_Disabled
} State_t;

static State_t state = State_Start;
static void TransitionTo( State_t newState );

static void HandleAlgorithmError(int16_t data[], uint8_t size);
static void HandleNoiseError( uint16_t resets );
static void HandleCommunicationError( void );
static void HandleCriticalError( ALGORITHM_ERROR_t error );

void signal_tracker_init( void )
{
  PRINTF_TS("signal_tracker_init\n");
  cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);
  Noise_Monitor_Init(HandleNoiseError);
  
  FlashUtility_LoadConfiguration();

  TimerInit( &ErrorTimeoutTimer, OnErrorTimeoutEvent);

  magnetometer_controller_init(HandleCommunicationError);
}

void signal_tracker_enable( void )
{
  PRINTF_TS("signal_tracker_enable\n");
  TransitionTo(State_Start);
}

void signal_tracker_disable( void )
{
  PRINTF_TS("signal_tracker_disable\n");
  if (state != State_Disabled)
  {
    TransitionTo(State_Disabled);
  }
}

void signal_tracker_check_in( void )
{
  magnetometer_controller_check_in();

  switch (state) 
  {
    case State_Start:
    if(!magnetometer_controller_reset())
      {
        HandleCriticalError(ALGORITHM_FAILED_TO_INIT);
        PRINTF_TS("ERROR Enable Mag Failed\n\r");
      } else {
        TransitionTo(State_BaselineNoise);
      }
      break;
    case State_BaselineNoise:
      BaselineNoise_CheckIn();
      break;
    case State_Running:
      Polling_Algorithm_CheckIn();
      break;
    case State_Error:
    case State_Disabled:
      break;
  }
}

void signal_tracker_reset( void )
{
  TransitionTo(State_Start);
}

void signal_tracker_log( void )
{
  PRINTF_BASE("signal_tracker - state:%d noise:%d hc:%d\n", state, noise, cycle_counter_get_half_cycles());
  PRINTF_BASE("configs - percentNoiseBuffer:%f staticNoiseBuffer:%d maxNoise_mG:%d\n", percentNoiseBuffer, staticNoiseBuffer, maxNoise_mG);
  PRINTF_BASE("configs - noiseDecreaseMin:%d percentNoiseDecrease:%f #timestampsToAvg:%d\n", noiseDecreaseMin, percentOfNoiseDecreaseForReset, maxNumberOfTimestampsToAverage);
  PRINTF_BASE("configs - maxTimeSinceLastPulse:%d maxSignalFrequency:%d\n", maxTimeSinceLastPulse, maxSignalFrequency);

  switch (state)
  {
    case State_Running:
      Polling_Algorithm_Log();
      break;
    default:
      break;
  }
}

bool signal_tracker_is_configured( void )
{
  return state == State_Running;
}

void signal_tracker_update_variable( uint8_t variable, uint32_t value )
{
  switch(variable)
  {
    case 2:
      percentNoiseBuffer = value /1000.0f;
      PRINTF_BASE("percentNoiseBuffer to %f\n",percentNoiseBuffer);
      break;
    case 3:
      maxNoise_mG = value & 0xFFFF;
      PRINTF_BASE("maxNoise_mG to %d\n",maxNoise_mG);
      break;
    case 6:
      staticNoiseBuffer = value & 0xFFFF;
      PRINTF_BASE("staticNoiseBuffer to %d\n",staticNoiseBuffer);
      break;
    case 7:
      noiseDecreaseMin = value & 0xFFFF;
      PRINTF_BASE("noiseDecreaseMin to %d\n",noiseDecreaseMin);
      break;
    case 8:
      percentOfNoiseDecreaseForReset = value / 1000.0f;
      PRINTF_BASE("percentOfNoiseDecreaseForReset to %f\n",percentOfNoiseDecreaseForReset);
      break;
    case 9:
      maxNumberOfTimestampsToAverage = value & 0xFF;
      PRINTF_BASE("maxNumberOfTimestampsToAverage to %d\n",maxNumberOfTimestampsToAverage);
      break;
    case 10:
      maxTimeSinceLastPulse = (value & 0xFFFFFFFF) * 1000 * 60; // in minutes
      PRINTF_BASE("maxTimeSinceLastPulse to %d\n",maxTimeSinceLastPulse);
      break;
    case 11:
      maxSignalFrequency = value & 0xFFFF;
      PRINTF_BASE("maxSignalFrequency to %d\n",maxSignalFrequency);
      break;
    case 12:
      debugAlerts = value & 0x01;
      PRINTF_BASE("debugAlerts to %d\n",debugAlerts);
      break;
    case 13:
      percentOfLastMinMaxBuffer = value / 1000.0f;
      PRINTF_BASE("percentOfLastMinMaxBuffer to %f\n",percentOfLastMinMaxBuffer);
      break;
    case 14: {
        PRINTF_BASE("getting data\n");
        uint8_t size = 1;
        int16_t data[4];
        data[0] = state;
        switch (state)
        {
          case State_Running:
            Polling_Algorithm_Get_Data(data, &size);
            break;
          default:
            break;
        }
        Alert_Data(ERROR_CODE_NO_ERROR_DATA, 0, data, size);
      }
      break;
    default:
      PRINTF_BASE("unhandled variable sdf%d\n",variable);
      break;
  }

  FlashUtility_SaveConfiguration();
}

static void TransitionTo( State_t newState )
{
  State_t lastState = state;
  state = newState;

  PRINTF_TS("TransitionTo %d -> %d\n", lastState, newState);

  // exiting
  switch ( lastState )
  {
    case State_BaselineNoise:
      Noise_Monitor_Stop();
      break;
    case State_Running:
      Polling_Algorithm_DeInit();
      break;
    default:
      break;
  }

  // entering
  switch ( newState )
  {
    case State_Start:
      // To known good state
      magnetometer_controller_set_to_default(false);
      break;
    case State_BaselineNoise:
      Noise_Monitor_Start(magnetometer_controller_get_lpf());
      break;
    case State_Running: {
      if (debugAlerts)
      {
        const uint8_t size = 2;
        const uint16_t version = 2;

        int16_t configData[size] = { noise, magnetometer_controller_get_lpf() };
        Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_SET, version, configData, size);
      }
      LowestFlowOverPeriodTracker_Reset();
      Polling_Algorithm_Init(noise, HandleAlgorithmError);
      break;
    }
    case State_Error:
      if (lastState != State_Error)
      {
        // Check if it errored out quickly
        if (lastErrorTime != 0 && TimerGetElapsedTime(lastErrorTime) < 1000*60*5)
        {
          // backoff, getting multiple errors in a row
          if (errorTimeout < 1000*60*10) // max out at 10 minutes
          {
            errorTimeout += 1000*60;
          }
          PRINTF_BASE("Error backoff %d\n", errorTimeout);
          TimerSetValue(&ErrorTimeoutTimer, errorTimeout);
          TimerStart(&ErrorTimeoutTimer);
        } else {
          // Timeout error right away
          OnErrorTimeoutEvent();
        }
      }
    case State_Disabled: 
      magnetometer_controller_set_to_default(true);
      magnetometer_reset();
      break;
  }
}

static void BaselineNoise_CheckIn( void )
{
  bool finished = Noise_Monitor_CheckIn();
  if (finished)
  {
    uint16_t xAxis, yAxis, zAxis;
    Noise_Monitor_GetNoise(&xAxis, &yAxis, &zAxis);
    noise = zAxis;
    PRINTF_TS("Noise Amplitude: %d ", noise);
    if (!magnetometer_controller_get_lpf() && noise > maxNoise_mG)
    {
      magnetometer_controller_set_lpf(true);
      TransitionTo(State_BaselineNoise);
    } else {
      TransitionTo(State_Running);
    }
  }
}

static void HandleNoiseError( uint16_t resets )
{
  if (debugAlerts)
  {
    uint8_t version = 1;
    int16_t data[3] = { RESET_REASON_NOISE_MONITOR_TIMEOUT, magnetometer_controller_get_lpf(), resets };
    Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_RESET, version, data, 3);
  }
  TransitionTo(State_Error);
}

static void HandleAlgorithmError( int16_t data[], uint8_t size )
{
  if (debugAlerts)
  {
    uint8_t version = 1;
    Alert_Data(ERROR_CODE_ALGORITHM_CONFIG_RESET, version, data, size);
  }
  TransitionTo(State_Error);
}

static void HandleCommunicationError( void )
{
  HandleCriticalError(ALGORITHM_FAILED_COMMUNICATION);
}

static void HandleCriticalError( ALGORITHM_ERROR_t error )
{
  PRINTF_TS("HandleCriticalError %d\n", error);
  if (debugAlerts) {
    int16_t errorData[1] = { error };
    Alert_Data(ERROR_CODE_ALGORITHM, 0, errorData , 1);
  }
  TransitionTo(State_Error);
}

static void OnErrorTimeoutEvent( void )
{
  PRINTF_TS("OnErrorTimeoutEvent\n");
  lastErrorTime = TimerGetCurrentTime();
  TransitionTo(State_Start);
}

#endif // ALGORITHM_POLL_AND_INT
