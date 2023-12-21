#pragma once

#include "gb-sound.h"
#include "axaka-sound.h"
#include "axaka-sequencer.h"


// exactly 0x1000000 Hz
#define GBA_CLOCK_RATE (16*1024*1024)
// h-dots * vdots * 4 cycles per dot
#define CYCLES_PER_FRAME (308 * 228 * 4)
// this is the rate that the axaka music data is processed
#define GBA_FRAME_RATE (GBA_CLOCK_RATE / CYCLES_PER_FRAME)

// exactly 0x8000 Hz
#define GBA_SAMPLE_RATE 32768
// one sample per 512 clocks
#define SAMPLE_CLOCK_DIVIDER (GBA_CLOCK_RATE / GBA_SAMPLE_RATE)

// in ms
#define CALLBACK_INTERVAL (1000.0 / GBA_FRAME_RATE)
// in frames (samples / channels)
#define AUDIO_BUFFER_SIZE 0x400

#define GB_SOUND_CLOCK_DIVIDER (GBA_CLOCK_RATE / GB_CLOCK_RATE)



typedef struct {
	gb_sound_t gbs;
	axaka_sound_t as;
	
	axaka_sequencer_t asq;
	
	unsigned frame;
	unsigned cyc;
	unsigned frame_cyc;
	unsigned axaka_sample_cyc;
	
	int16_t audio_buffer_left[AUDIO_BUFFER_SIZE];
	int16_t audio_buffer_right[AUDIO_BUFFER_SIZE];
	unsigned audio_buffer_read_index;
	unsigned audio_buffer_write_index;
	unsigned audio_buffer_queue_size;
	
	unsigned ready_event;
} machine_t;




void machine_init(machine_t * m, unsigned ready_event);
int machine_clock(machine_t * m, int * left, int * right);

void machine_fill_audio_buffer(machine_t * m);
void machine_dump_audio_buffer(machine_t * m, int16_t * dest, unsigned len);


