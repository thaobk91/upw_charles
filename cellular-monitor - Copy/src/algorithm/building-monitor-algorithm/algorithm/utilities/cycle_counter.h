#ifndef __CYCLE_COUNTER_H__
#define __CYCLE_COUNTER_H__

#include <stdint.h>
#include "timeServer.h"

// Usage tracking modes
typedef enum {
    CYCLE_MODE_PULSE_COUNTING = 0,  // Traditional pulse/cycle counting
    CYCLE_MODE_CUMULATIVE_USAGE = 1 // Cumulative usage reading (e.g., Sensus)
} cycle_counter_mode_t;

void cycle_counter_init(cycle_counter_mode_t mode);
void cycle_counter_reset(bool resetHalfCycles);
void cycle_counter_reset_timestamps(void);
uint32_t cycle_counter_get_half_cycles(void);
uint32_t cycle_counter_add_half_cycle(bool increment);
uint32_t cycle_counter_decrement_half_cycles(uint32_t by);
uint8_t cycle_counter_number_timestamps(void);
TimerTime_t cycle_counter_get_timestamp(uint8_t index);
bool cycle_counter_get_longest_duration_between_pulses_in_ms( TimerTime_t *longestDuration );
void cycle_counter_start_new_period(void);
void cycle_counter_delete_recent_cycles(void);
void cycle_counter_save_half_cycles(void);

// Function for cumulative usage tracking
cycle_counter_mode_t cycle_counter_get_mode(void);
void cycle_counter_update_cumulative_usage(uint32_t new_total_usage);

#endif
