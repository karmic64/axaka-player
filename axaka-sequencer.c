
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "axaka-sequencer.h"



//////////////////////////////////////////////////////////////////////////////
// init


void axaka_sequencer_init(axaka_sequencer_t * asq, axaka_sound_t * as, gb_sound_t * gbs) {
	memset(asq, 0, sizeof(*asq));
	asq->as = as;
	asq->gbs = gbs;
	
	for (unsigned ix = 0; ix < AXAKA_TRACKS; ix++) {
		asq->tr[ix].note_time = -1u;
	}
}


void axaka_sequencer_init_sound(axaka_sequencer_t * asq) {
	axaka_sound_init(asq->as);
	gb_sound_init(asq->gbs);
}


// clear playback variables and clear sound but don't unload song/inst
void axaka_sequencer_reset(axaka_sequencer_t * asq) {
	asq->data = NULL;
	asq->data_loop = NULL;
	asq->tick_rate = 0;
	asq->tick_accum = 0;
	asq->tick_count = 0;
	
	memset(asq->percussion_tbl, 0, sizeof(asq->percussion_tbl));
	
	memset(asq->tr, 0, sizeof(asq->tr));
	for (unsigned ix = 0; ix < AXAKA_TRACKS; ix++) {
		asq->tr[ix].note_time = -1u;
	}
	
	axaka_sequencer_init_sound(asq);
}





int axaka_sequencer_is_valid(axaka_sequencer_t * asq) {
	// don't check instruments to make hajirusu tunes playable
	return asq->data_start != NULL /*&& asq->inst != NULL*/;
}

int axaka_sequencer_can_play(axaka_sequencer_t * asq) {
	return axaka_sequencer_is_valid(asq) && asq->data;
}





void axaka_sequencer_free_song_data(axaka_sequencer_t * asq) {
	void * old = asq->data_start;
	
	if (asq->song_name) {
		void * old_name = asq->song_name;
		asq->song_name = NULL;
		free(old_name);
	}
	
	axaka_sequencer_reset(asq);
	if (old) {
		asq->data_start = NULL;
		free(old);
	}
}


void axaka_sequencer_free_instrument_list(axaka_sequencer_t * asq) {
	unsigned old_max = asq->max_instrument;
	axaka_instrument_t * old = asq->inst;
	
	if (asq->inst_name) {
		void * old_name = asq->inst_name;
		asq->inst_name = NULL;
		free(old_name);
	}
	
	// if the instruments are invalid, then the song is too
	axaka_sequencer_free_song_data(asq);
	
	if (old) {
		asq->max_instrument = 0;
		asq->inst = NULL;
		
		for (unsigned i = 0; i < old_max; i++) {
			free(old[i].data);
		}
		free(old);
	}
}






////////////////////////////////////////////////////////////////////////
// data/command processing


unsigned axaka_sequencer_get_byte(axaka_sequencer_t * asq) {
	return *(asq->data++);
}

unsigned axaka_sequencer_get_half(axaka_sequencer_t * asq) {
	unsigned o = 0;
	for (unsigned b = 0; b < 16; b+=8) {
		o |= axaka_sequencer_get_byte(asq) << b;
	}
	return o;
}

unsigned axaka_sequencer_get_24(axaka_sequencer_t * asq) {
	unsigned o = 0;
	for (unsigned b = 0; b < 24; b+=8) {
		o |= axaka_sequencer_get_byte(asq) << b;
	}
	return o;
}

unsigned axaka_sequencer_get_word(axaka_sequencer_t * asq) {
	unsigned o = 0;
	for (unsigned b = 0; b < 32; b+=8) {
		o |= axaka_sequencer_get_byte(asq) << b;
	}
	return o;
}




void axaka_sequencer_track_command(axaka_sequencer_t * asq, unsigned b) {
	unsigned track_index = b & 7;
	if (track_index >= AXAKA_TRACKS) {
		printf("bad track index command byte $%02X found in axaka sequence\n", b);
		return;
	}
	
	unsigned command = b & 0x78;
	axaka_track_t * tr = &asq->tr[track_index];
	axaka_channel_t * ch = &asq->as->ch[track_index];
	
	switch (command) {
		case 0x10:
		{
			// play note
			unsigned n = axaka_sequencer_get_byte(asq);
			if (track_index < AXAKA_MELODIC_TRACKS) {
				tr->note = n;
				if (ch->instrument) {
					ch->sample_offset = 0;
				}
			} else {
				unsigned i = asq->percussion_tbl[track_index - AXAKA_MELODIC_TRACKS][n];
				ch->instrument = i < asq->max_instrument ? &asq->inst[i] : NULL;
				if (ch->instrument) {
					if (!ch->instrument->length) {
						ch->instrument = NULL;
					} else {
						ch->sample_offset = 0;
					}
				}
			}
			tr->note_time = 0;
			break;
		}
		case 0x18:
			// stop channel
			ch->sample_offset = -1u;
			break;
		case 0x20:
			// set volume
			ch->volume = axaka_sequencer_get_byte(asq);
			tr->volume_slide_rate = 0;
			break;
		case 0x28:
		{
			// set instrument
			unsigned i = axaka_sequencer_get_byte(asq);
			ch->sample_offset = -1u;
			ch->instrument = i < asq->max_instrument ? &asq->inst[i] : NULL;
			if (ch->instrument && !ch->instrument->length) {
				ch->instrument = NULL;
			}
			break;
		}
		case 0x30:
			// tie note
			tr->note = axaka_sequencer_get_byte(asq);
			break;
		case 0x38:
			// detune
			tr->detune = axaka_sequencer_get_byte(asq);
			tr->pitch_slide_rate = 0;
			break;
		case 0x40:
			// pitch slide
			tr->detune = axaka_sequencer_get_byte(asq);
			tr->pitch_slide_rate = axaka_sequencer_get_word(asq);
			break;
		case 0x48:
			// volume slide
			ch->volume = axaka_sequencer_get_byte(asq);
			tr->volume_slide_rate = axaka_sequencer_get_word(asq);
			break;
		default:
			printf("bad track command byte $%02X found in axaka sequence\n", b);
			break;
	}
}



unsigned axaka_sequencer_command(axaka_sequencer_t * asq, int is_init) {
	unsigned b = axaka_sequencer_get_byte(asq);
	
	if (b == 0x00) {
		// wait
		asq->tick_count = axaka_sequencer_get_byte(asq);
	} else if (b == 0x01 || (b == 0x02 && is_init)) {
		// set loop here
		asq->data_loop = asq->data;
	} else if (b == 0x02) {
		// loop now
		asq->data = asq->data_loop;
	} else if (b == 0x03) {
		// set speed
		asq->tick_rate = axaka_sequencer_get_24(asq);
	} else if (b >= 0x10 && b <= 0x5f) {
		// track command
		axaka_sequencer_track_command(asq, b);
	} else if (b >= 0x60 && b <= 0x7f) {
		// gb sound
		gb_sound_write(asq->gbs, b, axaka_sequencer_get_half(asq));
	} else if (b == 0x80) {
		// gb sound panning
		asq->gbs->panning = axaka_sequencer_get_half(asq) >> 8;
	} else if (b == 0x81) {
		// gb wave channel wave
		for (unsigned i = 0; i < 32; i++) {
			unsigned w = axaka_sequencer_get_byte(asq);
			asq->gbs->wave.wave_ram[i*2] = w >> 4;
			asq->gbs->wave.wave_ram[i*2 + 1] = w & 0xf;
		}
	} else if (b >= 0x82 && b <= 0x83) {
		// percussion table
		for (unsigned i = 0; i < 4; i++) {
			asq->percussion_tbl[b - 0x82][i] = axaka_sequencer_get_byte(asq);
		}
	}
	else {
		printf("bad command byte $%02X found in axaka sequence\n", b);
	}
	
	
	return b;
}






/////////////////////////////////////////////////////////////////////////
// user control


// start the music playback
void axaka_sequencer_play_music(axaka_sequencer_t * asq) {
	if (axaka_sequencer_is_valid(asq)) {
		axaka_sequencer_reset(asq);
		
		// read header
		asq->data = asq->data_start;
		unsigned init_data_offset = axaka_sequencer_get_half(asq);
		axaka_sequencer_get_half(asq);
		asq->tick_rate = axaka_sequencer_get_word(asq);
		
		// read init data
		asq->data = asq->data_start + init_data_offset;
		while (1) {
			unsigned b = axaka_sequencer_command(asq, 1);
			if (b == 0x02) {
				break;
			}
		}
		
		// init vars
		asq->tick_count = 1;
	}
}







// run sequencer for one tick
void axaka_sequencer_tick(axaka_sequencer_t * asq) {
	// do effects
	for (unsigned track_index = 0; track_index < AXAKA_TRACKS; track_index++) {
		axaka_track_t * tr = &asq->tr[track_index];
		axaka_channel_t * ch = &asq->as->ch[track_index];
		
		// volume slide
		int volume_slide_accum = tr->volume_slide_accum;
		volume_slide_accum += tr->volume_slide_rate;
		int volume_slide_diff = volume_slide_accum >> 16;
		ch->volume += volume_slide_diff;
		tr->volume_slide_accum = volume_slide_accum;
		
		// pitch slide
		int pitch_slide_accum = tr->pitch_slide_accum;
		pitch_slide_accum += tr->pitch_slide_rate;
		int pitch_slide_diff = pitch_slide_accum >> 16;
		tr->detune += pitch_slide_diff;
		tr->pitch_slide_accum = pitch_slide_accum;
	}
	
	// read music data
	if (!(--asq->tick_count)) {
		while (axaka_sequencer_command(asq, 0)) {
			
		}
	}
	
	// setup track->channel parameters
	for (unsigned track_index = 0; track_index < AXAKA_TRACKS; track_index++) {
		axaka_track_t * tr = &asq->tr[track_index];
		axaka_channel_t * ch = &asq->as->ch[track_index];
		
		// set sample rate
		if (ch->instrument && ch->sample_offset != -1u) {
			if (track_index < AXAKA_MELODIC_TRACKS) {
				ch->rate = round(ch->instrument->rate * pow(2.0, ((tr->note - 36) / 12.0) + (tr->detune / 128.0)));
			} else {
				ch->rate = 0x10000;
			}
		}
	}
}



// run sequencer for one clock
void axaka_sequencer_clock(axaka_sequencer_t * asq) {
	for (unsigned ix = 0; ix < AXAKA_TRACKS; ix++) {
		axaka_track_t * tr = &asq->tr[ix];
		if (tr->note_time != -1u) {
			tr->note_time++;
		}
	}
	
	if (axaka_sequencer_can_play(asq)) {
		unsigned o = asq->tick_accum;
		o += asq->tick_rate;
		unsigned ticks = o >> 16;
		asq->tick_accum = o;
		
		while (ticks--) {
			axaka_sequencer_tick(asq);
		}
	}
}

