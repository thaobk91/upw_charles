#ifndef __POLLING_ALGORITHM_H
#define __POLLING_ALGORITHM_H

#include <stdint.h>
#include <stdbool.h>
#include "algorithm.h"

typedef void (*AlgorithmErrorCallback)(int16_t data[], uint8_t size);

void Polling_Algorithm_Init( uint16_t noiseAmplitude, AlgorithmErrorCallback inputCallback);

void Polling_Algorithm_DeInit( void );

void Polling_Algorithm_CheckIn( void );

void Polling_Algorithm_Log( void );

void Polling_Algorithm_Get_Data(int16_t data[], uint8_t *size);

#endif // __POLLING_ALGORITHM_H
