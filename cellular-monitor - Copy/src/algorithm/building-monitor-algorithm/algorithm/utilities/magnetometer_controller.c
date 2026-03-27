#ifdef USE_MAGNETOMETER
#include <assert.h>
#include "communication_monitor.h"
#include "error_handler.h"
#include "algo_hw.h"
#include "magnetometer_controller.h"
#include "log.h"
#include "bsp.h"
#include "magnetometer.h"

#define MAX_REPEATS 2

#if defined(LIS2MDL) || defined(SIMULATED_MAG)
#define MIN_ODR   10.0f
#define MAX_ODR   100.0f
#endif

#if defined(LOG_MAGNETOMETER_DATA)
#define LogMag(...) PRINTF(__VA_ARGS__)
#else 
#define LogMag(...)
#endif

extern MAGNETO_Drv_t MagnetometerDrv;
extern DrvContextTypeDef handle;

static MagnetometerFailureCallback failureCallback;

bool readToCheck = false;

static bool actualLPFMode = false;
static bool desiredLPFMode = false;

static float actualODRFrequency = 0.0f;
static float desiredODRFrequency = 0.0f;

static uint16_t actualThreshold = 0;
static uint16_t desiredThreshold = 0;

static int16_t actualOffset = 0;
static int16_t desiredOffset = 0;
static SENSOR_AXIS_t axis = SENSOR_AXIS_X;

static SensorAxes_t lastValue;
static uint8_t repeats = 0;

static MAG_OPERATING_MODE_t actualOperatingMode = MAG_OPERATING_MODE_POWER_DOWN;
static MAG_OPERATING_MODE_t desiredOperatingMode = MAG_OPERATING_MODE_POWER_DOWN;

static bool interruptEnabled = false;
static INTERRUPT_SOURCE_t interruptSource;
static InterruptHandler interruptHandler;
static SENSOR_AXIS_t interruptAxis;
static bool UpdateInterrupt( void );
static bool EnableMagnetometer( void );
static void CheckLPFMode( void );
static void CheckThreshold( void );
static void CheckOffset( void );
static void CheckOperatingMode( void);
static bool UpdateOperatingMode( void );
static void CheckODRFrequency( void );

static bool CommunicationMonitorReset( void );
static void CommunicationMonitorFailure( void );

void magnetometer_controller_init( MagnetometerFailureCallback inputFailureCallback )
{
  assert(failureCallback == NULL);
  failureCallback = inputFailureCallback;

  Communication_Monitor_Init(CommunicationMonitorFailure, CommunicationMonitorReset);
}

void magnetometer_controller_check_in( void )
{
  CheckODRFrequency();
  CheckThreshold();
  CheckOffset();
  CheckLPFMode();
  CheckOperatingMode();
}

void magnetometer_controller_set_to_default( bool resetLPF )
{
  if (resetLPF)
  {
    desiredLPFMode = false;
  }
  desiredODRFrequency = 0.0f;
  desiredThreshold = 0;
  desiredOperatingMode = MAG_OPERATING_MODE_POWER_DOWN;
}

/**
 * @brief Reset the magnetometer and controller
 * 
 * @return true if successful
 * @return false if failure likely due to issue with connection
 */
bool magnetometer_controller_reset( void )
{
  PRINTF("Resetting Mag\n\r");
  // Power cycle mag
  magnetometer_reset();

  // Everything gets reset
  actualOperatingMode = MAG_OPERATING_MODE_POWER_DOWN;
  actualLPFMode = false;
  actualODRFrequency = 0.0f;
  actualThreshold = 0;

  if (!EnableMagnetometer()){
    return false;
  }
  
  magnetometer_controller_check_in();
  
  if (!UpdateInterrupt())
  {
    return false;
  }
  
  HAL_Delay(1000/MIN_ODR); // delay long enough that a new sample is taken even at 10Hz

  PRINTF("Magnetometer reset complete\n");
  return true;
}

bool magnetometer_controller_get_pin_status( void )
{
  bool status;
  MagnetometerDrv.Get_PIN_Status(&handle, &status);
  return status;
}

bool magnetometer_controller_get_drdy_status( bool *status )
{
  uint8_t stat = 0;
  if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Get_DRDY_Status(&handle, &stat)))
  {
    PRINTF("ERROR MAG get drdy status\n\r");
    return false;
  }
  *status = stat;
  return true;
}

bool magnetometer_controller_get_source(MAG_SOURCE_t *xAxis, MAG_SOURCE_t *yAxis, MAG_SOURCE_t *zAxis)
{
  if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Get_Source(&handle, xAxis, yAxis, zAxis)))
  {
    PRINTF("ERROR MAG get int source\n\r");
    return false;
  }
  return true;
}

bool magnetometer_controller_get_axis( SENSOR_AXIS_t axis, int16_t *value )
{
  if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Get_Axis(&handle, axis, value)))
  {
    PRINTF("ERROR MAG get axis\n\r");
    return false;
  }

  int16_t lastAxisValue;
  switch (axis)
  {
    case SENSOR_AXIS_X:
      lastAxisValue = lastValue.AXIS_X;
      break;
    case SENSOR_AXIS_Y:
      lastAxisValue = lastValue.AXIS_Y;
      break;
    case SENSOR_AXIS_Z:
      lastAxisValue = lastValue.AXIS_Z;
      break;
    default:
      return false;
  }

  if (lastAxisValue == *value)
  {
    repeats++;
    if (repeats > MAX_REPEATS)
    {
      repeats = 0;
      // Print out the last value
      LogMag("Repeat value %d\n", *value);
      magnetometer_controller_reset(); //ALGORITHM_ERROR_REPEAT_VALUE
      return false;
    }
  } else {
    repeats = 0;
  }

  switch(axis)
  {
    case SENSOR_AXIS_X:
      lastValue.AXIS_X = *value;
      break;
    case SENSOR_AXIS_Y:
      lastValue.AXIS_Y = *value;
      break;
    case SENSOR_AXIS_Z:
      lastValue.AXIS_Z = *value;
      break;
  }
  return true;
}

bool magnetometer_controller_get_axes( SensorAxes_t *axes )
{
  if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Get_Axes(&handle, axes)))
  {
    PRINTF("ERROR MAG get axes\n\r");
    return false;
  }

  // check if same as last value
  if (axes->AXIS_X == lastValue.AXIS_X && axes->AXIS_Y == lastValue.AXIS_Y && axes->AXIS_Z == lastValue.AXIS_Z)
  {
    repeats++;
    if (repeats > MAX_REPEATS)
    {
      repeats = 0;
      // Print out the last value
      LogMag("Repeat value %d,%d,%d\n", axes->AXIS_X, axes->AXIS_Y, axes->AXIS_Z);
      magnetometer_controller_reset(); //ALGORITHM_ERROR_REPEAT_VALUE
      return false;
    }
  } else {
    repeats = 0;
  }

  lastValue = *axes;
  return true;
}

bool magnetometer_controller_start_interrupt_drdy( InterruptHandler inputInterruptHandler )
{
  interruptEnabled = true;
  interruptSource = INTERRUPT_SOURCE_DRDY;
  interruptHandler = inputInterruptHandler;
  return UpdateInterrupt();
}

bool magnetometer_controller_start_interrupt_threshold( InterruptHandler inputInterruptHandler, SENSOR_AXIS_t inputInterruptAxis )
{
  interruptEnabled = true;
  interruptSource = INTERRUPT_SOURCE_INT;
  interruptHandler = inputInterruptHandler;
  interruptAxis = inputInterruptAxis;
  return UpdateInterrupt();
}

void magnetometer_controller_stop_interrupt( void )
{
  interruptEnabled = false;
  interruptHandler = NULL;
  magnetometer_stop_interrupt();
}

void magnetometer_controller_set_odr( float inputODR )
{
    // Check min and max freq
  if (inputODR < MIN_ODR) { inputODR = MIN_ODR; }; // Minimum frequency
  if (inputODR > MAX_ODR) { inputODR = MAX_ODR; };
  
  // ODR can only be updated in steps, so we don't want to constantly update ODR if it's not within a new range.

	#if defined(LIS2MDL) || defined(SIMULATED_MAG)
  float minUsableODR = MIN_ODR;
  if (desiredLPFMode)
  {
    minUsableODR = MAX_ODR;
  }

	desiredODRFrequency = 
						( inputODR <=  minUsableODR ) ? minUsableODR
          : ( inputODR >= MAX_ODR ) ? MAX_ODR
					: ( inputODR <=  10.0f ) ? 10.0f
					: ( inputODR <=  20.0f ) ? 20.0f 
					: ( inputODR <=  50.0f ) ? 50.0f
					: MAX_ODR;
	#endif

  if (desiredODRFrequency != actualODRFrequency)
  {
    PRINTF("ODR: %.2f -> %.2f, input %.2f\n", actualODRFrequency, desiredODRFrequency, inputODR);
  }
  
  CheckODRFrequency();
}

float magnetometer_controller_get_odr( void )
{
  return desiredODRFrequency;
}

void magnetometer_controller_set_interrupt_threshold( uint16_t threshold )
{
  desiredThreshold = threshold;
  CheckThreshold();
}

int16_t magnetometer_controller_get_offset( void )
{
  return actualOffset;
}

bool magnetometer_controller_set_offset( SENSOR_AXIS_t input_axis,  int16_t input_offset)
{
  if (desiredOffset == input_offset && axis == input_axis)
  {
    return false;
  }
  desiredOffset = input_offset;
  axis = input_axis;
  CheckOffset();
  return true;
}

void magnetometer_controller_set_lpf( bool lpfEnabled )
{
  desiredLPFMode = lpfEnabled;
  PRINTF("Set LPF %d\n", desiredLPFMode);
  CheckLPFMode();
}

bool magnetometer_controller_get_lpf( void )
{
  return desiredLPFMode;
}

bool magnetometer_controller_set_operating_mode( MAG_OPERATING_MODE_t mode )
{
  desiredOperatingMode = mode;
  return UpdateOperatingMode();
}

/**
 * @brief Tests the connection to the magnetometer
 * 
 * @return MAGNETOMETER_CONNECTION_t 
 * @note this changes the settings of the magnetometer
 */
MAGNETOMETER_CONNECTION_t magnetometer_controller_test_connection( void )
{
  // Reset to put in a good state
  if(!magnetometer_controller_reset())
  {
    PRINTF("ERROR failed to reset\n");
    return MAGNETOMETER_CONNECTION_FAILURE_REASON_I2C;
  }
  
  // Get the magnetometer to set the pin in low then high
  if (!magnetometer_controller_start_interrupt_drdy(NULL))
  {
    PRINTF("ERROR failed to start interrupt for int test\n");
    return MAGNETOMETER_CONNECTION_FAILURE_REASON_I2C;
  }
   
  bool inInterrupt;
  if (!magnetometer_controller_get_drdy_status(&inInterrupt))
  {
    PRINTF("ERROR Could not get source\n");
    return MAGNETOMETER_CONNECTION_FAILURE_REASON_I2C;
  }

  PRINTF_TS("Int pin status %d\n", inInterrupt);

  if (inInterrupt)
  {
    magnetometer_int_pin_pull_down();
  } else {
    magnetometer_int_pin_pull_up();
  }

  HAL_Delay(1); // Small delay to let pull to take hold

#ifndef SIMULATED_MAG // Could bring this back just need to implement DRDY one signal for simulated
  if (magnetometer_controller_get_pin_status() != inInterrupt)
  {
    PRINTF_TS("ERROR int pin test FAILED\n");
    return MAGNETOMETER_CONNECTION_FAILURE_REASON_INTERRUPT;
  }
#endif
  // clean up
  magnetometer_controller_stop_interrupt();
  magnetometer_int_pin_pull_down(); // Don't want to cause an issue with the magnetometer pin in high z
  
  return MAGNETOMETER_CONNECTION_NO_ISSUE;
}

static void CheckODRFrequency( void )
{
  if (desiredODRFrequency == actualODRFrequency) 
  {
    return;
  }

  if (!Communication_Monitor_HandleStatus(MagnetometerDrv.Set_ODR_Value(&handle, desiredODRFrequency))) {
    PRINTF("ERROR Failed to set ODR to %.2f\n\r", desiredODRFrequency);
    return;
  }
  
  PRINTF("ODR=%.2f\n\r", desiredODRFrequency);

  actualODRFrequency = desiredODRFrequency;
}

static void CheckThreshold( void )
{
  if (desiredThreshold == actualThreshold)
  {
    return;
  }
  
	float sensitivity;
	if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Get_Sensitivity(&handle, &sensitivity))) 
	{
		PRINTF("ERROR MAG get sensitivity FAILED\n\r");
		return;
	}
	
	uint16_t thresholdLSB = desiredThreshold / sensitivity;
  if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_Threshold(&handle, thresholdLSB)))
  {
    PRINTF("ERROR MAG set threshold FAILED\n\r");
		return;
  }
  
  if (readToCheck){
    // Tests with it commented out looked 100 uA better on power consumption
    //HAL_Delay(1); // Small delay to let set threshold take hold
    uint16_t readThreshold;
    if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Get_Threshold(&handle, &readThreshold)))
    {
      PRINTF("ERROR MAG get threshold FAILED\n\r");
      return;
    } else {
      if ( readThreshold == thresholdLSB)
      {
        actualThreshold = desiredThreshold;
        LogMag("Int update %d,%d\n", desiredThreshold);
      } else {
        PRINTF("ERROR failed to update thresh\n");
      }
    }
  } else {
    LogMag("Int update %d,%d\n", desiredThreshold);
    actualThreshold = desiredThreshold;
  }
}

static void CheckOffset( void )
{
  if (desiredOffset == actualOffset)
  {
    return;
  }
  
  float sensitivity;
  if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Get_Sensitivity(&handle, &sensitivity))) 
  {
    PRINTF("ERROR MAG get sensitivity FAILED\n\r");
    return;
  }
	
	int16_t offsetLSB = desiredOffset / sensitivity;
  if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_Offset(&handle, axis, offsetLSB)))
  {
    PRINTF("ERROR MAG set offset FAILED\n\r");
		return;
  }
  
  if (readToCheck)
  {
    // HAL_Delay(1); // Small delay to let set offset take hold
    int16_t realOffset;
    if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Get_Offset(&handle, axis, &realOffset)) )
    {
      PRINTF("ERROR MAG get offset FAILED\n\r");
      return;
    } else {
      if ( realOffset == offsetLSB)
      {
        actualOffset = desiredOffset;
        LogMag("Offset update %d,%d\n", desiredThreshold);
      } else {
        PRINTF("ERROR failed to update offset\n");
      }
    }
  } else {
    LogMag("Offset update %d,%d\n", desiredThreshold);
    actualOffset = desiredOffset;
  }
}

static void CheckLPFMode( void )
{
  if (desiredLPFMode == actualLPFMode)
  {
    return;
  }

  SENSOR_TOGGLE_t value = desiredLPFMode ? SENSOR_TOGGLE_ENABLE : SENSOR_TOGGLE_DISABLE;
  
  if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_LPF(&handle, value)))
  {
    PRINTF("ERROR Failed to set LPF\n");
    return;
  }

  PRINTF("LPF %d\n", desiredLPFMode);
  actualLPFMode = desiredLPFMode;
  return;
}

/**
  * Check the operating mode of the magnetometer
  * @note If the operating mode is single, it takes one measurements then goes to power down mode
  **/
static void CheckOperatingMode( void )
{
  if (desiredOperatingMode == actualOperatingMode)
  {
    return;
  }
  UpdateOperatingMode();
}

static bool UpdateOperatingMode( void )
{
  if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_Operating_Mode(&handle, desiredOperatingMode)))
  {
    PRINTF("ERROR Failed to set OpMode\n");
    return false;
  }

  //PRINTF("OpMode %d\n", desiredOperatingMode); // Hits a lot in single mode
  actualOperatingMode = desiredOperatingMode;
  return true;
}

static bool UpdateInterrupt( void )
{
  if (interruptEnabled)
  {
    if (interruptSource == INTERRUPT_SOURCE_DRDY)
    {
        if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_DRDY_on_PIN(&handle, SENSOR_TOGGLE_ENABLE )))
        {
          PRINTF("ERROR Failed EN DRDY\n");
          return false;
        }

        // Have to make sure Int is disabled or it overrides
        if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_INT_on_PIN(&handle, SENSOR_TOGGLE_DISABLE)))
        {
          PRINTF("ERROR MAG DIS INT on PIN\n\r");
          return false;
        }
        
        // Just interrupt on new data
        magnetometer_start_interrupt(interruptHandler, INTERRUPT_TRIGGER_RISING);
        PRINTF("Started int on DRDY\n");
    } else {
        if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_Interrupt_Source(&handle, interruptAxis)))
        {
          PRINTF("ERROR MAG set int source\n\r");
          return false;
        }
        
        if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_INT_on_PIN(&handle, SENSOR_TOGGLE_ENABLE)))
        {
          PRINTF("ERROR MAG EN INT on PIN\n\r");
          return false;
        }
        
#ifdef ALGORITHM_MIN_MAX
        // Configure to be always above the 0 axis with a tolerance
        magnetometer_start_interrupt(interruptHandler, INTERRUPT_TRIGGER_RISING_FALLING);
#else 
        magnetometer_start_interrupt(interruptHandler, INTERRUPT_TRIGGER_RISING);
#endif
        PRINTF("Started int on threshold axis:%d\n", interruptAxis);
    }
  } else 
  {
    if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_INT_on_PIN(&handle, SENSOR_TOGGLE_DISABLE)))
    {
      PRINTF("ERROR MAG set EN INT ON PIN\n\r");
      return false;
    }
    
    if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_DRDY_on_PIN( &handle, SENSOR_TOGGLE_DISABLE )))
    {
      PRINTF("ERROR Failed DIS DRDY\n");
      return false;
    }
    
    magnetometer_stop_interrupt();
    PRINTF("Stopped int\n");
  }
  return true;
}

static bool EnableMagnetometer( void )
{
  handle.isInitialized=0;
  handle.isEnabled=0;
	if (MagnetometerDrv.Init( &handle) != COMPONENT_OK)
  {
		PRINTF("ERROR Mag FAILED Initialize\n\r"); 
    return false;
	}

#if defined(ALGORITHM_MIN_MAX) || defined(ALGORITHM_POLL_AND_INT)
	// Configure to be always above the 0 axis with a tolerance
	if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_Offset(&handle, SENSOR_AXIS_X, INT16_MIN / 2)))
	{
		PRINTF("ERROR setting offsets\n");
		return false;
	}

	// Configure to be always above the 0 axis with a tolerance
	if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_Offset(&handle, SENSOR_AXIS_Y, INT16_MIN / 2)))
	{
		PRINTF("ERROR setting offsets\n");
		return false;
	}
  
  	// Configure to be always above the 0 axis with a tolerance
	if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Set_Offset(&handle, SENSOR_AXIS_Z, INT16_MIN / 2)))
	{
		PRINTF("ERROR setting offsets\n");
		return false;
	}
#endif

  if ( !Communication_Monitor_HandleStatus(MagnetometerDrv.Interrupt_Enable(&handle, SENSOR_TOGGLE_ENABLE)))
  {
    PRINTF("ERROR MAG set int enable\n\r");
    // If this fails a lot, it could just retry
    return false;
  }
  
  PRINTF("Mag initialized\n");
  return true;
}

static bool CommunicationMonitorReset( void )
{
  PRINTF("CommMonitor Reset\n");
  return magnetometer_controller_reset();
}

static void CommunicationMonitorFailure( void )
{
  failureCallback();
}
#endif
