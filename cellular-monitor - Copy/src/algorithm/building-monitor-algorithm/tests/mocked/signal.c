#include <stdlib.h>
#include <math.h>
#include <time.h> // Seeding rand

#include "log.h" 
#include "algo_hw.h"
#include "signal.h"
#include "timeServer.h"
#include "magnetometer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const int32_t xDefaultOffset = 1000;
static const int32_t yDefaultOffset = 1000;
static const int32_t zDefaultOffset = 1000;

static int32_t xOffset = 0;
static int32_t yOffset = 0;
static int32_t zOffset = 0;
static float xScale = 1;
static float yScale = 1;
static float zScale = 1;
static float xInnerScale = 0.5;
static float yInnerScale = 0.5;
static float zInnerScale = 0.5;

static SENSOR_AXIS_t interruptAxis;
static bool interruptState = false;
static uint16_t threshold = 0;
static bool DRDY_on_PIN = false;
static bool INT_on_PIN = false;
static MAG_SOURCE_t xState = MAG_SOURCE_NONE;
static MAG_SOURCE_t yState = MAG_SOURCE_NONE;
static MAG_SOURCE_t zState = MAG_SOURCE_NONE;

static bool interruptEnabled = false;
static HW_INTERRUPT_TRIGGER_t interruptMode;
static void (*interruptHandler)(void);
static int32_t amplitude = 0;
static float frequency = 0;
static float noiseFrequency = 60;
static float noiseAmplitude = 0;
static float noiseReducer = 1;
static bool lpfEnable = false;
static SensorAxes_t lastSample;
static bool continuousEnable = true;
static float baselineNoiseAmplitude = 30;

static bool popupEnable = true;
static float percentPopUp = 10; // 10%
static float percentOffsetBump = 10; // 10%
static bool pulseEnable = false;
static bool fluctuateNoise = false;
static bool offsetBump = false;

static float samplingFrequency = 0;
static TimerEvent_t samplingTimer;
static void SamplingTimerEvent( void );

static TimerEvent_t noiseTimer;
void NoiseTimerEvent( void );

static uint32_t signalTime = 0;
static uint32_t magSamples = 0;
static float lastSampleTime = 0;
static const uint32_t pulsePeriod = 70000;
static const uint32_t noisePeriod = 30000;// Needs to be longer than time to notice decreased noise which is 20 seconds
static float noiseFluctuatingAmplitude = 1.0f;

void Signal_Init(Signal_Config config)
{
    PRINTF("Signal_Init %d\n", config.pulseEnable);
    amplitude = config.signalAmplitude;
    frequency = config.signalFrequency;
    noiseAmplitude = config.noiseAmplitude;
    pulseEnable = config.pulseEnable;
    fluctuateNoise = config.fluctuateNoise;
    popupEnable = config.popUp;
    if (fluctuateNoise) {
        noiseFluctuatingAmplitude = 0; // start at 0 to mimic worst case
    }
    xScale = config.xScale;
    yScale = config.yScale;
    zScale = config.zScale;
    xInnerScale = config.xInnerScale;
    yInnerScale = config.yInnerScale;
    zInnerScale = config.zInnerScale;
    offsetBump = config.offsetBump;
    srand(time(NULL));

    TimerInit( &samplingTimer, SamplingTimerEvent );
    if (fluctuateNoise) {
        TimerInit( &noiseTimer, NoiseTimerEvent );
        TimerSetValue( &noiseTimer, noisePeriod );
        TimerStart( &noiseTimer );
    }
}

SensorAxes_t Signal_Get_Value(void) 
{
    return lastSample;
}

float Signal_Get_HalfCycles( void ) {
    // printf("f %.2f t %d\n", frequency, signalTime);
    return 2.0f * frequency * signalTime / 1000.0f;;
}

uint32_t Signal_Get_NumSamples( void )
{
    return magSamples;
}

void Signal_Set_LPF( bool inputLPFEnable )
{
    // printf("Signal LPF %d\n", inputLPFEnable);
    lpfEnable = inputLPFEnable;
    if (lpfEnable)
    {
        noiseReducer = 0.3;
    } else {
        noiseReducer = 1;
    }
}

bool Signal_Get_LPF( void )
{
    return lpfEnable;
}

void Signal_Set_ODR(float odr )
{
    PRINTF("ODR %f\n", odr);
    samplingFrequency = odr; 
    TimerSetValue( &samplingTimer, 1000 / samplingFrequency );
    TimerStart( &samplingTimer );
}

void Signal_Set_Interrupt_Axis(SENSOR_AXIS_t axis)
{
    interruptAxis = axis;
}

void Signal_Get_Source(MAG_SOURCE_t *xAxis, MAG_SOURCE_t *yAxis, MAG_SOURCE_t *zAxis)
{
    // Should be separate per
    *xAxis = xState;
    *yAxis = yState;
    *zAxis = zState;
}

void Signal_Set_Offset(SENSOR_AXIS_t axis, int16_t inputOffset)
{
    switch(axis)
    {
        case SENSOR_AXIS_X:
            xOffset = inputOffset;
            break;
        case SENSOR_AXIS_Y:
            yOffset = inputOffset;
            break;
        case SENSOR_AXIS_Z:
            zOffset = inputOffset;
            break;
        default:
            PRINTF("ERROR bad axis %d\n", axis);
            while(true){
                // fail
            }
    }
}

void Signal_Get_Offset(SENSOR_AXIS_t axis, int16_t *offset )
{
    switch(axis)
    {
        case SENSOR_AXIS_X:
            *offset = xOffset;
            break;
        case SENSOR_AXIS_Y:
            *offset = yOffset;
            break;
        case SENSOR_AXIS_Z:
            *offset = zOffset;
            break;
        default:
            PRINTF("ERROR bad axis %d\n", axis);
            while(true){
                // fail
            }
    }

}

void Signal_Set_Threshold(uint16_t inputThreshold)
{
    threshold = inputThreshold;
}

void Signal_Get_Threshold(uint16_t *inputThreshold)
{
    *inputThreshold = threshold;
}

void Signal_Set_DRDY_on_PIN(bool enabled)
{
    DRDY_on_PIN = enabled;
}

void Signal_Set_INT_on_PIN( bool enabled )
{
    INT_on_PIN = enabled;
}

void Signal_Get_Pin_Status(bool *isSet)
{
    *isSet = INT_on_PIN && (xState != MAG_SOURCE_NONE || yState != MAG_SOURCE_NONE || zState != MAG_SOURCE_NONE);
}

void Signal_Set_Interrupt_Enabled( bool enabled )
{
    PRINTF("Interrupt enabled %d\n", enabled);
    interruptEnabled = enabled;
}

void Signal_Stop_Interrupt( void )
{
    PRINTF("Stop interrupt\n");
    interruptEnabled = false;
    interruptHandler = NULL;
    interruptMode = 0;
}

void Signal_Set_InterruptHandler(void (*inputInterruptHandler)(void), HW_INTERRUPT_TRIGGER_t mode)
{
    interruptEnabled = true;
    if (mode != 0 && mode != INTERRUPT_TRIGGER_RISING_FALLING && mode != INTERRUPT_TRIGGER_RISING && mode != INTERRUPT_TRIGGER_FALLING)
    {
        PRINTF("ERROR bad mode %d\n", mode);
        while(true){
            // fail
        }
    }
    interruptMode = mode;

    interruptHandler = inputInterruptHandler;
}

void Signal_Set_Continuous( bool inputContinuousEnable)
{
    continuousEnable = inputContinuousEnable;
    if (continuousEnable)
    {
        TimerSetValue( &samplingTimer, 1000 / samplingFrequency );
        TimerStart( &samplingTimer );
    } else 
    {
        TimerSetValue( &samplingTimer, 6); // some amount of delay
        TimerStart( &samplingTimer ); 
    }
}

static void Take_Sample(void)
{
    // PRINTF_BASE("Take_Sample\n");
    uint32_t currentMs = HW_RTC_GetTimerValue_MS();
    magSamples++;
    if (pulseEnable)
    {
        // Signal stops for 1 second every 2 seconds on the even seconds
        if (currentMs % pulsePeriod > pulsePeriod / 2)
        {
            // don't increment signalTime
            // PRINTF_BASE("Pulse at %d\n", currentMs);
            lastSampleTime = 0;
            
        } else {
            // from the start of the most recent on time
            if (lastSampleTime == 0)
            {
                lastSampleTime = currentMs - (currentMs % 2000);
            }
            signalTime += (currentMs - lastSampleTime);
            lastSampleTime = currentMs;
        }
    } else {
        signalTime = currentMs;
    }

    // Get x value from current time and frequency
    float t = frequency * (signalTime / 1000.0) * 2 * M_PI; // radians
    
    // Get the signal scale for the current phase
    // Each axis should switch to inner scale every other 2π cycle when it crosses zero
    float xSignalScale = 1.0f;
    float ySignalScale = 1.0f;
    float zSignalScale = 1.0f;
    
    // X-axis uses cosine, so it crosses zero at π/2 and 3π/2
    // We need to determine which 2π cycle we're in, accounting for the π/2 offset
    // The cycle starts when cosine crosses zero (at π/2)
    uint32_t xCycle = (uint32_t)((t - M_PI/2) / (2 * M_PI));
    if (xCycle % 2 == 1) {
        xSignalScale = xInnerScale;
    }
    
    // Y-axis uses sine with π phase shift (sin(t + π) = -sin(t))
    // This crosses zero at the same points as regular sine (0 and π)
    // The cycle starts when sine crosses zero (at 0)
    uint32_t yCycle = (uint32_t)(t / (2 * M_PI));
    if (yCycle % 2 == 1) {
        ySignalScale = yInnerScale;
    }
    
    // Z-axis uses sine, so it crosses zero at 0 and π
    // The cycle starts when sine crosses zero (at 0)
    uint32_t zCycle = (uint32_t)(t / (2 * M_PI));
    if (zCycle % 2 == 1) {
        zSignalScale = zInnerScale;
    }

    float x = 0;
    float y = 0;
    float z = 0;
    
    uint32_t cycles = ceil((t / (2*M_PI)));
    float signalAmplitude = amplitude;
    if (popupEnable)
    {
        if (( cycles % 100) > (100 - percentPopUp))
        {
            signalAmplitude = signalAmplitude * 1.5;
        }
    }

    if (offsetBump)
    {
        // Using 150 to differ from popup
        if ((cycles % 150 ) > (150 - (percentOffsetBump * 1.5)))
        {
            x += 500;
            y += 500;
            z += 500;
        }
    }

    // Y and Z are opposites X is 90 degrees out of phase
    // Divide by 2 to get the amplitude of the signal
    z += zSignalScale * signalAmplitude * sin(t) / 2;
    x += xSignalScale * signalAmplitude * cos(t) / 2; // cos because out of phase 90
    y += ySignalScale * signalAmplitude * sin(t + M_PI) / 2; // z and y are usually opposite

    // Add noise
    float noise = noiseReducer * noiseFluctuatingAmplitude * noiseAmplitude * cos(2 * M_PI * noiseFrequency * currentMs / 1000.0) / 2;
    x += noise;
    y += noise;
    z += noise;

    // Baseline noise
    float random = -0.5 + (float)rand() / (float)RAND_MAX;
    // PRINTF("Random noise: %f\n", random);
    x += baselineNoiseAmplitude * random;
    y += baselineNoiseAmplitude * random;
    z += baselineNoiseAmplitude * random;
    // printf("%d, %.2f\n", currentMs, x);

    // Add offsets after other manipulations
    x = (xScale * x) - xOffset + xDefaultOffset;
    y = (yScale * y) - yOffset + yDefaultOffset;
    z = (zScale * z) - zOffset + zDefaultOffset;

    bool fireInterrupt = false;
    if (interruptEnabled) {
        int16_t interruptValue;
        MAG_SOURCE_t *interruptSource;
        switch(interruptAxis)
        {
            case SENSOR_AXIS_X:
                interruptValue = x;
                interruptSource = &xState;
                break;
            case SENSOR_AXIS_Y:
                interruptValue = y;
                interruptSource = &yState;
                break;
            case SENSOR_AXIS_Z:
                interruptValue = z;
                interruptSource = &zState;
                break;
            default:
                PRINTF("ERROR bad axis %d\n", interruptAxis);
                while(true);
        }
        // PRINTF("interruptEnabled: %d, INT_on_PIN: %d, threshold: %d,x: %.1f,y: %.1f,z: %.1f, interruptSource: %d\n", interruptEnabled, INT_on_PIN, threshold, x,y,z, interruptSource);

        if ( INT_on_PIN && threshold != 0)
        {
            MAG_SOURCE_t nextState;
            if (interruptValue > threshold)
            {
                nextState = MAG_SOURCE_POSITIVE;
            } 
            else if (interruptValue < -threshold)
            {
                nextState = MAG_SOURCE_NEGATIVE;
            } 
            else {
                nextState = MAG_SOURCE_NONE;
            }

            if (nextState != *interruptSource)
            {
                bool newInterruptState = nextState != MAG_SOURCE_NONE;

                if (newInterruptState != interruptState)
                {
                    // Change of pin state
                    interruptState = newInterruptState;
                    
                    if (interruptState &&  // Edge is Rising
                        (interruptMode == INTERRUPT_TRIGGER_RISING || interruptMode == INTERRUPT_TRIGGER_RISING_FALLING)) {
                        fireInterrupt = true;
                    } else if (!interruptState && // Edge is Falling
                        (interruptMode == INTERRUPT_TRIGGER_FALLING || interruptMode == INTERRUPT_TRIGGER_RISING_FALLING)) {
                        fireInterrupt = true;
                    }
                }
            }
            *interruptSource = nextState;
        } else if (!INT_on_PIN && DRDY_on_PIN)
        {
            fireInterrupt = true;
        }
    }
    
    SensorAxes_t value;
    value.AXIS_X = x;
    value.AXIS_Y = y;
    value.AXIS_Z = z;
    // PRINTF_TS("X: %.2f Y: %.2f Z: %.2f act:%.1f\n", x, y, z, Signal_Get_HalfCycles() );
    // PRINTF_TS("True X: %.2f Y: %.2f Z: %.2f act:%.1f\n", x + xOffset, y + yOffset, z + zOffset, Signal_Get_HalfCycles() );

    lastSample = value;

    // Fire interrupt last once data is updated
    if (fireInterrupt)
    {
        if (interruptHandler != NULL)
        {
            // PRINTF("interruptEnabled: %d, INT_on_PIN: %d, threshold: %d, x: %.1f\n", interruptEnabled, INT_on_PIN, threshold, x);
            interruptHandler();
        } else {
            PRINTF("WARN No interrupt handler\n");
        }
    }
}

static void SamplingTimerEvent( void )
{
    Take_Sample();
    if (continuousEnable)
    {
        TimerSetValue( &samplingTimer, 1000 / samplingFrequency );
        TimerStart( &samplingTimer );
    }
}

static bool rising = false;
void NoiseTimerEvent( void )
{
    // random fluctuating noise
    //noiseFluctuatingAmplitude = (float)rand() / (float)RAND_MAX;

    // slowly increasing and decreasing noise
    if (noiseFluctuatingAmplitude >= 1.0) {
        rising = false;
    } else if (noiseFluctuatingAmplitude <= 0.0) {
        rising = true;
    }

    if (rising) {
        noiseFluctuatingAmplitude += 0.1;
    } else {
        noiseFluctuatingAmplitude -= 0.1;
    }

    PRINTF("New Noise %.2f\n", noiseFluctuatingAmplitude);
    TimerReset( &noiseTimer );
}