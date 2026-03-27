#ifndef __CLOUD_H__
#define __CLOUD_H__

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "utils.h"
#include "codec.h"

int cloud_init(struct app_info_t app_info);
int cloud_start(void);
int cloud_stop(void);
int cloud_queue_data(struct data_packet_t data_packet);
int cloud_queue_alert(struct alert_packet_t alert_packet);

// Add these declarations at the appropriate place in the header file
extern void enable_device(void);
extern void disable_device(void);

int send_device_state_to_cloud(bool is_enabled);

/**
 * @brief Send data now
 *
 * @return int error code
 * @retval 0 on success
 * @retval -ENOTCONN if not connected to the cloud
 * @retval -ESHUTDOWN if cloud is not started
 */
int cloud_send_data_now(void);

#endif // CLOUD_H__