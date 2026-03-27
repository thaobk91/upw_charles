#ifndef __SIGNAL_H
#define __SIGNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "algo_hw.h"
#include "sensor.h"
#include "magnetometer.h"

typedef struct {
    float noiseAmplitude;
    float signalFrequency;
    float signalAmplitude;
    bool pulseEnable;
    bool fluctuateNoise;
    bool popUp;
    float xScale;
    float yScale;
    float zScale;
    float xInnerScale;
    float yInnerScale;
    float zInnerScale;
    bool offsetBump;
} Signal_Config;

void Signal_Init(Signal_Config config);

SensorAxes_t Signal_Get_Value(void);

float Signal_Get_HalfCycles(void);

uint32_t Signal_Get_NumSamples( void );

bool Signal_Get_LPF( void );
void Signal_Set_LPF( bool lpfEnable );

void Signal_Set_ODR(float odr );

void Signal_Get_Source(MAG_SOURCE_t *xAxis, MAG_SOURCE_t *yAxis, MAG_SOURCE_t *zAxis);

void Signal_Set_Interrupt_Axis(SENSOR_AXIS_t axis);
void Signal_Set_Offset(SENSOR_AXIS_t axis, int16_t offset );
void Signal_Get_Offset(SENSOR_AXIS_t axis, int16_t *offset );

void Signal_Stop_Interrupt( void );
void Signal_Set_Interrupt_Enabled( bool enabled );
void Signal_Set_InterruptHandler( void (*handler)(void), HW_INTERRUPT_TRIGGER_t trigger);
void Signal_Set_Threshold(uint16_t threshold);
void Signal_Get_Threshold(uint16_t *threshold);

void Signal_Set_Continuous( bool continuousEnable);
void Signal_Get_Single( void );

void Signal_Set_DRDY_on_PIN(bool enabled);
void Signal_Set_INT_on_PIN( bool enabled );

void Signal_Get_Pin_Status(bool *isSet);

#endif // __SIGNAL_H
