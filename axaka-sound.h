#pragma once

#include <stdint.h>


// (16780000.0 / (0x10000 - 0xfce2)), so actually more like:
// 21,027.568922305764411027568922306 Hz
// for some reason that sounds too low so just subtract a random number
#define AXAKA_CLOCK_DIVIDER (0x10000 - 0xfce2 - 16)

#define AXAKA_CHANNELS 6


typedef struct {
	int8_t * data;
	uint32_t length;
	uint32_t loop_length;
	uint32_t rate;
} axaka_instrument_t;

typedef struct {
	axaka_instrument_t * instrument;
	uint32_t sample_offset;
	uint16_t offset_accum;
	uint32_t rate;
	int16_t volume;
} axaka_channel_t;

typedef struct {
	axaka_channel_t ch[AXAKA_CHANNELS];
} axaka_sound_t;



void axaka_sound_init(axaka_sound_t * as);

void axaka_sound_clock(axaka_sound_t * as);
int axaka_sound_get_sample(axaka_sound_t * as);

