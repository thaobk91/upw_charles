#include "button_manager.h"
#include "flash_utility.h"
#include "interaction_manager.h"
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(button_manager, CONFIG_APP_LOG_LEVEL);

#define BUTTON_NODE DT_ALIAS(power_button)

#if !DT_NODE_HAS_STATUS(BUTTON_NODE, okay)
#error "Unsupported board: powerbutton devicetree alias is not defined"
#endif

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(BUTTON_NODE, gpios, {0});
extern bool device_enabled; // Use the global state from main.c
static int64_t button_press_time = 0;
static struct gpio_callback button_cb_data;
static device_action_t enable_device;
static device_action_t disable_device;
static struct k_timer long_press_timer; // Timer for long press detection
static struct k_timer debounce_timer;   // Timer for debounce
static int last_stable_state = -1;      // Track last stable button state

#define DEBOUNCE_TIME_MS 100

// Work queue items for button actions
static struct k_work quick_press_work;
static struct k_work long_press_work;

// Work handlers
static void quick_press_work_handler(struct k_work *work)
{
    interaction_manager_quick_press();
}

static void long_press_work_handler(struct k_work *work)
{
#if CONFIG_PIPE_MONITOR_BOARD
    if (device_enabled)
    {
        LOG_INF("Cannot disable a pipe monitor board");
        return;
    }
#elif !CONFIG_PROVISIONING
    if (device_enabled)
    {
        LOG_INF("Rebooting..");
        sys_reboot(0);
        return;
    }
#endif

    device_enabled = !device_enabled;

    LOG_INF("Device power state changed to: %s", device_enabled ? "ON" : "OFF");
    FlashUtility_SaveDeviceState(device_enabled);

    if (device_enabled)
    {
        if (enable_device != NULL)
        {
            enable_device();
        }
        else
        {
            LOG_ERR("No enable function defined");
        }
    }
    else
    {
        if (disable_device != NULL)
        {
            disable_device();
        }
        else
        {
            LOG_ERR("No disable function defined");
        }
    }
}

// Timer callback for debounce - checks if button state is stable
static void debounce_timer_handler(struct k_timer *timer)
{
    int button_state = gpio_pin_get_dt(&button);

    // Only process if state has changed from last stable state
    if (button_state == last_stable_state)
    {
        LOG_INF("Button state hasn't changed, ignore");
        return; // State hasn't changed, ignore
    }

    LOG_INF("Stable button state: %d", button_state);
    last_stable_state = button_state;

    if (button_state)
    { // Button pressed (active high)
        button_press_time = k_uptime_get();
        // Start the long press timer
        if (device_enabled)
        {
            LOG_INF("Starting long press timer for disable");
            k_timer_start(&long_press_timer, K_MSEC(LONG_PRESS_TIMEOUT_MS_DISABLE), K_NO_WAIT);
        }
        else
        {
            k_timer_start(&long_press_timer, K_MSEC(LONG_PRESS_TIMEOUT_MS_ENABLE), K_NO_WAIT);
        }
    }
    else
    { // Button released
        if (button_press_time == 0)
        {
            return;
        }

        // Cancel the long press timer
        k_timer_stop(&long_press_timer);

        int64_t duration = k_uptime_delta(&button_press_time);
        LOG_INF("Button duration: %lld", duration);

        if (duration < QUICK_PRESS_MAX_DURATION_MS)
        {
            // Quick press - schedule immediate data upload work
            k_work_submit(&quick_press_work);
        }
    }
}

// Timer callback for long press
static void long_press_timer_handler(struct k_timer *timer)
{
    LOG_INF("Long press timer expired");
    // Submit long press work directly after timeout
    k_work_submit(&long_press_work);
}

static void button_changed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Restart debounce timer on any interrupt
    // After 100ms, we'll check if the button state is stable
    k_timer_start(&debounce_timer, K_MSEC(DEBOUNCE_TIME_MS), K_NO_WAIT);
}

void button_manager_init(device_action_t enable_func, device_action_t disable_func)
{
    LOG_INF("Button manager init");
    int ret;

    enable_device = enable_func;
    disable_device = disable_func;

    // Initialize work queue items
    k_work_init(&quick_press_work, quick_press_work_handler);
    k_work_init(&long_press_work, long_press_work_handler);

    // Initialize timers
    k_timer_init(&debounce_timer, debounce_timer_handler, NULL);
    k_timer_init(&long_press_timer, long_press_timer_handler, NULL);

    if (!device_is_ready(button.port))
    {
        LOG_ERR("Error: button device %s is not ready", button.port->name);
        return;
    }

    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret != 0)
    {
        LOG_ERR("Error %d: failed to configure %s pin %d", ret, button.port->name, button.pin);
        return;
    }

    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
    if (ret != 0)
    {
        LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", ret, button.port->name, button.pin);
        return;
    }

    gpio_init_callback(&button_cb_data, button_changed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    LOG_INF("Button manager initialized. Button set up at %s pin %d", button.port->name, button.pin);
}
