#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "led_manager.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(led_manager, CONFIG_APP_LOG_LEVEL);

// Queue data structures
typedef struct
{
    led_type_t type;
    uint32_t duration_ms;
} led_action_t;

#define MAX_LED_QUEUE_SIZE 5
static led_action_t led_queue[MAX_LED_QUEUE_SIZE];
static uint8_t queue_head = 0;
static uint8_t queue_tail = 0;
static uint8_t queue_count = 0;
static bool queue_processing = false;

static void timer_event_handler(struct k_timer *timer_id);
K_TIMER_DEFINE(led_timer, timer_event_handler, NULL);

static const struct gpio_dt_spec info_led = GPIO_DT_SPEC_GET_BY_IDX(DT_ALIAS(infoled), gpios, 0);
static const struct gpio_dt_spec success_led = GPIO_DT_SPEC_GET_BY_IDX(DT_ALIAS(successled), gpios, 0);
static const struct gpio_dt_spec error_led = GPIO_DT_SPEC_GET_BY_IDX(DT_ALIAS(errorled), gpios, 0);

// Forward declarations
static void led_manager_set(led_state_t state, led_type_t type);
static bool queue_is_empty(void);
static bool queue_is_full(void);
static bool queue_add(led_type_t type, uint32_t duration_ms);
static bool queue_remove(led_action_t *action);
static void process_next_led_action(void);

void led_manager_init(void)
{
    int err = 0;

    if (!device_is_ready(success_led.port))
    {
        LOG_ERR("conn_led not ready");
        return;
    }

    err = gpio_pin_configure_dt(&success_led, GPIO_OUTPUT_INACTIVE);
    if (err < 0)
    {
        LOG_ERR("conn_led not configured");
        return;
    }

    if (!device_is_ready(info_led.port))
    {
        LOG_ERR("conn_led not ready");
        return;
    }

    err = gpio_pin_configure_dt(&info_led, GPIO_OUTPUT_INACTIVE);
    if (err < 0)
    {
        LOG_ERR("conn_led not configured");
        return;
    }

    if (!device_is_ready(error_led.port))
    {
        LOG_ERR("conn_led not ready");
        return;
    }

    err = gpio_pin_configure_dt(&error_led, GPIO_OUTPUT_INACTIVE);
    if (err < 0)
    {
        LOG_ERR("conn_led not configured");
        return;
    }

    // Initialize queue
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    queue_processing = false;

    led_manager_set(LED_OFF, LED_NONE);
}

static void led_manager_set(led_state_t state, led_type_t type)
{
    LOG_DBG("led_manager type %d set %d", type, state);

    if (type == LED_NONE)
    {
        gpio_pin_set_dt(&success_led, 0);
        gpio_pin_set_dt(&info_led, 0);
        gpio_pin_set_dt(&error_led, 0);
        return;
    }

    switch (state)
    {
    case LED_OFF:
        switch (type)
        {
        case LED_SUCCESS:
            gpio_pin_set_dt(&success_led, 0);
            break;
        case LED_INFO:
            gpio_pin_set_dt(&info_led, 0);
            break;
        case LED_ERROR:
            gpio_pin_set_dt(&error_led, 0);
            break;
        default:
            break;
        }
        break;
    case LED_ON:
        switch (type)
        {
        case LED_SUCCESS:
            gpio_pin_set_dt(&success_led, 1);
            break;
        case LED_INFO:
            gpio_pin_set_dt(&info_led, 1);
            break;
        case LED_ERROR:
            gpio_pin_set_dt(&error_led, 1);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

static bool queue_is_empty(void)
{
    return queue_count == 0;
}

static bool queue_is_full(void)
{
    return queue_count >= MAX_LED_QUEUE_SIZE;
}

static bool queue_add(led_type_t type, uint32_t duration_ms)
{
    if (queue_is_full())
    {
        LOG_WRN("LED queue is full, dropping action");
        return false;
    }

    led_queue[queue_tail].type = type;
    led_queue[queue_tail].duration_ms = duration_ms;
    queue_tail = (queue_tail + 1) % MAX_LED_QUEUE_SIZE;
    queue_count++;

    LOG_DBG("Added LED action to queue: type=%d, duration=%d, queue_count=%d", type, duration_ms, queue_count);
    return true;
}

static bool queue_remove(led_action_t *action)
{
    if (queue_is_empty())
    {
        return false;
    }

    *action = led_queue[queue_head];
    queue_head = (queue_head + 1) % MAX_LED_QUEUE_SIZE;
    queue_count--;

    LOG_DBG("Removed LED action from queue: type=%d, duration=%d, queue_count=%d",
            action->type, action->duration_ms, queue_count);
    return true;
}

static void process_next_led_action(void)
{
    led_action_t action;

    if (queue_remove(&action))
    {
        LOG_DBG("Processing LED action: type=%d, duration=%d", action.type, action.duration_ms);
        queue_processing = true;
        led_manager_set(LED_ON, action.type);
        k_timer_start(&led_timer, K_MSEC(action.duration_ms), K_NO_WAIT);
    }
    else
    {
        queue_processing = false;
        LOG_DBG("LED queue is empty, stopping processing");
    }
}

void led_manager_led_on_for(uint32_t ms, led_type_t type)
{
    if (type == LED_NONE)
    {
        LOG_WRN("Invalid LED type LED_NONE, ignoring request");
        return;
    }

    LOG_INF("led_manager %d on for %d ms (queuing)", type, ms);

    // Add action to queue
    if (!queue_add(type, ms))
    {
        return; // Queue is full
    }

    // If not currently processing, start processing
    if (!queue_processing)
    {
        process_next_led_action();
    }
}

static void timer_event_handler(struct k_timer *timer_id)
{
    // Turn off all LEDs
    led_manager_set(LED_OFF, LED_NONE);

    // Process next action in queue
    process_next_led_action();
}
