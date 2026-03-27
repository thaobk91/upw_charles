#ifndef __SIMULATED_MAG_H
#define __SIMULATED_MAG_H

#include "magnetometer.h"

extern MAGNETO_Drv_t SIMULATED_MAG_Drv;
extern DrvContextTypeDef handle;

uint32_t GetStats( void );

typedef enum
{
  LIS2MDL_MAG_MD_CONTINUOUS      = 0x00,
  LIS2MDL_MAG_MD_SINGLE      = 0x01,
  LIS2MDL_MAG_MD_POWER_DOWN      = 0x02,
  LIS2MDL_MAG_MD_POWER_DOWN_AUTO     = 0x03,
} LIS2MDL_MAG_MD_t;

#endif  /* __SIMULATED_MAG_H */