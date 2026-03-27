#include <stdint.h>
#include <stdbool.h>

// Initialize the tracker.
void pulse_tracker_init( void );

// Process pin changes in the main loop.
void pulse_tracker_check_in( void );

// Reset the number of gallons.
void pulse_tracker_reset( void );

// Log data
void pulse_tracker_log( void );

bool pulse_tracker_is_configured( void );

void pulse_tracker_update_variable(  uint8_t variable, uint32_t value  );

void pulse_tracker_enable( void );

void pulse_tracker_disable( void );
