#ifndef __SENSUS_PROTOCOL_APP_H__
#define __SENSUS_PROTOCOL_APP_H__

#include <stdint.h>
#include <stdbool.h>
#include "algorithm.h"

/**
 * @file sensus_protocol_app.h
 * @brief Sensus Protocol implementation for water meter reading
 * 
 * This implementation supports the Sensus Protocol (UI-1203) for reading water meters.
 * 
 * Supported Data Format:
 * Sensus Protocol: "V;RBxxxxxxx;IByyyyy;Kmmmmm\r"
 * - V: Version field
 * - RB: Meter reading value (up to 12 digits, may include formatting chars like '?')
 * - IB: Meter ID (optional)
 * - K: K number (optional)
 * 
 * Hardware Requirements:
 * - P0.00 (magpower): Power/Clock control for meter (VDD/CLK)
 * - P0.02 (magint): Data signal with internal pull-up
 * 
 * Protocol Timing:
 * - Power reset: >1 second to reset meter
 * - Power cycle: Brief removal to advance bits
 * - Data format: Start bit (0) + 7 data bits (LSB first) + parity + stop bit (1)
 */

// Function declarations for the Sensus Protocol algorithm
void sensus_protocol_init(void);
void sensus_protocol_check_in(void);
void sensus_protocol_reset(void);
void sensus_protocol_log(void);
bool sensus_protocol_is_configured(void);
void sensus_protocol_update_variable(uint8_t variable, uint32_t value);
void sensus_protocol_enable(void);
void sensus_protocol_disable(void);
void sensus_protocol_set_variables(uint32_t *variables, uint8_t length);
void sensus_protocol_get_variables(uint32_t *variables, uint8_t *length);

// Sensus protocol specific defines  
#define SENSUS_POWER_RESET_TIME_MS      100   // Time to reset meter
#define SENSUS_POWER_CYCLE_TIME_MS      2       // Time to briefly remove power (increased for stability)
#define SENSUS_DATA_SETTLE_TIME_MS      2      // Time to wait for data to settle (increased for stability)
#define SENSUS_MAX_DATA_LENGTH          30      // Maximum expected data length
#define SENSUS_READING_INTERVAL_MS      60000   // How often to read meter (1 minute
#define SENSUS_ERROR_TIMEOUT_MS         2000    // Time to wait for error recovery (> 1 second for error recovery)

// Data format constants
#define SENSUS_START_BIT               0        // Expected start bit
#define SENSUS_STOP_BIT                1        // Expected stop bit  
#define SENSUS_DATA_BITS               7        // Number of data bits per byte
#define SENSUS_BITS_PER_FRAME          10       // start + 7 data + parity + stop

// Sensus protocol states
typedef enum {
    SENSUS_STATE_IDLE = 0,
    SENSUS_STATE_POWER_RESET,
    SENSUS_STATE_READING_DATA,
    SENSUS_STATE_PROCESSING_DATA,
    SENSUS_STATE_ERROR,
    SENSUS_STATE_DISABLED
} sensus_state_t;

// Sensus reading result
typedef struct {
    bool valid;
    uint32_t reading_value;       // Meter reading value (from RB field)
    char meter_id[16];            // Meter ID (from IB field, optional)
    char k_number[16];            // K number (from K field, optional)
    char raw_data[SENSUS_MAX_DATA_LENGTH + 1];  // Complete raw data string
    uint32_t timestamp;
} sensus_reading_t;



#endif // __SENSUS_PROTOCOL_APP_H__ 