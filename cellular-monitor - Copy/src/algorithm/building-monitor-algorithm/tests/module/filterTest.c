#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "log.h"
#include "algo_hw.h"
#include "timeServer.h"

// Algorithm inputs
#include "signal.h"

#include "so_hpf.h"
#include "so_butterworth_hpf.h"
#include "FIRFilter.h"
#include "simulation.h"

bool run = true;

#define RUNTIME 8000
#define START_SAMPLE 10
#define MAX_SAMPLES 30
#define SPEED_MULTIPLIER 100

TimerEvent_t timer;
void TimerEvent( void );

TimerEvent_t signalTimer;
void SignalTimerEvent( void );
float min = 1000000.0f;
float max = -1000000.0f;
float firMin = 1000000.0f;
float firMax = -1000000.0f;
uint16_t samples = 0;

FIRFilter filterContext;

int main( int argc, char *argv[] )
{
    //PRINTF_BASE("Program name %s\n", argv[0]);
    float noiseAmplitude = atof(argv[1]);
    float signalFrequency = atof(argv[2]);
    float signalAmplitude = atof(argv[3]);
    float samplingFrequency = atof(argv[4]);

    printf("noiseAmplitude %.1f signalFrequency %.3f signalAmplitude %.2f samplingFrequency %.2f  ", noiseAmplitude, signalFrequency, signalAmplitude, samplingFrequency);
    
    HW_RTC_SetMultiplier(SPEED_MULTIPLIER);

    FIRFilter_init(&filterContext);
    float cutoffFrequency = 45.0f;
    // so_hpf_calculate_coeffs(0.707, cutoffFrequency, samplingFrequency); // Q=0.707 is to make it a Butterworth filter
    so_butterworth_hpf_calculate_coeffs(cutoffFrequency, samplingFrequency);
    TimerInit( &timer, TimerEvent );
    TimerSetValue( &timer, RUNTIME);
    TimerStart( &timer );

    TimerInit( &signalTimer, SignalTimerEvent );
    TimerSetValue( &signalTimer, 1000 / samplingFrequency );
    TimerStart( &signalTimer );

    Signal_Config config = {
        .noiseAmplitude = noiseAmplitude,
        .signalFrequency = signalFrequency,
        .signalAmplitude = signalAmplitude,
        .pulseEnable = false,
        .fluctuateNoise = false,
        .popUp = false,
    };
    Signal_Init(config);
    Signal_Set_ODR(samplingFrequency);
    Signal_Set_Offset(1000);
    
    while( MAX_SAMPLES > samples)
    // while(HW_RTC_GetTimerValue_MS() < RUNTIME)
    {
        simulation_checkin();
    }
    printf("\n");
    printf("min\t %f max\t %f\n", min, max);
    // printf("firMin\t %f firMax\t %f\n", firMin, firMax);
}

void SignalTimerEvent( void )
{
    samples++;
    TimerReset( &signalTimer);
    SensorAxes_t signal = Signal_Get_Value();
    FIRFilter_put(&filterContext, signal.AXIS_Z);
    // int16_t firValue = FIRFilter_get(&filterContext);

    // if (firValue < firMin)
    //     firMin = firValue;
    // if (firValue > firMax)
    //     firMax = firValue;
    // int16_t filteredValue = so_hpf_filter(signal.AXIS_Z);
    int16_t filteredValue = so_butterworth_hpf_filter(signal.AXIS_Z);
    if (samples > START_SAMPLE)
    {
        if (filteredValue < min)
            min = filteredValue;
        if (filteredValue > max)
            max = filteredValue;
    }

    printf("[%d] %d %d\n",HW_RTC_GetTimerValue_MS() , signal.AXIS_Z, filteredValue);
}

void TimerEvent( void )
{
    run = false;
}
