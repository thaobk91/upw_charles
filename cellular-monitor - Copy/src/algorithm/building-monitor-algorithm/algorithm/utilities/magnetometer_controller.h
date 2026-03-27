
#ifndef __MAGNETOMETER_CONTROLLER_H__
#define __MAGNETOMETER_CONTROLLER_H__

#include <stdint.h>
#include "magnetometer.h"

typedef enum {
    INTERRUPT_SOURCE_INT,
    INTERRUPT_SOURCE_DRDY
} INTERRUPT_SOURCE_t;

typedef enum {
    MAGNETOMETER_CONNECTION_NO_ISSUE = 0,
    MAGNETOMETER_CONNECTION_FAILURE_REASON_INTERRUPT = -1,
    MAGNETOMETER_CONNECTION_FAILURE_REASON_I2C = -2
} MAGNETOMETER_CONNECTION_t;

typedef void (*MagnetometerFailureCallback)( void );

typedef void (*InterruptHandler)(void);

void magnetometer_controller_init( MagnetometerFailureCallback failureCallback);

void magnetometer_controller_check_in( void );

void magnetometer_controller_set_to_default( bool resetLPF );

bool magnetometer_controller_reset( void );

bool magnetometer_controller_get_pin_status( void );

bool magnetometer_controller_get_source(MAG_SOURCE_t *xAxis, MAG_SOURCE_t *yAxis, MAG_SOURCE_t *zAxis);

bool magnetometer_controller_get_axis( SENSOR_AXIS_t axis, int16_t *value );

bool magnetometer_controller_get_axes( SensorAxes_t *axis);

bool magnetometer_controller_start_interrupt_threshold( void (*interruptHandler)(void), SENSOR_AXIS_t inputInterruptAxis );

bool magnetometer_controller_start_interrupt_drdy( void (*interruptHandler)(void) );

void magnetometer_controller_stop_interrupt( void );

void magnetometer_controller_set_odr( float value );

void magnetometer_controller_set_interrupt_threshold( uint16_t threshold );

/**
 * \brief Sets the offset for the magnetometer
 * \param [IN] axis the axis to set the offset for  
 * \param [IN] offset the offset to set
 * \return true if the offset was set and there as a change, false if the offset was already set
 */
bool magnetometer_controller_set_offset( SENSOR_AXIS_t axis, int16_t offset );

float magnetometer_controller_get_odr( void );

int16_t magnetometer_controller_get_offset( void );

void magnetometer_controller_set_lpf( bool lpfEnabled );

bool magnetometer_controller_get_lpf( void );

bool magnetometer_controller_set_operating_mode( MAG_OPERATING_MODE_t mode );

bool magnetometer_controller_get_drdy_status( bool *status );

/**
 * @brief Test if interrupt pin is connected, the interrupt pin should be pulled HIGH before calling this function
 * 
 * @return MAGNETOMETER_CONNECTION_t
 */
MAGNETOMETER_CONNECTION_t magnetometer_controller_test_connection( void );

#endif
