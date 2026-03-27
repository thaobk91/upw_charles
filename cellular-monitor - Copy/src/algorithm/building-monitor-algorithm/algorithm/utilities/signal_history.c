#include "signal_history.h"

#define SIGNAL_HISTORY_SIZE 20

static int16_t signal_history[SIGNAL_HISTORY_SIZE] = {0};
static uint8_t signal_history_index = 0;
static uint8_t signal_history_count = 0;
static int16_t min = INT16_MAX;
static int16_t max = INT16_MIN;

void signal_history_reset( void )
{
  for (uint8_t i = 0; i < SIGNAL_HISTORY_SIZE; i++)
  {
    signal_history[i] = 0;
  }
  signal_history_index = 0;
  signal_history_count = 0;
  min = INT16_MAX;
  max = INT16_MIN;
}

// Add a new signal to the history and return the largest difference between any two signals
bool signal_history_add( int16_t signal, uint16_t *diff )
{
  signal_history[signal_history_index++] = signal;
  if (signal_history_index >= SIGNAL_HISTORY_SIZE)
  {
    signal_history_index = 0;
  }

  signal_history_count++;
  if ( signal_history_count > SIGNAL_HISTORY_SIZE )
  {
    signal_history_count = SIGNAL_HISTORY_SIZE;
  }

  min = signal_history[0];
  max = signal_history[0];

  for (uint8_t i = 1; i < SIGNAL_HISTORY_SIZE; i++)
  {
    if (signal_history[i] == 0)
    {
      continue;
    }
    if (signal_history[i] < min)
    {
      min = signal_history[i];
    }
    if (signal_history[i] > max)
    {
      max = signal_history[i];
    }
  }
  
  if (signal_history_count < SIGNAL_HISTORY_SIZE)
  {
    return false;
  }

  return signal_history_get_min_max_diff( diff );
}

bool signal_history_get_min_max_diff( uint16_t *diff )
{
  if (max > min)
  {
    *diff = max - min;
    return true;
  }
  return false;
}
