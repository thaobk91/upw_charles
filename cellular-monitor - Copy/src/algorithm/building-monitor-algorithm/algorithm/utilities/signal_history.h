#ifndef __SIGNAL_HISTORY_H__
#define __SIGNAL_HISTORY_H__

#include <stdbool.h>
#include <stdint.h>

void signal_history_reset( void );

// Add a new signal to the history and return the average of the history
bool signal_history_add( int16_t signal, uint16_t *diff );

bool signal_history_get_min_max_diff( uint16_t *diff );

#endif // __SIGNAL_HISTORY_H__
