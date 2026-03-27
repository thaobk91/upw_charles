#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define QUICK_PRESS_MAX_DURATION_MS 1000
#define LONG_PRESS_TIMEOUT_MS_DISABLE 6000
#define LONG_PRESS_TIMEOUT_MS_ENABLE 3000

typedef void (*device_action_t)(void);

void button_manager_init(device_action_t enable_func, device_action_t disable_func);

#endif /* BUTTON_MANAGER_H */
