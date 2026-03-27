/**
 *******************************************************************************
 * @file    LIS2MDL_MAG_driver_HL.c
 * @author  Charles Fayal
 * @version V3.0.0
 * @date    12-August-2016
 * @brief   This file provides a set of high-level functions needed to manage
            the LIS2MDL sensor, adapted from LIS3MDL
 *******************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT(c) 2021 NOWi Sensor LLC</center></h2>
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
#ifdef LIS2MDL
/* Includes ------------------------------------------------------------------*/
#include "LIS2MDL_MAG_driver_HL.h"
#include <math.h>
#include "log.h"
#include "magnetometer.h"

/** @addtogroup BSP BSP
 * @{
 */

/** @addtogroup COMPONENTS COMPONENTS
 * @{
 */

/** @addtogroup LIS2MDL LIS2MDL
 * @{
 */

/** @addtogroup LIS2MDL_Private_FunctionPrototypes Private function prototypes
 * @{
 */

static DrvStatusTypeDef LIS2MDL_Get_Axes_Raw( DrvContextTypeDef *handle, int16_t* pData );
static DrvStatusTypeDef LIS2MDL_Get_Axis_Raw( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t* pData );

/**
 * @}
 */

/** @addtogroup LIS2MDL_Callable_Private_FunctionPrototypes Callable private function prototypes
 * @{
 */

static DrvStatusTypeDef LIS2MDL_Init( DrvContextTypeDef *handle );
static DrvStatusTypeDef LIS2MDL_DeInit( DrvContextTypeDef *handle );
static DrvStatusTypeDef LIS2MDL_Sensor_Enable( DrvContextTypeDef *handle );
static DrvStatusTypeDef LIS2MDL_Sensor_Disable( DrvContextTypeDef *handle );
static DrvStatusTypeDef LIS2MDL_Get_WhoAmI( DrvContextTypeDef *handle, uint8_t *who_am_i );
static DrvStatusTypeDef LIS2MDL_Check_WhoAmI( DrvContextTypeDef *handle );
static DrvStatusTypeDef LIS2MDL_Get_Axes( DrvContextTypeDef *handle, SensorAxes_t *magnetic_field );
static DrvStatusTypeDef LIS2MDL_Get_Axis( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *value );
static DrvStatusTypeDef LIS2MDL_Get_AxesRaw( DrvContextTypeDef *handle, SensorAxesRaw_t *value );
static DrvStatusTypeDef LIS2MDL_Get_AxisRaw( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *value );
static DrvStatusTypeDef LIS2MDL_Get_Sensitivity( DrvContextTypeDef *handle, float *sensitivity );
static DrvStatusTypeDef LIS2MDL_Get_ODR( DrvContextTypeDef *handle, float *odr );
static DrvStatusTypeDef LIS2MDL_Set_ODR( DrvContextTypeDef *handle, SensorOdr_t odr );
static DrvStatusTypeDef LIS2MDL_Set_ODR_Value( DrvContextTypeDef *handle, float odr );
static DrvStatusTypeDef LIS2MDL_Read_Reg( DrvContextTypeDef *handle, uint8_t reg, uint8_t *data );
static DrvStatusTypeDef LIS2MDL_Write_Reg( DrvContextTypeDef *handle, uint8_t reg, uint8_t data );
static DrvStatusTypeDef LIS2MDL_Get_DRDY_Status( DrvContextTypeDef *handle, uint8_t *status );
static DrvStatusTypeDef LIS2MDL_Set_Threshold( DrvContextTypeDef *handle, uint16_t threshold );
static DrvStatusTypeDef LIS2MDL_Get_Threshold( DrvContextTypeDef *handle, uint16_t *threshold );
static DrvStatusTypeDef LIS2MDL_Get_Source( DrvContextTypeDef *handle, MAG_SOURCE_t *xAxis, MAG_SOURCE_t *yAxis, MAG_SOURCE_t *zAxis );
static DrvStatusTypeDef LIS2MDL_Set_Interrupt_Axis( DrvContextTypeDef *handle, SENSOR_AXIS_t axis);
static DrvStatusTypeDef LIS2MDL_Set_Offset( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t Offset );
static DrvStatusTypeDef LIS2MDL_Get_Offset( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *offset );
static DrvStatusTypeDef LIS2MDL_Set_Interrupt_Enabled(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable );
static DrvStatusTypeDef LIS2MDL_Set_LPF(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable );
static DrvStatusTypeDef LIS2MDL_Set_OperatingMode( DrvContextTypeDef*, MAG_OPERATING_MODE_t );
static DrvStatusTypeDef LIS2MDL_Set_DRDY_on_PIN( DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable );
static DrvStatusTypeDef LIS2MDL_Set_Int_on_PIN( DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable );
static DrvStatusTypeDef LIS2MDL_Get_PIN_Status( DrvContextTypeDef *handle, bool *isSet );

/**
 * @}
 */

/** @addtogroup LIS2MDL_Private_Variables Private variables
 * @{
 */

/**
 * @brief LIS2MDL driver structure
 */
MAGNETO_Drv_t LIS2MDLDrv =
{
  LIS2MDL_Init,
  LIS2MDL_DeInit,
  LIS2MDL_Sensor_Enable,
  LIS2MDL_Sensor_Disable,
  LIS2MDL_Get_WhoAmI,
  LIS2MDL_Check_WhoAmI,
  LIS2MDL_Get_Axes,
	LIS2MDL_Get_Axis,
  LIS2MDL_Get_AxesRaw,
  LIS2MDL_Get_AxisRaw,
  LIS2MDL_Get_Sensitivity,
  LIS2MDL_Get_ODR,
  LIS2MDL_Set_ODR,
  LIS2MDL_Set_ODR_Value,
  NULL,
  NULL,
  NULL,
  LIS2MDL_Read_Reg,
  LIS2MDL_Write_Reg,
  LIS2MDL_Get_DRDY_Status,
	LIS2MDL_Set_Threshold,
	LIS2MDL_Get_Threshold,
	LIS2MDL_Get_Source,
	LIS2MDL_Set_Interrupt_Axis,
	LIS2MDL_Set_Offset,
  LIS2MDL_Get_Offset,
  LIS2MDL_Set_Interrupt_Enabled,
  LIS2MDL_Set_LPF,
  LIS2MDL_Set_OperatingMode,
  LIS2MDL_Set_DRDY_on_PIN,
  LIS2MDL_Set_Int_on_PIN,
  LIS2MDL_Get_PIN_Status
};

const float magnetometer_max_datarate = 100.0f; // LIS2MDL

/**
 * @}
 */

/** @addtogroup LIS2MDL_Callable_Private_Functions Callable private functions
 * @{
 */

/**
 * @brief Initialize the LIS2MDL sensor
 * @param handle the device handle
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Init( DrvContextTypeDef *handle )
{
//  #ifdef USE_MAG_SPI
//    if ( LIS2MDL_MAG_W_I2C( (void *)handle, LIS2MDL_MAG_I2C_DISABLE ) != MEMS_SUCCESS )
//  {
//    PRINTF("Failed I2C\n");
//    return COMPONENT_ERROR;
//  }
//    
//  LIS2MDL_MAG_I2C_t value;
//  if ( LIS2MDL_MAG_R_I2C( (void *)handle, &value ) != MEMS_SUCCESS )
//  {
//    PRINTF("Failed I2C\n");
//    return COMPONENT_ERROR;
//  }
//  
//  if (value != LIS2MDL_MAG_I2C_DISABLE)
//  {
//    PRINTF("Failed I2C Write\n");
//    return COMPONENT_ERROR;
//  }
//  #endif
  
  if ( LIS2MDL_Check_WhoAmI( handle ) != COMPONENT_OK )
  {
    PRINTF("Failed WhoAmI\n");
    return COMPONENT_ERROR;
  }

  /* Operating mode selection - power down */
  if ( LIS2MDL_MAG_W_SystemOperatingMode( (void *)handle, LIS2MDL_MAG_MD_POWER_DOWN ) != MEMS_SUCCESS )
  {
    PRINTF("Failed System Op\n");
    return COMPONENT_ERROR;
  }

	handle->isEnabled = 1;
	
  /* Enable BDU */
  if ( LIS2MDL_MAG_W_BlockDataUpdate( (void *)handle, LIS2MDL_MAG_BDU_ENABLE ) != MEMS_SUCCESS )
  {
    PRINTF("Failed BlockDateUpdate\n");
    return COMPONENT_ERROR;
  }

  if ( LIS2MDL_MAG_W_CompensateTemperature( (void *)handle, LIS2MDL_MAG_COMP_TEMP_EN_ENABLE ) != MEMS_SUCCESS )
  {
    PRINTF("Failed Compensate\n");
    return COMPONENT_ERROR;
  }

  // Interrupt config
  if ( LIS2MDL_MAG_W_LatchInterruptRq(&handle, LIS2MDL_MAG_IEL_PULSED) != MEMS_SUCCESS)
  {
    PRINTF("Failed Latch\n");
    return COMPONENT_ERROR;
  }
  
  if ( LIS2MDL_MAG_W_InterruptActive(&handle, LIS2MDL_MAG_IEA_HIGH) != MEMS_SUCCESS)
  {
    PRINTF("Failed IA\n");
    return COMPONENT_ERROR;
  }
	
	if ( LIS2MDL_MAG_W_INT_on_DataOFF(&handle, LIS2MDL_MAG_INT_on_DataOFF_ENABLED) != MEMS_SUCCESS)
  {
    PRINTF("Failed INT DataOff\n");
    return COMPONENT_ERROR;
  }
  
	if ( LIS2MDL_MAG_W_LP(&handle, LIS2MDL_MAG_LP_ENABLED) != MEMS_SUCCESS)
  {
    PRINTF("Failed LP\n");
    return COMPONENT_ERROR;
  }

  handle->isInitialized = 1;

  return COMPONENT_OK;
}


/**
 * @brief Deinitialize the LIS2MDL sensor
 * @param handle the device handle
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_DeInit( DrvContextTypeDef *handle )
{

  if ( LIS2MDL_Check_WhoAmI( handle ) == COMPONENT_ERROR )
  {
    return COMPONENT_ERROR;
  }

  /* Disable the component */
  if ( LIS2MDL_Sensor_Disable( handle ) == COMPONENT_ERROR )
  {
    return COMPONENT_ERROR;
  }

  handle->isInitialized = 0;

  return COMPONENT_OK;
}



/**
 * @brief Enable the LIS2MDL sensor
 * @param handle the device handle
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Sensor_Enable( DrvContextTypeDef *handle )
{

  /* Check if the component is already enabled */
  if ( handle->isEnabled == 1 )
  {
    return COMPONENT_OK;
  }

  /* Operating mode selection */
  if ( LIS2MDL_MAG_W_SystemOperatingMode( (void *)handle, LIS2MDL_MAG_MD_CONTINUOUS ) != MEMS_SUCCESS)
  {
    return COMPONENT_ERROR;
  }

  handle->isEnabled = 1;

  return COMPONENT_OK;
}



/**
 * @brief Disable the LIS2MDL sensor
 * @param handle the device handle
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Sensor_Disable( DrvContextTypeDef *handle )
{

  /* Check if the component is already disabled */
  if ( handle->isEnabled == 0 )
  {
    return COMPONENT_OK;
  }

  /* Operating mode selection - power down */
  if ( LIS2MDL_MAG_W_SystemOperatingMode( (void *)handle, LIS2MDL_MAG_MD_POWER_DOWN ) == MEMS_ERROR )
  {
    return COMPONENT_ERROR;
  }

  handle->isEnabled = 0;

  return COMPONENT_OK;
}



/**
 * @brief Get the WHO_AM_I ID of the LIS2MDL sensor
 * @param handle the device handle
 * @param who_am_i pointer to the value of WHO_AM_I register
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_WhoAmI( DrvContextTypeDef *handle, uint8_t *who_am_i )
{

  /* Read WHO AM I register */
  if ( LIS2MDL_MAG_R_WHO_AM_I_( (void *)handle, ( uint8_t* )who_am_i ) == MEMS_ERROR )
  {
    return COMPONENT_ERROR;
  }

  return COMPONENT_OK;
}



/**
 * @brief Check the WHO_AM_I ID of the LIS2MDL sensor
 * @param handle the device handle
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Check_WhoAmI( DrvContextTypeDef *handle )
{

  uint8_t who_am_i = 0x00;

  if ( LIS2MDL_Get_WhoAmI( handle, &who_am_i ) == COMPONENT_ERROR )
  {
    return COMPONENT_ERROR;
  }
  if ( who_am_i != LIS2MDL_MAG_WHO_AM_I )
  {
    PRINTF("who_am_i %d\n", who_am_i);
    return COMPONENT_ERROR;
  }

  PRINTF("who_am_i succ %d\n", who_am_i);

  return COMPONENT_OK;
}



/**
 * @brief Get the LIS2MDL sensor axes
 * @param handle the device handle
 * @param magnetic_field pointer where the values of the axes are written
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_Axes( DrvContextTypeDef *handle, SensorAxes_t *magnetic_field )
{

  int16_t pDataRaw[3];
  float sensitivity = 0;

  /* Read raw data from LIS2MDL output register. */
  if ( LIS2MDL_Get_Axes_Raw( handle, pDataRaw ) == COMPONENT_ERROR )
  {
    return COMPONENT_ERROR;
  }

  /* Get LIS2MDL actual sensitivity. */
  if ( LIS2MDL_Get_Sensitivity( handle, &sensitivity ) == COMPONENT_ERROR )
  {
    return COMPONENT_ERROR;
  }

  /* Calculate the data. */
  magnetic_field->AXIS_X = ( int32_t )( pDataRaw[0] * sensitivity );
  magnetic_field->AXIS_Y = ( int32_t )( pDataRaw[1] * sensitivity );
  magnetic_field->AXIS_Z = ( int32_t )( pDataRaw[2] * sensitivity );

  return COMPONENT_OK;
}


/**
 * @brief Get the LIS3MDL sensor axes
 * @param handle the device handle
 * @param the axis to get the value for
 * @param magnetic_field pointer where the value of the axis is written
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_Axis( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *value )
{

  int16_t dataRaw;
  float sensitivity = 0;

  /* Read raw data from LIS2MDL output register. */
  if ( LIS2MDL_Get_Axis_Raw( handle, axis, &dataRaw ) == COMPONENT_ERROR )
  {
    return COMPONENT_ERROR;
  }

  /* Get LIS2MDL actual sensitivity. */
  if ( LIS2MDL_Get_Sensitivity( handle, &sensitivity ) == COMPONENT_ERROR )
  {
    return COMPONENT_ERROR;
  }

  /* Calculate the data. */
	*value = ( int32_t )( dataRaw * sensitivity );

  return COMPONENT_OK;
}


/**
 * @brief Get the LIS2MDL sensor raw axes
 * @param handle the device handle
 * @param value pointer where the raw values of the axes are written
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_AxesRaw( DrvContextTypeDef *handle, SensorAxesRaw_t *value )
{

  int16_t pDataRaw[3];

  /* Read raw data from LIS2MDL output register. */
  if ( LIS2MDL_Get_Axes_Raw( handle, pDataRaw ) == COMPONENT_ERROR )
  {
    return COMPONENT_ERROR;
  }

  /* Set the raw data. */
  value->AXIS_X = pDataRaw[0];
  value->AXIS_Y = pDataRaw[1];
  value->AXIS_Z = pDataRaw[2];

  return COMPONENT_OK;
}

/**
 * @brief Get the LIS2MDL sensor raw axes
 * @param handle the device handle
 * @param sensor axis
 * @param value pointer where the raw values of the axes are written
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_AxisRaw( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *value )
{

  int16_t pDataRaw;

  /* Read raw data from LIS2MDL output register. */
  if ( LIS2MDL_Get_Axis_Raw( handle, axis, &pDataRaw ) == COMPONENT_ERROR )
  {
    return COMPONENT_ERROR;
  }

  /* Set the raw data. */
  *value = pDataRaw;

  return COMPONENT_OK;
}


/**
 * @brief Get the LIS2MDL sensor sensitivity
 * @param handle the device handle
 * @param sensitivity pointer where the sensitivity value is written [LSB/gauss]
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_Sensitivity( DrvContextTypeDef *handle, float *sensitivity )
{
	*sensitivity = ( float )LIS2MDL_MAG_SENSITIVITY_FOR_FS;
	
  return COMPONENT_OK;
}



/**
 * @brief Get the LIS2MDL sensor output data rate
 * @param handle the device handle
 * @param odr pointer where the output data rate is written
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_ODR( DrvContextTypeDef *handle, float *odr )
{
  LIS2MDL_MAG_ODR_t mag_odr;

  if ( LIS2MDL_MAG_R_OutputDataRate( (void *)handle, &mag_odr ) == MEMS_ERROR )
  {
    return COMPONENT_ERROR;
  }

  switch( mag_odr )
  {
    case LIS2MDL_MAG_ODR_10Hz:
      *odr = 10.000f;
      break;
    case LIS2MDL_MAG_ODR_20Hz:
      *odr = 20.000f;
      break;
    case LIS2MDL_MAG_ODR_50Hz:
      *odr = 50.000f;
      break;
    case LIS2MDL_MAG_ODR_100Hz:
      *odr = 100.000f;
      break;
    default:
      *odr = -1.000f;
      return COMPONENT_ERROR;
  }

  return COMPONENT_OK;
}



/**
 * @brief Set the LIS2MDL sensor output data rate
 * @param handle the device handle
 * @param odr the functional output data rate to be set
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Set_ODR( DrvContextTypeDef *handle, SensorOdr_t odr )
{

  LIS2MDL_MAG_ODR_t new_odr;

  switch( odr )
  {
    case ODR_LOW:
      new_odr = LIS2MDL_MAG_ODR_10Hz;
      break;
    case ODR_MID_LOW:
      new_odr = LIS2MDL_MAG_ODR_20Hz;
      break;
    case ODR_MID:
      new_odr = LIS2MDL_MAG_ODR_50Hz;
      break;
    case ODR_MID_HIGH:
      new_odr = LIS2MDL_MAG_ODR_100Hz;
      break;
    case ODR_HIGH:
      new_odr = LIS2MDL_MAG_ODR_100Hz;
      break;
    default:
      return COMPONENT_ERROR;
  }

  if ( LIS2MDL_MAG_W_OutputDataRate( (void *)handle, new_odr ) == MEMS_ERROR )
  {
    return COMPONENT_ERROR;
  }

  return COMPONENT_OK;
}



/**
 * @brief Set the LIS2MDL sensor output data rate for the Z Axis
 * @param handle the device handle
 * @param odr the output data rate value to be set
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Set_ODR_Value( DrvContextTypeDef *handle, float odr )
{
  LIS2MDL_MAG_ODR_t new_odr = ( odr <=  10.0f ) ? LIS2MDL_MAG_ODR_10Hz
														: ( odr <=  20.0f ) ? LIS2MDL_MAG_ODR_20Hz
														: ( odr <=  50.0f ) ? LIS2MDL_MAG_ODR_50Hz
														:                     LIS2MDL_MAG_ODR_100Hz;
  
	if ( LIS2MDL_MAG_W_OutputDataRate( (void *)handle, new_odr ) == MEMS_ERROR )
  {
    return COMPONENT_ERROR;
  }

  return COMPONENT_OK;
}

/**
 * @brief Set the LIS2MDL sensor offset
 * @param handle the device handle
 * @param axis the axis to set the offset for
 * @param offset the offset
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Set_Offset( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t offset )
{
	u8_t reg;
	uint8_t buffer[2];

	buffer[1] = offset >> 8;
  buffer[0] = offset;

	switch(axis)
	{
		case SENSOR_AXIS_X:
			reg = LIS2MDL_MAG_OFFSET_X_REG_L;
		break;
		case SENSOR_AXIS_Y:
			reg = LIS2MDL_MAG_OFFSET_Y_REG_L;
		break;
		case SENSOR_AXIS_Z:
			reg = LIS2MDL_MAG_OFFSET_Z_REG_L;
		break;
    default:
      return COMPONENT_ERROR;
	}
	
  if ( LIS2MDL_MAG_WriteReg(&handle, reg, buffer, 2) == MEMS_ERROR)
	{
		return COMPONENT_ERROR;
	}
		
	return COMPONENT_OK;
}

/**
 * @brief Get the LIS2MDL offset
 * @param handle the device handle
 * @param axis the axis to get the offset for
 * @param offset pointer where the full scale is written
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_Offset( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *offset )
{
	uint8_t buffer[2];
	u8_t reg;

	switch(axis)
	{
		case SENSOR_AXIS_X:
			reg = LIS2MDL_MAG_OFFSET_X_REG_L;
		break;
		case SENSOR_AXIS_Y:
			reg = LIS2MDL_MAG_OFFSET_Y_REG_L;
		break;
		case SENSOR_AXIS_Z:
			reg = LIS2MDL_MAG_OFFSET_Z_REG_L;
		break;
    default:
      return COMPONENT_ERROR;
	}

	if ( LIS2MDL_MAG_ReadReg( (void *)handle, reg, buffer, 2) == MEMS_ERROR )
  {
    return COMPONENT_ERROR;
  }
	
	*offset = (buffer[1]<<8) + buffer[0];

  return COMPONENT_OK;
}

/**
 * @brief Set the LIS2MDL sensor threshold
 * @param handle the device handle
 * @param treshold as an absolute value
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Set_Threshold( DrvContextTypeDef *handle, uint16_t threshold )
{
	 uint8_t thresholdBuffer[2];
	thresholdBuffer[1] = threshold >> 8;
  thresholdBuffer[0] = threshold;
  if ( LIS2MDL_MAG_Set_MagneticThreshold(&handle, thresholdBuffer) == MEMS_ERROR)
	{
		return COMPONENT_ERROR;
	}
	
	return COMPONENT_OK;
}

/**
 * @brief Get the LIS2MDL threshold
 * @param handle the device handle
 * @param fullScale pointer where the full scale is written
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_Threshold( DrvContextTypeDef *handle, uint16_t *threshold )
{
	uint8_t thresholdBuffer[2];

	if ( LIS2MDL_MAG_Get_MagneticThreshold(&handle, thresholdBuffer) == MEMS_ERROR)
  {
    return COMPONENT_ERROR;
  }
	
	*threshold = (thresholdBuffer[1]<<8) + thresholdBuffer[0];

  return COMPONENT_OK;
}

static MAG_SOURCE_t GetSource(uint8_t regValue, uint8_t positiveFlag, uint8_t negativeFlag){
  if ((regValue & positiveFlag) == positiveFlag) // positive interrupt
  {
    return MAG_SOURCE_POSITIVE;
  } else if ((regValue & negativeFlag) == negativeFlag){ // negative interrupt
    return MAG_SOURCE_NEGATIVE;
  }
  return MAG_SOURCE_NONE;
}

/**
 * @brief Set the LIS2MDL sensor threshold
 * @param handle the device handle
 * @param source from the x axis
 * @param source from the y axis
 * @param source from the z axis
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_Source( DrvContextTypeDef *handle, MAG_SOURCE_t* xSource, MAG_SOURCE_t* ySource, MAG_SOURCE_t* zSource){
	uint8_t regValue;
	if ( LIS2MDL_Read_Reg(handle, LIS2MDL_MAG_INT_SOURCE_REG, &regValue) != COMPONENT_OK)
	{
    return COMPONENT_ERROR;
	}
	
	*xSource = GetSource(regValue, LIS2MDL_MAG_PTH_X_UP, LIS2MDL_MAG_NTH_X_UP);

	*ySource = GetSource(regValue, LIS2MDL_MAG_PTH_Y_UP, LIS2MDL_MAG_NTH_Y_UP);

	*zSource = GetSource(regValue, LIS2MDL_MAG_PTH_Z_UP, LIS2MDL_MAG_NTH_Z_UP);
	
	return COMPONENT_OK;
}

static DrvStatusTypeDef LIS2MDL_Set_Interrupt_Axis( DrvContextTypeDef *handle, SENSOR_AXIS_t axis)
{
	uint8_t regValue = LIS2MDL_MAG_IEL_PULSED | LIS2MDL_MAG_IEA_HIGH | LIS2MDL_MAG_IEN_ENABLE;

	switch(axis)
  {
		case SENSOR_AXIS_X:
      //x 
      regValue |= LIS2MDL_MAG_XIEN_ENABLE;
      break;
    case SENSOR_AXIS_Y:
      //y
      regValue |= LIS2MDL_MAG_YIEN_ENABLE;
      break;
    case SENSOR_AXIS_Z:
      //z
      regValue |= LIS2MDL_MAG_ZIEN_ENABLE;
      break;
  }
	
	if ( LIS2MDL_Write_Reg( handle, LIS2MDL_MAG_INT_CRTL_REG, regValue) != COMPONENT_OK)
  {
		return COMPONENT_ERROR;
  }
  
  uint8_t readValue;
  if ( LIS2MDL_Read_Reg( handle, LIS2MDL_MAG_INT_CRTL_REG, &readValue) != COMPONENT_OK)
  {
		return COMPONENT_READ_ERROR;
  }
  
  if (regValue != readValue)
  {
    //PRINTF("ERROR int axis wrong %d %d\n",regValue,readValue);
    return COMPONENT_READ_MISMATCH;
  }
	return COMPONENT_OK;
}

/**
 * @brief Read the data from register
 * @param handle the device handle
 * @param reg register address
 * @param data register data
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Read_Reg( DrvContextTypeDef *handle, uint8_t reg, uint8_t *data )
{

  if ( LIS2MDL_MAG_ReadReg( (void *)handle, reg, data, 1 ) == MEMS_ERROR )
  {
    return COMPONENT_ERROR;
  }

  return COMPONENT_OK;
}

/**
 * @brief Write the data to register
 * @param handle the device handle
 * @param reg register address
 * @param data register data
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Write_Reg( DrvContextTypeDef *handle, uint8_t reg, uint8_t data )
{

  if ( LIS2MDL_MAG_WriteReg( (void *)handle, reg, &data, 1 ) == MEMS_ERROR )
  {
    return COMPONENT_ERROR;
  }

  return COMPONENT_OK;
}

/**
 * @brief Get magnetometer data ready status
 * @param handle the device handle
 * @param status the data ready status
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_DRDY_Status( DrvContextTypeDef *handle, uint8_t *status )
{

  LIS2MDL_MAG_ZYXDA_t status_raw;

  if ( LIS2MDL_MAG_R_NewXYZData( (void *)handle, &status_raw ) == MEMS_ERROR )
  {
    return COMPONENT_ERROR;
  }

  switch( status_raw )
  {
    case LIS2MDL_MAG_ZYXDA_AVAILABLE:
      *status = 1;
      break;
    case LIS2MDL_MAG_ZYXDA_NOT_AVAILABLE:
      *status = 0;
      break;
    default:
      return COMPONENT_ERROR;
  }

  return COMPONENT_OK;
}

/**
 * @}
 */

/** @addtogroup LIS2MDL_Private_Functions Private functions
 * @{
 */

/**
 * @brief Get the LIS2MDL sensor raw axes
 * @param handle the device handle
 * @param pData pointer where the raw values of the axes are written
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_Axes_Raw( DrvContextTypeDef *handle, int16_t* pData )
{

  uint8_t regValue[6] = {0, 0, 0, 0, 0, 0};

  /* Read output registers from LIS2MDL_MAG_OUTX_L to LIS2MDL_MAG_OUTZ_H. */
  if ( LIS2MDL_MAG_Get_Magnetic( (void *)handle, ( uint8_t* )regValue ) == MEMS_ERROR )
  {
    return COMPONENT_ERROR;
  }

  /* Format the data. */
  pData[0] = ( ( ( ( int16_t )regValue[1] ) << 8 ) + ( int16_t )regValue[0] );
  pData[1] = ( ( ( ( int16_t )regValue[3] ) << 8 ) + ( int16_t )regValue[2] );
  pData[2] = ( ( ( ( int16_t )regValue[5] ) << 8 ) + ( int16_t )regValue[4] );

  return COMPONENT_OK;
}


/**
 * @brief Get the LIS2MDL sensor raw axes
 * @param handle the device handle
 * @param pData pointer where the raw values of the axes are written
 * @retval COMPONENT_OK in case of success
 * @retval COMPONENT_ERROR in case of failure
 */
static DrvStatusTypeDef LIS2MDL_Get_Axis_Raw(DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *pData )
{

  uint8_t regValue[2] = {0, 0};

  /* Read output registers from LIS2MDL_MAG_OUTX_L to LIS2MDL_MAG_OUTZ_H. */
  if ( LIS2MDL_MAG_Get_Magnetic_Axis( (void *)handle, axis,  ( uint8_t* )regValue ) == MEMS_ERROR )
  {
    return COMPONENT_ERROR;
  }

  /* Format the data. */
  *pData = ( ( ( ( int16_t )regValue[1] ) << 8 ) + ( int16_t )regValue[0] );

  return COMPONENT_OK;
}

static DrvStatusTypeDef LIS2MDL_Set_Interrupt_Enabled(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable )
{
  LIS2MDL_MAG_IEA_t mag_iea_t = LIS2MDL_MAG_IEA_HIGH;
  if (enable == SENSOR_TOGGLE_DISABLE)
  {
    mag_iea_t = LIS2MDL_MAG_IEA_LOW;
  }
  
  if ( LIS2MDL_MAG_W_InterruptActive((void *)handle, mag_iea_t) != MEMS_SUCCESS)
  {
    return COMPONENT_ERROR;
  }

  return COMPONENT_OK;
}

static DrvStatusTypeDef LIS2MDL_Set_LPF(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable )
{
  LIS2MDL_MAG_LPF_t value = LIS2MDL_MAG_LPF_ENABLED;
  if (enable == SENSOR_TOGGLE_DISABLE)
  {
    value = LIS2MDL_MAG_LPF_DISABLED;
  }
  
  if ( LIS2MDL_MAG_W_LPF((void *)handle, value) != MEMS_SUCCESS)
  {
    return COMPONENT_ERROR;
  }

  return COMPONENT_OK;
}

static DrvStatusTypeDef LIS2MDL_Set_OperatingMode( DrvContextTypeDef *handle, MAG_OPERATING_MODE_t operatingMode)
{
  LIS2MDL_MAG_MD_t value;
  switch (operatingMode)
  {
    case MAG_OPERATING_MODE_CONTINUOUS:
      value = LIS2MDL_MAG_MD_CONTINUOUS;
      break;
    case MAG_OPERATING_MODE_SINGLE:
      value = LIS2MDL_MAG_MD_SINGLE;
      break;
    case MAG_OPERATING_MODE_POWER_DOWN:
      value = LIS2MDL_MAG_MD_POWER_DOWN;
      break;
    case MAG_OPERATING_MODE_POWER_DOWN_AUTO:
      value = LIS2MDL_MAG_MD_POWER_DOWN_AUTO;
      break;
    default:
      return COMPONENT_ERROR;
  }
  
  if ( LIS2MDL_MAG_W_SystemOperatingMode((void *)handle, value) != MEMS_SUCCESS)
  {
      return COMPONENT_ERROR;
  }
  
  return COMPONENT_OK;
}

static DrvStatusTypeDef LIS2MDL_Set_DRDY_on_PIN( DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable )
{
  
  LIS2MDL_MAG_DRDY_on_PIN_t value = LIS2MDL_MAG_DRDY_on_PIN_ENABLED;
  if (enable == SENSOR_TOGGLE_DISABLE)
  {
    value = LIS2MDL_MAG_DRDY_on_PIN_DISABLED;
  }
  
  if ( LIS2MDL_MAG_W_DRDY_on_PIN((void *)handle, value) != MEMS_SUCCESS)
  {
    return COMPONENT_ERROR;
  }

  return COMPONENT_OK;
}


static DrvStatusTypeDef LIS2MDL_Set_Int_on_PIN( DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable )
 {
  
  LIS2MDL_MAG_INT_on_PIN_t value = LIS2MDL_MAG_INT_on_PIN_ENABLED;
  if (enable == SENSOR_TOGGLE_DISABLE)
  {
    value = LIS2MDL_MAG_INT_on_PIN_DISABLED;
  }
  
  if ( LIS2MDL_MAG_W_INT_on_PIN((void *)handle, value) != MEMS_SUCCESS)
  {
    return COMPONENT_ERROR;
  }

  return COMPONENT_OK;
}

static DrvStatusTypeDef LIS2MDL_Get_PIN_Status( DrvContextTypeDef *handle, bool *isSet )
{
  *isSet= magnetometer_read_interrupt();
  return COMPONENT_OK;
}

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
#endif // LIS2MDL

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
