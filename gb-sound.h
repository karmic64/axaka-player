#pragma once

#include <stdint.h>


// exactly 0x400000 Hz
#define GB_CLOCK_RATE 4194304
#define GB_PULSE_CLOCK_DIVIDER 4
#define GB_PULSE_CLOCK_RATE (GB_CLOCK_RATE / GB_NOISE_CLOCK_DIVIDER)
#define GB_WAVE_CLOCK_DIVIDER 2
#define GB_WAVE_CLOCK_RATE (GB_CLOCK_RATE / GB_WAVE_CLOCK_DIVIDER)
// exactly 0x80000 Hz
#define GB_NOISE_CLOCK_RATE 524288
#define GB_NOISE_CLOCK_DIVIDER (GB_CLOCK_RATE / GB_NOISE_CLOCK_RATE)

// exactly 0x200 Hz
#define GB_DIV_APU_CLOCK_RATE 512
#define GB_DIV_APU_CLOCK_DIVIDER (GB_CLOCK_RATE / GB_DIV_APU_CLOCK_RATE)
// exactly 0x80 Hz
#define GB_SWEEP_CLOCK_RATE 128
#define GB_SWEEP_CLOCK_DIVIDER (GB_CLOCK_RATE / GB_SWEEP_CLOCK_RATE)
// exactly 0x40 Hz
#define GB_ENVELOPE_CLOCK_RATE 64
#define GB_ENVELOPE_CLOCK_DIVIDER (GB_CLOCK_RATE / GB_ENVELOPE_CLOCK_RATE)
// exactly 0x100 Hz
#define GB_LENGTH_COUNTER_CLOCK_RATE 256
#define GB_LENGTH_COUNTER_CLOCK_DIVIDER (GB_CLOCK_RATE / GB_LENGTH_COUNTER_CLOCK_RATE)




typedef struct {
	uint8_t enable;
	uint16_t initial;
	uint16_t current;
	uint16_t maximum;
} gb_sound_length_counter_t;

typedef struct {
	uint16_t * period_ptr;
	uint8_t shift;
	uint8_t direction;
	uint8_t time;
	uint8_t count;
} gb_sound_sweep_t;

typedef struct {
	uint8_t nrx2;
	uint8_t * volume_ptr;
	uint8_t initial_volume;
	uint8_t direction;
	uint8_t time;
	uint8_t count;
} gb_sound_envelope_t;




typedef struct {
	uint8_t volume;
	uint8_t duty;
	uint16_t period;
	uint16_t count;
	uint8_t sample;
	
	unsigned retrig_time;
} gb_sound_pulse_t;

typedef struct {
	uint8_t enable;
	uint8_t volume;
	uint16_t period;
	uint16_t count;
	uint8_t index;
	uint8_t wave_ram[64];
	
	unsigned retrig_time;
} gb_sound_wave_t;

typedef struct {
	uint8_t volume;
	uint16_t time;
	uint16_t count;
	uint8_t mode;
	uint16_t lfsr;
	
	unsigned retrig_time;
} gb_sound_noise_t;




typedef struct {
	uint16_t cyc;
	uint8_t panning;
	
	gb_sound_pulse_t pulse_1;
	gb_sound_sweep_t pulse_1_sweep;
	gb_sound_envelope_t pulse_1_envelope;
	gb_sound_length_counter_t pulse_1_length_counter;
	
	gb_sound_pulse_t pulse_2;
	gb_sound_envelope_t pulse_2_envelope;
	gb_sound_length_counter_t pulse_2_length_counter;
	
	gb_sound_wave_t wave;
	gb_sound_length_counter_t wave_length_counter;
	
	gb_sound_noise_t noise;
	gb_sound_envelope_t noise_envelope;
	gb_sound_length_counter_t noise_length_counter;
} gb_sound_t;





void gb_sound_init(gb_sound_t * gbs);

void gb_sound_clock(gb_sound_t * gbs);
int gb_sound_get_sample(gb_sound_t * gbs, int * left, int * right);

void gb_sound_write(gb_sound_t * gbs, unsigned reg, unsigned v);



int gb_sound_pulse_1_is_enabled(gb_sound_t * gbs);
int gb_sound_pulse_2_is_enabled(gb_sound_t * gbs);
int gb_sound_wave_is_enabled(gb_sound_t * gbs);
int gb_sound_noise_is_enabled(gb_sound_t * gbs);

