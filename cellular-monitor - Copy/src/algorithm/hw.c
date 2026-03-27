#include <zephyr/kernel.h>

#include "app.h"
#include "algo_hw.h"
#include "timeServer.h"
#include "twi_driver.h" // For Sensor_IO_Write() and Sensor_IO_Read()

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(hw, CONFIG_APP_LOG_LEVEL);

DrvContextTypeDef handle;

static uint32_t timerContext = 0;


static void timer_event_handler(struct k_timer *timer_id)
{
    TimerIrqHandler();
    app_check_in();
}

K_TIMER_DEFINE(hw_timer, timer_event_handler , NULL);

uint32_t HW_RTC_GetTimerValue( void )
{
    uint32_t timerVal = sys_clock_tick_get_32();
    //LOG_INF("HW_RTC_GetTimerValue()=%d", timerVal);
    return timerVal;
}

uint32_t HW_RTC_GetTimerValue_MS( void )
{
    return HW_RTC_Tick2ms(HW_RTC_GetTimerValue());
}

uint32_t HW_RTC_ms2Tick( uint32_t ms )
{
    // Floor so it doesn't give a time in the future
    return k_ms_to_ticks_floor32(ms);
}

uint32_t HW_RTC_Tick2ms( uint32_t tick)
{
    // Doesn't matter as much because we're going from more to less precise
    return k_ticks_to_ms_floor32(tick);
}

uint32_t HW_RTC_SetTimerContext( void )
{
    timerContext = HW_RTC_GetTimerValue();
    return timerContext;
}

uint32_t HW_RTC_GetTimerContext( void )
{
    return timerContext;
}

uint32_t HW_RTC_GetMinimumTimeout( void )
{
    return 1;
}

// Supposed to return ticks
uint32_t HW_RTC_GetTimerElapsedTime( void )
{
    return HW_RTC_GetTimerValue() - timerContext;
}

/**
 * @brief Set the alarm
 * @param time in system ticks
*/
void HW_RTC_SetAlarm( uint32_t time){
    uint32_t ticks = time - HW_RTC_GetTimerElapsedTime();
    //LOG_INF("HW_RTC_SetAlarm(%d) ms=%d ticks=%d", time, HW_RTC_Tick2ms(time), ticks);
    k_timer_start(&hw_timer, K_TICKS(ticks), K_NO_WAIT);
}

void HW_RTC_StopAlarm( void )
{
    //LOG_INF("HW_RTC_StopAlarm()");
    k_timer_stop(&hw_timer);
}

/**
 * 
 * @brief This function provides a delay in milliseconds.
 *
 * The `HAL_Delay` function is used to introduce a delay in the program execution for a specified number of milliseconds.
 * It is commonly used in embedded systems to control timing and synchronization between different parts of the system.
 *
 * @param milliseconds The number of milliseconds to delay.
 *
 * @note This function is implemented by the hardware abstraction layer (HAL) and its actual behavior may vary depending on the target platform.
 *       It is important to consult the documentation of the specific HAL implementation for accurate timing information.
*/
void HAL_Delay(uint32_t delay_ms)
{
    k_sleep(K_MSEC(delay_ms));
}

/** I2C Device Address 8 bit format **/
#define LIS2MDL_I2C_ADD     0x1EU // 0011110 is the slave address. 0x3DU was what was originally here

uint8_t Sensor_IO_Write( void *handle, uint8_t WriteAddr, uint8_t *pBuffer, uint16_t nBytesToWrite )
{
    return twi_write(LIS2MDL_I2C_ADD, WriteAddr, pBuffer, nBytesToWrite);
}

uint8_t Sensor_IO_Read( void *handle, uint8_t ReadAddr, uint8_t *pBuffer, uint16_t nBytesToRead )
{
    return twi_read(LIS2MDL_I2C_ADD, ReadAddr, pBuffer, nBytesToRead);
}
