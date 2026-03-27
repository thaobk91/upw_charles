#ifndef __SIGNAL_TRACKER_H
#define __SIGNAL_TRACKER_H

#include <stdint.h>
#include <stdbool.h>
#include "algorithm.h"

void signal_tracker_init( void );

void signal_tracker_check_in( void );

void signal_tracker_reset( void );

void signal_tracker_log( void );

bool signal_tracker_is_configured( void );

void signal_tracker_update_variable(uint8_t variable, uint32_t value);

void signal_tracker_enable( void );

void signal_tracker_disable( void );

#endif // __SIGNAL_TRACKER_H
