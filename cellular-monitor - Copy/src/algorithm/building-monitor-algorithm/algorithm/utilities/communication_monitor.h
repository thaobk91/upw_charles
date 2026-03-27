#ifndef COMMUNICATION_MONITOR_H
#define COMMUNICATION_MONITOR_H

#include <stdbool.h>
#include "magnetometer.h"

#define MAX_COMMUNICATION_ERRORS 5

typedef void (*CommunicationMonitorFailureCallback)( void );
typedef bool (*CommunicationMonitorResetCallback)( void );

void Communication_Monitor_Init( CommunicationMonitorFailureCallback inputFailureCallback, CommunicationMonitorResetCallback inputResetCallback);

/*
    * @brief Handle the status of the communication
    * @param status: the status of the communication
    * @retval true if the communication is still good, false if the communication has failed
*/
bool Communication_Monitor_HandleStatus(DrvStatusTypeDef status);

#endif // COMMUNICATION_MONITOR_H
