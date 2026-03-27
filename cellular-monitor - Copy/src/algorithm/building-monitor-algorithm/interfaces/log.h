#ifndef __LOG_H__
#define __LOG_H__

#include "log_implementation.h"

// log_implementation.h should define the following:
// Base printf functions that should always print if called, these should be defined as part of the interface
//#define PRINTF_BASE(...)
// Polling version
//#define PPRINTF_BASE(...)
// Adding timestamp
//#define PRINTF_BASE_TS(...)

// The following should not be defined:
#ifdef DISABLE_LOGGING
#define PRINTF(...)
#define PPRINTF(...)
#define PRINTF_TS(...)
#else
#define PRINTF(...) PRINTF_BASE(__VA_ARGS__)
#define PPRINTF(...)     PPRINTF_BASE(__VA_ARGS__)
#define PRINTF_TS(...) PRINTF_BASE_TS(__VA_ARGS__)
#endif


#endif // __LOG_H__
