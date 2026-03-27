#ifndef __BSP_H__
#define __BSP_H__

#include "algo_hw.h"
#include "magnetometer.h"

#ifdef LIS3MDL
#include "LIS3MDL_MAG_driver_HL.h"
#define MagnetometerDrv		LIS3MDLDrv
#endif
#ifdef LIS2MDL
#include "LIS2MDL_MAG_driver_HL.h"
#define MagnetometerDrv		LIS2MDLDrv
#endif
#ifdef SIMULATED_MAG
#include "simulated_MAG_driver_HL.h"
#define MagnetometerDrv		SIMULATED_MAG_Drv
#endif

#endif
