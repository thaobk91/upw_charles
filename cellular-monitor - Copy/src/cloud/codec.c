/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <cJSON_os.h>
#include "utils.h"
#include "codec.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(codec, CONFIG_APP_LOG_LEVEL);

static size_t packet_count = 0;
static size_t alert_count = 0;

// Callback function to encode the char buffers
bool buffer_encode_callback(pb_ostream_t *stream, const pb_field_t *field, void *const *arg)
{
	const uint8_t *buffer = (const uint8_t *)(*arg);
	size_t buffer_size = strlen((const char *)buffer); // Assuming null-terminated string for simplicity

	if (!pb_encode_tag_for_field(stream, field))
	{
		return false;
	}

	return pb_encode_string(stream, buffer, buffer_size);
}

// Callback function to encode the repeated DataPacket field
bool packets_encode_callback(pb_ostream_t *stream, const pb_field_t *field, void *const *arg)
{
	DataPacket *packets = (DataPacket *)(*arg);
	for (size_t i = 0; i < packet_count; i++)
	{

		if (!pb_encode_tag_for_field(stream, field))
		{
			return false;
		}
		if (!pb_encode_submessage(stream, DataPacket_fields, &packets[i]))
		{
			return false;
		}
	}
	return true;
}

// Callback function to encode the repeated AlertPacket field
bool alerts_encode_callback(pb_ostream_t *stream, const pb_field_t *field, void *const *arg)
{
	AlertPacket *alerts = (AlertPacket *)(*arg);

	for (size_t i = 0; i < alert_count; i++)
	{
		if (!pb_encode_tag_for_field(stream, field))
		{
			return false;
		}
		if (!pb_encode_submessage(stream, AlertPacket_fields, &alerts[i]))
		{
			return false;
		}
	}
	return true;
}

bool encode_message(struct shadow_update_t update, uint8_t *buffer, size_t buffer_size, size_t *message_length)
{
	bool status;

	/* Allocate space on the stack to store the message data.
	 *
	 * Nanopb generates msg struct definitions for all the messages.
	 * - check out the contents of msg.pb.h!
	 * It is a good idea to always initialize your structures
	 * so that you do not have garbage data from RAM in there.
	 */
	ShadowUpdate message = ShadowUpdate_init_zero;

	/* Create a stream that will write to our buffer. */
	pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);

	message.fcnt = update.app_packet_count;

	if (update.has_dev == true)
	{
		message.has_dev = true;

		// Fill in the dev fields

		message.dev.devid.funcs.encode = buffer_encode_callback;
		message.dev.devid.arg = (void *)update.dev.device_id;

		message.dev.iccid.funcs.encode = buffer_encode_callback;
		message.dev.iccid.arg = (void *)update.dev.iccid;

		message.dev.imei.funcs.encode = buffer_encode_callback;
		message.dev.imei.arg = (void *)update.dev.imei;

		message.dev.fwv.funcs.encode = buffer_encode_callback;
		message.dev.fwv.arg = (void *)update.dev.fw_version;

		// Deprecated
		message.dev.algo.funcs.encode = buffer_encode_callback;
		message.dev.algo.arg = (void *)update.dev.algo_type;

		message.dev.mfwv.funcs.encode = buffer_encode_callback;
		message.dev.mfwv.arg = (void *)update.dev.mfw_version;

		message.dev.prvs = update.dev.provis_state;

		// this will make sure to only include it when it is enabled (true)
		message.dev.has_log = update.dev.algo_log_enabled;
		message.dev.log = update.dev.algo_log_enabled;

		message.dev.ts = update.dev.ts;
	}

	message.has_roam = true;

	// only include when it has a value
	message.roam.has_rsrp = update.roam.rsrp ? true : false;
	message.roam.rsrp = update.roam.rsrp;

	message.roam.has_snr = update.roam.snr ? true : false;
	message.roam.snr = update.roam.snr;

	// only include when it has a value
	message.roam.has_bat = update.roam.bat_voltage ? true : false;
	message.roam.bat = update.roam.bat_voltage;

	// only include when it enabled
	message.roam.has_psm = update.roam.psm_enabled;
	message.roam.psm = update.roam.psm_enabled;

	// only include when it has a value
	message.roam.has_tau = update.roam.tau ? true : false;
	message.roam.tau = update.roam.tau;

	// only include when it has a value
	message.roam.has_act = update.roam.active_time ? true : false; // Tested with -1 in online compiler and this works
	message.roam.act = update.roam.active_time;

	message.roam.ts = update.roam.ts;

	// only include when it has a value
	message.roam.has_edrx = update.roam.edrx_changed;
	message.roam.edrx = update.roam.edrx;

	// only include when it has a value
	message.roam.has_ptw = update.roam.edrx_changed;
	message.roam.ptw = update.roam.ptw;

	// If it has any of the roam fields, then it has roam
	if (!(message.roam.has_rsrp || message.roam.has_bat || message.roam.has_psm || message.roam.has_tau || message.roam.has_act || message.roam.has_edrx || message.roam.has_ptw))
	{
		message.has_roam = false;
	}

	if (update.has_cfg == true)
	{
		message.has_cfg = true;
		message.cfg.udc = update.cfg.upload_duty_cycle;
		message.cfg.adc = update.cfg.app_duty_cycle;
		message.cfg.has_devstate = true;
		message.cfg.devstate = update.cfg.devstate;
		message.cfg.has_ispulse = true;
		message.cfg.ispulse = update.cfg.is_pulse_tracker;
		message.cfg.algo.funcs.encode = buffer_encode_callback;
		message.cfg.algo.arg = (void *)update.cfg.algo;
		message.cfg.has_log = true;
		message.cfg.log = update.cfg.log_enabled;
	}

	if (update.packets.packets_count > 0)
	{
		packet_count = update.packets.packets_count; // used by the encoding function
		message.env.funcs.encode = packets_encode_callback;
		message.env.arg = (void *)update.packets.data;
	}

	if (update.alerts.alerts_count > 0)
	{
		alert_count = update.alerts.alerts_count; // used by the encoding function
		message.alrt.funcs.encode = alerts_encode_callback;
		message.alrt.arg = (void *)update.alerts.alert;
	}

	/* Now we are ready to encode the message! */
	status = pb_encode(&stream, ShadowUpdate_fields, &message);
	*message_length = stream.bytes_written;

	if (!status)
	{
		LOG_ERR("Encoding failed: %s", PB_GET_ERROR(&stream));
	}

	return status;
}

bool decode_message(uint8_t *buffer, size_t message_length)
{
	bool status;

	/* Allocate space for the decoded message. */
	ShadowUpdate message = ShadowUpdate_init_zero;

	/* Create a stream that reads from the buffer. */
	pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);

	/* Now we are ready to decode the message. */
	status = pb_decode(&stream, ShadowUpdate_fields, &message);

	/* Check for errors... */
	if (status)
	{
		/* Print the data contained in the message. */
		LOG_INF("Your app_packet_count was %d", (int)message.fcnt);
	}
	else
	{
		LOG_ERR("Decoding failed: %s", PB_GET_ERROR(&stream));
	}

	return status;
}
