#include "algorithm.h"
#include "algo_hw.h"
#include "signal.h"
#include "log.h"
#include "mocked_app.h"

static void (*interruptHandler)(void);

static void mocked_hw_interrupt_handler( void )
{
    if (interruptHandler != NULL)
    {
        interruptHandler();
    }

    mocked_app_set_flag();
}

void HW_GPIO_SetInterruptOnPin(HW_INTERRUPT_TRIGGER_t trigger, void (*callback)(void) )
{
    interruptHandler = callback;
    Signal_Set_InterruptHandler(mocked_hw_interrupt_handler, trigger);
}

// TODO should sepearate this
HW_GPIO_PIN_STATE_t HW_GPIO_GetInterruptState(void)
{
    bool isSet;
    Signal_Get_Pin_Status(&isSet);
    if (isSet)
    {
        return HW_GPIO_PIN_STATE_SET;
    }
    else
    {
        return HW_GPIO_PIN_STATE_RESET;
    }
}
