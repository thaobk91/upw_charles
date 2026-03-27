#ifndef __ALGORITHM_H__
#define __ALGORITHM_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
  /* Called at startup up */
  void ( *Init ) ( void );
  /* Should be called every loop */
  void ( *Check_In )( void );
  /* Resets the algorithm */
  void ( *Reset )( void );
  /* Logs information about the current state of the algorithm for debugging*/
  void ( *Log )( void );
  /* Helper function to see if the algorithm has configured itself. */
  bool ( *Is_Configured )( void );
  /* Updates variables of the algorithm, used for debugging and OTA updates */
  void ( *Update_Variable )( uint8_t variable, uint32_t value );
  /* Enables the algorithm, it is default enabled. */
  void ( *Enable )( void );
  /* Disables the algorithm, mainly to save power. */
  void ( *Disable )( void );
  /* Set variables loaded from memory */
  void ( *Set_Variables )( uint32_t *variables, uint8_t length );
  /* Get variables to save to memory */
  void ( *Get_Variables )( uint32_t *variables, uint8_t *length );
} ALGORITHM_t;

/**
 * \brief Reasons that a configuration reset occurred
**/
typedef enum {
  RESET_REASON_NORMAL = 0,
  RESET_REASON_DIFF_TOO_SMALL = 1,
  RESET_REASON_DIFF_TOO_LARGE = 2,
  RESET_REASON_INVALID_VALUE = 3,
  RESET_REASON_MISSED_CYCLES = 4,
  RESET_REASON_BAD_PEAK = 5,
  RESET_REASON_BAD_VALLEY = 6,
  RESET_REASON_BAD_NOISE = 7, // Unused
  RESET_REASON_TIMEOUT = 8,
  RESET_REASON_MAX_FREQUENCY = 9,
  RESET_REASON_DIFF_BELOW_NOISE = 10,
  RESET_REASON_NOISE_DECREASE = 11,
  RESET_REASON_NO_NEW_PULSES = 12,
  RESET_REASON_NOISE_MONITOR_TIMEOUT = 13,
  RESET_REASON_NOISE_INCREASE = 14,
  RESET_REASON_NOISE_INCREASE_DURING_CONFIGURATION = 15,
  RESET_REASON_NOISE_DECREASE_DURING_CONFIGURATION = 16,
  RESET_REASON_BAD_CONFIGURATION = 17, // General one for bad configuration
  RESET_REASON_CONFIG_NULL = 18, // Configuration is null
  RESET_REASON_BAD_CONFIGURATION_WITH_LPF = 19 // Bad configuration with LPF
} RESET_REASON_t;

typedef enum {
  ALGORITHM_ERROR_NO_ERROR = 0x00,
  ALGORITHM_ERROR_REPEAT_VALUE = 0x01,
  ALGORITHM_ERROR_TIMEOUT = 0x02,
  ALGORITHM_FAILED_TO_START_INTERRUPT = 0x03,
  ALGORITHM_FAILED_TO_STOP_INTERRUPT = 0x04,
  ALGORITHM_FAILED_TO_INIT = 0x05,
  ALGORITHM_FAILED_TO_READ = 0x06,
  ALGORITHM_FAILED_TO_SET_LPF = 0x07,
  ALGORITHM_FAILED_TO_SET_OFFSET = 0x08,
  ALGORITHM_FAILED_TO_GET_OFFSET = 0x09,
  ALGORITHM_FAILED_TO_SET_THRESHOLD = 0x0A,
  ALGORITHM_FAILED_TO_SET_ODR = 0x0B,
  ALGORITHM_FAILED_TO_GET_SOURCE = 0x0C,
  ALGORITHM_FAILED_TO_SET_OPERATING_MODE = 0x0D,
  ALGORITHM_FAILED_NOISE_MONITOR_INIT = 0x0E,
  ALGORITHM_FAILED_NOISE_MONITOR_DEINIT = 0x0F,
  ALGORITHM_FAILED_COMMUNICATION = 0x10
} ALGORITHM_ERROR_t;

extern ALGORITHM_t Algorithm;
extern ALGORITHM_t Pulse_Tracker;
extern ALGORITHM_t Sensus_Protocol;

#if defined(ALGORITHM_MIN_MAX)
#include "cycle_tracker.h"
#endif

#if defined(ALGORITHM_FOLLOWER)
#include "follower_algorithm.h"
#endif

#if defined(ALGORITHM_POLL_AND_INT)
#include "signal_tracker.h"
#endif

#ifdef ALGORITHM_LOGGER
#include "logger_algorithm.h"
#endif

#endif /* __ALGORITHM_H__ */
