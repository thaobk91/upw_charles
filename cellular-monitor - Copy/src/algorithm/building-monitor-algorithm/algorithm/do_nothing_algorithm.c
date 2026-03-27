// A logging algorithm to log the magnetic field over time

#ifdef ALGORITHM_DO_NOTHING
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "log.h"
#include "algorithm.h"

void do_nothing_algorithm_init( void )
{
  PRINTF("do_nothing_algorithm_init\n");
}

void do_nothing_algorithm_check_in( void )
{
    PRINTF("do_nothing_algorithm_check_in\n");
}

void do_nothing_algorithm_reset( void )
{
}

void do_nothing_algorithm_log( void )
{
  PRINTF_BASE("Do nothing algorithm log\n");
}

bool do_nothing_algorithm_is_configured( void )
{
  return true;
}

void do_nothing_algorithm_update_variable(uint8_t variable, uint32_t value)
{
}

void do_nothing_algorithm_enable( void )
{
  PRINTF("do_nothing_algorithm_enable\n");
}

void do_nothing_algorithm_disable( void )
{
  PRINTF("do_nothing_algorithm_disable\n");
}

ALGORITHM_t Algorithm = {
  do_nothing_algorithm_init,
  do_nothing_algorithm_check_in,
  do_nothing_algorithm_reset,
  do_nothing_algorithm_log,
  do_nothing_algorithm_is_configured,
  do_nothing_algorithm_update_variable,
  do_nothing_algorithm_enable,
  do_nothing_algorithm_disable
};

#endif // ALGORITHM_DO_NOTHING
