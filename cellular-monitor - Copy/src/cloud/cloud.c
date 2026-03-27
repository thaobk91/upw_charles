#include <stdio.h>
#include <stdlib.h>
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <modem/modem_key_mgmt.h>
#include <modem/modem_info.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>

#include <net/aws_iot.h>
#include <date_time.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <cJSON.h>
#include <cJSON_os.h>
#include <hw_id.h>
#include "flash_utility.h"
#include "app.h"
#include "cloud.h"
#include "led_manager.h"
#include "cycle_counter.h"
#include "error_handler.h"
#include "log_handler.h"
#include <zephyr/logging/log.h>

#if defined(CONFIG_ADD_CLAIM_CERTIFICATES)
#include "claim_certs.h"
#endif

LOG_MODULE_REGISTER(cloud, CONFIG_APP_LOG_LEVEL);

#define AWS_CLOUD_CLIENT_ID_LEN (HW_ID_LEN - 1)

#define UPLINK_TOPIC "nowi/cellular/devices/%s/uplink"
#define UPLINK_TOPIC_LEN (29 + AWS_CLOUD_CLIENT_ID_LEN)
#define DOWNLINK_TOPIC "nowi/cellular/devices/%s/downlink"
#define DOWNLINK_TOPIC_LEN (31 + AWS_CLOUD_CLIENT_ID_LEN)
#define SHADOW_DELTA_TOPIC "$aws/things/%s/shadow/update/delta"
#define SHADOW_DELTA_TOPIC_LEN (32 + AWS_CLOUD_CLIENT_ID_LEN)

#define PROVIS_ACP_TOPIC "provisioning/%s/accepted"
#define PROVIS_ACP_TOPIC_LEN (22 + AWS_CLOUD_CLIENT_ID_LEN)
#define PROVIS_REJ_TOPIC "provisioning/%s/rejected"
#define PROVIS_REJ_TOPIC_LEN (22 + AWS_CLOUD_CLIENT_ID_LEN)
#define PROVIS_REQ_TOPIC "provisioning/%s/json"
#define PROVIS_REQ_TOPIC_LEN (18 + AWS_CLOUD_CLIENT_ID_LEN)

#define APP_SUB_TOPICS_COUNT 3
#define APP_PUB_TOPICS_COUNT 1

#define APP_SUB_TOPIC_IDX_DOWNLINK 0
#define APP_SUB_TOPIC_IDX_PROVIS_ACP 1
#define APP_SUB_TOPIC_IDX_PROVIS_REJ 2

#define APP_PUB_TOPIC_IDX_PROVIS_REQ 0

#define MAX_SEND_RETRIES 5
#define SEND_RETRY_DELAY_SECONDS 60

/* RSRP offset value for converting modem values to dBm */
#define RSRP_OFFSET_VAL 141

/* New constants for MQTT and power management */
#define MAX_MQTT_KEEPALIVE_SECONDS 1200 /* 20 minutes */
/**
 * If PSM is inactive and the upload duty cycle is greater than this value,
 * the device will go offline between uploads.
 */
#define MAX_UDC_FOR_INACTIVE_PSM_TO_STAY_ONLINE 1200 /* 20 minutes */
/* These could be changed to active time (ACT) when in PSM mode */
#define MQTT_ACK_TIMEOUT_MS 10000 /* 10 seconds */
#define MQTT_RX_TIMEOUT_MS 10000	/* 10 seconds */


//fix undeclared 
#define RSRP_OFFSET_VAL 140

extern bool device_enabled;
static bool cloud_started = false;

static char client_id_buf[AWS_CLOUD_CLIENT_ID_LEN + 1] = {0};
static char downlink_topic[DOWNLINK_TOPIC_LEN + 1] = {0};
static char uplink_topic[UPLINK_TOPIC_LEN + 1] = {0};
static char delta_topic[SHADOW_DELTA_TOPIC_LEN + 1] = {0};
static char provis_request_topic[PROVIS_REQ_TOPIC_LEN + 1] = {0};
static char provis_accepted_topic[PROVIS_ACP_TOPIC_LEN + 1] = {0};
static char provis_rejected_topic[PROVIS_REJ_TOPIC_LEN + 1] = {0};

static struct mqtt_topic sub_topics[APP_SUB_TOPICS_COUNT];
static struct mqtt_topic pub_topics[APP_PUB_TOPICS_COUNT];

/* New variables for transmission management */
static volatile bool waiting_for_puback = false;
static volatile int32_t pending_message_id = -1;
static volatile bool waiting_for_rx = false;
static volatile bool psm_active = false;

// Temp storage for data that's been sent but not yet ACKed
static uint16_t sent_packets_count = 0;
static uint16_t sent_alerts_count = 0;

static volatile bool update_shadow_flag = true; // We want to update shadow immediately after connecting
static volatile bool cellular_connected = false;
static volatile bool cellular_registered = false;
static volatile bool aws_cloud_connected = false;
static volatile bool cloud_initialized = false;
static volatile bool psm_enabled = true;
static volatile bool psm_changed = true;
static volatile bool psm_values_changed = true;
static volatile bool edrx_values_changed = true;
static volatile bool cfg_changed = true;
// Add tracking variables to know if changes have been sent
static volatile bool cfg_changes_sent = false;
static volatile bool psm_changes_sent = false;
static bool device_provisioned = false;
static bool private_key_changed = false;
static bool certificate_pem_changed = false;
static char private_key[2048] = {0};		 // Could move to heap, only used once
static char certificate_pem[2048] = {0}; // Could move to heap, only used once

static uint32_t upload_duty_cycle = CONFIG_DEFAULT_UPLOAD_DUTY_CYCLE_SECONDS;

#define FIRST_PACKET_COUNT 1
static uint16_t app_packet_count = FIRST_PACKET_COUNT; // AWS IoT requires a non-zero value for message_id
static volatile uint32_t last_rsrp = INT_MIN;
static volatile int16_t last_snr = INT16_MIN;

static struct k_work_delayable shadow_update_work;
static struct k_work_delayable lte_connect_work;
static struct k_work_delayable aws_connect_work;
static struct k_work_delayable provisioning_work;

/* New work items for transmission process */
static struct k_work_delayable puback_timeout_work;
static struct k_work_delayable rx_timeout_work;
static struct k_work_delayable finish_transmission_work;
static struct k_work_delayable reconnect_work;

static volatile bool lte_connect_attempted = false;			 // Track if we've already attempted to connect
static volatile int64_t last_periodic_search_uptime = 0; // Track when we last performed a periodic search request

/* Static functions */
static void rsrp_handler(char rsrp_value);
static void lte_handler(const struct lte_lc_evt *const evt);
static void shadow_update_timer_event_handler(struct k_timer *timer_id);
static void connection_reconnect_timer_event_handler(struct k_timer *timer_id);
static void connection_timer_event_handler(struct k_timer *timer_id);
static int json_add_obj(cJSON *parent, const char *str, cJSON *item);
static int json_add_str(cJSON *parent, const char *str, const char *item);
static bool string_compare(const char *str1, const char *str2, const uint16_t len);
static void trigger_shadow_update(void);

/* New function prototypes for transmission management */
static void puback_timeout_handler(struct k_work *work);
static void rx_timeout_handler(struct k_work *work);
static void finish_transmission_handler(struct k_work *work);
static void reconnect_work_fn(struct k_work *work);
static void received_ack(void);

K_TIMER_DEFINE(shadow_update_timer, shadow_update_timer_event_handler, NULL);
K_TIMER_DEFINE(reconnect_timer, connection_reconnect_timer_event_handler, NULL);
K_TIMER_DEFINE(connection_timer, connection_timer_event_handler, NULL);
K_MSGQ_DEFINE(uplink_msgq, sizeof(struct data_packet_t), 400, 1);
K_MSGQ_DEFINE(alert_msgq, sizeof(struct alert_packet_t), 10, 1);

void uplink_queue_schedule(uint32_t _upload_duty_cycle_s)
{

#ifdef CONFIG_SYNC_UPLOAD
	int ret = 0;
	int64_t timestamp = 0;
	ret = date_time_now(&timestamp);
	if (ret)
	{
		// Typically means it's not connected to the cloud
		LOG_DBG("date_time_now() failed, ret: %d", ret);
		// Just in case getting the time failed!
		k_timer_start(&shadow_update_timer, K_SECONDS(_upload_duty_cycle_s), K_SECONDS(_upload_duty_cycle_s));
		return;
	}

	int64_t time_left_for_top_hour_ms = MS_IN_HOUR - (timestamp % MS_IN_HOUR);
	int64_t time_til_next_upload_ms = time_left_for_top_hour_ms % (_upload_duty_cycle_s * 1000);

	LOG_INF("Upload duty cycle(s): %d, time till top hour(ms): %llu, time till next upload(ms): %llu", _upload_duty_cycle_s, time_left_for_top_hour_ms, time_til_next_upload_ms);

	k_timer_start(&shadow_update_timer, K_MSEC(time_til_next_upload_ms), K_SECONDS(_upload_duty_cycle_s));
#else
	LOG_INF("Upload duty cycle(s): %d", _upload_duty_cycle_s);
	k_timer_start(&shadow_update_timer, K_SECONDS(_upload_duty_cycle_s), K_SECONDS(_upload_duty_cycle_s));
#endif
}

static void provisioning_work_fn(struct k_work *work)
{
	int err = 0;

	LOG_INF("Started provisioning work, going offline...");

	err = aws_iot_disconnect();
	if (err)
	{
		LOG_ERR("aws_iot_disconnect() failed, err %d", err);
	}

	err = lte_lc_offline(); // CFUN=4 to the modem. // lte_lc_power_off() is CFUN=0 which will require a power cycle to wake up
	if (err)
	{
		LOG_ERR("lte_lc_offline() failed, err %d", err);
	}

	lte_connect_attempted = false;

	err = modem_key_mgmt_write(CONFIG_MQTT_HELPER_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_PUBLIC_CERT, certificate_pem, (strlen(certificate_pem) - 1));
	if (err)
	{
		LOG_ERR("modem_key_mgmt_write() on PUBLIC_CERT failed, err %d", err);
	}
	else
	{
		LOG_INF("modem_key_mgmt_write() on PUBLIC_CERT success, len %d", strlen(certificate_pem));
		// LOG_INF("OBJECT_PROVIS_CERT_PEM (strlen %d):%s", strlen(certificate_pem), certificate_pem);

		err = modem_key_mgmt_write(CONFIG_MQTT_HELPER_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_PRIVATE_CERT, private_key, (strlen(private_key) - 1));
		if (err)
		{
			LOG_ERR("modem_key_mgmt_write() on PRIVATE_CERT failed, err %d", err);
		}
		else
		{
			LOG_INF("modem_key_mgmt_write() on PRIVATE_CERT success, len %d", strlen(private_key));
			// LOG_INF("OBJECT_PROVIS_PRIV_KEY (strlen %d):%s", strlen(private_key), private_key);

			if (FlashUtility_SetProvisioning_State(true) == FlashStatus_Success)
			{
				LOG_INF("Provisioning successful, reconnecting device...");
				// Reconnect to AWS IoT broker to use the new credentials
				device_provisioned = true;
				private_key_changed = false;
				certificate_pem_changed = false;
				k_work_schedule(&reconnect_work, K_NO_WAIT);
			}
			else
			{
				LOG_ERR("Provisioning failed, could not save new provisioning state to the flash");
			}
		}
	}

	LOG_INF("Triggering shadow update after provisioning");
	trigger_shadow_update();
}

void link_action_take(char *action)
{
	int err = 0;

	if (strcmp(action, "psmen") == 0)
	{
		LOG_INF("Enabling PSM...");

		err = lte_lc_psm_req(true);
		if (err)
		{
			LOG_ERR("Requesting PSM failed, err: %d", err);
			return;
		}

		psm_enabled = true;
		psm_changed = true;
		psm_changes_sent = false; // Reset since we have new changes to send
	}

	else if (strcmp(action, "psmdis") == 0)
	{

		LOG_INF("Disabling PSM...");

		err = lte_lc_psm_req(false);
		if (err)
		{
			LOG_ERR("Disabling PSM failed, err: %d", err);
			return;
		}
		psm_enabled = false;
		psm_changed = true;
		psm_changes_sent = false; // Reset since we have new changes to send
	}
	else
	{
		LOG_WRN("link_action_take unknown action %s", action);
	}
}

static void handle_received_data(const char *buf, const char *topic, size_t topic_len)
{
	int err = 0;
	char *str = NULL;
	cJSON *root_obj = NULL;

	topics_t topic_type = UNKNOWN_TOPIC_TYPE;

	root_obj = cJSON_Parse(buf);
	if (root_obj == NULL)
	{
		LOG_ERR("cJSON Parse failure");
		return;
	}

	str = cJSON_Print(root_obj);
	if (str == NULL)
	{
		LOG_ERR("Failed to print JSON object");
		goto clean_exit;
	}

#if CONFIG_THREAD_ANALYZER
	thread_analyzer_print();
#endif

	LOG_INF("Data received from AWS IoT console: Topic: %s\n Message: \n%s\n", topic, str);

	if (string_compare(topic, downlink_topic, topic_len))
	{
		LOG_INF("Data RX on downlink topic");
		topic_type = DOWNLINK_TOPIC_TYPE;
	}
	else if (string_compare(topic, delta_topic, topic_len))
	{
		LOG_INF("Data RX on delta topic");
		topic_type = SHADOW_DELTA_TOPIC_TYPE;
	}
	else if (string_compare(topic, provis_accepted_topic, topic_len))
	{
		LOG_INF("Data RX on provisioning accepted topic");
		topic_type = PROVIS_ACCEPT_TOPIC_TYPE;
	}
	else if (string_compare(topic, provis_rejected_topic, topic_len))
	{
		LOG_INF("Data RX on provisioning rejected topic");
		topic_type = PROVIS_REJECT_TOPIC_TYPE;
	}
	else
	{
		LOG_WRN("Data RX on unknown topic (%s)", topic);
	}

	if (topic_type == SHADOW_DELTA_TOPIC_TYPE)
	{
		cJSON *state_obj = cJSON_GetObjectItem(root_obj, "state");
		if (state_obj == NULL)
		{
			LOG_ERR("No state object");
			goto clean_exit;
		}

		cJSON *config_obj = cJSON_GetObjectItem(state_obj, "cfg");
		if (state_obj == NULL)
		{
			LOG_ERR("No config object");
			goto clean_exit;
		}

		cJSON *obj_value = cJSON_GetObjectItem(config_obj, "adc");
		if (obj_value != NULL)
		{
			uint32_t adc_obj_value = cJSON_GetNumberValue(obj_value);
			LOG_INF("state->cfg->adc, adc_obj_value: %d", adc_obj_value);
			app_update_set_duty_cycle(adc_obj_value);
			cfg_changed = true;
			cfg_changes_sent = false; // Reset since we have new changes to send
		}
		obj_value = NULL;

		obj_value = cJSON_GetObjectItem(config_obj, "udc");
		if (obj_value != NULL)
		{
			uint32_t udc_obj_value = cJSON_GetNumberValue(obj_value);
			LOG_INF("state->cfg->udc, udc_obj_value: %d", udc_obj_value);
			upload_duty_cycle = udc_obj_value;
			uplink_queue_schedule(upload_duty_cycle);
			cfg_changed = true;
			cfg_changes_sent = false; // Reset since we have new changes to send
		}
		obj_value = NULL;

		obj_value = cJSON_GetObjectItem(config_obj, "ispulse");
		if (obj_value != NULL)
		{
			bool is_pulse_tracker = cJSON_IsTrue(obj_value);
			LOG_INF("state->cfg->ispulse: %d", is_pulse_tracker);
			if (is_pulse_tracker != (app_get_device_type() == DEVICE_TYPE_PULSE_TRACKER))
			{
				LOG_INF("Device type changed, setting new type and rebooting");
				if (is_pulse_tracker)
				{
					LOG_INF("Changing device type to Pulse Tracker");
					app_set_device_type(DEVICE_TYPE_PULSE_TRACKER);
				}
				else
				{
					LOG_INF("Device is not pulse tracker, it could be anything else. Relying on algo setting");
				}
			}
		}
		obj_value = NULL;

		obj_value = cJSON_GetObjectItem(config_obj, "log");
		if (obj_value != NULL)
		{
			bool log_enabled = cJSON_IsTrue(obj_value);
			LOG_INF("state->cfg->log: %d", log_enabled);
			log_handler_set_enabled(log_enabled);
			cfg_changed = true;
			cfg_changes_sent = false;
		}
		obj_value = NULL;

		cJSON *algo_obj = cJSON_GetObjectItem(config_obj, "algo");
		if (algo_obj != NULL)
		{
			LOG_INF("Received algorithm configuration");
			if (cJSON_IsString(algo_obj) && (algo_obj->valuestring != NULL))
			{
				char *algo_type = cJSON_GetStringValue(algo_obj);
				LOG_INF("Algorithm type: %s", algo_type);

				// Validate algorithm type string length
				if (strlen(algo_type) >= MAX_ALGO_TYPE_LEN)
				{
					LOG_ERR("Algorithm type string too long: %s", algo_type);
				}
				else
				{
					// Log current device type for debugging
					LOG_INF("Current device type: %d", app_get_device_type());

					// Handle algorithm type changes
					if (strcmp(algo_type, "PULSE_TRACKER") == 0)
					{
						if (app_get_device_type() != DEVICE_TYPE_PULSE_TRACKER)
						{
							LOG_INF("Algorithm type changed to Pulse Tracker, updating device type");
							app_set_device_type(DEVICE_TYPE_PULSE_TRACKER);
							cfg_changed = true;
							// app_set_device_type will handle the reboot
						}
						else
						{
							LOG_INF("Device already configured as Pulse Tracker");
						}
					}
					else if (strcmp(algo_type, "SENSUS_PROTOCOL") == 0)
					{
						if (app_get_device_type() != DEVICE_TYPE_SENSUS_PROTOCOL)
						{
							LOG_INF("Algorithm type changed to Sensus Protocol, updating device type");
							app_set_device_type(DEVICE_TYPE_SENSUS_PROTOCOL);
							cfg_changed = true;
							// app_set_device_type will handle the reboot
						}
						else
						{
							LOG_INF("Device already configured as Sensus Protocol");
						}
					}
					else if (strcmp(algo_type, "MAGNETOMETER") == 0)
					{
						if (app_get_device_type() != DEVICE_TYPE_MAGNETOMETER)
						{
							LOG_INF("Algorithm type changed to %s, updating device type to Magnetometer", algo_type);
							app_set_device_type(DEVICE_TYPE_MAGNETOMETER);
							cfg_changed = true;
							// app_set_device_type will handle the reboot
						}
						else
						{
							LOG_INF("Device already configured as Magnetometer");
						}
					}
					else
					{
						LOG_WRN("Unknown algorithm type: %s", algo_type);
					}
				}
			}
			else
			{
				LOG_WRN("Invalid algorithm configuration format");
			}
		}

		cJSON *algo_vars_obj = cJSON_GetObjectItem(config_obj, "avrs");
		if (algo_vars_obj != NULL)
		{
			LOG_INF("Received algorithm variables");
			if (cJSON_IsObject(algo_vars_obj))
			{
				for (int i = 0; i < ALGORITHM_VARIABLES_COUNT; i++)
				{
					obj_value = cJSON_GetObjectItem(algo_vars_obj, algorithm_variables_s[i]);
					if (obj_value != NULL)
					{
						if (cJSON_IsNumber(obj_value))
						{
							uint32_t value = cJSON_GetNumberValue(obj_value);
							LOG_INF("state->cfg->avrs->%s: %d", algorithm_variables_s[i], value);
							app_update_variable(algorithm_variables_s[i], value);
						}
						else
						{
							LOG_WRN("Invalid algorithm variable %s format", algorithm_variables_s[i]);
						}
					}
				}
			}
			else
			{
				LOG_WRN("Invalid algorithm variables format");
			}
		}
	}

	else if (topic_type == DOWNLINK_TOPIC_TYPE)
	{
		cJSON *algo_obj = cJSON_GetObjectItemCaseSensitive(root_obj, "algo");
		if (algo_obj != NULL)
		{
			char *algo = cJSON_GetStringValue(algo_obj);
			LOG_INF("algo: %s", algo);
			if (strcmp(algo, "PULSE_TRACKER") == 0)
			{
				app_set_device_type(DEVICE_TYPE_PULSE_TRACKER);
			}
			else if (strcmp(algo, "MAGNETOMETER") == 0)
			{
				app_set_device_type(DEVICE_TYPE_MAGNETOMETER);
			}
			else if (strcmp(algo, "SENSUS_PROTOCOL") == 0)
			{
				app_set_device_type(DEVICE_TYPE_SENSUS_PROTOCOL);
			}
		}

		cJSON *is_pulse_tracker_obj = cJSON_GetObjectItemCaseSensitive(root_obj, "is_pulse_tracker");
		if (is_pulse_tracker_obj != NULL)
		{
			bool is_pulse_tracker = cJSON_GetNumberValue(is_pulse_tracker_obj);
			LOG_INF("is_pulse_tracker: %d", is_pulse_tracker);
			app_set_device_type(is_pulse_tracker ? DEVICE_TYPE_PULSE_TRACKER : DEVICE_TYPE_MAGNETOMETER);
		}

		cJSON *action_obj = cJSON_GetObjectItemCaseSensitive(root_obj, "action");
		if (cJSON_IsString(action_obj) && (action_obj->valuestring != NULL))
		{
			if (strcmp(action_obj->valuestring, "disable_device") == 0)
			{
				LOG_INF("Received command to disable device");
				disable_device();
				cfg_changed = true;
			}
			else if (strcmp(action_obj->valuestring, "enable_device") == 0)
			{
				LOG_INF("Received command to enable device");
				enable_device();
				cfg_changed = true;
			}
		}
		else
		{
			LOG_WRN("Unknown action: %s", action_obj->valuestring);
		}

		cJSON *app_action_obj = cJSON_GetObjectItem(root_obj, "apact");
		if (app_action_obj != NULL)
		{
			char *app_action = cJSON_GetStringValue(app_action_obj);
			LOG_INF("apact: %s", app_action);
			app_action_take(app_action);
		}

		cJSON *link_action_obj = cJSON_GetObjectItem(root_obj, "lnkact");
		if (link_action_obj != NULL)
		{
			char *link_action = cJSON_GetStringValue(link_action_obj);
			LOG_INF("lnkact: %s", link_action);
			link_action_take(link_action);
		}
	}
	else if (topic_type == PROVIS_ACCEPT_TOPIC_TYPE && device_provisioned == false)
	{
		cJSON *certpem_obj = cJSON_GetObjectItem(root_obj, "certificatePem");
		if (certpem_obj != NULL)
		{
			char *cert_pem = cJSON_GetStringValue(certpem_obj);
			memcpy(certificate_pem, cert_pem, strlen(cert_pem));
			private_key_changed = true;
		}

		cJSON *privkey_obj = cJSON_GetObjectItem(root_obj, "privateKey");
		if (privkey_obj != NULL)
		{
			char *priv_key = cJSON_GetStringValue(privkey_obj);
			memcpy(private_key, priv_key, strlen(priv_key));
			certificate_pem_changed = true;
		}

		if (private_key_changed && certificate_pem_changed)
		{
			printk("Received OBJECT_PROVIS_CERT_PEM (strlen %d):\n%s\n", strlen(certificate_pem), certificate_pem);
			printk("Received OBJECT_PROVIS_PRIV_KEY (strlen %d):\n%s\n", strlen(private_key), private_key);

			err = k_work_schedule(&provisioning_work, K_NO_WAIT);
			if (err < 0)
			{
				LOG_ERR("k_work_schedule(&provisioning_work) failed, err %d", err);
			}
		}
	}

	cJSON_FreeString(str);

clean_exit:
	cJSON_Delete(root_obj);
}

void aws_iot_event_handler(const struct aws_iot_evt *const evt)
{
	int err = 0;

	switch (evt->type)
	{
	case AWS_IOT_EVT_CONNECTING:
		LOG_INF("AWS_IOT_EVT_CONNECTING");
		led_manager_led_on_for(LED_INFO_DURATION_MS, LED_INFO);
		break;

	case AWS_IOT_EVT_CONNECTED:
		LOG_INF("AWS_IOT_EVT_CONNECTED");

		aws_cloud_connected = true;
		led_manager_led_on_for(LED_SUCCESS_CONNECTED_DURATION_MS, LED_SUCCESS);

		(void)k_work_cancel_delayable(&aws_connect_work);

		if (evt->data.persistent_session)
	{
		LOG_INF("Persistent session enabled");
	}

	/** Successfully connected to AWS IoT broker, mark image as
	 *  working to avoid reverting to the former image upon reboot.
	 */
	boot_write_img_confirmed();

	k_work_schedule(&shadow_update_work, K_NO_WAIT);
	break;	
	
	case AWS_IOT_EVT_DISCONNECTED:
		LOG_INF("AWS_IOT_EVT_DISCONNECTED");
		led_manager_led_on_for(LED_ERROR_DISCONNECTED_DURATION_MS, LED_ERROR);
		aws_cloud_connected = false;
		break;

	case AWS_IOT_EVT_DATA_RECEIVED:
		LOG_INF("AWS_IOT_EVT_DATA_RECEIVED, on [%s] topic", evt->data.msg.topic.str);
		led_manager_led_on_for(LED_INFO_DURATION_MS, LED_INFO);
		handle_received_data(evt->data.msg.ptr, evt->data.msg.topic.str,
												 evt->data.msg.topic.len);

		waiting_for_rx = false;

		/* Cancel the RX timeout since we received data */
		k_work_cancel_delayable(&rx_timeout_work);

		/* Check if we've already received PUBACK */
		if (!waiting_for_puback)
		{
			/* If PUBACK already received, finish the transmission */
			k_work_schedule(&finish_transmission_work, K_NO_WAIT);
		}
		/* Otherwise keep waiting for PUBACK */
		break;

	case AWS_IOT_EVT_PUBACK:
		LOG_INF("AWS_IOT_EVT_PUBACK, message ID: %d", evt->data.message_id);
		/* Process PUBACK for tracking message delivery */
		if (waiting_for_puback && evt->data.message_id == pending_message_id)
		{
			LOG_INF("Received ACK for pending message %d", pending_message_id);
			waiting_for_puback = false;

			/* Cancel the PUBACK timeout since we received the ACK */
			k_work_cancel_delayable(&puback_timeout_work);

			received_ack();
			/* Check if RX data already received or timed out */
			if (!waiting_for_rx)
			{
				k_work_schedule(&finish_transmission_work, K_NO_WAIT);
			}
			/* Otherwise keep waiting for RX timeout or data */
		}
		break;

	case AWS_IOT_EVT_FOTA_START:
		LOG_INF("AWS_IOT_EVT_FOTA_START");
		app_stop(); // Saves data
		break;

	case AWS_IOT_EVT_FOTA_ERASE_PENDING:
		LOG_INF("AWS_IOT_EVT_FOTA_ERASE_PENDING");
		LOG_INF("Disconnect LTE link or reboot");
		err = lte_lc_offline();
		if (err)
		{
			LOG_ERR("Error disconnecting from LTE");
		}
		break;

	case AWS_IOT_EVT_FOTA_ERASE_DONE:
		LOG_INF("AWS_FOTA_EVT_ERASE_DONE");
		LOG_INF("Reconnecting the LTE link");
		// Reset the connection attempt flag and try to connect
		lte_connect_attempted = false;
		k_work_schedule(&lte_connect_work, K_NO_WAIT);
		break;

	case AWS_IOT_EVT_FOTA_DONE:
		LOG_INF("AWS_IOT_EVT_FOTA_DONE");
		LOG_INF("FOTA done, rebooting device");
		aws_iot_disconnect();
		sys_reboot(SYS_REBOOT_COLD);
		app_start(); // Doesn't really do anything because the system reboots
		break;

	case AWS_IOT_EVT_FOTA_DL_PROGRESS:
		LOG_INF("AWS_IOT_EVT_FOTA_DL_PROGRESS, (%d%%)", evt->data.fota_progress);
	case AWS_IOT_EVT_ERROR:
		LOG_ERR("AWS_IOT_EVT_ERROR, %d", evt->data.err);
		break;
	case AWS_IOT_EVT_PINGRESP:
		LOG_INF("AWS_IOT_EVT_PINGRESP");
		break;
	case AWS_IOT_EVT_FOTA_ERROR:
		LOG_ERR("AWS_IOT_EVT_FOTA_ERROR");
		app_start(); // restart the app
		break;

	default:
		LOG_WRN("Unknown AWS IoT event type: %d", evt->type);
		break;
	}
}

static void lte_connect_work_fn(struct k_work *work)
{
	int err = 0;
	if (cellular_connected && cellular_registered)
	{
		LOG_WRN("LTE Connect: cellular already connected and registered");
		return;
	}

	// Try to reconnect only if we haven't attempted yet
	if (!lte_connect_attempted)
	{
		LOG_INF("Attempting to reconnect cellular");
		err = lte_lc_connect_async(lte_handler);
		if (err)
		{
			LOG_ERR("lte_lc_connect_async() failed, err %d", err);
		}
		else
		{
			lte_connect_attempted = true;
			LOG_INF("LTE connection attempt initiated");
		}
	}
	else
	{
		LOG_INF("LTE connection already attempted, waiting for connection events");
	}
}
static void aws_connect_work_fn(struct k_work *work)
{
	int err = 0;

	if (!cellular_connected || !cellular_registered)
	{
		LOG_WRN("AWS Connect: cellular not connected %d, or registered %d", cellular_connected, cellular_registered);
		k_work_schedule(&lte_connect_work, K_NO_WAIT);
		return;
	}

	if (aws_cloud_connected)
	{
		LOG_INF("AWS Connect: Already connected to AWS IoT broker");
		return;
	}

	LOG_INF("Connecting to AWS IoT broker");
	const struct aws_iot_config config = {
			.client_id = client_id_buf,
			.host_name = CONFIG_AWS_IOT_BROKER_HOST_NAME,
	};

	int start_time = k_uptime_get();
	// This is synchronous and cant take a while (30+ seconds)
	err = aws_iot_connect(&config);
	if (err)
	{
		LOG_WRN("aws_iot_connect() failed, err %d", err);
	}

	// aws_cloud can connect during aws_iot_connect function
	if (!aws_cloud_connected && cellular_connected && cellular_registered)
	{
		LOG_INF("Next connection retry in %d seconds", CONFIG_AWS_IOT_APP_CONNECTION_RETRY_TIMEOUT_SECONDS);

		err = k_work_schedule(&aws_connect_work, K_SECONDS(CONFIG_AWS_IOT_APP_CONNECTION_RETRY_TIMEOUT_SECONDS));
		if (err < 0)
		{
			LOG_ERR("k_work_schedule(&aws_connect_work) failed, err %d", err);
		}
	}

	LOG_INF("aws_iot_connect() took %lld ms", k_uptime_get() - start_time);
}

static void date_time_event_handler(const struct date_time_evt *evt)
{
	switch (evt->type)
	{
	case DATE_TIME_OBTAINED_MODEM:
	case DATE_TIME_OBTAINED_NTP:

	case DATE_TIME_OBTAINED_EXT:
		LOG_INF("DATE_TIME_OBTAINED");
		// de-register handler as the time is obtained
		date_time_register_handler(NULL);
		app_sync_upload();
		break;

	case DATE_TIME_NOT_OBTAINED:
		LOG_INF("DATE_TIME_NOT_OBTAINED");
		break;

	default:
		LOG_WRN("Unknown event: %d", evt->type);
		break;
	}
}

static int shadow_update(void)
{
	int err = 0;
	int len = 0;
	uint8_t message[CONFIG_MQTT_HELPER_RX_TX_BUFFER_SIZE] = {0};
	size_t message_length = 0;
	int64_t time_s, time_ms = 0;

	int16_t bat_voltage = 0;
	uint32_t packets_num = 0;
	uint32_t alerts_num = 0;
	struct data_packet_t packet = {0};
	struct alert_packet_t alert = {0};

	LOG_INF("Updating the shadow... fcnt: %d", app_packet_count);

	err = date_time_now(&time_ms);
	if (err)
	{
		// Typically means it's not connected to the cloud
		LOG_DBG("date_time_now() failed, err %d", err);
		return err;
	}
	time_s = time_ms / 1000; // Convert to seconds

	struct shadow_roam_t roam = {0};
	struct shadow_update_t update = {0};

	update.app_packet_count = app_packet_count;
	app_packet_count++;

	// Data to just send once
	if (update.app_packet_count == FIRST_PACKET_COUNT)
	{
		struct shadow_dev_t dev = {0};
		strcpy(dev.device_id, client_id_buf);

		uint8_t iccid_buf_s[MAX_ICCID_LEN] = {0};

		len = modem_info_string_get(MODEM_INFO_ICCID, iccid_buf_s, sizeof(iccid_buf_s));
		if (len > 0)
		{
			LOG_INF("MODEM_INFO_ICCID: %s", iccid_buf_s);
			strcpy(dev.iccid, iccid_buf_s);
		}
		else
		{
			LOG_ERR("MODEM_INFO_ICCID: failed, err %d", err);
			strcpy(dev.iccid, "unknown");
		}

		uint8_t imei_buf_s[30] = {0};
		len = modem_info_string_get(MODEM_INFO_IMEI, imei_buf_s, sizeof(imei_buf_s));
		if (len > 0)
		{
			LOG_INF("MODEM_INFO_IMEI: %s", imei_buf_s);
			strcpy(dev.imei, imei_buf_s);
		}
		else
		{
			LOG_WRN("MODEM_INFO_IMEI: failed, err %d", err);
			strcpy(dev.iccid, "unknown");
		}

		strcpy(dev.fw_version, CONFIG_APP_VERSION);
#ifdef CONFIG_ADD_CLAIM_CERTIFICATES
		strcat(dev.fw_version, "-claim");
#endif
		// Deprecated
		switch (app_get_device_type())
		{
		case DEVICE_TYPE_PULSE_TRACKER:
			strcpy(dev.algo_type, "PULSE_TRACKER");
			break;
		case DEVICE_TYPE_SENSUS_PROTOCOL:
			strcpy(dev.algo_type, "SENSUS_PROTOCOL");
			break;
		default:
			strcpy(dev.algo_type, CONFIG_ALGORITHM);
		}

		uint8_t mfwv_buf_s[30] = {0};
		len = modem_info_string_get(MODEM_INFO_FW_VERSION, mfwv_buf_s, sizeof(mfwv_buf_s));
		if (len > 0)
		{
			LOG_INF("MODEM_INFO_FW_VERSION: %s", mfwv_buf_s);
			strcpy(dev.mfw_version, mfwv_buf_s);
		}
		else
		{
			LOG_WRN("MODEM_INFO_FW_VERSION: failed, err %d", err);
			strcpy(dev.mfw_version, "unknown");
		}

		dev.provis_state = device_provisioned;

		// Set logging state based on log handler
		// DEPRECATED
		dev.algo_log_enabled = log_handler_is_enabled();

		dev.ts = time_s;
		update.has_dev = true; // include dev on first time
		update.has_cfg = true; // include cfg on first time

		memcpy(&update.dev, &dev, sizeof(dev));
	}

	int snr = 0;
	int snr_err = modem_info_get_snr(&snr);
	if (snr_err)
	{
		// Pretty common for this to fail if it's been a while since last RX
		LOG_INF("modem_info_get_snr() failed, err: %d", snr_err);
		if (last_snr == INT_MIN)
		{
			roam.snr = 0;
		}
		else
		{
			roam.snr = last_snr;
		}
	}
	else
	{
		LOG_INF("modem_info_get_snr() success, snr: %d", snr);
		roam.snr = snr;
	}
	last_snr = INT16_MIN; // Resetting it so we don't keep using the same value

	int rsrp = 0;
	int rsrp_err = modem_info_get_rsrp(&rsrp);
	if (rsrp_err)
	{
		LOG_WRN("modem_info_get_rsrp() failed, err: %d", rsrp_err);
		if (last_rsrp == INT_MIN)
		{
			roam.rsrp = 0;
		}
		else
		{
			roam.rsrp = last_rsrp;
		}
	}
	else
	{
		LOG_INF("modem_info_get_rsrp() success, rsrp: %d", rsrp);
		roam.rsrp = rsrp;
	}

	last_rsrp = INT_MIN; // Resetting it so we don't keep using the same value

	if (psm_values_changed)
	{
		psm_values_changed = false;
		int tau = 0, active_time = 0;
		int get_psm_err = lte_lc_psm_get(&tau, &active_time);
		if (get_psm_err)
		{
			LOG_WRN("lte_lc_psm_get() failed, err: %d", get_psm_err);
		}
		else
		{
			roam.psm_changed = true;
			roam.tau = tau;
			roam.active_time = active_time;
		}
	}

	if (edrx_values_changed)
	{
		edrx_values_changed = false;
		struct lte_lc_edrx_cfg edrx_cfg = {0};
		int edrx_err = lte_lc_edrx_get(&edrx_cfg);
		if (edrx_err != 0)
		{
			LOG_WRN("lte_lc_edrx_get() failed, err: %d", edrx_err);
		}
		else
		{
			roam.edrx_changed = true;
			roam.edrx = edrx_cfg.edrx;
			roam.ptw = edrx_cfg.ptw;
		}
	}

	len = modem_info_short_get(MODEM_INFO_BATTERY, &bat_voltage);
	if (len != sizeof(bat_voltage))
	{
		LOG_WRN("modem_info_short_get() failed, err: %d", len);
		roam.bat_voltage = 0;
	}
	else
	{
		roam.bat_voltage = bat_voltage;
	}

	if (psm_changed)
	{
		roam.psm_changed = true;
		if (psm_enabled)
		{
			roam.psm_enabled = true;
		}
		else
		{
			roam.psm_enabled = false;
		}
		/* Mark that we're sending PSM changes */
		psm_changes_sent = true;
	}
	else
	{
		roam.psm_changed = false;
	}

	roam.ts = time_s;

	memcpy(&update.roam, &roam, sizeof(roam));

	struct shadow_cfg_t cfg = {0};

	if (cfg_changed)
	{
		/* Don't reset cfg_changed here, wait for PUBACK confirmation */
		update.has_cfg = true; // include cfg when changed
		cfg.cfg_changed = true;
		cfg.upload_duty_cycle = upload_duty_cycle;
		cfg.app_duty_cycle = app_update_get_duty_cycle();
		cfg.devstate = device_enabled;
		cfg.is_pulse_tracker = app_get_device_type() == DEVICE_TYPE_PULSE_TRACKER;
		/* Mark that we're sending cfg changes */
		cfg_changes_sent = true;
		cfg.has_algo = true;
		cfg.log_enabled = log_handler_is_enabled();
		switch (app_get_device_type())
		{
		case DEVICE_TYPE_PULSE_TRACKER:
			strcpy(cfg.algo, "PULSE_TRACKER");
			break;
		case DEVICE_TYPE_SENSUS_PROTOCOL:
			strcpy(cfg.algo, "SENSUS_PROTOCOL");
			break;
		default:
			strcpy(cfg.algo, "MAGNETOMETER");
		}
	}
	else
	{
		cfg.cfg_changed = false;
	}

	memcpy(&update.cfg, &cfg, sizeof(cfg));

	uint16_t packets_sent = 0;
	packets_num = k_msgq_num_used_get(&uplink_msgq);
	LOG_DBG("uplink queue count %d", packets_num);

	if (packets_num)
	{
		update.packets.packets_count = (packets_num > CONFIG_MAX_PACKETS_PER_UPLINK_MESSAGE) ? CONFIG_MAX_PACKETS_PER_UPLINK_MESSAGE : packets_num;
		for (uint16_t i = 0; i < update.packets.packets_count; i++)
		{
			err = k_msgq_peek_at(&uplink_msgq, &packet, i);
			if (err == 0)
			{
				// Calculate the correct packet timestamp
				int64_t packet_hold_time = k_uptime_get() - packet.uptime_ts;
				int64_t packet_timestamp = time_ms - packet_hold_time;
				int64_t packet_timestamp_s = packet_timestamp / 1000;

				update.packets.data[i].half_cycles = packet.half_cycles;
				update.packets.data[i].uptime_ts = packet_timestamp_s;

				if (packet.acfg == false)
				{
					update.packets.data[i].has_acfg = true; // used for protobuf encode
					update.packets.data[i].acfg = false;
				}
				else
				{

					// Data is only relevant if the algorithm is configured
					update.packets.data[i].has_acfg = true; // used for protobuf encode
					update.packets.data[i].acfg = true;
#ifdef CONFIG_SEND_LOWEST_DATA
					update.packets.data[i].has_ldbp = packet.has_ldbp; // used for protobuf encode
					update.packets.data[i].ldbp = packet.ldbp;

					update.packets.data[i].has_lpr = packet.has_lpr; // used for protobuf encode
					update.packets.data[i].lpr = packet.lpr;
#endif
				}
				packets_sent++;
			}
			else
			{
				LOG_ERR("k_msgq_peek_at() failed, err %d", err);
				break;
			}
		}
	}

	uint16_t alerts_sent = 0;
	alerts_num = k_msgq_num_used_get(&alert_msgq);
	LOG_INF("alerts queue count %d", alerts_num);

	if (alerts_num)
	{
		update.alerts.alerts_count = (alerts_num > CONFIG_MAX_ALERTS_PER_UPLINK_MESSAGE) ? CONFIG_MAX_ALERTS_PER_UPLINK_MESSAGE : alerts_num;
		for (int i = 0; i < update.alerts.alerts_count; i++)
		{
			err = k_msgq_peek_at(&alert_msgq, &alert, i);
			if (err == 0)
			{
				// Calculate the correct alert timestamp
				int64_t alert_hold_time = k_uptime_get() - alert.uptime_ts;
				int64_t alert_timestamp = time_ms - alert_hold_time;
				int64_t alert_timestamp_s = alert_timestamp / 1000;

				update.alerts.alert[i].error_code = alert.error_code;
				update.alerts.alert[i].version = alert.version;
				update.alerts.alert[i].uptime_ts = alert_timestamp_s;
				update.alerts.alert[i].size = alert.size;
				update.alerts.alert[i].data = alert.data;

				alerts_sent++;
			}
			else
			{
				LOG_ERR("k_msgq_peek_at() failed, err %d", err);
				break;
			}
		}
	}

	LOG_INF("data about to be encoded");

	/* Encode our message */
	if (!encode_message(update, message, sizeof(message), &message_length))
	{
		LOG_ERR("encode_message() failed, err %d", err);
		return err;
	}

	struct aws_iot_data tx_data = {
			.qos = MQTT_QOS_1_AT_LEAST_ONCE, // Need ACK to confirm data sent to reset timers and data
			.topic.str = uplink_topic,
			.topic.len = strlen(uplink_topic),
			.ptr = message,
			.len = message_length,
			// From logs this message_id gets overriden by AWS IoT library
			.message_id = update.app_packet_count};

#if CONFIG_THREAD_ANALYZER
	thread_analyzer_print();
#endif

	LOG_HEXDUMP_INF(message, message_length, "Publishing message");

	LOG_INF("Message size %d, Packets sent %d, alerts sent %d, message id %d", message_length, packets_sent, alerts_sent, tx_data.message_id);

	/* Save counts of sent data for potential retry */
	sent_packets_count = packets_sent;
	sent_alerts_count = alerts_sent;

	/* Mark that we're waiting for a PUBACK */
	waiting_for_puback = true;
	waiting_for_rx = true;

	err = aws_iot_send(&tx_data);
	update_shadow_flag = false;
	if (err)
	{
		LOG_ERR("aws_iot_send() failed, err: %d", err);
		led_manager_led_on_for(LED_ERROR_SEND_DURATION_MS, LED_ERROR);
		waiting_for_puback = false;
		waiting_for_rx = false;

		/* Reset tracking variables since changes weren't sent */
		cfg_changes_sent = false;
		psm_changes_sent = false;
	}
	else
	{
		/* Store message ID for PUBACK tracking */
		pending_message_id = tx_data.message_id;
		LOG_INF("Sent message with ID: %d, waiting for PUBACK", pending_message_id);

		/* Start PUBACK timeout */
		k_work_schedule(&puback_timeout_work, K_MSEC(MQTT_ACK_TIMEOUT_MS));

		/* Start RX timeout immediately, don't wait for PUBACK */
		k_work_schedule(&rx_timeout_work, K_MSEC(MQTT_RX_TIMEOUT_MS));

		// Don't update any state until we get the PUBACK
	}

	return err;
}

static int provision_request_send(void)
{
	int err = 0;

	char *provis_request_message = NULL;
	cJSON *provis_request_obj = cJSON_CreateObject();

	if (provis_request_obj == NULL)
	{
		LOG_ERR("object returned NULL");
		cJSON_Delete(provis_request_obj);
		err = -ENOMEM;
		return err;
	}

	err += json_add_str(provis_request_obj, "ThingName", client_id_buf);
	err += json_add_str(provis_request_obj, "SerialNumber", client_id_buf);
	if (err)
	{
		LOG_ERR("json_add_str() failed, err= %d", err);
		return err;
	}

	provis_request_message = cJSON_Print(provis_request_obj);
	if (provis_request_message == NULL)
	{
		LOG_ERR("cJSON_Print, err: returned NULL");
		return err;
	}

	LOG_INF("Sending provisioning request JSON..., length %d, message:", strlen(provis_request_message));

	printk("\n%s\n", provis_request_message);

	struct aws_iot_data provis_request_aws_iot_tx = {
			.qos = MQTT_QOS_1_AT_LEAST_ONCE,
			.topic.str = provis_request_topic,
			.topic.len = strlen(provis_request_topic),
			.ptr = provis_request_message,
			.len = strlen(provis_request_message)};

	err = aws_iot_send(&provis_request_aws_iot_tx);
	if (err)
	{
		LOG_ERR("provisioning request aws_iot_send() failed, err: %d", err);
	}
	else
	{
		LOG_INF("provisioning request aws_iot_send() success");
	}

	cJSON_Delete(provis_request_obj);
	cJSON_FreeString(provis_request_message);

	return err;
}

static void shadow_update_work_fn(struct k_work *work)
{
	int err = 0;

	if (cloud_started == false)
	{
		LOG_ERR("Cloud not started, not attempting to send shadow update");
		return;
	}

	if (device_provisioned &&
			!update_shadow_flag)
	{
		LOG_WRN("shadow_update_work_fn() update_shadow_flag is false");
		return;
	}

	// Need to check cellular connected and registered before trying to connect to AWS IoT
	// Note: AWS Cloud could be "connected" but cellular not connected and registered
	if (!cellular_connected || !cellular_registered)
	{
		LOG_WRN("shadow_update_work_fn() cellular not connected %d, or registered %d", cellular_connected, cellular_registered);
		k_work_schedule(&lte_connect_work, K_NO_WAIT);
		return;
	}

	// This is the only place where we connect to AWS Iot
	// We only want to try to connect if we have a reason to, which is mainly if we need to send data
	if (!aws_cloud_connected)
	{
		LOG_WRN("shadow_update_work_fn() AWS IoT not connected");
		k_work_schedule(&aws_connect_work, K_NO_WAIT);
		return;
	}

	if (!device_provisioned)
	{
		err = provision_request_send();
		if (err)
		{
			LOG_ERR("provision_request_send() failed, err: %d", err);
			// just in case this failed, try rescheduling the shadow update again, so we can try again.
			// trying to use a different retry timeout for that
			err = k_work_reschedule(&shadow_update_work, K_SECONDS(CONFIG_PROVIS_REQUEST_RETRY_TIMEOUT_SECONDS));
			if (err < 0)
			{
				LOG_ERR("k_work_reschedule() &shadow_update_work failed, err %d", err);
			}
		}
		return; // no need to continue if not provisioned
	}

	// Only send one shadow update and wait for PUBACK
	// If there are more messages in the queue, received_ack will trigger another update
	err = shadow_update();
	if (err)
	{
		LOG_ERR("shadow_update() failed, err: %d", err);
	}

	LOG_INF("Shadow update finished, waiting for PUBACK %d", pending_message_id);
}

static void work_init(void)
{
	k_work_init_delayable(&lte_connect_work, lte_connect_work_fn);
	k_work_init_delayable(&aws_connect_work, aws_connect_work_fn);
	k_work_init_delayable(&provisioning_work, provisioning_work_fn);
	k_work_init_delayable(&shadow_update_work, shadow_update_work_fn);

	/* Initialize new work items */
	k_work_init_delayable(&puback_timeout_work, puback_timeout_handler);
	k_work_init_delayable(&rx_timeout_work, rx_timeout_handler);
	k_work_init_delayable(&finish_transmission_work, finish_transmission_handler);
	k_work_init_delayable(&reconnect_work, reconnect_work_fn);
}

static void lte_handler(const struct lte_lc_evt *const evt)
{
	bool cellular_change = false;
	switch (evt->type)
	{
	case LTE_LC_EVT_NW_REG_STATUS:
		// Sometimes this comes before and sometimes after LTE mode and cell ID
		// Network registration is separate from cellular connectivity
		switch (evt->nw_reg_status)
		{
		case LTE_LC_NW_REG_NOT_REGISTERED:
			LOG_INF("Network registration status: Not registered");
			cellular_registered = false;
			break;
		case LTE_LC_NW_REG_REGISTERED_HOME:
			LOG_INF("Network registration status: Connected - home network");
			cellular_registered = true;
			break;
		case LTE_LC_NW_REG_SEARCHING:
			LOG_INF("Network registration status: Searching");
			cellular_registered = false;
			break;
		case LTE_LC_NW_REG_REGISTRATION_DENIED:
			LOG_INF("Network registration status: Registration denied");
			cellular_registered = false;
			break;
		case LTE_LC_NW_REG_UNKNOWN:
			LOG_INF("Network registration status: Unknown");
			cellular_registered = false;
			break;
		case LTE_LC_NW_REG_REGISTERED_ROAMING:
			LOG_INF("Network registration status: Connected - roaming");
			cellular_registered = true;
			break;
		case LTE_LC_NW_REG_UICC_FAIL:
			LOG_ERR("Network registration status: UICC fail");
			led_manager_led_on_for(LED_ERROR_ICCID_DURATION_MS, LED_ERROR);
			cellular_registered = false;
			break;
		default:
			LOG_INF("Network registration status: Unknown %d", evt->nw_reg_status);
			break;
		}
		cellular_change = true;

		break;
	case LTE_LC_EVT_PSM_UPDATE:
		LOG_INF("PSM parameter update: TAU: %d, Active time: %d",
						evt->psm_cfg.tau, evt->psm_cfg.active_time);
		psm_values_changed = true;
		/* Store the active time for power management decisions */
		psm_active = evt->psm_cfg.active_time > 0;
		break;

	case LTE_LC_EVT_EDRX_UPDATE:
	{
		char log_buf[60];
		ssize_t len;

		len = snprintf(log_buf, sizeof(log_buf),
									 "eDRX parameter update: eDRX: %f, PTW: %f",
									 evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
		if (len > 0)
		{
			LOG_INF("%s", log_buf);
		}
		edrx_values_changed = true;
		break;
	}

	case LTE_LC_EVT_RRC_UPDATE:
		LOG_INF("RRC mode: %s",
						evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
		break;

	case LTE_LC_EVT_CELL_UPDATE:
		LOG_INF("LTE cell changed: Cell ID: %d, Tracking area: %d",
						evt->cell.id, evt->cell.tac);
		break;

	case LTE_LC_EVT_LTE_MODE_UPDATE:
		switch (evt->lte_mode)
		{
		case LTE_LC_LTE_MODE_NONE:
			// Disconnected
			LOG_INF("LTE mode: NONE");
			lte_connect_attempted = false; // Reset the connection attempt flag when disconnected
			break;
		case LTE_LC_LTE_MODE_LTEM:
			LOG_INF("LTE mode: LTE-M");
			break;
		case LTE_LC_LTE_MODE_NBIOT:
			LOG_INF("LTE mode: NB-IoT");
			break;
		default:
			LOG_ERR("LTE mode: Unknown");
			break;
		}

		cellular_connected = evt->lte_mode != LTE_LC_LTE_MODE_NONE;
		cellular_change = true;
		break;
	case LTE_LC_EVT_MODEM_EVENT:
		LOG_INF("Modem generic event: %d", evt->modem_evt);
		break;

	default:
		LOG_INF("lte_handler unknown event type: %d", evt->type);
		break;
	}

	if (cellular_change)
	{
		if (cellular_connected && cellular_registered)
		{
			LOG_INF("cellular connected and registered");
			date_time_update_async(date_time_event_handler);
			// We don't want to set shadow_update_flag because it would cause the device to try to send data every time it reconnects
			// In low connectivity scenarios the device can connect and disconnect a lot.
			// trigger_shadow_update();
			int err = k_work_schedule(&shadow_update_work, K_NO_WAIT);
			if (err < 0)
			{
				LOG_ERR("k_work_schedule() failed, err %d", err);
			}
		}
		else
		{
			LOG_INF("cellular disconnected %d, registered %d", cellular_connected, cellular_registered);
			// cloud disconnected, no point in trying either.
			k_work_cancel_delayable(&aws_connect_work);
			k_work_cancel_delayable(&shadow_update_work); // no point in trying to uplink if not connected
																										// could try to connect here, but it'd happen every time there's a change and we just want to do it when we need to
		}
	}
}

static int app_topics_subscribe(void)
{
	int err = 0;

	// uplink topic
	err = snprintf(uplink_topic, sizeof(uplink_topic), UPLINK_TOPIC,
								 client_id_buf);
	if (err != UPLINK_TOPIC_LEN)
	{
		LOG_ERR("UPLINK_TOPIC_LEN");
		return -ENOMEM;
	}

	// delta topic
	err = snprintf(delta_topic, sizeof(delta_topic), SHADOW_DELTA_TOPIC,
								 client_id_buf);
	if (err != SHADOW_DELTA_TOPIC_LEN)
	{
		LOG_ERR("SHADOW_DELTA_TOPIC_LEN");
		return -ENOMEM;
	}

	// downlink topic
	err = snprintf(downlink_topic, sizeof(downlink_topic), DOWNLINK_TOPIC,
								 client_id_buf);
	if (err != DOWNLINK_TOPIC_LEN)
	{
		LOG_ERR("DOWNLINK_TOPIC_LEN");
		return -ENOMEM;
	}

	sub_topics[APP_SUB_TOPIC_IDX_DOWNLINK].topic.utf8 = downlink_topic;
	sub_topics[APP_SUB_TOPIC_IDX_DOWNLINK].topic.size = DOWNLINK_TOPIC_LEN;

	// provisioning request
	err = snprintf(provis_request_topic, sizeof(provis_request_topic),
								 PROVIS_REQ_TOPIC, client_id_buf);
	if (err != PROVIS_REQ_TOPIC_LEN)
	{
		LOG_ERR("PROVIS_REQ_TOPIC_LEN");
		return -ENOMEM;
	}

	pub_topics[APP_PUB_TOPIC_IDX_PROVIS_REQ].topic.utf8 = provis_request_topic;
	pub_topics[APP_PUB_TOPIC_IDX_PROVIS_REQ].topic.size = PROVIS_REQ_TOPIC_LEN;

	// provisioning accepted
	err = snprintf(provis_accepted_topic, sizeof(provis_accepted_topic),
								 PROVIS_ACP_TOPIC, client_id_buf);
	if (err != PROVIS_ACP_TOPIC_LEN)
	{
		LOG_ERR("PROVIS_ACP_TOPIC_LEN");
		return -ENOMEM;
	}

	sub_topics[APP_SUB_TOPIC_IDX_PROVIS_ACP].topic.utf8 = provis_accepted_topic;
	sub_topics[APP_SUB_TOPIC_IDX_PROVIS_ACP].topic.size = PROVIS_ACP_TOPIC_LEN;

	// provisioning rejected
	err = snprintf(provis_rejected_topic, sizeof(provis_rejected_topic),
								 PROVIS_REJ_TOPIC, client_id_buf);
	if (err != PROVIS_REJ_TOPIC_LEN)
	{
		LOG_ERR("PROVIS_REJ_TOPIC_LEN");
		return -ENOMEM;
	}

	sub_topics[APP_SUB_TOPIC_IDX_PROVIS_REJ].topic.utf8 = provis_rejected_topic;
	sub_topics[APP_SUB_TOPIC_IDX_PROVIS_REJ].topic.size = PROVIS_REJ_TOPIC_LEN;

	err = aws_iot_application_topics_set(sub_topics, ARRAY_SIZE(sub_topics));
	if (err)
	{
		LOG_ERR("cloud_ep_subscriptions_add, error: %d", err);
		return err;
	}

	return err;
}

int cloud_init(struct app_info_t app_info)
{
	int err = 0;

	if (cloud_initialized == false)
	{

		device_provisioned = app_info.is_provisioned;
		memcpy(client_id_buf, app_info.device_id_buf, AWS_CLOUD_CLIENT_ID_LEN);

		LOG_INF("********************************************");
		LOG_INF(" Cellular building app started");
		LOG_INF(" Version:     %s", CONFIG_APP_VERSION);
		LOG_INF(" Client ID:   %s", app_info.device_id_buf);
		LOG_INF(" Endpoint:    %s", CONFIG_AWS_IOT_BROKER_HOST_NAME);
		LOG_INF(" Provisioned: %s", app_info.is_provisioned ? "true" : "false");
		LOG_INF("********************************************");

		cJSON_Init();

		err = aws_iot_init(aws_iot_event_handler);
		if (err)
		{
			LOG_ERR("AWS IoT library could not be initialized, err %d", err);
			return err;
		}

		err = app_topics_subscribe();
		if (err)
		{
			LOG_ERR("Adding application specific topics failed, err %d", err);
		}

		work_init();

		psm_enabled = true;
		psm_changed = true;
		psm_changes_sent = false;
		cfg_changed = true;
		cfg_changes_sent = false;

		err = modem_info_init();
		if (err)
		{
			LOG_ERR("Failed initializing modem info module, err %d", err);
			return err;
		}

		modem_info_rsrp_register(rsrp_handler);

		err = nrf_modem_lib_init();
		if (err)
		{
			LOG_ERR("nrf_modem_lib_init failed, err %d", err);
			return err;
		}
		LOG_INF("nrf_modem_lib_init done");

		cloud_initialized = true;
	}

	return err;
}

int cloud_start(void)
{
	if (cloud_initialized == false)
	{
		LOG_ERR("Cloud not initialized");
		return -EINVAL;
	}

	if (cloud_started == true)
	{
		LOG_ERR("Cloud already started");
		return -EINVAL;
	}

	int err = 0;

#ifdef CONFIG_ADD_CLAIM_CERTIFICATES
	bool ca_chain_exists, private_key_exists, public_key_exists = false;

	// NOTE: Could use this to check if any certificates are there (claim or new)
	err += modem_key_mgmt_exists(CONFIG_MQTT_HELPER_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, &ca_chain_exists);
	err += modem_key_mgmt_exists(CONFIG_MQTT_HELPER_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_PRIVATE_CERT, &private_key_exists);
	err += modem_key_mgmt_exists(CONFIG_MQTT_HELPER_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_PUBLIC_CERT, &public_key_exists);

	if (err || !ca_chain_exists || !private_key_exists || !public_key_exists)
	{
		printk("Missing certificates on sec_tag %d, writing claims ... \n", CONFIG_MQTT_HELPER_SEC_TAG);

		lte_lc_offline();

		err += modem_key_mgmt_write(CONFIG_MQTT_HELPER_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, CLAIM_CA_CERT, strlen(CLAIM_CA_CERT));
		if (err)
		{
			printk("CLAIM_CA_CERT write failed, err %d", err);
		}
		else
		{
			printk("CLAIM_CA_CERT write, success\n");
		}

		err += modem_key_mgmt_write(CONFIG_MQTT_HELPER_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_PUBLIC_CERT, CLAIM_CLIENT_CERT, strlen(CLAIM_CLIENT_CERT));
		if (err)
		{
			printk("CLAIM_CLIENT_CERT write failed, err %d", err);
		}
		else
		{
			printk("CLAIM_CLIENT_CERT write, success\n");
		}

		err += modem_key_mgmt_write(CONFIG_MQTT_HELPER_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_PRIVATE_CERT, CLAIM_CLIENT_KEY, strlen(CLAIM_CLIENT_KEY));
		if (err)
		{
			printk("CLAIM_CLIENT_KEY write failed, err %d", err);
		}
		else
		{
			printk("CLAIM_CLIENT_KEY write, success\n");
		}
	}
	else
	{
		printk("Certificates already exist on sec_tag %d\n", CONFIG_MQTT_HELPER_SEC_TAG);
	}

	if (err)
	{
		printk("Failed to write certificates, rebooting ...");
		k_msleep(3000);
		sys_reboot(SYS_REBOOT_COLD);
	}
#endif

	k_timer_start(&reconnect_timer, K_MINUTES(CONFIG_RECONNECTION_TIMER_TIMEOUT_MINUTES), K_NO_WAIT);
	k_timer_start(&connection_timer, K_HOURS(CONFIG_CONNECTION_TIMER_TIMEOUT_HOURS), K_NO_WAIT);

	// The proprietary %XDATAPRFL command can be used to provide information on the application use case to modem so that it can optimize power consumption
	// https://devzone.nordicsemi.com/f/nordic-q-a/95169/lte-connection-timeout-retry-policy
	// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fref_at_commands%2FREF%2Fat_commands%2Fnw_service%2Fperiodicsearchconf_set.html
	// mode 0 = AT%PERIODICSEARCHCONF=0,0,0,1,"0,10,40,,5","1,300,600,1800,1800,3600"
	// Note: Consider UICC NVM wear when setting power saving levels.
	err = nrf_modem_at_printf("AT%%XDATAPRFL=0");
	if (err)
	{
		LOG_ERR("nrf_modem_at_printf(AT_XDATAPRFL=0) failed, err %d", err);
	}
	else
	{
		LOG_DBG("AT%%XDATAPRFL=0");
	}

	err = lte_lc_normal();
	if (err)
	{
		LOG_ERR("lte_lc_normal() failed, err %d", err);
	}

	uint8_t iccid_buf_s[MAX_ICCID_LEN] = {0};

	int len = modem_info_string_get(MODEM_INFO_ICCID, iccid_buf_s, sizeof(iccid_buf_s));
	if (len > 0)
	{
		LOG_INF("MODEM_INFO_ICCID: %s", iccid_buf_s);
	}
	else
	{
		LOG_ERR("MODEM_INFO_ICCID: failed, err %d", err);
	}

	cfg_changed = true; // Send initial state to cloud
	cloud_started = true;

	// Handles dependent connetions needed and triggering connections
	trigger_shadow_update();
	uplink_queue_schedule(upload_duty_cycle);

	return err;
}

/**
 * @brief Turns off the modem to save power
 *
 * @note Have not tested how this function operates if the modem was turned on. Only tested from get go off
 *
 * @return int error code
 */
int cloud_stop(void)
{

	int err = 0;

	LOG_INF("cloud_stop");

	if (!cloud_initialized)
	{
		LOG_ERR("cloud_stop but cloud was not initialized");
		return -EINVAL;
	}

	// Cancel the connect work
	(void)k_work_cancel_delayable(&aws_connect_work);

	// Stop the connection timer
	k_timer_stop(&connection_timer);
	k_timer_stop(&reconnect_timer);
	k_timer_stop(&shadow_update_timer);

	k_work_cancel_delayable(&lte_connect_work);
	k_work_cancel_delayable(&aws_connect_work);
	k_work_cancel_delayable(&shadow_update_work);
	k_work_cancel_delayable(&puback_timeout_work);
	k_work_cancel_delayable(&rx_timeout_work);
	k_work_cancel_delayable(&finish_transmission_work);
	k_work_cancel_delayable(&reconnect_work);

	if (cloud_started)
	{
		// Need to disconnect to make reconnecting quickly work. Othwerise it gets stuck in a weird state
		LOG_INF("disconnecting from aws");
		aws_iot_disconnect();
	}

	lte_lc_offline();
	lte_connect_attempted = false; // Reset the connection attempt flag when going offline
	// err = nrf_modem_lib_shutdown();
	// if(err) {
	// 	LOG_ERR("nrf_modem_lib_shutdown, err %d", err);
	// }

	LOG_INF("uninit the cloud done");
	cloud_started = false;
	return err;
}

int cloud_queue_data(struct data_packet_t data_packet)
{
	int err = 0;

	LOG_INF("queueing hc: %d uptime_ts: %llu", data_packet.half_cycles, data_packet.uptime_ts);

	err = k_msgq_put(&uplink_msgq, &data_packet, K_NO_WAIT);
	if (err)
	{
		LOG_ERR("k_msgq_put() failed, err %d", err);
		LOG_ERR("uplink queue count %d", k_msgq_num_used_get(&uplink_msgq));
		return err;
	}

	LOG_INF("uplink queue count %d", k_msgq_num_used_get(&uplink_msgq));

	return err;
}

int cloud_queue_alert(struct alert_packet_t alert_packet)
{
	int err = 0;
	LOG_WRN("queueing alert data: %lli, size: %d, err_code: %d, version: %d", alert_packet.data, alert_packet.size, alert_packet.error_code, alert_packet.version);

	if (alert_packet.size > CONFIG_ALERT_MSG_MAX_SIZE)
	{
		LOG_ERR("alert message too long");
		return -1;
	}

	err = k_msgq_put(&alert_msgq, &alert_packet, K_NO_WAIT);
	if (err)
	{
		LOG_ERR("alert k_msgq_put() failed, err %d", err);
		LOG_ERR("alert queue count %d", k_msgq_num_used_get(&alert_msgq));
		return err;
	}

	LOG_INF("alert queue count %d, data size %d", k_msgq_num_used_get(&alert_msgq), alert_packet.size);

	return err;
}

static void shadow_update_timer_event_handler(struct k_timer *timer)
{
	LOG_INF("shadow_update_timer_event_handler");
	trigger_shadow_update();
}

int send_device_state_to_cloud(bool is_enabled)
{
	// Check if the device is connected to the cloud
	if (!aws_cloud_connected)
	{
		LOG_WRN("Device not connected to cloud. Skipping state update.");
		return -ENOTCONN;
	}

	int err = 0;
	char *state_message = NULL;

	cJSON *root_obj = cJSON_CreateObject();
	cJSON *state_obj = cJSON_CreateObject();
	cJSON *reported_obj = cJSON_CreateObject();
	cJSON *cfg_obj = cJSON_CreateObject();

	if (root_obj == NULL || state_obj == NULL || reported_obj == NULL || cfg_obj == NULL)
	{
		LOG_ERR("Failed to create JSON objects");
		err = -ENOMEM;
		goto cleanup;
	}

	const char *state_str = is_enabled ? "enabled" : "disabled";
	err = json_add_str(cfg_obj, "devstate", state_str);
	if (err)
	{
		LOG_ERR("Failed to add devstate to JSON, err %d", err);
		goto cleanup;
	}
	cJSON_AddItemToObject(reported_obj, "cfg", cfg_obj);
	cJSON_AddItemToObject(state_obj, "reported", reported_obj);
	cJSON_AddItemToObject(root_obj, "state", state_obj);

	state_message = cJSON_Print(root_obj);
	if (state_message == NULL)
	{
		LOG_ERR("Failed to print JSON object");
		err = -ENOMEM;
		goto cleanup;
	}

	struct aws_iot_data state_message_data = {
			.qos = MQTT_QOS_1_AT_LEAST_ONCE,
			.topic.type = AWS_IOT_SHADOW_TOPIC_UPDATE,
			.ptr = state_message,
			.len = strlen(state_message)};

	LOG_INF("Sending device state to cloud: %s", state_message);

	err = aws_iot_send(&state_message_data);
	if (err)
	{
		LOG_ERR("Failed to send device state to cloud, err %d", err);
	}
	else
	{
		LOG_INF("Device state sent to cloud successfully");
	}

cleanup:
	cJSON_Delete(root_obj);
	if (state_message != NULL)
	{
		cJSON_free(state_message);
	}

	return err;
}

static void reconnect_work_fn(struct k_work *work)
{
	if (cloud_started == false)
	{
		LOG_ERR("Cloud not started, not attempting to reconnect");
		return;
	}

	LOG_INF("reconnect_work_fn: reconnecting...");
	// Can't use cloud_stop() and cloud_start() because it resets the connection timer
	// Add alert to cloud
	Alert_Data(ERROR_CODE_RECONNECTING, 0, NULL, 0);
	aws_iot_disconnect();
	lte_lc_offline();
	if (lte_lc_normal() != 0)
	{
		LOG_ERR("Failed to set modem to normal mode");
	}
	k_work_schedule(&lte_connect_work, K_NO_WAIT);
}

static void connection_reconnect_timer_event_handler(struct k_timer *timer)
{
	LOG_INF("connection_reconnect_timer_event_handler");
	k_work_schedule(&reconnect_work, K_NO_WAIT);
	k_timer_start(&reconnect_timer, K_MINUTES(CONFIG_RECONNECTION_TIMER_TIMEOUT_MINUTES), K_NO_WAIT);
}

static void connection_timer_event_handler(struct k_timer *timer)
{
	LOG_INF("connection_timer_event_handler");

	cycle_counter_save_half_cycles();
	sys_reboot(SYS_REBOOT_COLD);
}

static int json_add_obj(cJSON *parent, const char *str, cJSON *item)
{
	cJSON_AddItemToObject(parent, str, item);

	return 0;
}

static int json_add_str(cJSON *parent, const char *str, const char *item)
{
	cJSON *json_str;

	json_str = cJSON_CreateString(item);
	if (json_str == NULL)
	{
		LOG_ERR("cJSON_CreateString, err: returned NULL");
		return -ENOMEM;
	}

	return json_add_obj(parent, str, json_str);
}

static bool string_compare(const char *str1, const char *str2, const uint16_t len)
{
	for (uint16_t i = 0; i < (len - 1); i++) // (len-1) to exclude the null terminator
	{
		if (str1[i] != str2[i])
		{
			return false;
		}
	}

	return true;
}

static void rsrp_handler(char rsrp_value)
{
	uint16_t rsrp_value_uint = (uint16_t)rsrp_value;
	if (rsrp_value_uint == 255) // CELL_RSRP_INVALID
	{
		LOG_ERR("rsrp value invalid");
		// occurs occasioally, for some reaosn right before noramlly sending packets
		return;
	}

	int rsrp = (int)(rsrp_value_uint - RSRP_OFFSET_VAL); // Convert from char to dBm value
	LOG_INF("rsrp value: %d, dBm: %d", rsrp_value_uint, rsrp);
	// need to convert to dBm
	last_rsrp = rsrp;
}

int cloud_send_data_now(void)
{
	if (cloud_started == false)
	{
		LOG_ERR("Cloud not started, not attempting to send data now");
		return -ESHUTDOWN;
	}

	// Trigger immediate shadow update
	if (!cellular_connected || !cellular_registered)
	{
		LOG_WRN("cloud_send_data_now cellular not connected %d, or registered %d", cellular_connected, cellular_registered);

		// Check if we can perform a periodic search request (1 minute cooldown)
		int64_t current_uptime = k_uptime_get();
		int64_t time_since_last_search = current_uptime - last_periodic_search_uptime;
		int64_t one_minute_ms = 5 * 60 * 1000; // 5 minutes in milliseconds

		if (time_since_last_search >= one_minute_ms)
		{
			LOG_INF("Trying to reconnect to network");
			// lte_lc_periodic_search_request() was removed in NCS 3.0+
			// Use lte_lc_connect() instead
			int err = lte_lc_connect();
			if (err)
			{
				// Can fail if connection attempt already in progress
				LOG_ERR("lte_lc_connect() failed, err %d", err);
			}
			else
			{
				lte_connect_attempted = true;
				last_periodic_search_uptime = current_uptime;
				LOG_INF("LTE connect initiated");
			}
		}
		else
		{
			LOG_INF("Reconnect throttled, last attempt was %lld ms ago (cooldown: %lld ms)",
							time_since_last_search, one_minute_ms);
		}
		return -ENOTCONN;
	}

	trigger_shadow_update();
	return 0;
}

static void trigger_shadow_update(void)
{
	update_shadow_flag = true;
	int err = k_work_schedule(&shadow_update_work, K_NO_WAIT);
	if (err < 0)
	{
		LOG_ERR("k_work_schedule() failed, err %d", err);
	}
}

/**
 * @brief Handler for PUBACK timeout
 *
 * Called when we don't receive a PUBACK within the timeout period
 */
static void puback_timeout_handler(struct k_work *work)
{
	LOG_WRN("PUBACK timeout occurred for message ID: %d", pending_message_id);
	waiting_for_puback = false;
	pending_message_id = -1; // Reset pending message ID
	led_manager_led_on_for(LED_ERROR_SEND_DURATION_MS, LED_ERROR);

	if (!waiting_for_rx)
	{
		/* Always finish the transmission process after PUBACK timeout */
		k_work_schedule(&finish_transmission_work, K_NO_WAIT);
	}
}

/**
 * @brief Handler for RX timeout
 *
 * Called when we don't receive a response within the RX timeout period
 */
static void rx_timeout_handler(struct k_work *work)
{
	LOG_INF("RX timeout occurred, no response data received");
	waiting_for_rx = false;

	/* Check if we've already received PUBACK */
	if (!waiting_for_puback)
	{
		/* If PUBACK already received, finish the transmission */
		k_work_schedule(&finish_transmission_work, K_NO_WAIT);
	}
	/* Otherwise keep waiting for PUBACK */
}

/**
 * @brief Handle finishing of transmission process
 *
 * Manages power state based on upload duty cycle and PSM status
 */
static void finish_transmission_handler(struct k_work *work)
{
	int err = 0;
	LOG_INF("finish_transmission_handler");

	if (cloud_started == false)
	{
		LOG_ERR("Cloud not started, not attempting to finish transmission");
		return;
	}

	// Putting this logic here because we get SNR and RSRP after a transmission.
	int snr = 0;
	int snr_err = modem_info_get_snr(&snr);
	if (snr_err)
	{
		LOG_WRN("modem_info_get_snr() failed, err: %d", snr_err);
	}
	else
	{
		LOG_INF("modem_info_get_snr() success, snr: %d", snr);
		last_snr = snr;
	}

	int rsrp = 0;
	int rsrp_err = modem_info_get_rsrp(&rsrp);
	if (rsrp_err)
	{
		LOG_WRN("modem_info_get_rsrp() failed, err: %d", rsrp_err);
	}
	else
	{
		LOG_INF("modem_info_get_rsrp() success, rsrp: %d", rsrp);
		last_rsrp = rsrp;
	}

	// Only disconnect if provisioned, otherwise we need to wait for more RXs
	if (!device_provisioned)
	{
		LOG_INF("Not provisioned, waiting for more RXs");
		return;
	}

	/* Check if we should disconnect from MQTT based on upload duty cycle */
	if (upload_duty_cycle > MAX_MQTT_KEEPALIVE_SECONDS)
	{
		LOG_INF("Upload duty cycle (%d) > MAX_MQTT_KEEPALIVE_SECONDS (%d), disconnecting from MQTT",
						upload_duty_cycle, MAX_MQTT_KEEPALIVE_SECONDS);

		err = aws_iot_disconnect();
		if (err)
		{
			LOG_ERR("aws_iot_disconnect failed, err: %d", err);
		}
	}

	if (upload_duty_cycle > MAX_UDC_FOR_INACTIVE_PSM_TO_STAY_ONLINE)
	{
		/* Check if PSM mode is enabled and active time is sufficient */
		if (!psm_active)
		{
			LOG_WRN("PSM inactive, going to LTE offline mode");

			err = lte_lc_offline();
			if (err)
			{
				LOG_ERR("lte_lc_offline failed, err: %d", err);
			}
			else
			{
				lte_connect_attempted = false;
				cellular_connected = false;
				cellular_registered = false;
			}
		}
	}
}

/**
 * @brief Process data after receiving an ACK
 *
 * Clears queues of transmitted data and updates changed flags
 */
static void received_ack(void)
{
	int err = 0;
	struct data_packet_t packet = {0};
	struct alert_packet_t alert = {0};

	LOG_INF("Transmission was successful, clearing sent data");
	led_manager_led_on_for(LED_SUCCESS_DURATION_MS, LED_SUCCESS);

	/* Restart connection times because we have succesffully sent the data */
	k_timer_start(&reconnect_timer, K_MINUTES(CONFIG_RECONNECTION_TIMER_TIMEOUT_MINUTES), K_NO_WAIT);
	k_timer_start(&connection_timer, K_HOURS(CONFIG_CONNECTION_TIMER_TIMEOUT_HOURS), K_NO_WAIT);

	/* Only reset flags if we've successfully sent the changes */
	if (cfg_changes_sent)
	{
		LOG_INF("Config changes were sent successfully, resetting cfg_changed flag");
		cfg_changed = false;
		cfg_changes_sent = false;
	}

	if (psm_changes_sent)
	{
		LOG_INF("PSM changes were sent successfully, resetting psm_changed flag");
		psm_changed = false;
		psm_changes_sent = false;
	}

	/* Remove sent packets from queue */
	if (sent_packets_count)
	{
		for (uint16_t i = 0; i < sent_packets_count; i++)
		{
			err = k_msgq_get(&uplink_msgq, &packet, K_NO_WAIT);
			if (err)
			{
				LOG_ERR("k_msgq_get() failed for packet %d, err %d", i, err);
			}
		}
		LOG_INF("Uplink queue still has %d packets", k_msgq_num_used_get(&uplink_msgq));
	}

	/* Remove sent alerts from queue */
	if (sent_alerts_count)
	{
		for (uint16_t i = 0; i < sent_alerts_count; i++)
		{
			err = k_msgq_get(&alert_msgq, &alert, K_NO_WAIT);
			if (err)
			{
				LOG_ERR("k_msgq_get() failed for alert %d, err %d", i, err);
			}
		}
		LOG_INF("Alert queue still has %d alerts", k_msgq_num_used_get(&alert_msgq));
	}

	/* Check if there are more packets to send */
	uint16_t uplink_count = k_msgq_num_used_get(&uplink_msgq);
	uint16_t alert_count = k_msgq_num_used_get(&alert_msgq);

	// Set save_data flag in app after successful send
	app_set_save_data_flag();

	if (uplink_count > 0 || alert_count > 0)
	{
		LOG_INF("More data to send (uplinks: %d, alerts: %d), triggering another update",
						uplink_count, alert_count);
		update_shadow_flag = true;
		k_work_schedule(&shadow_update_work, K_NO_WAIT);
	}
	else
	{
		LOG_INF("No more data to send, waiting for RX data or timeout");
		/* Continue waiting for RX data or timeout - timeout was already scheduled when packet was sent */
	}
}