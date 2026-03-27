#ifndef __ALGO_HW_H__
#define __ALGO_HW_H__

#include <stdint.h>
#include <stdbool.h>
#include "component.h"

typedef enum {
  INTERRUPT_TRIGGER_RISING,
  INTERRUPT_TRIGGER_FALLING,
  INTERRUPT_TRIGGER_RISING_FALLING
} HW_INTERRUPT_TRIGGER_t;

typedef enum {
  HW_GPIO_PIN_STATE_RESET = 0,
  HW_GPIO_PIN_STATE_SET
} HW_GPIO_PIN_STATE_t;

typedef void( *hw_interrupt_handler )( void );

// The base unit of the RTS is a tick, like a clock tick.
uint32_t HW_RTC_GetTimerValue( void );
uint32_t HW_RTC_GetTimerValue_MS( void );
uint32_t HW_RTC_Tick2ms( uint32_t tick );
uint32_t HW_RTC_ms2Tick( uint32_t ms );
uint32_t HW_RTC_SetTimerContext( void );
uint32_t HW_RTC_GetTimerContext( void );
uint32_t HW_RTC_GetMinimumTimeout( void );
uint32_t HW_RTC_GetTimerElapsedTime( void );
void HW_RTC_SetAlarm( uint32_t alarm );
void HW_RTC_StopAlarm( void );

void HW_GPIO_SetInterruptOnPin(HW_INTERRUPT_TRIGGER_t trigger, hw_interrupt_handler interrupt_handler );

HW_GPIO_PIN_STATE_t HW_GPIO_GetInterruptState(void);

/**
 * @brief This function returns the number of ticks in ms.
 * @return returns the tick converted in ms.
 * @note Not used int he current algorithm
*/
uint32_t HAL_GetTick( void );

void HAL_Delay(uint32_t Delay);

#endif // __ALGO_HW_H__
