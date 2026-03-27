
#ifndef LOWEST_FLOW_OVER_PERIOD_TRACKER_H
#define LOWEST_FLOW_OVER_PERIOD_TRACKER_H

#include <stdint.h>
#include <stdbool.h>

void LowestFlowOverPeriodTracker_Init( void );

void LowestFlowOverPeriodTracker_StartNewPeriod( void );

void LowestFlowOverPeriodTracker_NewPulse( void );

void LowestFlowOverPeriodTracker_Reset( void );

/**
 *  @returns true if successful
**/
bool LowestFlowOverPeriodTracker_Get_LowestPulseRate( float* lowestPulseRatePerMin );

/**
 * @returns seconds
**/
uint16_t LowestFlowOverPeriodTracker_Get_BucketDuration( void );

void LowestFlowOverPeriodTracker_Set_BucketDuration( uint16_t seconds );
/**
 * @returns seconds
**/
uint16_t LowestFlowOverPeriodTracker_Get_NumberOfBuckets( void );

void LowestFlowOverPeriodTracker_Set_NumberOfBuckets( uint16_t inputNumberOfBuckets );

#endif
