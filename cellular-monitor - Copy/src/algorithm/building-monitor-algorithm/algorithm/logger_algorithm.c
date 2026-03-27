// A logging algorithm to log the magnetic field over time

#ifdef ALGORITHM_LOGGER
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "log.h"
#include "logger_algorithm.h"
#include "so_hpf.h"
#include "timeServer.h"
#include "communication_monitor.h"
#include "magnetometer_controller.h"

#define MAX_REPEATS 3

#define SAMPLING_RATE 136.0f
#define SAMPLING_RATE_LPF 100.0f

static bool flag_read = true;

static void HandleDataReadyEvent( void );

static int16_t lastValue = 0;
static uint8_t repeats = 0;
static uint16_t samplingRate;
static MAG_OPERATING_MODE_t opMode;
static bool initialized = false;
static TimerTime_t lastSampleTime = 0;

ALGORITHM_t Algorithm = {
  logger_algorithm_init,
  logger_algorithm_check_in,
  logger_algorithm_reset,
  logger_algorithm_log,
  logger_algorithm_is_configured,
  logger_algorithm_update_variable,
  logger_algorithm_enable,
  logger_algorithm_disable
};

void logger_algorithm_init( void )
{
  PRINTF("logger_algorithm_init\n");
  magnetometer_controller_init(logger_algorithm_reset);

  bool enableLPF = false;
  if (enableLPF)
  {
    opMode = MAG_OPERATING_MODE_CONTINUOUS;
    samplingRate = SAMPLING_RATE_LPF;
  } else {
    opMode = MAG_OPERATING_MODE_SINGLE;
    samplingRate = SAMPLING_RATE;
  }
}

void logger_algorithm_check_in( void )
{
  if (!initialized)
  {
    initialized = true;
    logger_algorithm_reset();
  }

  TimerTime_t elapsedTime = TimerGetElapsedTime(lastSampleTime);
  if (lastSampleTime != 0 && elapsedTime > 100.0f)
  {
    PRINTF_BASE("logger_algorithm_check_in timeout %d %d\n", elapsedTime, lastSampleTime);
    logger_algorithm_reset();
  }
  
  if (flag_read)
  {
    SensorAxes_t value;
    
    if (!magnetometer_controller_get_axes(&value))
    {
      return;
    }

    magnetometer_controller_set_operating_mode(MAG_OPERATING_MODE_SINGLE);
    
    if (value.AXIS_Z == lastValue)
    {
      repeats++;
      if (repeats > MAX_REPEATS)
      {
        repeats = 0;
        PRINTF_BASE("logger_algorithm repeat %d, restarting\n",value.AXIS_Z);
        logger_algorithm_reset();
        return;
      }
    } else {
      repeats = 0;
    }
    
    lastValue = value.AXIS_Z;
    
    PRINTF_BASE("%d,%d,%d,%d\n",TimerGetCurrentTime(), value.AXIS_X, value.AXIS_Y, value.AXIS_Z);
    flag_read = false;
    lastSampleTime = TimerGetCurrentTime();
  }
}

void logger_algorithm_reset( void )
{
    magnetometer_controller_reset();
    magnetometer_controller_set_operating_mode(opMode);
    if (opMode == MAG_OPERATING_MODE_CONTINUOUS)
    {
      magnetometer_controller_set_odr(samplingRate);
    }
    
    magnetometer_controller_start_interrupt_drdy(HandleDataReadyEvent);

    flag_read = true;
}

void logger_algorithm_log( void )
{
  PRINTF_BASE("Logger algorithm log last:%d\n", lastValue);
}

bool logger_algorithm_is_configured( void )
{
  return true;
}

void logger_algorithm_update_variable(uint8_t variable, uint32_t value)
{

}

static void HandleDataReadyEvent( void )
{
  flag_read = true;
}

void logger_algorithm_enable( void )
{
  PRINTF("logger_algorithm_enable\n");
}

void logger_algorithm_disable( void )
{
  PRINTF("logger_algorithm_disable\n");
}

#endif // ALGORITHM_LOGGER
