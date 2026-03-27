#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "log_implementation.h"
#include "mocked_app.h"
#include "algo_hw.h"
#include "timeServer.h"
#include "communication_monitor.h"
#include "noise_monitor.h"
#include "cycle_counter.h"
#include "lowest_flow_over_period_tracker.h"
#include "simulated_MAG_driver_HL.h" // For get stats
#include "simulation.h"
// Algorithm inputs
#include "signal.h"
#include "algorithm.h"

#define RUNTIME 1000000
#define SPEED_MULTIPLIER 100

extern ALGORITHM_t Algorithm;

bool run = true;

TimerEvent_t timer;
void TimerEvent( void );

TimerEvent_t appTimer;
void AppTimerEvent( void );
bool appDataFlag = false;
bool appCheckInFlag = false;

void HandleCommunicationError( void )
{
    PRINTF_BASE("HandleCommunicationError\n");
}

bool HandleCommunicationReset( void )
{
    PRINTF_BASE("HandleCommunicationReset\n");
    return true;
}

int main( int argc, char *argv[] )
{
    // PRINTF_BASE("Program name %s len %d\n", argv[0], argc);
    float noiseAmplitude = atof(argv[1]);
    float signalFrequency = atof(argv[2]);
    float signalAmplitude = atof(argv[3]);


    printf("n %.0f sigFreq %.2f sigAmp %.1f", noiseAmplitude, signalFrequency, signalAmplitude);
    
    Signal_Config config = {
        .noiseAmplitude = noiseAmplitude,
        .signalFrequency = signalFrequency,
        .signalAmplitude = signalAmplitude,
        .pulseEnable = false,
        .fluctuateNoise = false,
        .popUp = false,
        .xScale = 1,
        .yScale = 1,
        .zScale = 1,
        .xInnerScale = 0.5,
        .yInnerScale = 0.5,
        .zInnerScale = 0.5,
        .offsetBump = false,
    };

    if (argc > 4)
        config.pulseEnable = atof(argv[4]);
        if (config.pulseEnable) printf(" pulsed");

    if (argc > 5)
        config.fluctuateNoise = atof(argv[5]);
        if (config.fluctuateNoise) printf(" fluct");
    
    if (argc > 6)
        config.popUp = atof(argv[6]);
        if(config.popUp) printf(" popUp");

    for (int i = 0; i < argc; i++) {
        if (strstr(argv[i], "-xScale=") == argv[i]) { // Check if the argument starts with "-xScale="
            sscanf(argv[i], "-xScale=%f", &config.xScale);   // Extract the float value
            printf("x: %.1f ", config.xScale);
        } else if(strstr(argv[i], "-yScale=") == argv[i]) { // Check if the argument starts with "-yScale="
            sscanf(argv[i], "-yScale=%f", &config.yScale);   // Extract the float value
            printf("y: %.1f ", config.yScale);
        } else if(strstr(argv[i], "-zScale=") == argv[i]) { // Check if the argument starts with "-zScale="
            sscanf(argv[i], "-zScale=%f", &config.zScale);   // Extract the float value
            printf("z: %.1f ", config.zScale);
        } else if (strstr(argv[i], "-offsetBump=") == argv[i] ) {
            if (strstr(argv[i], "-offsetBump=0") == argv[i]) {
                config.offsetBump = false;
            }
        } else if (strstr(argv[i], "-xInnerScale=") == argv[i]) {
            sscanf(argv[i], "-xInnerScale=%f", &config.xInnerScale);   // Extract the float value
            printf("xInner: %.1f ", config.xInnerScale);
        } else if (strstr(argv[i], "-yInnerScale=") == argv[i]) {
            sscanf(argv[i], "-yInnerScale=%f", &config.yInnerScale);   // Extract the float value
            printf("yInner: %.1f ", config.yInnerScale);
        } else if (strstr(argv[i], "-zInnerScale=") == argv[i]) {
            sscanf(argv[i], "-zInnerScale=%f", &config.zInnerScale);   // Extract the float value
            printf("zInner: %.1f ", config.zInnerScale);
        }
    }

    if (config.offsetBump)  printf(" bump\n");

    simulation_speed_multiplier(SPEED_MULTIPLIER);

    TimerInit( &timer, TimerEvent );
    TimerSetValue( &timer, RUNTIME);
    TimerStart( &timer );

    TimerInit( &appTimer, AppTimerEvent );
    TimerSetValue( &appTimer, 60000);
    TimerStart( &appTimer );

    Signal_Init(config);

    Algorithm.Init();

    Communication_Monitor_Init( HandleCommunicationError, HandleCommunicationReset );

    mocked_app_set_flag();

    while(true)
    {
        simulation_checkin();

        mocked_app_check_in();

        if (appDataFlag)
        {
            appDataFlag = false;
            TimerReset( &appTimer );
            TimerTime_t longestDurationBetweenPulsesMS = 0;
            float lowestPulseRate = 0;
            bool algoIsConfigured = true;

            if (cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDurationBetweenPulsesMS) )
            {
                // printk("Duration mS %d\n",longestDurationBetweenPulsesMS);
            }

            if (LowestFlowOverPeriodTracker_Get_LowestPulseRate(&lowestPulseRate))
            {
                // printk("lowestPulseRate: %.2f\n", lowestPulseRate);
            }
            
            cycle_counter_start_new_period();
            
            if (!Algorithm.Is_Configured())
            {
                algoIsConfigured = false;
            }

            uint32_t halfCycles = cycle_counter_get_half_cycles();
            // Pull data no matter to simulate the app
            // PRINTF_BASE("App Data");
            // PRINTF_BASE(" halfCycles %d", halfCycles);
            // PRINTF_BASE(" longestDurationBetweenPulsesMS %d", longestDurationBetweenPulsesMS);
            // PRINTF_BASE(" lowestPulseRate %.2f", lowestPulseRate);
            // PRINTF_BASE(" algoIsConfigured %d\n", algoIsConfigured);
        }

        if (!run){
            float actualHalfCycles = Signal_Get_HalfCycles();
            uint16_t estimatedHalfCycles = cycle_counter_get_half_cycles();
            uint16_t xNoiseAmplitude, yNoiseAmplitude, zNoiseAmplitude;
            Noise_Monitor_GetNoise(&xNoiseAmplitude, &yNoiseAmplitude, &zNoiseAmplitude);

            uint32_t reads = GetStats();
            bool lpfEnabled = Signal_Get_LPF();
            uint32_t magSamples = Signal_Get_NumSamples();
            float accuracy = estimatedHalfCycles == 0 && actualHalfCycles == 0 ? 1 : (float)estimatedHalfCycles / (float)actualHalfCycles;
            PRINTF_BASE("\t lpf %d \tnoise: %d,%d,%d \thc_est:%d \thc_act:%.1f \tacc: %.0f%% \treads: %d \tsamples %d alerts:%d\n",lpfEnabled, xNoiseAmplitude, yNoiseAmplitude, zNoiseAmplitude, estimatedHalfCycles, actualHalfCycles, accuracy*100, reads, magSamples, get_alert_count());
            // Calculate battery life

            // Numbers taken from LoRa STML072, Cellular has about 1/4 power consumption.
            float powerFromMag = magSamples * 2.74e-9; // Wh
            float powerFromReads = reads *  21e-9; // Wh
            float baselineUsage = RUNTIME * 6.42e-12; // Wh
            float powerUsed = powerFromMag + powerFromReads + baselineUsage;
            float yearlyPowerUsed = powerUsed * 1000 * 60 * 60 * 24 * 365 / RUNTIME; // wh * ms/year/ms = wh/year
            // 28.05wh = 3.3v * 8.5Ah 
            // 28.05wh / 1
            float batteryCapacityWh = 28.05; // Battery capacity in watt-hours
            float batteryLife = batteryCapacityWh / yearlyPowerUsed;
            PRINTF_BASE("\tBL: %.1f\n", batteryLife);

            return 0;
        }
    }
}

void TimerEvent( void )
{
    run = false;
}

void AppTimerEvent( void )
{
    appDataFlag = true;
}
