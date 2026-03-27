#include "algorithm.h"
#include "app.h"
#include "cloud.h"
#include "error_handler.h"
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include "cycle_counter.h"
#include "flash_utility.h"
#include "wdt.h"
#include "lowest_flow_over_period_tracker.h"
#include "timeServer.h"
#include "magnetometer_controller.h"
#include "led_manager.h"
#include "pulse_tracker.h"
#include "utils.h"
#include <date_time.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

// Global algorithm pointer that will point to either Pulse_Tracker or Magnetometer algorithm
static ALGORITHM_t *Algorithm_Instance = NULL;

// External declarations for the algorithms
extern ALGORITHM_t Pulse_Tracker;
extern ALGORITHM_t Sensus_Protocol;
#ifdef CONFIG_USE_MAGNETOMETER
extern ALGORITHM_t Magnetometer_Algorithm;
#endif

const char *algorithm_variables_s[ALGORITHM_VARIABLES_COUNT] =
		{
				"percentSignalThreshold",									// 0
				"percentNoiseThreshold",									// 1
				"percentNoiseBuffer",											// 2
				"maxNoise_mG",														// 3
				"minNoise_mG",														// 4
				"monitorMissedCycles",										// 5
				"staticNoiseBuffer",											// 6
				"minNoiseChange",													// 7
				"percentOfNoiseDecreaseForReset",					// 8
				"maxNumberOfTimestampsToAverage",					// 9
				"maxTimeSinceLastPulse",									// 10
				"maxSignalFrequency",											// 11
				"debugAlerts",														// 12
				"readToCheck",														// 13
				"gettingData",														// 14
				"minConfigurationDifference",							// 15
				"checkConfig",														// 16
				"percentOfBufferForTarget",								// 17
				"percentOfBufferForUpdate",								// 18
				"maxNumberOfTimestampsToAverageForReset", // 19
				"checkNoNewPulses",												// 20
				"debounceTimeMs",													// 21
				"sensusPowerResetTimeMs",									// 22
				"sensusPowerCycleTimeMs",									// 23
				"sensusDataSettleTimeMs",									// 24
				"sensusReadingIntervalMs"									// 25
																									// If you add a new variable, you need to update the ALGORITHM_VARIABLES_COUNT
};

static struct k_work algorithm_work;
static void algorithm_work_fn(struct k_work *work);

static struct k_work reset_work;
static void reset_work_fn(struct k_work *work);

static struct k_work reboot_work;
static void reboot_work_fn(struct k_work *work);

static struct k_work_delayable queue_data_work;
static void queue_data_work_fn(struct k_work *work);

static uint32_t app_duty_cycle_s = 60; // 1 minute

static uint32_t wdtFeedTimeout = 10000; // 10 seconds
static TimerEvent_t wdtTimer;
static void wdtTimerCallback(void);

// Saved half cycles to flash every hour to wear on the flash
static uint32_t saveHalfCyclesTimeout = 1000 * 60 * 60; // 1 hour
static TimerEvent_t saveHalfCyclesTimer;
static void saveHalfCyclesTimerCallback(void);

static void packet_queue_schedule(uint32_t duty_cycle_s);

// Flags
static volatile bool save_data = false;
static volatile bool reset_wdt = true;

static volatile bool app_running = false;
static bool app_init_done = false;
static device_type_t device_type = DEVICE_TYPE_MAGNETOMETER;

// Track last queued uptime to prevent duplicates
static int64_t last_queue_uptime_ts = 0;

int app_init(void)
{
	int err = 0;

	if (app_init_done == true)
		return 0;

	LOG_INF("entered app_init");

#ifdef CONFIG_PIPE_MONITOR_BOARD
	LOG_INF("Forcing pulse tracker algorithm because pipe monitor board is enabled");
	device_type = DEVICE_TYPE_PULSE_TRACKER;
	FlashUtility_SaveDeviceType(device_type);
#endif

	// Load device type configuration
	if (FlashUtility_LoadDeviceType(&device_type) == FlashStatus_Success)
	{
		LOG_INF("Loaded device type: %d", device_type);
	}
	else
	{
		LOG_WRN("Failed to load device type, using default: Magnetometer");
		device_type = DEVICE_TYPE_MAGNETOMETER;
	}

	// Select the appropriate algorithm
	switch (device_type)
	{
	case DEVICE_TYPE_PULSE_TRACKER:
		Algorithm_Instance = &Pulse_Tracker;
		LOG_INF("Initializing as Pulse Tracker");
		break;
	case DEVICE_TYPE_SENSUS_PROTOCOL:
		Algorithm_Instance = &Sensus_Protocol;
		LOG_INF("Initializing as Sensus Protocol");
		break;
	case DEVICE_TYPE_MAGNETOMETER:
	default:
		Algorithm_Instance = &Algorithm;
		LOG_INF("Initializing as Magnetometer");
		break;
	}

	err = watchdogt_init();
	if (err)
	{
		LOG_ERR("watchdogt_init() failed, err %d", err);
		return 1;
	}

	TimerInit(&wdtTimer, wdtTimerCallback);
	TimerSetValue(&wdtTimer, wdtFeedTimeout);
	TimerStart(&wdtTimer);
	TimerInit(&saveHalfCyclesTimer, saveHalfCyclesTimerCallback);
	TimerSetValue(&saveHalfCyclesTimer, saveHalfCyclesTimeout);

	k_work_init(&algorithm_work, algorithm_work_fn);
	k_work_init(&reset_work, reset_work_fn);
	k_work_init(&reboot_work, reboot_work_fn);
	k_work_init_delayable(&queue_data_work, queue_data_work_fn);

	// Algorithm init after work init to avoid race condition
	Algorithm_Instance->Init();

	app_init_done = true;

	LOG_INF("app_init done");

	return 0;
}

void app_start(void)
{
	LOG_INF("entered app_start");

	struct alert_packet_t alert_packet = {0};

	if (app_running)
	{
		LOG_WRN("app already running");
		return;
	}

#if CONFIG_USE_MAGNETOMETER
	if (device_type == DEVICE_TYPE_MAGNETOMETER)
	{
		// This test checks if it's connected to the magnetometer interrupt
		MAGNETOMETER_CONNECTION_t connection = magnetometer_controller_test_connection();
		if (connection != MAGNETOMETER_CONNECTION_NO_ISSUE)
		{
			int16_t connection_value = (int16_t)connection;
			LOG_ERR("ERROR magnetometer_controller_test_connection failed %d", connection);
			Alert_Data(ERROR_CODE_HW_BSP_INTERRUPT_NOT_CONNECTED, 0, &connection_value, 1);
			led_manager_led_on_for(LED_ERROR_HW_MAG_CONNECTION_FAILED_DURATION_MS, LED_ERROR);
			// could prevent the app from starting.
			// With interrupt not connected it will keep trying noise_monitor and erroring out
			// But if it reconnects somehow then it would continue on normally
		}
	}
#endif

	app_running = true; // has to be before algorithm work submit
	int err = k_work_submit(&algorithm_work);
	if (err < 0)
	{
		LOG_ERR("app_start failed to submit algorithm_work, err %d", err);

		alert_packet.error_code = ERROR_CODE_ALGORITHM_FAILED_TO_START;
		alert_packet.data = 0;
		alert_packet.size = 1;
		alert_packet.uptime_ts = k_uptime_get();
		cloud_queue_alert(alert_packet);
	}

	// Queue data immediately after starting the algorithm so we have data at start up
	app_queue_data();
	packet_queue_schedule(app_duty_cycle_s);

	TimerStart(&saveHalfCyclesTimer);
	Algorithm_Instance->Enable();
}

void app_stop(void)
{
	LOG_INF("entered app_stop");

	app_running = false;
	Algorithm_Instance->Disable();
	k_work_cancel_delayable(&queue_data_work);
	watchdogt_feed();
	TimerReset(&wdtTimer);
	cycle_counter_save_half_cycles();
	TimerStop(&saveHalfCyclesTimer);
	LOG_INF("app_stop done");
}

void app_check_in(void)
{
	// LOG_INF("entered app_check_in");
	k_work_submit(&algorithm_work);
}

// would be good as a starting point, so we don't include the algorithm functions on the cloud
void app_update_variable(const char *variable, uint32_t value)
{
	for (int i = 0; i < ALGORITHM_VARIABLES_COUNT; i++)
	{
		if (strcmp(variable, algorithm_variables_s[i]) == 0)
		{
			// LOG_INF("app new variable update, variable %s, value %d", variable, value);
			Algorithm_Instance->Update_Variable(i, value);
			return;
		}
	}
	LOG_WRN("Could not find variable %s with value %d", variable, value);
}

void app_sync_upload(void)
{
	LOG_INF("app_sync_upload");
	packet_queue_schedule(app_duty_cycle_s);
}

void app_update_set_duty_cycle(uint32_t value)
{
	LOG_INF("app_update_set_duty_cycle %d", value);
	app_duty_cycle_s = value;
	packet_queue_schedule(app_duty_cycle_s);
}

void app_action_take(char *action)
{
	if (strcmp(action, "reset") == 0)
	{
		k_work_submit(&reset_work);
	}
	else if (strcmp(action, "reboot") == 0)
	{
		k_work_submit(&reboot_work);
	}
	else if (strcmp(action, "start") == 0)
	{
		app_start();
	}
	else if (strcmp(action, "stop") == 0)
	{
		app_stop();
	}
	else
	{
		printk("app_action_take unknown action %s\n", action);
	}
}

uint32_t app_update_get_duty_cycle(void)
{
	return app_duty_cycle_s;
}

void app_set_device_type(device_type_t new_device_type)
{
	LOG_INF("app_set_device_type %d", new_device_type);
	device_type = new_device_type;

	// Save using enum function
	FlashUtility_SaveDeviceType(new_device_type);

	LOG_INF("Rebooting to apply device type");
	sys_reboot(0);
}

device_type_t app_get_device_type(void)
{
	return device_type;
}

static void algorithm_work_fn(struct k_work *work)
{
	if (save_data)
	{
		save_data = false;
		cycle_counter_save_half_cycles();
		TimerReset(&saveHalfCyclesTimer);
	}

	if (reset_wdt)
	{
		// printk("reset wdt \n");
		reset_wdt = false;
		watchdogt_feed();
		TimerReset(&wdtTimer);
	}

	if (!app_running)
	{
		return;
	}

	Algorithm_Instance->Check_In();
}

static void reset_work_fn(struct k_work *work)
{
	LOG_INF("entered algorithm reset");
	// maybe need to stop the app first?
	app_stop();
	Algorithm_Instance->Reset();
	app_start();
}

static void reboot_work_fn(struct k_work *work)
{
	LOG_INF("enetered app reboot");
	cycle_counter_save_half_cycles();
	sys_reboot(1); // 0 warm, and 1 cold
}

int64_t calculate_ms_till_next(int64_t unix_ms, int32_t duty_cycle_s)
{
	// Convert duty cycle from seconds to milliseconds
	int64_t duty_cycle_ms = (int64_t)duty_cycle_s * 1000;

	// Calculate milliseconds since last clean boundary
	int64_t ms_since_last = unix_ms % duty_cycle_ms;

	// Calculate milliseconds until next boundary
	int64_t ms_till_next = duty_cycle_ms - ms_since_last;

	return ms_till_next;
}

static void packet_queue_schedule(uint32_t duty_cycle_s)
{
	int err = 0;
	int64_t timestamp = 0;
	struct alert_packet_t alert_packet = {0};

	err = date_time_now(&timestamp);
	if (err)
	{
		// Typically means it's not connected to the cloud
		LOG_DBG("date_time_now() failed, err: %d", err);
		// Just in case getting the time failed, schedule normally
		err = k_work_schedule(&queue_data_work, K_SECONDS(duty_cycle_s));
		if (err < 0)
		{
			LOG_ERR("app_start failed to schedule queue_data_work, err %d", err);
		}
		return;
	}

	// Calculate time until next period
	int64_t time_to_next_collection = calculate_ms_till_next(timestamp, duty_cycle_s);

	LOG_INF("Duty cycle(s): %d, time till next collection(ms): %lld", duty_cycle_s, time_to_next_collection);
	err = k_work_schedule(&queue_data_work, K_MSEC(time_to_next_collection));
	if (err < 0)
	{
		LOG_ERR("app_start failed to schedule queue_data_work, err %d", err);

		alert_packet.error_code = ERROR_CODE_ALGORITHM_FAILED_TO_START;
		alert_packet.data = 1;
		alert_packet.size = 1;
		alert_packet.uptime_ts = k_uptime_get();
		cloud_queue_alert(alert_packet);
	}
}

int app_queue_data(void)
{
	int64_t current_uptime_ts = k_uptime_get();

	// Check if we've already queued data for this uptime_ts
	if (current_uptime_ts == last_queue_uptime_ts)
	{
		LOG_WRN("Skipping duplicate data point for uptime_ts: %lld", current_uptime_ts);
		return 0;
	}

	uint32_t halfCycles = 0;
	TimerTime_t longestDurationBetweenPulsesMS = 0;
	float lowestPulseRate = 0;
	bool add_lowest_pulse_rate = false, add_longest_duration = false;

	// Check if we're in cumulative usage mode (e.g., Sensus protocol)
	bool is_cumulative_mode = (cycle_counter_get_mode() == CYCLE_MODE_CUMULATIVE_USAGE);

	if (!is_cumulative_mode)
	{
		// Only calculate flow metrics for pulse counting mode
		if (cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDurationBetweenPulsesMS))
		{
			LOG_INF("Duration mS %d", longestDurationBetweenPulsesMS);
			add_longest_duration = true;
		}

		if (LowestFlowOverPeriodTracker_Get_LowestPulseRate(&lowestPulseRate))
		{
			LOG_INF("lowestPulseRate: %.2f", lowestPulseRate);
			add_lowest_pulse_rate = true;
		}
	}
	else
	{
		LOG_INF("Cumulative usage mode - skipping flow rate calculations");
	}

	cycle_counter_start_new_period();

	halfCycles = cycle_counter_get_half_cycles();

	struct data_packet_t data_packet = {0};

	data_packet.half_cycles = halfCycles;
#ifdef CONFIG_SEND_LOWEST_DATA
	data_packet.has_ldbp = add_longest_duration;
	data_packet.ldbp = longestDurationBetweenPulsesMS;
	data_packet.has_lpr = add_lowest_pulse_rate;
	data_packet.lpr = lowestPulseRate;
#endif
	data_packet.acfg = Algorithm_Instance->Is_Configured();
	data_packet.uptime_ts = current_uptime_ts;

	int result = cloud_queue_data(data_packet);

	// Update last queue time on success
	if (result == 0)
	{
		last_queue_uptime_ts = current_uptime_ts;
	}

	return result;
}

static void queue_data_work_fn(struct k_work *work)
{
	if (!app_running)
	{
		return;
	}

	app_queue_data();
	packet_queue_schedule(app_duty_cycle_s);
}

static void wdtTimerCallback(void)
{
	reset_wdt = true;
}

static void saveHalfCyclesTimerCallback(void)
{
	save_data = true;
}

void app_set_save_data_flag(void)
{
	save_data = true;
}
