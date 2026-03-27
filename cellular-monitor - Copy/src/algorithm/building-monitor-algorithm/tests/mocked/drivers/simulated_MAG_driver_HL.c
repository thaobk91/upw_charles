#include "magnetometer.h"
#include "signal.h"
#include "log.h"

const float magnetometer_max_datarate = 100.0f; // LIS2MDL

uint32_t reads = 0;
uint32_t set_offset_writes = 0;
uint32_t set_threshold_writes = 0;
uint32_t set_odr_writes = 0;
uint32_t set_interrupt_axis_writes = 0;
uint32_t read_axis = 0;

uint32_t GetStats( void )
{
  uint32_t total_reads = 0;
  PRINTF("Reads: %d ", reads);
  total_reads += reads;
  PRINTF("Read Axis: %d ", read_axis);
  total_reads += read_axis;
  PRINTF("Set Offset Writes: %d ", set_offset_writes);
  total_reads += set_offset_writes;
  PRINTF("Set Threshold Writes: %d ", set_threshold_writes);
  total_reads += set_threshold_writes;
  PRINTF("Set ODR Writes: %d ", set_odr_writes);
  total_reads += set_odr_writes;
  PRINTF("Set Interrupt Axis Writes: %d ", set_interrupt_axis_writes);
  total_reads += set_interrupt_axis_writes;
  PRINTF("\n");

  return total_reads;
}
static DrvStatusTypeDef SIMULATED_MAG_Init( DrvContextTypeDef *handle );
// static DrvStatusTypeDef SIMULATED_MAG_DeInit( DrvContextTypeDef *handle );
// static DrvStatusTypeDef SIMULATED_MAG_Sensor_Enable( DrvContextTypeDef *handle );
// static DrvStatusTypeDef SIMULATED_MAG_Sensor_Disable( DrvContextTypeDef *handle );
// static DrvStatusTypeDef SIMULATED_MAG_Get_WhoAmI( DrvContextTypeDef *handle, uint8_t *who_am_i );
// static DrvStatusTypeDef SIMULATED_MAG_Check_WhoAmI( DrvContextTypeDef *handle );
static DrvStatusTypeDef SIMULATED_MAG_Get_Axes( DrvContextTypeDef *handle, SensorAxes_t *magnetic_field );
static DrvStatusTypeDef SIMULATED_MAG_Get_Axis( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *value );
// static DrvStatusTypeDef SIMULATED_MAG_Get_AxesRaw( DrvContextTypeDef *handle, SensorAxesRaw_t *value );
// static DrvStatusTypeDef SIMULATED_MAG_Get_AxisRaw( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *value );
static DrvStatusTypeDef SIMULATED_MAG_Get_Sensitivity( DrvContextTypeDef *handle, float *sensitivity );
static DrvStatusTypeDef SIMULATED_MAG_Get_ODR( DrvContextTypeDef *handle, float *odr );
// static DrvStatusTypeDef SIMULATED_MAG_Set_ODR( DrvContextTypeDef *handle, SensorOdr_t odr );
static DrvStatusTypeDef SIMULATED_MAG_Set_ODR_Value( DrvContextTypeDef *handle, float odr );
// static DrvStatusTypeDef SIMULATED_MAG_Read_Reg( DrvContextTypeDef *handle, uint8_t reg, uint8_t *data );
// static DrvStatusTypeDef SIMULATED_MAG_Write_Reg( DrvContextTypeDef *handle, uint8_t reg, uint8_t data );
static DrvStatusTypeDef SIMULATED_MAG_Get_DRDY_Status( DrvContextTypeDef *handle, uint8_t *status );
static DrvStatusTypeDef SIMULATED_MAG_Set_Threshold( DrvContextTypeDef *handle, uint16_t threshold );
static DrvStatusTypeDef SIMULATED_MAG_Get_Threshold( DrvContextTypeDef *handle, uint16_t *threshold );
static DrvStatusTypeDef SIMULATED_MAG_Get_Source( DrvContextTypeDef *handle, MAG_SOURCE_t *xAxis, MAG_SOURCE_t *yAxis, MAG_SOURCE_t *zAxis );
static DrvStatusTypeDef SIMULATED_MAG_Set_Interrupt_Axis( DrvContextTypeDef *handle, SENSOR_AXIS_t axis);
static DrvStatusTypeDef SIMULATED_MAG_Set_Offset( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t Offset );
static DrvStatusTypeDef SIMULATED_MAG_Get_Offset( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *Offset );
static DrvStatusTypeDef SIMULATED_MAG_Set_Interrupt_Enabled(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable );
static DrvStatusTypeDef SIMULATED_MAG_Set_LPF(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable );
static DrvStatusTypeDef SIMULATED_MAG_Set_Operating_Mode(DrvContextTypeDef *handle, MAG_OPERATING_MODE_t mode );
static DrvStatusTypeDef SIMULATED_MAG_Set_DRDY_on_PIN(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable );
static DrvStatusTypeDef SIMULATED_MAG_Set_INT_on_PIN(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable );
static DrvStatusTypeDef SIMULATED_MAG_Get_Pin_Status(DrvContextTypeDef *handle, bool *isSet );

MAGNETO_Drv_t SIMULATED_MAG_Drv =
{
  SIMULATED_MAG_Init,
  NULL, //SIMULATED_MAG_DeInit,
  NULL, //SIMULATED_MAG_Sensor_Enable,
  NULL, //SIMULATED_MAG_Sensor_Disable,
  NULL, //SIMULATED_MAG_Get_WhoAmI,
  NULL, //SIMULATED_MAG_Check_WhoAmI,
  SIMULATED_MAG_Get_Axes,
  SIMULATED_MAG_Get_Axis,
  NULL, //SIMULATED_MAG_Get_AxesRaw,
  NULL, //SIMULATED_MAG_Get_AxisRaw,
  SIMULATED_MAG_Get_Sensitivity,
  SIMULATED_MAG_Get_ODR,
  NULL, //SIMULATED_MAG_Set_ODR,
  SIMULATED_MAG_Set_ODR_Value,
  NULL,
  NULL,
  NULL,
  NULL, //SIMULATED_MAG_Read_Reg,
  NULL, //SIMULATED_MAG_Write_Reg,
  SIMULATED_MAG_Get_DRDY_Status,
  SIMULATED_MAG_Set_Threshold,
  SIMULATED_MAG_Get_Threshold,
  SIMULATED_MAG_Get_Source, 
  SIMULATED_MAG_Set_Interrupt_Axis,
  SIMULATED_MAG_Set_Offset,
  SIMULATED_MAG_Get_Offset,
  SIMULATED_MAG_Set_Interrupt_Enabled,
  SIMULATED_MAG_Set_LPF,
  SIMULATED_MAG_Set_Operating_Mode,
  SIMULATED_MAG_Set_DRDY_on_PIN,
  SIMULATED_MAG_Set_INT_on_PIN,
  SIMULATED_MAG_Get_Pin_Status
};

static float currentODR = 0;

static DrvStatusTypeDef SIMULATED_MAG_Init( DrvContextTypeDef *handle )
{
    // Do nothing for now
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Get_Axes( DrvContextTypeDef *handle, SensorAxes_t *magnetic_field )
{
  reads += 3; // Reads all 3 values
  *magnetic_field = Signal_Get_Value();
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Get_Axis( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *value )
{
  read_axis++;
  SensorAxes_t signalValue = Signal_Get_Value();

  switch (axis) {
    case SENSOR_AXIS_X:
      *value = signalValue.AXIS_X;
      break;
    case SENSOR_AXIS_Y:
      *value = signalValue.AXIS_Y;
      break;
    case SENSOR_AXIS_Z:
      *value = signalValue.AXIS_Z;
      break;
  }
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Get_Sensitivity( DrvContextTypeDef *handle, float *sensitivity )
{
  // doesn't read for this
  *sensitivity = 1;

  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Get_ODR( DrvContextTypeDef *handle, float *odr )
{
  reads++;
  *odr = currentODR;
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Set_ODR_Value( DrvContextTypeDef *handle, float odr )
{
  set_odr_writes+=2; // writes then reads to confirm
  currentODR = ( odr <=  10.0f ) ? 10.0f
    : ( odr <=  20.0f ) ? 20.0f
    : ( odr <=  50.0f ) ? 50.0f
    :                     magnetometer_max_datarate;
  Signal_Set_ODR( currentODR );
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Get_Source( DrvContextTypeDef *handle, MAG_SOURCE_t *xAxis, MAG_SOURCE_t *yAxis, MAG_SOURCE_t *zAxis )
{
  reads++;
  Signal_Get_Source( xAxis, yAxis, zAxis );
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Set_Interrupt_Axis( DrvContextTypeDef *handle, SENSOR_AXIS_t axis)
{
  set_interrupt_axis_writes++;
  Signal_Set_Interrupt_Axis( axis );
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Set_Offset( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t Offset )
{
  set_offset_writes++; // Writes then checks
  Signal_Set_Offset(axis, Offset);
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Get_Offset( DrvContextTypeDef *handle, SENSOR_AXIS_t axis, int16_t *Offset )
{
  set_offset_writes++; // Writes then checks
  Signal_Get_Offset(axis, Offset);
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Set_Interrupt_Enabled(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable )
{
  reads++;
  Signal_Set_Interrupt_Enabled( enable == SENSOR_TOGGLE_ENABLE );
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Set_LPF(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable )
{
  reads++;
  Signal_Set_LPF( enable == SENSOR_TOGGLE_ENABLE);
  return COMPONENT_OK;
}


// #region LIS2MDL_MAG

DrvStatusTypeDef SIMULATED_MAG_Set_Operating_Mode(DrvContextTypeDef *handle, MAG_OPERATING_MODE_t newValue)
{
  reads++;
  switch (newValue)
  {
    case MAG_OPERATING_MODE_CONTINUOUS:
      Signal_Set_Continuous( true );
      break;
    case MAG_OPERATING_MODE_SINGLE:
      Signal_Set_Continuous( false );
      break;
    default:
      Signal_Set_Continuous( false );
      break;
  }
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Set_Threshold( DrvContextTypeDef *handle, uint16_t threshold )
{
  set_threshold_writes++;
  Signal_Set_Threshold(threshold);
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Get_Threshold( DrvContextTypeDef *handle, uint16_t *threshold )
{
  reads++;
  Signal_Get_Threshold(threshold);
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Set_DRDY_on_PIN(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable )
{
  reads++;
  // PRINTF("SIMULATED_MAG_Set_DRDY_on_PIN(%d)", enable);
  Signal_Set_DRDY_on_PIN(enable == SENSOR_TOGGLE_ENABLE);
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Set_INT_on_PIN(DrvContextTypeDef *handle, SENSOR_TOGGLE_t enable )
{
  reads++;
  Signal_Set_INT_on_PIN(enable == SENSOR_TOGGLE_ENABLE);
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Get_Pin_Status(DrvContextTypeDef *handle, bool *isSet )
{
  Signal_Get_Pin_Status(isSet);
  return COMPONENT_OK;
}

static DrvStatusTypeDef SIMULATED_MAG_Get_DRDY_Status( DrvContextTypeDef *handle, uint8_t *status )
{
  *status = 1;
  return COMPONENT_OK;
}

// #endregion