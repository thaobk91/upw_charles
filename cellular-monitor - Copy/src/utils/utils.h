#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MS_IN_HOUR 3600000 // how many ms in an hour
#define MAX_ICCID_LEN 30
#define MAX_IMEI_LEN 30
#define MAX_FW_VERSION_LEN 30
#define MAX_MFW_VERSION_LEN 30
#define MAX_ALGO_TYPE_LEN 30

struct app_info_t
{
	uint32_t reset_cause;
	char device_id_buf[CONFIG_MAX_DEV_ID_LEN];
	uint8_t device_id_len;
	bool is_provisioned;
};

struct data_packet_t
{
	uint32_t half_cycles;
#ifdef CONFIG_SEND_LOWEST_DATA
	bool has_ldbp : 1;
	uint32_t ldbp;
	bool has_lpr : 1;
	float lpr;
#endif
	bool has_acfg : 1;
	bool acfg;
	int64_t uptime_ts;
};

struct alert_packet_t
{
	int64_t data;
	uint32_t size;
	uint32_t error_code;
	uint32_t version;
	int64_t uptime_ts;
};

typedef enum
{
	UNKNOWN_TOPIC_TYPE = 0,
	SHADOW_DELTA_TOPIC_TYPE = 1,
	DOWNLINK_TOPIC_TYPE = 2,
	PROVIS_ACCEPT_TOPIC_TYPE = 3,
	PROVIS_REJECT_TOPIC_TYPE = 4,
} topics_t;

struct shadow_dev_t
{
	char device_id[CONFIG_MAX_DEV_ID_LEN];
	char iccid[MAX_ICCID_LEN];
	char imei[MAX_IMEI_LEN];
	char fw_version[MAX_FW_VERSION_LEN];
	char algo_type[MAX_ALGO_TYPE_LEN];
	char mfw_version[MAX_MFW_VERSION_LEN];
	bool provis_state : 1;
	bool algo_log_enabled : 1;
	int64_t ts;
};

struct shadow_roam_t
{
	int16_t rsrp;
	int16_t bat_voltage;
	bool psm_changed : 1;
	bool psm_enabled : 1;
	int tau;
	int active_time;
	bool edrx_changed : 1;
	int edrx;
	int ptw;
	int64_t ts;
	int16_t snr;
};

struct shadow_cfg_t
{
	bool cfg_changed : 1;
	uint32_t upload_duty_cycle;
	uint32_t app_duty_cycle;
	bool devstate : 1;
	bool is_pulse_tracker : 1;
	bool has_algo : 1;
	char algo[MAX_ALGO_TYPE_LEN];
	bool log_enabled : 1;
};

struct shadow_packets_t
{
	uint8_t packets_count;
	struct data_packet_t data[CONFIG_MAX_PACKETS_PER_UPLINK_MESSAGE];
};

struct shadow_alerts_t
{
	uint8_t alerts_count;
	struct alert_packet_t alert[CONFIG_MAX_ALERTS_PER_UPLINK_MESSAGE];
};

struct shadow_update_t
{
	uint16_t app_packet_count;
	bool has_dev : 1;
	struct shadow_dev_t dev;
	struct shadow_roam_t roam;
	bool has_cfg : 1;
	struct shadow_cfg_t cfg;
	struct shadow_packets_t packets;
	struct shadow_alerts_t alerts;
};

#endif // __UTILS_H__