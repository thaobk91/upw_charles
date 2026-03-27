#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include "algorithm.h"
#include "sensus_protocol_app.h"
#include "cycle_counter.h"
#include "error_handler.h"
#include "timeServer.h"
#include "log.h"
#include "algo_hw.h"
#include "flash_utility.h"

// GPIO device tree specifications
static const struct gpio_dt_spec sensus_pwr = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(magpower), gpios, 0);
static const struct gpio_dt_spec sensus_data = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(magint), gpios, 0);

/*
 * Info on protocol:
 * https://github.com/michlv/sensus_protocol_lib
 * NOTE: We use GPIO instead of I2C/TWI for Sensus protocol because:
 * 1. Sensus requires complete power cycling (0V) which I2C SCL cannot provide
 * 2. Sensus timing doesn't match standard I2C timing requirements  
 * 3. Sensus data format is different from I2C (start/data/parity/stop bits)
 * 
 * Current pin mapping for nRF9151:
 * - P0.00 (magpower): Power control for meter (VDD/CLK)
 * - P0.02 (magint): Data signal with pull-up
 * 
 * I2C pins (P0.26/27, P0.30/31) remain available for other sensors.
 */
// Algorithm instance for Sensus Protocol
ALGORITHM_t Sensus_Protocol = {
    sensus_protocol_init,
    sensus_protocol_check_in,
    sensus_protocol_reset,
    sensus_protocol_log,
    sensus_protocol_is_configured,
    sensus_protocol_update_variable,
    sensus_protocol_enable,
    sensus_protocol_disable,
    sensus_protocol_set_variables,
    sensus_protocol_get_variables
};

// Timer for state machine operations
static TimerEvent_t sensus_timer;

// Global variables to store parsed fields for cross-function access
static char parsed_meter_id[16] = {0};
static char parsed_k_number[16] = {0};

// Timer for watchdog feeding (similar to pulse tracker)
static TimerEvent_t feed_dog_timer;
static void feed_dog_event(void);

// State machine variables
static sensus_state_t current_state = SENSUS_STATE_IDLE;
static volatile bool timer_expired = false;
static volatile bool data_ready = false;

// Configuration status - tracks if we've had at least one successful reading
static bool has_successful_reading = false;

// Configuration variables (can be updated via OTA)
static uint32_t power_reset_time_ms = SENSUS_POWER_RESET_TIME_MS;
static uint32_t power_cycle_time_ms = SENSUS_POWER_CYCLE_TIME_MS;
static uint32_t data_settle_time_ms = SENSUS_DATA_SETTLE_TIME_MS;
static uint32_t reading_interval_ms = SENSUS_READING_INTERVAL_MS;

// Reading state variables
static uint8_t current_bit = 0;
static uint8_t current_byte = 0;
static uint8_t bit_count = 0;
static uint8_t byte_buffer = 0;
static char data_buffer[SENSUS_MAX_DATA_LENGTH + 1];
static sensus_reading_t last_reading = {0};

// Internal function prototypes
static void sensus_timer_callback(void);
static void power_on_meter(void);
static void power_off_meter(void);
static void power_cycle_for_next_bit(void);
static bool read_data_bit(void);
static void process_bit(bool bit_value);
static void process_complete_byte(uint8_t byte_value);
static void process_data_complete(void);
static void transition_to_state(sensus_state_t new_state);
static uint32_t parse_meter_reading(const char* data);

void sensus_protocol_init(void)
{
    PRINTF_TS("sensus_protocol_init\n");
    
    // Initialize cycle counter for cumulative usage data reporting
    cycle_counter_init(CYCLE_MODE_CUMULATIVE_USAGE);
    
    // Initialize timers
    TimerInit(&sensus_timer, sensus_timer_callback);
    TimerInit(&feed_dog_timer, feed_dog_event);
    TimerSetValue(&feed_dog_timer, 10000);
    TimerStart(&feed_dog_timer);
    
    // Initialize GPIO
    if (!device_is_ready(sensus_pwr.port)) {
        PRINTF_TS("Error: Sensus power GPIO device %s is not ready\n", sensus_pwr.port->name);
        return;
    }
    
    if (!device_is_ready(sensus_data.port)) {
        PRINTF_TS("Error: Sensus data GPIO device %s is not ready\n", sensus_data.port->name);
        return;
    }
    
    // Configure P0.00 as power/clock control (output)
    gpio_pin_configure_dt(&sensus_pwr, GPIO_OUTPUT);
    
    // Configure P0.02 as data signal (input with pull-up)
    gpio_pin_configure_dt(&sensus_data, GPIO_INPUT | GPIO_PULL_UP);
    
    // Reset state machine
    current_state = SENSUS_STATE_IDLE;
    timer_expired = false;
    data_ready = false;
    current_bit = 0;
    current_byte = 0;
    bit_count = 0;
    has_successful_reading = false; // Reset configuration status
    memset(data_buffer, 0, sizeof(data_buffer));
    memset(&last_reading, 0, sizeof(last_reading));
    
    // Start first reading cycle
    transition_to_state(SENSUS_STATE_POWER_RESET);
    
    PRINTF_TS("sensus_protocol_init complete\n");
}

void sensus_protocol_check_in(void)
{
    // Handle timer expiration events
    if (timer_expired) {
        timer_expired = false;
        
        switch (current_state) {
            case SENSUS_STATE_POWER_RESET:
                power_cycle_for_next_bit(); // Start first bit read
                transition_to_state(SENSUS_STATE_READING_DATA);
                break;
                
            case SENSUS_STATE_READING_DATA:
                // Time to read next bit
                bool bit_value = read_data_bit();
                process_bit(bit_value);
                
                // Check if we're still in reading state (process_bit might have changed state on error)
                if (current_state == SENSUS_STATE_READING_DATA) {
                    // Continue reading if we haven't reached max data length
                    // Note: Carriage return termination is handled in process_complete_byte()
                    if (current_byte < SENSUS_MAX_DATA_LENGTH) {
                        power_cycle_for_next_bit();
                    } else {
                        PRINTF_TS("Sensus: Reached maximum data length without carriage return\n");
                        transition_to_state(SENSUS_STATE_ERROR);
                    }
                }
                break;
                
            case SENSUS_STATE_PROCESSING_DATA:
                // Process the received data
                process_data_complete();
                
                // Schedule next reading
                TimerSetValue(&sensus_timer, reading_interval_ms);
                TimerStart(&sensus_timer);
                transition_to_state(SENSUS_STATE_IDLE);
                break;
                
            case SENSUS_STATE_ERROR:
                // Error timeout, retry
                PRINTF_TS("Sensus error timeout, retrying\n");
                transition_to_state(SENSUS_STATE_POWER_RESET);
                break;
                
            case SENSUS_STATE_IDLE:
                // Time for next reading
                transition_to_state(SENSUS_STATE_POWER_RESET);
                break;
                
            default:
                // Unknown state, reset
                transition_to_state(SENSUS_STATE_POWER_RESET);
                break;
        }
    }
}

void sensus_protocol_reset(void)
{
    PRINTF_TS("sensus_protocol_reset\n");
    
    // Stop all timers
    TimerStop(&sensus_timer);
    
    // Reset state
    current_state = SENSUS_STATE_IDLE;
    timer_expired = false;
    data_ready = false;
    current_bit = 0;
    current_byte = 0;
    bit_count = 0;
    has_successful_reading = false; // Reset configuration status on reset
    memset(data_buffer, 0, sizeof(data_buffer));
    
    // Power off meter
    power_off_meter();
    
    // Restart
    transition_to_state(SENSUS_STATE_POWER_RESET);
}

void sensus_protocol_log(void)
{
    PRINTF_TS("Sensus: ======== STATUS LOG ========\n");
    PRINTF_TS("Sensus: Current State: %d\n", current_state);
    
#ifndef DISABLE_LOGGING
    const char* state_names[] = {
        "IDLE", "POWER_RESET", "READING_DATA", 
        "PROCESSING_DATA", "ERROR", "DISABLED"
    };
#endif
    
    if (current_state < 6) {
        PRINTF_TS("Sensus: State Name: %s\n", state_names[current_state]);
    }
    
    if (last_reading.valid) {
        PRINTF_TS("Sensus: Last Valid Reading:\n");
        PRINTF_TS("Sensus:   Total Usage: %d\n", last_reading.reading_value);
        PRINTF_TS("Sensus:   Raw Data: '%s'\n", last_reading.raw_data);
        PRINTF_TS("Sensus:   Timestamp: %d ms\n", last_reading.timestamp);
        PRINTF_TS("Sensus:   Age: %d ms ago\n", TimerGetCurrentTime() - last_reading.timestamp);
        
        if (last_reading.meter_id[0]) {
            PRINTF_TS("Sensus:   Meter ID: '%s'\n", last_reading.meter_id);
        }
        
        if (last_reading.k_number[0]) {
            PRINTF_TS("Sensus:   K Number: '%s'\n", last_reading.k_number);
        }
    } else {
        PRINTF_TS("Sensus: No valid reading available\n");
    }
    
    PRINTF_TS("Sensus: Configuration:\n");
    PRINTF_TS("Sensus:   Power Reset Time: %d ms\n", power_reset_time_ms);
    PRINTF_TS("Sensus:   Power Cycle Time: %d ms\n", power_cycle_time_ms);
    PRINTF_TS("Sensus:   Data Settle Time: %d ms\n", data_settle_time_ms);
    PRINTF_TS("Sensus:   Reading Interval: %d ms\n", reading_interval_ms);
    
    PRINTF_TS("Sensus: Current Reading Progress:\n");
    PRINTF_TS("Sensus:   Current Bit: %d\n", current_bit);
    PRINTF_TS("Sensus:   Current Byte: %d\n", current_byte);
    PRINTF_TS("Sensus:   Bit Count: %d\n", bit_count);
    
    PRINTF_TS("Sensus: ======== END STATUS LOG ========\n");
}

bool sensus_protocol_is_configured(void)
{
    return has_successful_reading; // Return true once we've had at least one successful reading
}

void sensus_protocol_update_variable(uint8_t variable, uint32_t value)
{
    switch (variable) {
        case 22: // Power reset time
            PRINTF_TS("power_reset_time_ms to %d\n", value);
            power_reset_time_ms = value;
            break;
        case 23: // Power cycle time
            PRINTF_TS("power_cycle_time_ms to %d\n", value);
            power_cycle_time_ms = value;
            break;
        case 24: // Data settle time
            PRINTF_TS("data_settle_time_ms to %d\n", value);
            data_settle_time_ms = value;
            break;
        case 25: // Reading interval
            PRINTF_TS("reading_interval_ms to %d\n", value);
            reading_interval_ms = value;
            break;
        default:
            break;
    }
    PRINTF_TS("sensus_protocol_update_variable %d %d\n", variable, value);
}

void sensus_protocol_enable(void)
{
    PRINTF_TS("sensus_protocol_enable\n");
    if (current_state == SENSUS_STATE_DISABLED) {
        transition_to_state(SENSUS_STATE_IDLE);
    }
}

void sensus_protocol_disable(void)
{
    PRINTF_TS("sensus_protocol_disable\n");
    TimerStop(&sensus_timer);
    power_off_meter();
    current_state = SENSUS_STATE_DISABLED;
}

void sensus_protocol_set_variables(uint32_t *variables, uint8_t length)
{
    // Load saved variables from flash
    if (length >= 4) {
        power_reset_time_ms = variables[0];
        power_cycle_time_ms = variables[1];
        data_settle_time_ms = variables[2];
        reading_interval_ms = variables[3];
        PRINTF_TS("Loaded Sensus variables from flash\n");
    }
}

void sensus_protocol_get_variables(uint32_t *variables, uint8_t *length)
{
    // Save variables to flash
    variables[0] = power_reset_time_ms;
    variables[1] = power_cycle_time_ms;
    variables[2] = data_settle_time_ms;
    variables[3] = reading_interval_ms;
    *length = 4;
    PRINTF_TS("Saved Sensus variables to flash\n");
}

// Internal functions
static void sensus_timer_callback(void)
{
    timer_expired = true;
}

static void feed_dog_event(void)
{
    TimerReset(&feed_dog_timer);
}

static void power_on_meter(void)
{
    // Set P0.00 (sensus_pwr) HIGH to power the meter
    gpio_pin_set_dt(&sensus_pwr, 1);
    // PRINTF_TS("Sensus: Power ON\n");
}

static void power_off_meter(void)
{
    // Set P0.00 (sensus_pwr) LOW to power off the meter
    gpio_pin_set_dt(&sensus_pwr, 0);
    // PRINTF_TS("Sensus: Power OFF\n");
}

static void power_cycle_for_next_bit(void)
{
    // Briefly remove power to advance to next bit
    power_off_meter();
    HAL_Delay(power_cycle_time_ms);
    power_on_meter();
    
    // Wait for data to settle
    TimerSetValue(&sensus_timer, data_settle_time_ms);
    TimerStart(&sensus_timer);
}

static bool read_data_bit(void)
{
    // Read the data line (P0.02 with pull-up)
    // HIGH = 1, LOW = 0
    int pin_value = gpio_pin_get_dt(&sensus_data);
    bool bit_value = (pin_value == 1);
    
    // PRINTF_TS("Sensus: Read bit %d\n", bit_value ? 1 : 0);
    return bit_value;
}

static void process_bit(bool bit_value)
{
    if (bit_count == 0) { 
        // Start bit - should be 0
        if (bit_value != SENSUS_START_BIT) {
            PRINTF_TS("Sensus: Invalid start bit %d\n", bit_value);
            transition_to_state(SENSUS_STATE_ERROR);
            return;
        }
    } else if (bit_count <= SENSUS_DATA_BITS) {
        // Data bits (LSB first)
        byte_buffer |= (bit_value ? 1 : 0) << (bit_count - 1);
    } else if (bit_count == SENSUS_DATA_BITS + 1) {
        // Parity bit (even parity)
        uint8_t parity = 0;
        for (int i = 0; i < 8; i++) {
            parity ^= (byte_buffer >> i) & 1;
        }
        if (parity != (bit_value ? 1 : 0)) {
            PRINTF_TS("Sensus: Parity error\n");
        }
    } else if (bit_count == SENSUS_DATA_BITS + 2) {
        // Stop bit - should be 1
        if (bit_value != SENSUS_STOP_BIT) {
            PRINTF_TS("Sensus: Invalid stop bit\n");
            transition_to_state(SENSUS_STATE_ERROR);
            return;
        }
        
        // Complete byte received
        process_complete_byte(byte_buffer);
        
        // Reset for next byte
        bit_count = 0;
        byte_buffer = 0;
        return;
    }
    
    bit_count++;
}

static void process_complete_byte(uint8_t byte_value)
{
    if (current_byte < SENSUS_MAX_DATA_LENGTH) {
        data_buffer[current_byte] = (char)byte_value;
        current_byte++;
        
        PRINTF_TS("Sensus: Received byte 0x%02X ('%c')\n", byte_value, 
                  (byte_value >= 32 && byte_value < 127) ? byte_value : '?');
        if( byte_value > 127) {
            PRINTF_TS("Sensus: Invalid byte value: 0x%02X\n", byte_value);
        }

        // Check for end of transmission (carriage return)
        if (byte_value == '\r') {
            data_buffer[current_byte] = '\0'; // Null terminate
            PRINTF_TS("Sensus: Carriage return detected - data transmission complete\n");
            transition_to_state(SENSUS_STATE_PROCESSING_DATA);
            return;
        }
    } else {
        PRINTF_TS("Sensus: Buffer overflow\n");
        transition_to_state(SENSUS_STATE_ERROR);
    }
}

static void process_data_complete(void)
{
    PRINTF_TS("Sensus: ======== DATA PROCESSING COMPLETE ========\n");
    PRINTF_TS("Sensus: Complete data received: '%s'\n", data_buffer);
    
    // Parse the meter reading
    uint32_t current_reading = parse_meter_reading(data_buffer);
    
    // Update last reading structure
    last_reading.valid = (current_reading > 0);
    last_reading.reading_value = current_reading;
    strncpy(last_reading.raw_data, data_buffer, sizeof(last_reading.raw_data) - 1);
    last_reading.raw_data[sizeof(last_reading.raw_data) - 1] = '\0';
    last_reading.timestamp = TimerGetCurrentTime();
    
    // Copy parsed fields (if available)
    strncpy(last_reading.meter_id, parsed_meter_id, sizeof(last_reading.meter_id) - 1);
    last_reading.meter_id[sizeof(last_reading.meter_id) - 1] = '\0';
    strncpy(last_reading.k_number, parsed_k_number, sizeof(last_reading.k_number) - 1);
    last_reading.k_number[sizeof(last_reading.k_number) - 1] = '\0';
    
    if (last_reading.valid) {
        // Log detailed usage information
        PRINTF_TS("Sensus: *** WATER USAGE SUMMARY ***\n");
        PRINTF_TS("Sensus: Current Total Usage: %d\n", current_reading);
        PRINTF_TS("Sensus: Reading Timestamp: %d ms\n", last_reading.timestamp);
        PRINTF_TS("Sensus: Raw Data: '%s'\n", last_reading.raw_data);
        
        if (last_reading.meter_id[0]) {
            PRINTF_TS("Sensus: Meter ID: '%s'\n", last_reading.meter_id);
        }
        
        if (last_reading.k_number[0]) {
            PRINTF_TS("Sensus: K Number: '%s'\n", last_reading.k_number);
        }
        
        // Update cumulative usage tracking
        cycle_counter_update_cumulative_usage(current_reading);
        
        // Mark as configured after first successful reading
        has_successful_reading = true;
        
        PRINTF_TS("Sensus: Total Cumulative Usage: %d\n", current_reading);
        PRINTF_TS("Sensus: *** END USAGE SUMMARY ***\n");
    } else {
        PRINTF_TS("Sensus: ERROR - Invalid reading format, no usage data extracted\n");
    }
    
    // Reset for next reading
    current_bit = 0;
    current_byte = 0;
    bit_count = 0;
    memset(data_buffer, 0, sizeof(data_buffer));
    
    PRINTF_TS("Sensus: ======== PROCESSING COMPLETE ========\n");
    
    // Schedule processing completion
    TimerSetValue(&sensus_timer, 1); // Short delay before next state
    TimerStart(&sensus_timer);
}

static void transition_to_state(sensus_state_t new_state)
{
    PRINTF_TS("Sensus: State %d -> %d\n", current_state, new_state);
    current_state = new_state;
    
    switch (new_state) {
        case SENSUS_STATE_POWER_RESET:
            power_on_meter();
            PRINTF_TS("Sensus: Initial startup - powering ON for meter initialization\n");
            TimerSetValue(&sensus_timer, power_reset_time_ms);
            TimerStart(&sensus_timer);
            break;
            
        case SENSUS_STATE_READING_DATA:
            current_bit = 0;
            current_byte = 0;
            bit_count = 0;
            byte_buffer = 0;
            break;
            
        case SENSUS_STATE_ERROR:
            if (has_successful_reading) {
                has_successful_reading = false;
                Alert_Data(ERROR_CODE_ALGORITHM_SENSUS_ERROR, 0, NULL, 0);
            }
            power_off_meter();
            PRINTF_TS("Sensus: Entering error state - will do power-off reset next\n");
            TimerSetValue(&sensus_timer, SENSUS_ERROR_TIMEOUT_MS);
            TimerStart(&sensus_timer);
            break;
            
        case SENSUS_STATE_PROCESSING_DATA:
            // Set a short timer to immediately trigger data processing
            TimerSetValue(&sensus_timer, 1);
            TimerStart(&sensus_timer);
            break;
            
        case SENSUS_STATE_IDLE:
            power_off_meter();
            break;
            
        default:
            break;
    }
}

static uint32_t parse_meter_reading(const char* data)
{
    // Expected format: "V;RBxxxxxxx;IByyyyy;Kmmmmm\r"
    // Where IB and K parts are optional
    // IB is the customer 
    // xxxxxxx is the meter read value (up to 12 digits)
    size_t data_len = strlen(data);
    
    PRINTF_TS("Sensus: Raw meter data: '%s' (length: %d)\n", data, data_len);
    
    // Check for carriage return at the end
    if (data_len > 0 && data[data_len - 1] != '\r') {
        PRINTF_TS("Sensus: Warning - data doesn't end with carriage return\n");
    }
    
    // Validate Sensus protocol format: V;RBxxxxxxx;IByyyyy;Kmmmmm
    if (data_len < 3 || data[0] != 'V' || data[1] != ';') {
        PRINTF_TS("Sensus: Invalid data format - expected 'V;' prefix\n");
        return 0;
    }
    
    PRINTF_TS("Sensus: Processing Sensus protocol format\n");
    
    // Split by semicolons and parse each field
    char* data_copy = malloc(strlen(data) + 1);
    strcpy(data_copy, data);
    
    uint32_t meter_value = 0;
    bool found_rb = false;
    
    // Clear global parsed fields
    memset(parsed_meter_id, 0, sizeof(parsed_meter_id));
    memset(parsed_k_number, 0, sizeof(parsed_k_number));
    
    char* token = strtok(data_copy, ";");
    while (token != NULL) {
        PRINTF_TS("Sensus: Processing token: '%s'\n", token);
        
        if (strncmp(token, "V", 1) == 0) {
            PRINTF_TS("Sensus: Found version field: '%s'\n", token);
        }
        else if (strncmp(token, "RB", 2) == 0) {
            // This is the meter reading value
            char* value_str = token + 2;  // Skip "RB"
            
            // Remove carriage return if present
            size_t len = strlen(value_str);
            if (len > 0 && value_str[len-1] == '\r') {
                value_str[len-1] = '\0';
                len--;
            }
            
            PRINTF_TS("Sensus: Found meter reading field: '%s'\n", value_str);
            
            // Convert to integer (up to 12 digits as per kmeter)
            // Skip non-numeric characters like '?' which may be formatting
            if (len > 0 && len <= 12) {
                meter_value = 0;
                bool valid = true;
                
                for (size_t i = 0; i < len; i++) {
                    if (value_str[i] >= '0' && value_str[i] <= '9') {
                        meter_value = meter_value * 10 + (value_str[i] - '0');
                    } else if (value_str[i] == '?') {
                        // Skip '?' characters - they're part of this meter's format
                        PRINTF_TS("Sensus: Skipping '?' character in meter value\n");
                        continue;
                    } else {
                        PRINTF_TS("Sensus: Invalid character '%c' in meter value\n", value_str[i]);
                        valid = false;
                        break;
                    }
                }
                
                if (valid) {
                    found_rb = true;
                    PRINTF_TS("Sensus: Parsed meter value: %u\n", meter_value);
                }
            }
        }
        else if (strncmp(token, "IB", 2) == 0) {
            // Optional meter ID field
            strncpy(parsed_meter_id, token + 2, sizeof(parsed_meter_id) - 1);
            // Remove carriage return if present
            size_t len = strlen(parsed_meter_id);
            if (len > 0 && parsed_meter_id[len-1] == '\r') {
                parsed_meter_id[len-1] = '\0';
            }
            PRINTF_TS("Sensus: Found meter ID: '%s'\n", parsed_meter_id);
        }
        else if (strncmp(token, "K", 1) == 0) {
            // Optional K number field
            strncpy(parsed_k_number, token + 1, sizeof(parsed_k_number) - 1);
            // Remove carriage return if present
            size_t len = strlen(parsed_k_number);
            if (len > 0 && parsed_k_number[len-1] == '\r') {
                parsed_k_number[len-1] = '\0';
            }
            PRINTF_TS("Sensus: Found K number: '%s'\n", parsed_k_number);
        }
        else {
            PRINTF_TS("Sensus: Unknown field: '%s'\n", token);
        }
        
        token = strtok(NULL, ";");
    }
    
    free(data_copy);
    
    if (found_rb) {
        PRINTF_TS("Sensus: Successfully parsed Sensus protocol - Value: %u", meter_value);
        if (parsed_meter_id[0]) PRINTF_TS(", Meter ID: %s", parsed_meter_id);
        if (parsed_k_number[0]) PRINTF_TS(", K Number: %s", parsed_k_number);
        PRINTF_TS("\n");
        return meter_value;
    }
    
    PRINTF_TS("Sensus: No meter reading found in Sensus protocol data\n");
    return 0;
}

