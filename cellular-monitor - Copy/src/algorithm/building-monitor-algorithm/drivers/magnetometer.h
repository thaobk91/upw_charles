/**
 ******************************************************************************
 * @file    magnetometer.h
 * @author  MEMS Application Team
 * @version V3.0.0
 * @date    12-August-2016
 * @brief   This header file contains the functions prototypes for the
 *          magnetometer driver
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAGNETOMETER_H
#define __MAGNETOMETER_H

#ifdef __cplusplus
extern "C" {
#endif



/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include "sensor.h"
#include "algo_hw.h"

/** @addtogroup BSP BSP
 * @{
 */

/** @addtogroup COMPONENTS COMPONENTS
 * @{
 */

/** @addtogroup COMMON COMMON
 * @{
 */

/** @addtogroup MAGNETOMETER MAGNETOMETER
 * @{
 */

/** @addtogroup MAGNETOMETER_Public_Types MAGNETOMETER Public types
 * @{
 */
 
typedef enum
{
  SENSOR_AXIS_X = 0,
  SENSOR_AXIS_Y = 1,
  SENSOR_AXIS_Z = 2,
} SENSOR_AXIS_t;

typedef enum
{
  SENSOR_TOGGLE_ENABLE = 0,
  SENSOR_TOGGLE_DISABLE = 1
} SENSOR_TOGGLE_t;

typedef enum {
  MAG_SOURCE_POSITIVE = 1,
  MAG_SOURCE_NEGATIVE = -1,
  MAG_SOURCE_NONE = 0
} MAG_SOURCE_t;

typedef enum { 
  MAG_OPERATING_MODE_CONTINUOUS = 0,
  MAG_OPERATING_MODE_SINGLE = 1,
  MAG_OPERATING_MODE_POWER_DOWN = 2,
  MAG_OPERATING_MODE_POWER_DOWN_AUTO = 3
} MAG_OPERATING_MODE_t;

/**
 * @brief  MAGNETOMETER driver structure definition
 */
typedef struct
{
  DrvStatusTypeDef ( *Init            ) ( DrvContextTypeDef* );
  DrvStatusTypeDef ( *DeInit          ) ( DrvContextTypeDef* );
  DrvStatusTypeDef ( *Sensor_Enable   ) ( DrvContextTypeDef* );
  DrvStatusTypeDef ( *Sensor_Disable  ) ( DrvContextTypeDef* );
  DrvStatusTypeDef ( *Get_WhoAmI      ) ( DrvContextTypeDef*, uint8_t* );
  DrvStatusTypeDef ( *Check_WhoAmI    ) ( DrvContextTypeDef* );
  DrvStatusTypeDef ( *Get_Axes        ) ( DrvContextTypeDef*, SensorAxes_t* );
	DrvStatusTypeDef ( *Get_Axis     		) ( DrvContextTypeDef*, SENSOR_AXIS_t, int16_t* );
  DrvStatusTypeDef ( *Get_AxesRaw     ) ( DrvContextTypeDef*, SensorAxesRaw_t* );
  DrvStatusTypeDef ( *Get_AxisRaw     ) ( DrvContextTypeDef*, SENSOR_AXIS_t, int16_t* );
  DrvStatusTypeDef ( *Get_Sensitivity ) ( DrvContextTypeDef*, float* );
  DrvStatusTypeDef ( *Get_ODR         ) ( DrvContextTypeDef*, float* );
  DrvStatusTypeDef ( *Set_ODR         ) ( DrvContextTypeDef*, SensorOdr_t );
  DrvStatusTypeDef ( *Set_ODR_Value   ) ( DrvContextTypeDef*, float );
  DrvStatusTypeDef ( *Get_FS          ) ( DrvContextTypeDef*, float* );
  DrvStatusTypeDef ( *Set_FS          ) ( DrvContextTypeDef*, SensorFs_t );
  DrvStatusTypeDef ( *Set_FS_Value    ) ( DrvContextTypeDef*, float );
  DrvStatusTypeDef ( *Read_Reg        ) ( DrvContextTypeDef*, uint8_t, uint8_t* );
  DrvStatusTypeDef ( *Write_Reg       ) ( DrvContextTypeDef*, uint8_t, uint8_t );
  DrvStatusTypeDef ( *Get_DRDY_Status ) ( DrvContextTypeDef*, uint8_t* );
	DrvStatusTypeDef ( *Set_Threshold		) ( DrvContextTypeDef*, uint16_t );
	DrvStatusTypeDef ( *Get_Threshold		) ( DrvContextTypeDef*, uint16_t* );
  DrvStatusTypeDef ( *Get_Source			) ( DrvContextTypeDef*, MAG_SOURCE_t*, MAG_SOURCE_t*, MAG_SOURCE_t* );
	DrvStatusTypeDef ( *Set_Interrupt_Source) ( DrvContextTypeDef*, SENSOR_AXIS_t );
	DrvStatusTypeDef ( *Set_Offset) ( DrvContextTypeDef*, SENSOR_AXIS_t, int16_t );
  DrvStatusTypeDef ( *Get_Offset) ( DrvContextTypeDef*, SENSOR_AXIS_t, int16_t* );
  DrvStatusTypeDef ( *Interrupt_Enable ) ( DrvContextTypeDef*, SENSOR_TOGGLE_t );
  DrvStatusTypeDef ( *Set_LPF ) ( DrvContextTypeDef*, SENSOR_TOGGLE_t );
  DrvStatusTypeDef ( *Set_Operating_Mode ) ( DrvContextTypeDef*, MAG_OPERATING_MODE_t );
  DrvStatusTypeDef ( *Set_DRDY_on_PIN ) ( DrvContextTypeDef*, SENSOR_TOGGLE_t );
  DrvStatusTypeDef ( *Set_INT_on_PIN ) ( DrvContextTypeDef*, SENSOR_TOGGLE_t );
  DrvStatusTypeDef ( *Get_PIN_Status ) ( DrvContextTypeDef*, bool* );
} MAGNETO_Drv_t;

/* Defines -------------------------------------------------------------------*/
/**
 * @brief  MAGNETOMETER data structure definition
 */
typedef struct
{
  void *pComponentData; /* Component specific data. */
  void *pExtData;       /* Other data. */
} MAGNETO_Data_t;

extern const float magnetometer_max_datarate;

#ifdef USE_MAGNETOMETER

typedef void( *MagnetometerInterruptHandler )( void );
   
void  magnetometer_reset( void );

void  magnetometer_start_interrupt( MagnetometerInterruptHandler interruptHandler, HW_INTERRUPT_TRIGGER_t interruptMode );

void magnetometer_stop_interrupt ( void );

void magnetometer_reset_comms( void );

/** Returns: True if successful **/
bool magnetometer_int_pin_pull_up( void );

/** Returns: True if successful **/
bool magnetometer_int_pin_no_pull( void );

/** Returns: True if successful **/
bool magnetometer_int_pin_pull_down( void );

bool magnetometer_read_interrupt( void);

#endif

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */


#ifdef __cplusplus
}
#endif

#endif /* __MAGNETOMETER_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
