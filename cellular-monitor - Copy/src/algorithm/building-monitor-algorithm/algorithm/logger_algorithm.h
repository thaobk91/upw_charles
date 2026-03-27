// A logging algorithm to log the magnetic field over time

#ifndef __ALGORITHM_LOGGER_H
#define __ALGORITHM_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include "algorithm.h"

void logger_algorithm_init( void );

void logger_algorithm_check_in( void );

void logger_algorithm_reset( void );

void logger_algorithm_log( void );

bool logger_algorithm_is_configured( void );

void logger_algorithm_update_variable(uint8_t variable, uint32_t value);

void logger_algorithm_enable( void );

void logger_algorithm_disable( void );

#endif // __ALGORITHM_LOGGER_H
