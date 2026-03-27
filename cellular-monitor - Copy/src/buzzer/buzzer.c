/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/byteorder.h>

#include "buzzer.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(buzzer, 3);

static const struct pwm_dt_spec buzzer_pwm_pin = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(buzzpwm), 0);

// Timer for auto-stopping tones
static struct k_timer stop_tone_timer;

// For async beep sequences
static struct
{
	uint8_t count;
	int beep_length;
	int gap_length;
	uint8_t current_beep;
	bool in_gap; // Track if we're in a gap between beeps
} beep_sequence = {0};

static void stop_tone_handler(struct k_timer *dummy)
{
	stop_tone();

	// If we're in a beep sequence, handle the next state
	if (beep_sequence.count > 0)
	{
		if (beep_sequence.in_gap)
		{
			// Gap is over, start next beep if not done
			if (beep_sequence.current_beep < beep_sequence.count - 1)
			{
				beep_sequence.current_beep++;
				beep_sequence.in_gap = false;
				pwm_set_dt(&buzzer_pwm_pin, PWM_HZ(DEFAULT_FREQ), PWM_HZ(DEFAULT_FREQ) / 2);
				k_timer_start(&stop_tone_timer, K_MSEC(beep_sequence.beep_length), K_NO_WAIT);
			}
			else
			{
				beep_sequence.count = 0; // Sequence complete
			}
		}
		else
		{
			// Beep is over, start gap
			beep_sequence.in_gap = true;
			k_timer_start(&stop_tone_timer, K_MSEC(beep_sequence.gap_length), K_NO_WAIT);
		}
	}
}

int buzzer_init(void)
{
	if (!device_is_ready(buzzer_pwm_pin.dev))
	{
		printk("Error: PWM device %s is not ready\n",
					 buzzer_pwm_pin.dev->name);
		return 1;
	}

	// Initialize the timer but don't start it
	k_timer_init(&stop_tone_timer, stop_tone_handler, NULL);

	return 0;
}

// Synchronous (blocking) implementations
void play_tone_sync(int length)
{
	pwm_set_dt(&buzzer_pwm_pin, PWM_HZ(DEFAULT_FREQ), PWM_HZ(DEFAULT_FREQ) / 2);
	k_msleep(length);
	stop_tone();
}

void play_note_sync(struct note_duration *note)
{
	pwm_set_dt(&buzzer_pwm_pin, PWM_HZ(note->note), PWM_HZ(note->note) / 2);
	k_msleep(note->duration);
	stop_tone();
}

void play_beeps_sync(uint8_t count, int beep_length, int gap_length)
{
	for (uint8_t i = 0; i < count; i++)
	{
		play_tone_sync(beep_length);
		if (i < count - 1)
		{
			k_msleep(gap_length);
		}
	}
}

// Asynchronous (non-blocking) implementations
void play_tone_async(int length)
{
	pwm_set_dt(&buzzer_pwm_pin, PWM_HZ(DEFAULT_FREQ), PWM_HZ(DEFAULT_FREQ) / 2);
	k_timer_start(&stop_tone_timer, K_MSEC(length), K_NO_WAIT);
}

void play_note_async(struct note_duration *note)
{
	pwm_set_dt(&buzzer_pwm_pin, PWM_HZ(note->note), PWM_HZ(note->note) / 2);
	k_timer_start(&stop_tone_timer, K_MSEC(note->duration), K_NO_WAIT);
}

void play_beeps_async(uint8_t count, int beep_length, int gap_length)
{
	if (count == 0)
		return;

	// Set up the beep sequence
	beep_sequence.count = count;
	beep_sequence.beep_length = beep_length;
	beep_sequence.gap_length = gap_length;
	beep_sequence.current_beep = 0;
	beep_sequence.in_gap = false; // Start with a beep

	// Start the first beep
	pwm_set_dt(&buzzer_pwm_pin, PWM_HZ(DEFAULT_FREQ), PWM_HZ(DEFAULT_FREQ) / 2);
	k_timer_start(&stop_tone_timer, K_MSEC(beep_length), K_NO_WAIT);
}

void stop_tone(void)
{
	pwm_set_pulse_dt(&buzzer_pwm_pin, 0);
}