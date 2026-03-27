#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include <zephyr/kernel.h>

struct note_duration
{
	int note;			/* hz */
	int duration; /* msec */
};

int buzzer_init(void);

// Synchronous functions (blocking)
void play_tone_sync(int length);
void play_note_sync(struct note_duration *note);
void play_beeps_sync(uint8_t count, int beep_length, int gap_length);

// Asynchronous functions (non-blocking)
void play_tone_async(int length);
void play_note_async(struct note_duration *note);
void play_beeps_async(uint8_t count, int beep_length, int gap_length);

void stop_tone(void);

// Constants
#define DEFAULT_FREQ 4000

// Note durations (ms)
#define sixteenth 38
#define eigth 75
#define quarter 150
#define half 300
#define whole 600

// Musical notes (Hz)
#define C4 262
#define Db4 277
#define D4 294
#define Eb4 311
#define E4 330
#define F4 349
#define Gb4 370
#define G4 392
#define Ab4 415
#define A4 440
#define Bb4 466
#define B4 494
#define C5 523
#define Db5 554
#define D5 587
#define Eb5 622
#define E5 659
#define F5 698
#define Gb5 740
#define G5 784
#define Ab5 831
#define A5 880
#define Bb5 932
#define B5 988
#define C6 1046
#define Db6 1109
#define D6 1175
#define Eb6 1245
#define E6 1319
#define F6 1397
#define Gb6 1480
#define G6 1568
#define Ab6 1661
#define A6 1760
#define Bb6 1865
#define B6 1976
#define REST 1

#endif