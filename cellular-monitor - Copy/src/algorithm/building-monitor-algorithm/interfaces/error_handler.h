/* 
  Handlers for alerts and errors
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ERROR_HANDLER_H__
#define __ERROR_HANDLER_H__

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/
typedef enum {
  ERROR_CODE_NO_ERROR = 0x0,
  ERROR_CODE_NO_ERROR_RESET = 0x90,
  ERROR_CODE_HW = 0x1,
  ERROR_CODE_HW_BSP = 0x10,
  ERROR_CODE_HW_BSP_INTERRUPT_NOT_CONNECTED = 0x11,
  ERROR_CODE_HAL = 0x2,
  ERROR_CODE_SPI = 0x3,
  ERROR_CODE_I2C = 0x4,
  ERROR_CODE_ALGORITHM = 0x5,
  ERROR_CODE_ALGORITHM_CONFIG = 0x50,
  ERROR_CODE_ALGORITHM_CONFIG_RESET = 0x51,
  ERROR_CODE_ALGORITHM_CONFIG_SET = 0x52, // Not really an error code as much as a debug code
  ERROR_CODE_ALGORITHM_TIMEOUT = 0x53,
  ERROR_CODE_ALGORITHM_FAILED_TO_START = 0x54,
  ERROR_CODE_ALGORITHM_CONFIURING_RESET = 0x55, // Reset during configuration
  ERROR_CODE_ALGORITHM_SENSUS_ERROR = 0x56,
  ERROR_CODE_ALGORITHM_CONFIG_AXIS_RESET = 0x57,
  ERROR_CODE_ALGORITHM_CONFIG_AXIS_DONE_INFO = 0x58,
  ERROR_CODE_ALGORITHM_CONFIG_AXIS_NOT_DONE_INFO = 0x59,
  ERROR_CODE_VCOM = 0x6,
  ERROR_CODE_WATCHDOG = 0x7,
  ERROR_CODE_SYS_CLOCK = 0x8,
  // Network related
  ERROR_CODE_FAILED_LINKCHECK = 0xA0,
  ERROR_CODE_RECONNECTING = 0xA1,
  ERROR_CODE_NO_ERROR_DATA = 0x91,
  ERROR_CODE_RESET_REASON = 0xB0
} ERROR_CODE_t;
/* Exported constants --------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */ 

/*!
 * \brief Handles critical errors, sends an alert to the cloud then restarts the device
 *
 * \param instantReset Whether to instantly reset the device or do so after a confirmation
 */
void Error_Handler( ERROR_CODE_t errorCode, bool instantReset );

/*!
 * \brief Sends an alert to the cloud
 *
 * \param version The error API version. Allows for backwards compatibility
 */
void Alert_Data(ERROR_CODE_t errorCode, uint16_t version, int16_t* errorData, uint8_t size);

/*!
 * \brief Sends an alert to the cloud about why the device reset
 * \param reason The reason the device reset
*/
void Reset_Reason(int32_t reason);

#ifdef __cplusplus
}
#endif

#endif /* __ERROR_HANDLER_H__ */
