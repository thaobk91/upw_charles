#include "app.h"
#include "algo_hw.h"
#include "magnetometer.h"
#include "magnetometer_controller.h"
#include "twi_driver.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mag, CONFIG_APP_LOG_LEVEL);

static const struct gpio_dt_spec mag_int = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(magint), gpios, 0);
static const struct gpio_dt_spec mag_pwr = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(magpower), gpios, 0);
static const struct gpio_dt_spec mag2_pwr = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(mag2power), gpios, 0);

static struct gpio_callback mag_int_cb;

static MagnetometerInterruptHandler interruptHandler = NULL;

static void mag_int_cb_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (interruptHandler != NULL)
    {
        interruptHandler();
    }
    app_check_in();
}

void magnetometer_reset(void)
{
    if (!device_is_ready(mag_pwr.port))
    {
        LOG_ERR("Error: gpio device %s is not ready",
                mag_pwr.port->name);
        return;
    }

    gpio_pin_configure_dt(&mag_pwr, GPIO_OUTPUT);
    gpio_pin_configure_dt(&mag2_pwr, GPIO_OUTPUT);

    gpio_pin_set_dt(&mag_pwr, 0);
    gpio_pin_set_dt(&mag2_pwr, 0);
    HAL_Delay(20);
    gpio_pin_set_dt(&mag_pwr, 1);
    gpio_pin_set_dt(&mag2_pwr, 1);
    HAL_Delay(20);
}

void magnetometer_start_interrupt(MagnetometerInterruptHandler inputInterruptHandler, HW_INTERRUPT_TRIGGER_t interruptMode)
{
    int err = 0;
    interruptHandler = inputInterruptHandler;

    if (!device_is_ready(mag_int.port))
    {
        LOG_ERR("Error: gpio device %s is not ready",
                mag_int.port->name);
        return;
    }

    err = gpio_pin_configure_dt(&mag_int, GPIO_INPUT);
    if (err != 0)
    {
        LOG_ERR("Error %d: failed to configure %s pin %d",
                err, mag_int.port->name, mag_int.pin);
        return;
    }

    gpio_flags_t trigger;

    switch (interruptMode)
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
    if (err != 0)
    {
        LOG_ERR("Error %d: failed to configure interrupt on %s pin %d",
                err, mag_int.port->name, mag_int.pin);
        return;
    }

    LOG_INF("Interrupt started");
    return;
}

void magnetometer_stop_interrupt(void)
{
    if (!device_is_ready(mag_int.port))
    {
        LOG_ERR("Error: gpio device %s is not ready",
                mag_int.port->name);
        return;
    }

    if (mag_int_cb.handler == NULL)
    {
        LOG_WRN("stop_interrupt interrupt was not started");
        return;
    }
    gpio_remove_callback(mag_int.port, &mag_int_cb);

    // Disable GPIO since it's not being used
    gpio_pin_configure_dt(&mag_int, GPIO_DISCONNECTED);
}

void magnetometer_reset_comms(void)
{
    twi_reset();
}

bool magnetometer_int_pin_no_pull(void)
{
    int err = 0;

    if (!device_is_ready(mag_int.port))
    {
        LOG_ERR("Error: gpio device %s is not ready",
                mag_int.port->name);
        return false;
    }

    // Put pin back in output no pull mode
    err = gpio_pin_configure_dt(&mag_int, GPIO_INPUT);
    if (err != 0)
    {
        LOG_ERR("Error %d: failed to configure %s pin %d",
                err, mag_int.port->name, mag_int.pin);
        return false;
    }
    return true;
}

bool magnetometer_int_pin_pull_up(void)
{
    int err = 0;

    if (!device_is_ready(mag_int.port))
    {
        LOG_ERR("Error: gpio device %s is not ready",
                mag_int.port->name);
        return false;
    }

    err = gpio_pin_configure_dt(&mag_int, GPIO_INPUT | GPIO_PULL_UP);
    if (err != 0)
    {
        LOG_ERR("Error %d: failed to configure %s pin %d",
                err, mag_int.port->name, mag_int.pin);
        return false;
    }
    return true;
}

bool magnetometer_int_pin_pull_down(void)
{
    int err = 0;

    if (!device_is_ready(mag_int.port))
    {
        LOG_ERR("Error: gpio device %s is not ready",
                mag_int.port->name);
        return false;
    }

    err = gpio_pin_configure_dt(&mag_int, GPIO_INPUT | GPIO_PULL_DOWN);
    if (err != 0)
    {
        LOG_ERR("Error %d: failed to configure %s pin %d",
                err, mag_int.port->name, mag_int.pin);
        return false;
    }
    return true;
}

bool magnetometer_read_interrupt(void)
{
    // Read the interrupt pin
    return gpio_pin_get(mag_int.port, mag_int.pin);
}
