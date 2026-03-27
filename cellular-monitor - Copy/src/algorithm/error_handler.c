/* 
  Handlers for alerts and errors
*/

#include "cloud.h"
#include "error_handler.h"
#include "algo_hw.h"
#include <zephyr/toolchain.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(err_handler, CONFIG_APP_LOG_LEVEL);

/**
 * \brief Handles critical errors, sends an alert to the cloud then restarts the device
 *
 * \param instantReset Whether to instantly reset the device or do so after a confirmation
**/
void Error_Handler( ERROR_CODE_t errorCode, bool instantReset )
{
    LOG_INF("Error_Handler errorCode=%d, instantReset=%d", errorCode, instantReset);
    // TODO try to save half cycles to flash

    if (instantReset)
    {
        LOG_INF("Instant reset");
        //HAL_Delay(1000);
        // cycle_counter_save_half_cycles();
        // sys_reboot();
    }
    else
    {
        LOG_INF("Reset after confirmation");

        struct alert_packet_t alert_packet = {
            .error_code = errorCode,
            .version = 0,
            .size = 0,
            .uptime_ts = k_uptime_get()
        };
        cloud_queue_alert(alert_packet);
        
        // TODO is this still necessary? Better ways to recover? It's only used if comms errors out too much
        // TODO send confirmation to cloud and wait for confirmation then reset
    }
}

/*!
 * \brief Sends an alert to the cloud
 *
 * \param version The error API version. Allows for backwards compatibility
 */
void Alert_Data(ERROR_CODE_t errorCode, uint16_t version, int16_t* errorData, uint8_t size)
{
    LOG_INF("Alert_Data errorCode=%d, version=%d, size=%d", errorCode, version, size);

    struct alert_packet_t alert_packet = {
        .error_code = errorCode,
        .version = version,
        .size = size,
        .uptime_ts = k_uptime_get()
    };

    if (size > 0)
    {
        if (size > CONFIG_ALERT_MSG_MAX_SIZE)
        {
            LOG_ERR("Alert message too long, only adding first 4 int16_t values");
            size = 4;
        }

        alert_packet.data = 0;  // Initialize to 0
        for (uint8_t i = 0; i < size; i++)
        {
            alert_packet.data |= ((int64_t)(errorData[i] & 0xFFFF) << (i * 16));
        }
    }

    cloud_queue_alert(alert_packet);
}

void Reset_Reason(int32_t reason)
{
    struct alert_packet_t alert_packet = {
        .error_code = ERROR_CODE_RESET_REASON,
        .version = 0,
        .size = 2,
        .uptime_ts = k_uptime_get()
    };

    alert_packet.data = (int64_t)reason;

    cloud_queue_alert(alert_packet);
}