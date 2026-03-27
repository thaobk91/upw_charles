#include "magnetometer.h"
#include "signal.h"

void magnetometer_start_interrupt( MagnetometerInterruptHandler interruptHandler, HW_INTERRUPT_TRIGGER_t interruptMode )
{
  HW_GPIO_SetInterruptOnPin(interruptMode, interruptHandler);
}

void magnetometer_stop_interrupt( void )
{
  Signal_Stop_Interrupt();
}

void magnetometer_reset( void)
{
  // Reset mag to default
}

void magnetometer_reset_comms( void )
{
  
}

bool magnetometer_test_connection( void )
{
  return true;
}

/** Returns: True if successful **/
bool magnetometer_int_pin_pull_up( void )
{
  return true;
}

/** Returns: True if successful **/
bool magnetometer_int_pin_no_pull( void )
{
  return true;
}

/** Returns: True if successful **/
bool magnetometer_int_pin_pull_down( void )
{
  return true;
}

/** Returns: True if successful **/
bool magnetometer_int_pin_disable( void );