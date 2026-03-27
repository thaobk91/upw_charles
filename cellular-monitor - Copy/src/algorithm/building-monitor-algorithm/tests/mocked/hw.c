#include <time.h>
#include "algorithm.h"
#include "algo_hw.h"
#include "timeServer.h"
#include "log.h"
#include "mocked_app.h"

bool eventBased = true; // otherwise go of clock

uint64_t clockTime = 0;
uint32_t alarmTime = 0;
uint32_t timerContext = 0;


static void timer_event_handler(struct k_timer *timer_id)
{
    TimerIrqHandler();
    mocked_app_set_flag();
}

static uint16_t speedMultiplier =  1; // At 10 it's the same, at 100 it starts to lose accuracy, 1000 is a bit less

void simulation_speed_multiplier( uint32_t multiplier )
{
    speedMultiplier = multiplier;
}

void simulation_checkin( void )
{
    if (eventBased)
    {
        clockTime += alarmTime;
    }
    // PRINTF_BASE("checkin %llu %d %llu\n", clockTime, eventBased, alarmTime);
    if (alarmTime != 0 && HW_RTC_GetTimerValue() >= timerContext + alarmTime)
    {
        alarmTime = 0;
        timer_event_handler(NULL);
    }
}

uint32_t HW_RTC_GetTimerValue( void )
{
    if (eventBased) {
        return clockTime;
    }
    else {
        return clock() * speedMultiplier;
    }
}

uint32_t HW_RTC_GetTimerValue_MS( void )
{
    return HW_RTC_Tick2ms(HW_RTC_GetTimerValue());
}

uint32_t HW_RTC_ms2Tick( uint32_t ms )
{
    return ms * CLOCKS_PER_SEC / 1000;
}

uint32_t HW_RTC_Tick2ms( uint32_t tick)
{
    return tick * 1000.0f / CLOCKS_PER_SEC;
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

void HW_RTC_SetAlarm( uint32_t time){
    alarmTime = time;
}

void HW_RTC_StopAlarm( void )
{
    alarmTime = 0;
}

void HAL_Delay(uint32_t Delay)
{
    uint32_t delayTicks = HW_RTC_ms2Tick(Delay);

    if (eventBased) {
        clockTime += delayTicks;
    } else {
        uint32_t start = HW_RTC_GetTimerValue();
        while((HW_RTC_GetTimerValue() - start) < delayTicks)
        {
        }
    }
}
