#pragma once

#include <stdint.h>

#include "gb-sound.h"
#include "axaka-sound.h"


#define AXAKA_MELODIC_TRACKS 4
#define AXAKA_PERCUSSION_TRACKS 2
#define AXAKA_TRACKS (AXAKA_MELODIC_TRACKS + AXAKA_PERCUSSION_TRACKS)

#define AXAKA_MAX_INSTRUMENTS 0x100
#define AXAKA_MAX_PERCUSSION 4


typedef struct {
	uint8_t note;
	int8_t detune;
	
	int32_t pitch_slide_rate;
	uint16_t pitch_slide_accum;
	
	int32_t volume_slide_rate;
	uint16_t volume_slide_accum;
	
	unsigned note_time;
} axaka_track_t;

typedef struct {
	uint8_t * data_start;
	uint8_t * data;
	uint8_t * data_loop;
	uint32_t tick_rate;
	uint16_t tick_accum;
	uint8_t tick_count;
	
	unsigned max_instrument;
	axaka_instrument_t * inst;
	
	char * song_name;
	char * inst_name;
	
	uint8_t percussion_tbl[AXAKA_PERCUSSION_TRACKS][AXAKA_MAX_PERCUSSION];
	
	axaka_track_t tr[AXAKA_TRACKS];
	
	axaka_sound_t * as;
	gb_sound_t * gbs;
} axaka_sequencer_t;



void axaka_sequencer_init(axaka_sequencer_t * asq, axaka_sound_t * as, gb_sound_t * gbs);
void axaka_sequencer_reset(axaka_sequencer_t * asq);

void axaka_sequencer_free_song_data(axaka_sequencer_t * asq);
void axaka_sequencer_free_instrument_list(axaka_sequencer_t * asq);

void axaka_sequencer_play_music(axaka_sequencer_t * asq);

void axaka_sequencer_clock(axaka_sequencer_t * asq);

