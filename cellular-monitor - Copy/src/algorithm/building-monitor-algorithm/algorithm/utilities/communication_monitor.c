#ifdef USE_MAGNETOMETER
#include <stdint.h>
#include <stddef.h>
#include "log.h"
#include "communication_monitor.h"

static uint16_t communicationErrors = 0;

static CommunicationMonitorFailureCallback failureCallback = NULL;
static CommunicationMonitorResetCallback resetCallback = NULL;

static void Communication_Monitor_HandleError( void );
static void Communication_Monitor_HandleSuccess( void );

void Communication_Monitor_Init( CommunicationMonitorFailureCallback inputFailureCallback, CommunicationMonitorResetCallback inputResetCallback)
{
  PRINTF("Communication_Monitor_Init\n");
  failureCallback = inputFailureCallback;
  resetCallback = inputResetCallback;
  communicationErrors = 0;
}

bool Communication_Monitor_HandleStatus(DrvStatusTypeDef status)
{
  if (status == COMPONENT_OK)
  {
    Communication_Monitor_HandleSuccess();
    return true;
  }
  else
  {
    PRINTF("Communication_Monitor_HandleStatus: status:%d\n", status);
    Communication_Monitor_HandleError();
    return false;
  }
}

static void Communication_Monitor_HandleError()
{
  communicationErrors++;
  if (communicationErrors == 1)
  {
    magnetometer_reset_comms();
  } else if (communicationErrors > MAX_COMMUNICATION_ERRORS)
  {
    // Full restart
    PRINTF_BASE("Communication_Monitor_HandleError: communicationErrors:%d\n", communicationErrors);
    communicationErrors = 0;
    if (failureCallback != NULL)
    {
      communicationErrors = 0;
      failureCallback();
    } else {
      PRINTF_BASE("Communication_Monitor_HandleError: failure callback is NULL\n");
    }
  } else {
    // More serious reset.
    if (resetCallback != NULL)
    {
      if (!resetCallback())
      {
        Communication_Monitor_HandleError();
      }
    } else {
      PRINTF_BASE("Communication_Monitor_HandleError: reset callback is NULL\n");
    }
  }
}

static void Communication_Monitor_HandleSuccess( void )
{
  communicationErrors = 0;
}
#endif
