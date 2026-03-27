
#include "cycle_counter.h"
#include "lowest_flow_over_period_tracker.h"
#include "flash_utility.h"
#include "log.h"

#define CYCLE_COUNTER_MAX_TIMESTAMPS 20
#define CYCLES_TO_AVERAGE_OVER_FOR_DURATION 2
#define RECENT_CYCLES_DURATION_MS 5000

static uint32_t halfCycles = 0;
static uint32_t halfCyclesAtLastSave = 0;
static uint8_t timeStampsFilled = 0;
static TimerTime_t timestamps[CYCLE_COUNTER_MAX_TIMESTAMPS];
static TimerTime_t longestDurationBetweenPulsesInMS = 0;

// New variables for cumulative usage mode
static cycle_counter_mode_t current_mode = CYCLE_MODE_PULSE_COUNTING;

TimerTime_t cycle_counter_get_averaged_duration_since_now(void);

void cycle_counter_init(cycle_counter_mode_t mode)
{
  halfCycles = 0;
  timeStampsFilled = 0;
  longestDurationBetweenPulsesInMS = 0;
  current_mode = mode;
  
  PRINTF_TS("cycle_counter_init with mode: %d\n", mode);
  
  // Load from flash
  FlashUtility_LoadHalfCycles( &halfCycles );
  halfCyclesAtLastSave = halfCycles;

  LowestFlowOverPeriodTracker_Init( );
}

void cycle_counter_reset(bool resetHalfCycles)
{
  if (resetHalfCycles)
  {
    halfCycles = 0;
    cycle_counter_save_half_cycles();
  }
  longestDurationBetweenPulsesInMS = 0;
  
  // zero out timestamps
  timeStampsFilled = 0;
  for(uint8_t i=0; i < CYCLE_COUNTER_MAX_TIMESTAMPS; i++)
  {
    timestamps[i] = 0;
  }
  LowestFlowOverPeriodTracker_Reset();
}

// Get mode function for other modules to check the current mode
cycle_counter_mode_t cycle_counter_get_mode(void)
{
  return current_mode;
}

void cycle_counter_update_cumulative_usage(uint32_t new_total_usage)
{
  if (current_mode != CYCLE_MODE_CUMULATIVE_USAGE) {
    PRINTF_TS("Warning: Updating cumulative usage while not in cumulative mode\n");
    return;
  }
  
  halfCycles = new_total_usage * 2;
  
  PRINTF_TS("Updated cumulative usage: %d or hc:%d\n", new_total_usage, halfCycles);
}

uint32_t cycle_counter_get_half_cycles(void)
{
  return halfCycles;
}

uint32_t cycle_counter_add_half_cycle(bool increment)
{
  if (current_mode == CYCLE_MODE_CUMULATIVE_USAGE) {
    PRINTF_TS("Warning: add_half_cycle called in cumulative mode - use update_cumulative_usage instead\n");
    return halfCycles;
  }
  
  TimerTime_t now = TimerGetCurrentTime();
  if (increment)
  {
    halfCycles++;
    PRINTF_TS("hc:%d\n", halfCycles);
  }
  LowestFlowOverPeriodTracker_NewPulse();

  if (timeStampsFilled > 0)
  {
    if (timeStampsFilled >= CYCLES_TO_AVERAGE_OVER_FOR_DURATION * 2) // x 2 because timestamps are half cycles
    {
      TimerTime_t averagedDuration = cycle_counter_get_averaged_duration_since_now();
      if (averagedDuration > longestDurationBetweenPulsesInMS)
      {
        longestDurationBetweenPulsesInMS = averagedDuration;
      }
    }

    for (uint8_t i = timeStampsFilled; i > 0; i--)
    {
      timestamps[i] = timestamps[i-1];
    }
  }

  timestamps[0] = now;

  if (timeStampsFilled < CYCLE_COUNTER_MAX_TIMESTAMPS)
  {
    timeStampsFilled++;
  }

  return halfCycles;
}

uint32_t cycle_counter_decrement_half_cycles(uint32_t by)
{
  if (by > halfCycles || halfCycles == 0)
  {
    halfCycles = 0;
  }
  else
  {
    halfCycles -= by;
  }
  PRINTF("hc decrement by:%d final:%d\n", by, halfCycles);
  return halfCycles;
}

uint8_t cycle_counter_number_timestamps(void)
{
  return timeStampsFilled;
}

TimerTime_t cycle_counter_get_timestamp(uint8_t index)
{
  if (index < timeStampsFilled)
  {
    return timestamps[index];
  }
  else
    return 0;
}

void cycle_counter_reset_timestamps(void)
{
  timeStampsFilled = 0;
}

bool cycle_counter_get_longest_duration_between_pulses_in_ms( TimerTime_t *longestDuration ){
  if (current_mode == CYCLE_MODE_CUMULATIVE_USAGE) {
    // For cumulative mode, longest duration between pulses doesn't make sense
    // since we read total usage, not individual pulses
    return false;
  }
  
  if (longestDurationBetweenPulsesInMS == 0 )
  {
    return false;
  }
  *longestDuration = longestDurationBetweenPulsesInMS;
  // Check if it's been longer since the last pulse. Could have a situation with pulses early then none for a while, and won't update longest duration
  TimerTime_t averagedDuration = cycle_counter_get_averaged_duration_since_now();
  if (averagedDuration > longestDurationBetweenPulsesInMS)
  {
    *longestDuration = averagedDuration;;
  }
  return true;
}

void cycle_counter_start_new_period( void )
{
  LowestFlowOverPeriodTracker_StartNewPeriod();
  
  if (current_mode == CYCLE_MODE_PULSE_COUNTING) {
    longestDurationBetweenPulsesInMS = 0;
  }
  // For cumulative mode, we don't reset the longest duration since it's not applicable
}

void cycle_counter_delete_recent_cycles( void )
{
  if (current_mode == CYCLE_MODE_CUMULATIVE_USAGE) {
    PRINTF_TS("delete_recent_cycles not applicable in cumulative mode\n");
    return;
  }
  
  uint8_t i = 0;
  while(timestamps[i] != 0 && i < timeStampsFilled && TimerGetElapsedTime(timestamps[i]) < RECENT_CYCLES_DURATION_MS)
  {
    i++;
    if (i >= CYCLE_COUNTER_MAX_TIMESTAMPS)
    {
      break;
    }
  }

  if (i > 0)
  {
    for (uint8_t j = 0; j < CYCLE_COUNTER_MAX_TIMESTAMPS - i; j++)
    {
      timestamps[j] = timestamps[j+i];
      timestamps[j+i] = 0;
    }
    halfCycles -= i;
    timeStampsFilled -= i;
    PRINTF_TS("Deleted %d timestamps and hc\n", i);
  }
  else
  {
    // No timestamps to delete
  }
}

void cycle_counter_save_half_cycles( void )
{
  if (halfCycles != halfCyclesAtLastSave)
  {
    halfCyclesAtLastSave = halfCycles;
    FlashUtility_SaveHalfCycles(halfCycles);
  }
}

// #region Private functions

TimerTime_t cycle_counter_get_averaged_duration_since_now(void)
{
  if (current_mode == CYCLE_MODE_CUMULATIVE_USAGE) {
    // For cumulative mode, averaged duration between pulses doesn't make sense
    return 0;
  }
  
  return TimerGetElapsedTime(cycle_counter_get_timestamp((CYCLES_TO_AVERAGE_OVER_FOR_DURATION * 2) - 1)) / CYCLES_TO_AVERAGE_OVER_FOR_DURATION; 
}
