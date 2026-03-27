
 /*******************************************************************************
  * @file    follower_algorithm.h
  * @author  Charles Fayal
  * @version V1.0.1
  * @date    7-April-2022
  * @brief   Header for driver cycle tracker
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2022 NOWI Sensors</center></h2>
  *
  ******************************************************************************
  */

#ifndef FOLLOWER_H
#define FOLLOWER_H

#include <stdint.h>
#include <stdbool.h>
#include "algorithm.h"
#include "axis_configuration.h"
#include "timeServer.h"

#if defined(LOG_MAGNETOMETER_DATA) || defined(ALGORITHM_LOGGER)
#define LogMag(...)  PRINTF_BASE(__VA_ARGS__)
#else 
#define LogMag(...)
#endif

extern ALGORITHM_t MinMaxAlgorithm;

/**
 * \brief Initializes the tracker
**/
void Follower_Init( void );

/**
 * \brief Should be called every main loop
**/
void Follower_ReadData( void );

/**
 * \brief Returns the minimum value between a and b
 *
 * \param [IN] resetData if true resets the saved data
**/
void Follower_Reset( void );

/**
 * \brief Logs the state and data for the algorithm
**/
void Follower_Log_Config( void );

bool Follower_Get_IsConfigured( void );

void Follower_Update_Variable(uint8_t variable, uint32_t value);

void Follower_Get_Data(int16_t data[], uint8_t *size);

void Follower_Enable( void );

void Follower_Disable( void );

void Follower_Get_Variables(uint32_t *variables, uint8_t *length);

void Follower_Set_Variables(uint32_t *variables, uint8_t length);

#endif

/************************ (C) COPYRIGHT NOWI Sensors *****END OF FILE****/
