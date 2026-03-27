#ifndef __APP_H__
#define __APP_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/** 
 * @brief      Initialize the application and starts it
 * @return     0 if successful, otherwise failed.
*/
int app_init(void);

/** 
 * @brief      Starts the application if it not already running
*/
void app_start(void);

/** 
 * @brief      Stops the application
*/
void app_stop(void);

void app_check_in(void);

/** 
 * @brief      Sets the duty cycle of the application. The rate it saves data from the algorithm
 * @param[in]  duty_cycle_s  The duty cycle in seconds
*/
void app_update_set_duty_cycle(uint32_t duty_cycle_s);

/** 
 * @brief      Gets the duty cycle of the application. The rate it saves data from the algorithm
 * @return     The duty cycle in seconds
*/
uint32_t app_update_get_duty_cycle(void);

/** 
 * @brief      Updates the algorithem variable.
 * 
 * @param[in]  variable  Algorithm variable id (string)
 * @param[in]  value     Algorithm variable value
 * @return     void
*/
void app_update_variable(const char *variable, uint32_t value);


#define ALGORITHM_VARIABLES_COUNT 26
extern const char *algorithm_variables_s[ALGORITHM_VARIABLES_COUNT];

// Device/Algorithm types
typedef enum device_type_e {
    DEVICE_TYPE_MAGNETOMETER = 0,
    DEVICE_TYPE_PULSE_TRACKER = 1,
    DEVICE_TYPE_SENSUS_PROTOCOL = 2
} device_type_t;

void app_action_take(char* action);

int app_queue_data(void);

void app_set_device_type(device_type_t device_type);

device_type_t app_get_device_type(void);

void app_sync_upload(void);

void app_set_save_data_flag(void);

#endif // __APP_H__