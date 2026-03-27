#ifdef USE_MAGNETOMETER
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "log.h"
#include "noise_monitor.h"
#include "so_hpf.h"
#include "timeServer.h"
#include "communication_monitor.h"
#include "bsp.h"
#include "magnetometer_controller.h"
#ifdef SIMULATION
#include "simulation.h"
#endif

#define MAX_SAMPLES 50
#define BASELINE_FIRST_SAMPLE 10
#define TIMEOUT_TIME 5000 // Milliseconds

#define NM_PRINTF(...)
// #define NM_PRINTF(...)     PRINTF_BASE(__VA_ARGS__)

#define CUTOFF_FREQUENCY 45.0f 
#define SAMPLING_RATE 142.0f
#define CUTOFF_FREQUENCY_LPF 25.0f 
#define SAMPLING_RATE_LPF 100.0f

typedef struct {
  int16_t min;
  int16_t max;
  filter_data filter_data;
} noise_data;

static uint16_t samples = 0;

static bool initialized = false;
static volatile bool flag_read = false;
static volatile bool flag_restart = false;

static NoiseMonitorErrorCallback errorCallback = NULL;

static TimerEvent_t timerNoiseMonitor;

static void HandleTimeout( void );
static void HandleDataReadyEvent( void );
static void ResetState( void );
static void CheckData(noise_data *data, int16_t value);

static uint16_t resets = 0;
static TimerTime_t start = 0;

static uint16_t samplingRate;

static noise_data xData;
static noise_data yData;
static noise_data zData;

void Noise_Monitor_Init( NoiseMonitorErrorCallback callback )
{
    PRINTF_TS("Noise_Monitor_Init\n");

    errorCallback = callback;
    if (!initialized)
    {
      TimerInit( &timerNoiseMonitor, HandleTimeout );
      TimerSetValue( &timerNoiseMonitor, TIMEOUT_TIME );
      initialized = true;
    } else {
      PRINTF_TS("WARN Noise_Monitor_Init already initialized\n");
    }
}

void Noise_Monitor_Start( bool enableLPF)
{
    PRINTF_TS("Noise_Monitor_Start lpf=%d\n", enableLPF);

    resets = 0;
    float cutoffFrequency;
    MAG_OPERATING_MODE_t opMode;
    if (enableLPF)
    {
      opMode = MAG_OPERATING_MODE_CONTINUOUS;
      samplingRate = SAMPLING_RATE_LPF;
      cutoffFrequency = CUTOFF_FREQUENCY_LPF;
    } else {
      opMode = MAG_OPERATING_MODE_SINGLE;
      samplingRate = SAMPLING_RATE;
      cutoffFrequency = CUTOFF_FREQUENCY;
    }
    
    ResetState();

    // Q=0.707 is to make it a Butterworth filter
    so_hpf_calculate_coeffs(0.7071067812, cutoffFrequency, samplingRate, &xData.filter_data); 
    so_hpf_calculate_coeffs(0.7071067812, cutoffFrequency, samplingRate, &yData.filter_data);
    so_hpf_calculate_coeffs(0.7071067812, cutoffFrequency, samplingRate, &zData.filter_data);

    // get to a clean state
    magnetometer_controller_set_to_default(false);

    // setup
    if (!magnetometer_controller_set_operating_mode(opMode))
    {
      PRINTF_TS("noise_monitor_init ERROR Failed set operating mode\n");
    }
    
    if (opMode == MAG_OPERATING_MODE_CONTINUOUS)
    {
      magnetometer_controller_set_odr(samplingRate);
    }
    
    if (!magnetometer_controller_start_interrupt_drdy(HandleDataReadyEvent))
    {
      PRINTF_TS("noise_monitor_init ERROR Failed start interrupt\n");
    }
    
    TimerReset( &timerNoiseMonitor );
}

void Noise_Monitor_Stop( void )
{
  PRINTF_TS("Noise_Monitor_Stop\n");
  
  magnetometer_controller_stop_interrupt();

	TimerStop( &timerNoiseMonitor );
}

bool Noise_Monitor_CheckIn( void )
{
  PRINTF_TS("Noise_Monitor_CheckIn\n");
  // we want to loop instead of normal check ins because we don't want to let other processes interrupt this
  // For example connecting to the cloud could interrupt this for a few seconds causing a timeout
  while(1)
  {
#ifdef SIMULATION // Necessary or the time never updates in event based
    simulation_checkin();
#endif

    if (flag_restart)
    {
      PRINTF_TS("Noise_Monitor_CheckIn: timed out\n");
      flag_restart = false;
      errorCallback(resets);
      return false;
    }

    if (flag_read)
    {
        flag_read = false;
        if (start == 0 )
        {
          start = TimerGetCurrentTime();
        }
        samples++;
       
        // set up right away to make it as fast as possible
        if (samplingRate > 100.0f){
          // Over 100hz need to use single mode
          if (!magnetometer_controller_set_operating_mode(MAG_OPERATING_MODE_SINGLE))
          {
              PRINTF_TS("Failed Set op\n");
              // start over
              ResetState();
              continue;
              // return false;
          }
        }

        bool status;
        if (!magnetometer_controller_get_drdy_status(&status))
        {
          PRINTF_TS("Failed DRDY\n");
          continue;
          // return false;
        }
        
        if (!status)
        {
          PRINTF_TS("No DRDY\n");
          flag_read = false;
          continue;
          // return false;
        }

        SensorAxes_t value;
        if (!magnetometer_controller_get_axes(&value))
        {
            PRINTF_TS("Failed get axes\n");
            // start over
            ResetState();
            continue;
            // return false;
        }

        NM_PRINTF("t:%d s:%d v:%d,%d,%d ",TimerGetCurrentTime(), samples, value.AXIS_X, value.AXIS_Y, value.AXIS_Z);

        int16_t filteredValueX = so_hpf_filter(value.AXIS_X, &xData.filter_data);
        int16_t filteredValueY = so_hpf_filter(value.AXIS_Y, &yData.filter_data);
        int16_t filteredValueZ = so_hpf_filter(value.AXIS_Z, &zData.filter_data);

        NM_PRINTF("f: %d,%d,%d\n", filteredValueX, filteredValueY, filteredValueZ);
        if (samples > BASELINE_FIRST_SAMPLE)
        {
          CheckData(&xData, filteredValueX);
          CheckData(&yData, filteredValueY);
          CheckData(&zData, filteredValueZ);
        }

        if(samples > MAX_SAMPLES)
        {
            // Figure out noise and transition
            PRINTF_TS("Noise_Monitor_CheckIn: finished noiseAmplitude:%d %d %d time: %d \n", xData.max - xData.min, yData.max - yData.min, zData.max - zData.min, TimerGetCurrentTime() - start);
            PRINTF_TS("x min:%d max:%d\n", xData.min, xData.max);
            PRINTF_TS("y min:%d max:%d\n", yData.min, yData.max);
            PRINTF_TS("z min:%d max:%d\n", zData.min, zData.max);
            return true;
        }
    }
  }

  return false;
}

void Noise_Monitor_GetNoise( uint16_t *xNoise, uint16_t *yNoise, uint16_t *zNoise )
{
  *xNoise = xData.max - xData.min;
  *yNoise = yData.max - yData.min;
  *zNoise = zData.max - zData.min;
}

static void ResetState( void )
{
  flag_read = true;
  samples = 0;
  start = 0;
  xData.min = 0;
  xData.max = 0;
  yData.min = 0;
  yData.max = 0;
  zData.min = 0;
  zData.max = 0;
  resets++;
}

static void HandleDataReadyEvent( void )
{
    flag_read = true;
}

static void HandleTimeout( void )
{
    flag_restart = true;
}

static void CheckData(noise_data *data, int16_t value)
{
  if (value < data->min)
  {
    data->min = value;
  } else if (value > data->max)
  {
    data->max = value;
  }
}
#endif // USE_MAGNETOMETER
