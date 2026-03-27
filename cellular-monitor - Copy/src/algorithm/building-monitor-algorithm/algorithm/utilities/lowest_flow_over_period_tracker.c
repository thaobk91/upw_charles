
// Includes
#include <math.h>
#include <string.h>
#include "lowest_flow_over_period_tracker.h"
#include "timeServer.h"
#include "log.h"
#include "flash_utility.h"

#define PULSES_PER_CYCLE  2 // Defined in algorithm right now
#define DEFAULT_NUMBER_BUCKETS  10*60// 10 min @ 1 second per bucket
#define DEFAULT_BUCKET_DURATION_SECONDS 1

// Private Variables
#define MAX_BUCKETS   15 * 60 // 10 minutes at 60 second buckets
static uint16_t bucketDurationSeconds = DEFAULT_BUCKET_DURATION_SECONDS;
static uint16_t numberBuckets = DEFAULT_NUMBER_BUCKETS;

static TimerTime_t startTime;
static uint16_t lowestPulses = UINT16_MAX;
static uint16_t periodBuckets[MAX_BUCKETS] = {0};
static uint16_t lastIndex = 0;

static void GoToCurrentIndex( void );

void LowestFlowOverPeriodTracker_Init( void )
{
  FlashUtility_Load_LowestFlowOverPeriodTrackerData( &numberBuckets, &bucketDurationSeconds);
  LowestFlowOverPeriodTracker_Reset();
}

void LowestFlowOverPeriodTracker_Reset( void )
{
  memset(periodBuckets, 0, MAX_BUCKETS); 
  lastIndex = 0;
  lowestPulses = UINT16_MAX;
  startTime = TimerGetCurrentTime();
}

void LowestFlowOverPeriodTracker_StartNewPeriod( void )
{
  lowestPulses = UINT16_MAX;
}

void LowestFlowOverPeriodTracker_NewPulse( void )
{
  GoToCurrentIndex();
  // last index is now current index
  uint16_t currentArrayIndex = lastIndex % numberBuckets;
  periodBuckets[currentArrayIndex]++;
}

bool LowestFlowOverPeriodTracker_Get_LowestPulseRate( float* lowestPulseRatePerMin )
{
  // Go to current index to get lowest up til now
  GoToCurrentIndex();
  TimerTime_t elapsedTime = TimerGetElapsedTime(startTime);
  if (elapsedTime < numberBuckets * bucketDurationSeconds * 1000)
  {
    PRINTF_TS("not enough time lapsed %d\n", elapsedTime);
    return false;
  }

  *lowestPulseRatePerMin = lowestPulses * 60.0f / (numberBuckets * bucketDurationSeconds * PULSES_PER_CYCLE);
  return true;
}

/**
 * @returns seconds
*/
uint16_t LowestFlowOverPeriodTracker_Get_BucketDuration( void )
{
  return bucketDurationSeconds;
}

void LowestFlowOverPeriodTracker_Set_BucketDuration( uint16_t seconds )
{
  if (seconds > 600)
  {
    PRINTF_BASE("ERROR bucket duration %d exceeds max %d\n", seconds, seconds);
    return;
  } else if (seconds == 0)
  {
    PRINTF_BASE("ERROR bucket duration cannot be zero\n");
    return;
  }
  
  bucketDurationSeconds = seconds;
  FlashUtility_Save_LowestFlowOverPeriodTrackerData(numberBuckets, bucketDurationSeconds);
}

/**
 * @returns seconds
*/
uint16_t LowestFlowOverPeriodTracker_Get_NumberOfBuckets( void )
{
  return bucketDurationSeconds;
}

void LowestFlowOverPeriodTracker_Set_NumberOfBuckets( uint16_t inputNumberOfBuckets )
{
  if (inputNumberOfBuckets > MAX_BUCKETS)
  {
    PRINTF_BASE("ERROR num buckets %d exceeds max %d\n", inputNumberOfBuckets, MAX_BUCKETS);
    return;
  } else if (inputNumberOfBuckets == 0) {
    PRINTF_BASE("ERROR number of buckets cannot be zero");
  }
  numberBuckets = inputNumberOfBuckets;
  FlashUtility_Save_LowestFlowOverPeriodTrackerData(numberBuckets, bucketDurationSeconds);
  LowestFlowOverPeriodTracker_Reset();
}

static void GoToCurrentIndex( void )
{
  uint16_t currentIndex = (uint16_t)floor(TimerGetElapsedTime(startTime) / (1000 * bucketDurationSeconds)) % 0xFFFF;
  
  if (lastIndex != currentIndex)
  {
    //PRINTF("currentIndex %d lastIndex %d", currentIndex, lastIndex);

    // zero out indexes that were skipped over
    lastIndex++; // Don't erase the last index
    while (currentIndex != lastIndex)
    {
      uint16_t arrayIndex = lastIndex % numberBuckets;
      periodBuckets[arrayIndex] = 0;
      lastIndex++;
    }
    
    // Get lowest pulses
    if (currentIndex > numberBuckets)
    {
      uint16_t sum = 0;
      for(uint16_t i=0; i <numberBuckets; i++)
      {
        sum += periodBuckets[i];
      }
      
      if (sum < lowestPulses)
      {
        lowestPulses = sum;
      }
      PRINTF(" lowestPulses %d", lowestPulses);
    }
    
    // Also need to zero out current index once it's been accounted for
    uint16_t arrayIndex = currentIndex % numberBuckets;;
    periodBuckets[arrayIndex] = 0;
    
    PRINTF("\n");
  }
}
