#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "algorithm.h"
#include "cycle_counter.h"
#include "pulse_tracker.h"
#include "timeServer.h"
#include "log.h"
#include "algo_hw.h"

ALGORITHM_t Pulse_Tracker = {
  pulse_tracker_init,
  pulse_tracker_check_in,
  pulse_tracker_reset,
  pulse_tracker_log,
  pulse_tracker_is_configured,
  pulse_tracker_update_variable,
  pulse_tracker_enable,
  pulse_tracker_disable
};

static TimerEvent_t FeedDog;
static void FeedDogEvent( void )
{
	TimerReset( &FeedDog );
}

// Timer for debouncing
static TimerEvent_t DebounceTimerPin1;

// MARK: Configuration values

// Time to wait before accepting a ping change without more changes.
static unsigned int debounce_time_ms = 1; // TUF-2000B has a 6ms pulse width

// MARK : Flags
// Flag for when a pin changes
static volatile bool StateChangedPin1 = false;

// Flag for when a debounce timer expires 
static volatile bool DebounceTimerFiredPin1 = false;

static HW_GPIO_PIN_STATE_t lastSettledStatePin1 = HW_GPIO_PIN_STATE_RESET;

// Internal function
// Handle pin changed
static void HandlePin1StateChanged( void );
// Handle debounce timer expired
static void DebounceTimerPin1Event( void );
static void ProcessPinDebounceTimerFired(HW_GPIO_PIN_STATE_t *lastSettledState);

void pulse_tracker_init( void )
{
	PRINTF_TS("pulse_tracker_init\n");
	cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);

	// better way to deal with WDT?
	TimerInit(&FeedDog, FeedDogEvent);
	TimerSetValue(&FeedDog, 1000);
	TimerStart(&FeedDog);

	TimerInit(&DebounceTimerPin1, DebounceTimerPin1Event);
	TimerSetValue(&DebounceTimerPin1, debounce_time_ms);
	HW_GPIO_SetInterruptOnPin(INTERRUPT_TRIGGER_RISING_FALLING, HandlePin1StateChanged);

	lastSettledStatePin1 = HW_GPIO_GetInterruptState(); // get initial state
	PRINTF_TS("pulse_tracker_init lastSettledStatePin1 %d\n", lastSettledStatePin1);
}

void pulse_tracker_reset( void )
{
}

void pulse_tracker_log( void )
{
	lastSettledStatePin1 = HW_GPIO_GetInterruptState(); // get initial state
	PRINTF_TS("pulse_tracker_log lastSettledStatePin1 %d hc %d\n", lastSettledStatePin1, cycle_counter_get_half_cycles());
}

void pulse_tracker_check_in( void )
{
	if( StateChangedPin1 )
	{
		PRINTF_TS("pulse_tracker_check_in %d\n", HW_GPIO_GetInterruptState());
		StateChangedPin1 = false;
		TimerReset( &DebounceTimerPin1 );
	}
	
	if ( DebounceTimerFiredPin1 ) 
	{
		DebounceTimerFiredPin1 = false;
		ProcessPinDebounceTimerFired(&lastSettledStatePin1);
	}
}

bool pulse_tracker_is_configured( void )
{
	return true;
}

void pulse_tracker_update_variable(  uint8_t variable, uint32_t value  )
{
	switch (variable)
	{
		case 21:
			PRINTF_TS("debounce_time_ms to %d\n", value);
			debounce_time_ms = value;
			TimerSetValue(&DebounceTimerPin1, debounce_time_ms);
			break;
		default:
			break;
	}
	PRINTF_TS("pulse_tracker_update_variable %d %d\n", variable, value);
}

void pulse_tracker_enable( void )
{

}

void pulse_tracker_disable( void )
{

}

// MARK: IRQ Pin changed handlers
static void HandlePin1StateChanged( void )
{
	StateChangedPin1 = true;
}

static void DebounceTimerPin1Event( void )
{
	DebounceTimerFiredPin1 = true;
}

static void ProcessPinDebounceTimerFired(HW_GPIO_PIN_STATE_t *lastSettledState)
{
	HW_GPIO_PIN_STATE_t currentState = HW_GPIO_GetInterruptState();
	PRINTF_TS("Pin current %d last %d \n\r", currentState, *lastSettledState);
	if (currentState != *lastSettledState)
	{
		// Only trigger new pulse if it has changed state since last settled state
		cycle_counter_add_half_cycle(true);
	}
	*lastSettledState = currentState;
}