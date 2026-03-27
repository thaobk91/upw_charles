#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "app.h"
#include "algo_hw.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(hw_gpio, CONFIG_APP_LOG_LEVEL);

static const struct gpio_dt_spec mag_int = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(magint), gpios, 0);
static const struct gpio_dt_spec mag_pwr = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(magpower), gpios, 0);
static const struct gpio_dt_spec mag2_pwr = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(mag2power), gpios, 0);

static struct gpio_callback mag_int_cb;
static hw_interrupt_handler interruptHandler = NULL;

static void mag_int_cb_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (interruptHandler != NULL)
    {
        interruptHandler();
    }
    app_check_in();
}

void HW_GPIO_SetInterruptOnPin(HW_INTERRUPT_TRIGGER_t inputTrigger, hw_interrupt_handler _interrupt_handler )
{
    int err = 0;
    
    interruptHandler = _interrupt_handler;

    if (!device_is_ready(mag_pwr.port))
    {
        LOG_ERR("Error: gpio device %s is not ready",
                mag_pwr.port->name);
        return;
    }

    // Have to bring power high for pulse with Sensus OMNI meters to work
    gpio_pin_configure_dt(&mag_pwr, GPIO_OUTPUT);
    gpio_pin_configure_dt(&mag2_pwr, GPIO_OUTPUT);

    gpio_pin_set_dt(&mag_pwr, 1);
    gpio_pin_set_dt(&mag2_pwr, 1);

    if (!device_is_ready(mag_int.port)) {
        LOG_ERR("Error: gpio device %s is not ready", mag_int.port->name);
        return;
    }

    err = gpio_pin_configure_dt(&mag_int, GPIO_INPUT | GPIO_PULL_UP);
    if (err != 0) {
        LOG_ERR("Error %d: failed to configure %s pin %d",
            err, mag_int.port->name, mag_int.pin);
        return;
    }

    gpio_flags_t trigger;

    switch(inputTrigger)
    {
        case INTERRUPT_TRIGGER_RISING:
            trigger = GPIO_INT_EDGE_TO_ACTIVE;
            break;
        case INTERRUPT_TRIGGER_FALLING:

            trigger = GPIO_INT_EDGE_TO_INACTIVE;
            break;
        case INTERRUPT_TRIGGER_RISING_FALLING:
            trigger = GPIO_INT_EDGE_BOTH;
            break;
        default:
            LOG_ERR("Error: invalid interrupt mode");
            return;
    }

    gpio_init_callback(&mag_int_cb, mag_int_cb_handler, BIT(mag_int.pin));
    gpio_add_callback(mag_int.port, &mag_int_cb);

    err = gpio_pin_interrupt_configure_dt(&mag_int, trigger);
    if (err != 0) {
        LOG_ERR("Error %d: failed to configure interrupt on %s pin %d",
            err, mag_int.port->name, mag_int.pin);
        return;
    }

    LOG_INF("Interrupt started");
    return;
}

HW_GPIO_PIN_STATE_t HW_GPIO_GetInterruptState(void)
{
    if ( gpio_pin_get(mag_int.port, mag_int.pin))
    {
        return HW_GPIO_PIN_STATE_SET;
    }
    else
    {
        return HW_GPIO_PIN_STATE_RESET;
    }
}