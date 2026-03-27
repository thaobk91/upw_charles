#include <zephyr/kernel.h>
#include "app.h"
#include "interaction_manager.h"
#include "led_manager.h"
#include "buzzer.h"
#include "cloud.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(interaction_manager, CONFIG_APP_LOG_LEVEL);

extern bool device_enabled;

void interaction_manager_init(void)
{
    buzzer_init();
}

void interaction_manager_device_enabled(void)
{
    // Visual feedback
    led_manager_led_on_for(LED_SUCCESS_DEVICE_ENABLED_DURATION_MS, LED_SUCCESS);

    // Audio feedback - longer beep for enable
    play_tone_async(2000);
}

void interaction_manager_device_disabled(void)
{
    // Visual feedback
    led_manager_led_on_for(LED_ERROR_DEVICE_DISABLED_DURATION_MS, LED_ERROR);

    // Audio feedback - longer tone for disable
    play_tone_async(5000);
}

void interaction_manager_quick_press(void)
{
    if (!device_enabled)
    {
        LOG_INF("Device is disabled, skipping data upload");
        led_manager_led_on_for(LED_ERROR_DEVICE_IS_DISABLED_DURATION_MS, LED_ERROR);
        play_beeps_async(5, 100, 100); // Three beeps for error
        return;
    }

    int err = app_queue_data();
    if (err < 0)
    {
        LOG_ERR("Failed to queue data after quick press, err %d", err);
    }

    // quick beep to show trying

    err = cloud_send_data_now();
    if (err == 0)
    {
        // Data sent successfully
        led_manager_led_on_for(LED_SUCCESS_DURATION_MS, LED_SUCCESS);
        play_beeps_async(1, 100, 100);
    }
    else if (err == -ENOTCONN)
    {
        // Device is connecting
        led_manager_led_on_for(LED_SUCCESS_DEVICE_CONNECTING_DURATION_MS, LED_SUCCESS);
        play_beeps_async(1, 1000, 100);
    }
}