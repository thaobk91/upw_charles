#include "error_handler.h"
#include "log.h"
#include "simulation.h"

static uint16_t alertCount = 0;

uint16_t get_alert_count( void )
{
    return alertCount;
}

void Alert_Data(ERROR_CODE_t errorCode, uint16_t version, int16_t* errorData, uint8_t size)
{
    alertCount++;
    PRINTF("Alert_Data errorCode:%d  version:%d  size:%d ", errorCode, version, size);
    // Print data
    for (int i = 0; i < size; i++)
    {
        PRINTF("[%d]:%d ", i, errorData[i]);
    }
    PRINTF("\n");
}

void Error_Handler( ERROR_CODE_t errorCode, bool instantReset )
{
    PRINTF_BASE("ERROR_HANDLER error:%d instantReset:%d\n", errorCode, instantReset);
    while(true) {
        
    }
}