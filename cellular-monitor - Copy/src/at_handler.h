#ifndef AT_HANDLER_H
#define AT_HANDLER_H

/**
 * @brief AT commands supported:
 *
 * AT+DEVICEID=<id>      - Set the device ID (up to 16 characters)
 * AT+DEVICEID?          - Get information about the device ID command
 * AT+DEVICEID=?         - Test the device ID functionality
 *
 * AT+PROVISIONED=<0|1>  - Set the provisioning state (0=not provisioned, 1=provisioned)
 * AT+PROVISIONED?       - Get information about the provisioning command
 * AT+PROVISIONED=?      - Test the provisioning functionality
 *
 * AT+PULSE=<y|n>        - Set the device type (y=pulse tracker, n=magnetometer)
 * AT+PULSE?             - Get information about the pulse command
 * AT+PULSE=?            - Test the pulse functionality
 */

#endif