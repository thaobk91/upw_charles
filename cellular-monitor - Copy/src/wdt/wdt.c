#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>

#include "wdt.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wdt, CONFIG_APP_LOG_LEVEL);

#define WDT_OPT 0

int wdt_channel_id;
const struct device *const wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));

static void wdt_callback(const struct device *wdt_dev, int channel_id)
{
	static int wdt_callbacks;
	wdt_callbacks++;

	LOG_INF("wdt callback %d", wdt_callbacks);

#if CONFIG_THREAD_ANALYZER
	thread_analyzer_print();
#endif
}

int watchdogt_init(void)
{
	int err = 0;

	LOG_INF("entered watchdogt_init");

	if (!device_is_ready(wdt))
	{
		LOG_ERR("%s: wdt device not ready", wdt->name);
		return 1;
	}

	struct wdt_timeout_cfg wdt_config = {
			/* Reset SoC when watchdog timer expires. */
			.flags = WDT_FLAG_RESET_SOC,

			/* Expire watchdog after max window */
			.window.min = CONFIG_WDT_MIN_WINDOW_MS,
			.window.max = CONFIG_WDT_MAX_WINDOW_MS,
	};

	/* Set up watchdog callback. */
	wdt_config.callback = wdt_callback;

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	if (wdt_channel_id == -ENOTSUP)
	{
		LOG_WRN("Callback support rejected, continuing anyway");
		wdt_config.callback = NULL;
		wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	}

	if (wdt_channel_id < 0)
	{
		LOG_ERR("Watchdog install error");
		return 1;
	}

#ifdef CONFIG_DEBUG
	err = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
#else
	err = wdt_setup(wdt, WDT_OPT);
#endif
	if (err < 0)
	{
		LOG_ERR("Watchdog setup error");
		return 1;
	}

	wdt_feed(wdt, wdt_channel_id); // wdt feed

	return 0;
}

int watchdogt_feed(void)
{
	return wdt_feed(wdt, wdt_channel_id);
}
