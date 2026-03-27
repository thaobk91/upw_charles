#ifndef __NOISE_MONITOR_H
#define __NOISE_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*NoiseMonitorErrorCallback)( uint16_t restarts );

void Noise_Monitor_Init( NoiseMonitorErrorCallback callback );

bool Noise_Monitor_DeInit( void );

void Noise_Monitor_Start( bool enableLPF) ;

void Noise_Monitor_Stop( void );

bool Noise_Monitor_CheckIn( void );

void Noise_Monitor_GetNoise( uint16_t *xNoise, uint16_t *yNoise, uint16_t *zNoise );

#endif // __NOISE_MONITOR_H
