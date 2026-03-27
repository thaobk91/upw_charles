### Building Monitor Algorithm

An algorithm to monitor a water meter's internal magnet's cycles I.e. rotations. Each cycle corresponds to a certain amount of water flowing through the meter. For example, 12 cycles might be 1 liter of water.

## Preprocessor Smybols

USE_MAGNETOMETER - To use the magnetometer rather than read pulses from the meter.

LIS2MDL - defines the LIS2MDL as the magnetometer we are using.

SIMULATED_MAG - simulated version of the magnetometer for testing.

Which Algorithm to use:

ALGORITHM_POLL_AND_INT - This Is the main algorithm to use in production

ALGORITHM_LOGGER

## Files To Implement

Header files can be found in the Interfaces folder.

The algorithm uses a lot of timing. You can either Implement hw.c and Implement the Real Time Clock functions or if there's a timer library available you can just implement that and override timeServer.c and h.

- flash_utility.c - loading/saving data
- error_handler.c - sending debug data to the cloud
- log_implementation.h (see log_Implementation.h in tests/mocked for an example)

## Functions To Implement

extern uint8_t Sensor_IO_Write( void *handle, uint8_t WriteAddr, uint8_t *pBuffer, uint16_t nBytesToWrite );

extern uint8_t Sensor_IO_Read( void *handle, uint8_t ReadAddr, uint8_t *pBuffer, uint16_t nBytesToRead );

## Calling The Algorithm

There's an example of how the algorithm can be called In the /module/main.c file.

Every X seconds (where  x Is the applications duty cycle) the device should send up data to the cloud. Here's an example of what the data should look like. The formatting of the nowi_protocol was built for LoRa which highly constrained on data size. This may not be true of other Implementations.

The application duty cycle should be remotely configurable.

Exampling of sending data:

```c

  if (Algorithm.Is_Configured())
  {
    if (app_tx_dutycycle <= 10* 60 * 1000)
    {
      TimerTime_t longestDurationBetweenPulsesMS;
      if (cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDurationBetweenPulsesMS) ){
        if (longestDurationBetweenPulsesMS > UINT16_MAX)
        {
          // Do not need good resolution when duration is this long, roughly 65 seconds
          uint16_t longestDurationS = ceil(longestDurationBetweenPulsesMS / 1000.0f);
          nowi_protocol_add_data_type(&protocol, NOWI_DATA_TYPE_LONGEST_DURATION_BETWEEN_PULSES_S); // 1 Byte
          nowi_protocol_add_unsigned(&protocol, longestDurationS); // 2 Byte
          PRINTF("Duration S %d\n",longestDurationS);
        } else {
          nowi_protocol_add_data_type(&protocol, NOWI_DATA_TYPE_LONGEST_DURATION_BETWEEN_PULSES_MS); // 1 Byte
          nowi_protocol_add_unsigned(&protocol, longestDurationBetweenPulsesMS); // 2 Byte
          PRINTF("Duration mS %d\n",longestDurationBetweenPulsesMS);
        }
      }
    } else {
      float pulseRate;
      if (LowestFlowOverPeriodTracker_Get_LowestPulseRate(&pulseRate))
      {
        PRINTF("LowestPulseRate: %.2f\n", pulseRate);
        nowi_protocol_add_version(&protocol, NOWI_DATA_TYPE_LOWEST_PULSE_RATE); // 1 Byte
        nowi_protocol_add_float(&protocol, pulseRate); // 2 Byte
      }
    }
  
    cycle_counter_start_new_period();
  } else {
    nowi_protocol_add_data_type(&protocol, NOWI_DATA_TYPE_NOT_CONFIGURED); // 1 Byte
  }  

```
