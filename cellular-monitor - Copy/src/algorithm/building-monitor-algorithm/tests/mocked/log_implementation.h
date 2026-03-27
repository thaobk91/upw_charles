#ifndef __LOG_IMPLEMENTATION_H__
#define __LOG_IMPLEMENTATION_H__

#include <stdio.h>
#include "algo_hw.h"

#define PRINTF_BASE(...)     printf(__VA_ARGS__)
#define PPRINTF_BASE(...)    printf(__VA_ARGS__)
#define PRINTF_BASE_TS(...)     printf("[%d.%d] ", HW_RTC_GetTimerValue_MS()/1000, HW_RTC_GetTimerValue_MS()), printf(__VA_ARGS__)


#endif // __LOG_IMPLEMENTATION_H__