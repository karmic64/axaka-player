#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <locale.h>

#include <SDL.h>
#include <SDL2_gfxPrimitives.h>

#include "machine.h"
#include "axaka-sequencer-files.h"





///////////////////////////////////////////////////////////////////////
// audio callback


void machine_audio_callback(void * userdata, Uint8 * stream, int len) {
	machine_t * m = userdata;
	int16_t * buf = (int16_t *) stream;
	int samples = len / 2 / 2;
	
	// dump the audio we have
	machine_dump_audio_buffer(m, buf, samples);
	
	// signal main
	// only do it if the event is not in the queue (to avoid unresponsiveness when lagging)
	if (!SDL_HasEvent(m->ready_event)) {
		SDL_Event e;
		e.type = m->ready_event;
		SDL_PushEvent(&e);
	}
}




/////////////////////////////////////////////////////////////////////
// error reporting


// try opening an error message box, otherwise settle for stderr
void err(SDL_Window * parent, const char * format, ...) {
	char msg[0x100];
	va_list args;
	va_start(args, format);
	vsprintf(msg, format, args);
	va_end(args);
	
	if (
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "AXAKA Player", msg, parent)
	) {
		fputs(msg, stderr);
	}
}







/////////////////////////////////////////////////////////////////////////////
// rendering


#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

#define STR_BUF_SIZE (WINDOW_WIDTH / CHAR_WIDTH)



#define TEXT_COLOR 0xffffffff
#define INACTIVE_COLOR 0xff666666

unsigned get_time_fade_color(unsigned time) {
	if (time > 0x0c) {
		time = 0x0c;
	}
	unsigned color = 0xff - (time * 8);
	return
		(color << 0) | 
		(color << 8) | 
		(color << 16) | 
		(0xff << 24);
}


const char note_names[12][2] = {
	"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"
};


const char * get_gb_sound_period_note(unsigned period) {
	if (period >= 0x800)
		return "out of range";
	
	double freq = (double)GB_PULSE_CLOCK_RATE / (0x800 - period) / 8.0;
	// -4 to start at C-1 instead of A-0
	double note_full = 12.0 * log2(freq / 440.0) + 49.0 - 4.0;
	// not used, but can't pass NULL to modf
	double note_int;
	
	int note = round(note_full);
	int cents = round(modf(note_full, &note_int) * 100.0);
	if (cents >= 50) {
		cents -= 100;
	}
	
	static char name[16];
	sprintf(name,
		"%c%c%d %+03d",
		note_names[note % 12][0], note_names[note % 12][1], note / 12,
		cents);
	
	return name;
}





void render_axaka_sequencer(SDL_Renderer * renderer, axaka_sequencer_t * asq, unsigned x, unsigned y) {
	char str[STR_BUF_SIZE];
	
	// header
	sprintf(str, "Now playing: %s", asq->song_name ? asq->song_name : "<none>");
	stringColor(renderer, x, y, str, TEXT_COLOR);
	sprintf(str, "Instrument file: %s", asq->inst_name ? asq->inst_name : "<none>");
	stringColor(renderer, x, y + 1*CHAR_HEIGHT, str, TEXT_COLOR);
	
	// song playback
	if (asq->data_start && asq->data) {
		sprintf(str,
			"Speed: %06X   "
			"Ticks: %02X   "
			"Data offset: %06tX   "
			"Loop offset: %06tX",
			asq->tick_rate,
			asq->tick_count,
			asq->data - asq->data_start,
			asq->data_loop - asq->data_start
			);
		stringColor(renderer, x, y + 3*CHAR_HEIGHT, str, TEXT_COLOR);
	} else if (asq->data_start) {
		stringColor(renderer, x, y + 3*CHAR_HEIGHT, "Song stopped", INACTIVE_COLOR);
	} else {
		stringColor(renderer, x, y + 3*CHAR_HEIGHT, "No song loaded", INACTIVE_COLOR);
	}
}




void render_axaka_instrument(SDL_Renderer * renderer, axaka_sequencer_t * asq, axaka_instrument_t * ins, unsigned x, unsigned y) {
	char str[STR_BUF_SIZE];
	if (ins) {
		sprintf(str,
			"Instrument: %02tX   "
			"Length: %06X   "
			"Loop: %06X   "
			"Rate: %06X",
			ins - asq->inst,
			ins->length,
			ins->loop_length,
			ins->rate
			);
		stringColor(renderer, x, y, str, TEXT_COLOR);
	} else {
		stringColor(renderer, x, y, "No instrument loaded", INACTIVE_COLOR);
	}
}






void render_axaka_track(SDL_Renderer * renderer, axaka_sequencer_t * asq, unsigned ix, unsigned x, unsigned y) {
	char str[STR_BUF_SIZE];
	axaka_track_t * tr = &asq->tr[ix];
	axaka_channel_t * ch = &asq->as->ch[ix];
	
	// header
	sprintf(str, "Sample channel %u:", ix);
	stringColor(renderer, x, y, str, get_time_fade_color(tr->note_time));
	
	// instrument
	render_axaka_instrument(renderer, asq, ch->instrument, x + CHAR_WIDTH, y + 1*CHAR_HEIGHT);
	
	// sample playback
	unsigned main_color = ch->sample_offset == -1u ? INACTIVE_COLOR : TEXT_COLOR;
	
	sprintf(str,
		"Sample offset: %06X   "
		"Current rate: %06X   "
		"Note: %c%c%u (%02X)   "
		"Volume: %02X",
		ch->sample_offset & 0xffffff,
		ch->rate,
		note_names[tr->note % 12][0], note_names[tr->note % 12][1], tr->note / 12, tr->note,
		ch->volume
		);
	stringColor(renderer, x + CHAR_WIDTH, y + 2*CHAR_HEIGHT, str, main_color);
	
	// effects
	sprintf(str,
		"Detune: %02X   "
		"Volume slide rate: %06X   "
		"Pitch slide rate: %06X",
		tr->detune & 0xff,
		tr->volume_slide_rate & 0xffffff,
		tr->pitch_slide_rate & 0xffffff
		);
	stringColor(renderer, x + CHAR_WIDTH, y + 3*CHAR_HEIGHT, str, main_color);
}





void render_axaka_percussion(SDL_Renderer * renderer, axaka_sequencer_t * asq, unsigned x, unsigned y) {
	char str[STR_BUF_SIZE];
	
	stringColor(renderer, x, y, "Percussion:", TEXT_COLOR);
	for (unsigned ix = 0; ix < AXAKA_PERCUSSION_TRACKS; ix++) {
		axaka_track_t * tr = &asq->tr[ix + AXAKA_MELODIC_TRACKS];
		axaka_channel_t * ch = &asq->as->ch[ix + AXAKA_MELODIC_TRACKS];
		
		unsigned color = get_time_fade_color(tr->note_time);
		
		unsigned current_px;
		uint8_t * current_px_ptr =
			ch->instrument ?
			memchr(asq->percussion_tbl[ix],
				ch->instrument - asq->inst,
				AXAKA_MAX_PERCUSSION) :
			NULL;
		current_px = current_px_ptr ? current_px_ptr - asq->percussion_tbl[ix] : -1;
		
		for (unsigned px = 0; px < AXAKA_MAX_PERCUSSION; px++) {
			unsigned xx = x + (16 * CHAR_WIDTH) +
			(ix * ((AXAKA_MAX_PERCUSSION * 3 + 2) * CHAR_WIDTH)) +
			(px * 3 * CHAR_WIDTH);
			sprintf(str, "%02X", asq->percussion_tbl[ix][px]);
			stringColor(renderer, xx, y, str, px == current_px ? color : INACTIVE_COLOR);
		}
	}
}




void render_axaka_tracks(SDL_Renderer * renderer, axaka_sequencer_t * asq, unsigned x, unsigned y) {
	for (unsigned ix = 0; ix < AXAKA_TRACKS; ix++) {
		render_axaka_track(renderer, asq, ix, x, y + (CHAR_HEIGHT * 5 * ix));
	}
	
	render_axaka_percussion(renderer, asq, x, y + CHAR_HEIGHT * 5 * 6);
}





void render_gb_sound_panning(SDL_Renderer * renderer, unsigned panning, unsigned x, unsigned y) {
	const uint8_t mask_tbl[] = {0x10,0x20,0x40,0x80, 0x01,0x02,0x04,0x08};
	const char channel_tbl[] = "12WN";
	
	stringColor(renderer, x, y, "GB panning:", TEXT_COLOR);
	for (unsigned i = 0; i < 8; i++) {
		characterColor(renderer, x + (13 + i + (i >= 4))*CHAR_WIDTH, y, channel_tbl[i%4], (panning & mask_tbl[i] ? TEXT_COLOR : INACTIVE_COLOR));
	}
}






void render_gb_sound_length_counter(SDL_Renderer * renderer, gb_sound_length_counter_t * lc, unsigned x, unsigned y) {
	char str[STR_BUF_SIZE];
	sprintf(str,
		"Length: %03X",
		lc->initial);
	stringColor(renderer, x, y, str, lc->enable ? TEXT_COLOR : INACTIVE_COLOR);
}



void render_gb_sound_sweep(SDL_Renderer * renderer, gb_sound_sweep_t * sw, unsigned x, unsigned y) {
	char str[STR_BUF_SIZE];
	sprintf(str,
		"Sweep shift: %X   "
		"Sweep direction: %s   "
		"Sweep speed: %X   ",
		sw->shift,
		sw->direction ? "Down" : "Up  ",
		sw->time);
	stringColor(renderer, x, y, str, sw->time ? TEXT_COLOR : INACTIVE_COLOR);
}



void render_gb_sound_envelope(SDL_Renderer * renderer, gb_sound_envelope_t * env, unsigned x, unsigned y) {
	char str[STR_BUF_SIZE];
	sprintf(str,
		"NRx2: %02X   "
		"Env initial: %X   "
		"Env direction: %s   "
		"Env speed: %X   ",
		env->nrx2,
		env->initial_volume,
		env->direction ? "Up  " : "Down",
		env->time);
	stringColor(renderer, x, y, str, env->time ? TEXT_COLOR : INACTIVE_COLOR);
}



void render_gb_sound_pulse(SDL_Renderer * renderer, gb_sound_pulse_t * pul, unsigned x, unsigned y, unsigned color) {
	char str[STR_BUF_SIZE];
	sprintf(str,
		"Volume: %X   "
		"Duty: %X   "
		"Period: %03X (%s)",
		pul->volume,
		pul->duty,
		pul->period, get_gb_sound_period_note(pul->period));
	stringColor(renderer, x, y, str, color);
}



void render_gb_sound_wave(SDL_Renderer * renderer, gb_sound_wave_t * wav, unsigned x, unsigned y, unsigned color) {
	char str[STR_BUF_SIZE];
	sprintf(str,
		"Volume: %X   "
		"Period: %03X (%s)",
		wav->volume,
		wav->period, get_gb_sound_period_note(wav->period));
	stringColor(renderer, x, y, str, color);
	
	memcpy(str, "Wave: ", 6);
	for (unsigned i = 0; i < 64; i++) {
		unsigned w = wav->wave_ram[i];
		str[i + 6] = w < 0x0a ? w + '0' : w - 0x0a + 'A';
	}
	str[64 + 6] = '\0';
	stringColor(renderer, x, y + CHAR_HEIGHT, str, color);
}



void render_gb_sound_noise(SDL_Renderer * renderer, gb_sound_noise_t * noi, unsigned x, unsigned y, unsigned color) {
	char str[STR_BUF_SIZE];
	sprintf(str,
		"Volume: %X   "
		"Period: %03X   "
		"Mode: %X",
		noi->volume,
		noi->time,
		noi->mode);
	stringColor(renderer, x, y, str, color);
}




void render_gb_sound(SDL_Renderer * renderer, gb_sound_t * gbs, unsigned x, unsigned y) {
	//
	render_gb_sound_panning(renderer, gbs->panning, x+(CHAR_WIDTH*53), y);
	
	// pulse 1
	stringColor(renderer, x, y + 1*CHAR_HEIGHT, "GB pulse channel 1:", get_time_fade_color(gbs->pulse_1.retrig_time));
	render_gb_sound_pulse(renderer, &gbs->pulse_1, x + CHAR_WIDTH, y + 2*CHAR_HEIGHT, gb_sound_pulse_1_is_enabled(gbs) ? TEXT_COLOR : INACTIVE_COLOR);
	render_gb_sound_length_counter(renderer, &gbs->pulse_1_length_counter, x + 512, y + 2*CHAR_HEIGHT);
	render_gb_sound_envelope(renderer, &gbs->pulse_1_envelope, x + CHAR_WIDTH, y + 3*CHAR_HEIGHT);
	render_gb_sound_sweep(renderer, &gbs->pulse_1_sweep, x + CHAR_WIDTH, y + 4*CHAR_HEIGHT);
	
	// pulse 2
	stringColor(renderer, x, y + 6*CHAR_HEIGHT, "GB pulse channel 2:", get_time_fade_color(gbs->pulse_2.retrig_time));
	render_gb_sound_pulse(renderer, &gbs->pulse_2, x + CHAR_WIDTH, y + 7*CHAR_HEIGHT, gb_sound_pulse_2_is_enabled(gbs) ? TEXT_COLOR : INACTIVE_COLOR);
	render_gb_sound_length_counter(renderer, &gbs->pulse_2_length_counter, x + 512, y + 7*CHAR_HEIGHT);
	render_gb_sound_envelope(renderer, &gbs->pulse_2_envelope, x + CHAR_WIDTH, y + 8*CHAR_HEIGHT);
	
	// wave
	stringColor(renderer, x, y + 10*CHAR_HEIGHT, "GB wave channel:", get_time_fade_color(gbs->wave.retrig_time));
	render_gb_sound_wave(renderer, &gbs->wave, x + CHAR_WIDTH, y + 11*CHAR_HEIGHT, gb_sound_wave_is_enabled(gbs) ? TEXT_COLOR : INACTIVE_COLOR);
	render_gb_sound_length_counter(renderer, &gbs->wave_length_counter, x + 512, y + 11*CHAR_HEIGHT);
	
	// noise
	stringColor(renderer, x, y + 14*CHAR_HEIGHT, "GB noise channel:", get_time_fade_color(gbs->noise.retrig_time));
	render_gb_sound_noise(renderer, &gbs->noise, x + CHAR_WIDTH, y + 15*CHAR_HEIGHT, gb_sound_noise_is_enabled(gbs) ? TEXT_COLOR : INACTIVE_COLOR);
	render_gb_sound_length_counter(renderer, &gbs->noise_length_counter, x + 512, y + 15*CHAR_HEIGHT);
	render_gb_sound_envelope(renderer, &gbs->noise_envelope, x + CHAR_WIDTH, y + 16*CHAR_HEIGHT);
}









///////////////////////////////////////////////////////////////////////////
// main




int main(int argc, char * argv[]) {
	setlocale(LC_ALL, "");
	
	// variables
	int return_status = EXIT_FAILURE;
	
	machine_t * m = NULL;
	
	int audio_status = -1;
	SDL_Window * window = NULL;
	SDL_Renderer * renderer = NULL;
	
	const char * err_msg = NULL;
	
	// machine init
	m = malloc(sizeof(*m));
	if (!m) {
		err(NULL, "Can't allocate machine");
		goto cleanup;
	}
	machine_init(m, SDL_RegisterEvents(1));
	
	
	// try loading command line files
	for (int i = 1; i < argc; i++) {
		err_msg = axaka_sequencer_load_file(&m->asq, argv[i]);
		if (err_msg) {
			err(window, "Error loading %s: %s", argv[i], err_msg);
		}
	}
	
	
	
	// SDL init
	
	if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
		err(window, "Error initializing SDL: %s", SDL_GetError());
		goto cleanup;
	}
	
	SDL_AudioSpec audiospec;
	audiospec.freq = GBA_SAMPLE_RATE;
	audiospec.format = AUDIO_S16SYS;
	audiospec.channels = 2;
	audiospec.samples = AUDIO_BUFFER_SIZE;
	audiospec.callback = machine_audio_callback;
	audiospec.userdata = m;
	audio_status = SDL_OpenAudio(&audiospec, NULL);
	if (audio_status < 0) {
		err(window, "Error opening audio device:\n%s", SDL_GetError());
		goto cleanup;
	}
	
	if (SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer)) {
		err(window, "Error creating window:\n%s", SDL_GetError());
		goto cleanup;
	}
	SDL_SetWindowTitle(window, "AXAKA Player");
	
	
	// main
	return_status = EXIT_SUCCESS;
	
	SDL_PauseAudio(0);
	
	SDL_Event e;
	while (SDL_WaitEvent(&e)) {
		if (e.type == SDL_QUIT) {
			// quit, end loop
			break;
		} else if (e.type == SDL_DROPFILE) {
			// load file
			err_msg = axaka_sequencer_load_file(&m->asq, e.drop.file);
			if (err_msg) {
				err(window, "Error loading %s: %s", e.drop.file, err_msg);
			}
			SDL_free(e.drop.file);
		} else if (e.type == SDL_KEYDOWN) {
			// keypress
			switch (e.key.keysym.sym) {
				case SDLK_SPACE:
					axaka_sequencer_play_music(&m->asq);
					break;
				case SDLK_ESCAPE:
					axaka_sequencer_reset(&m->asq);
					break;
			}
		} else if (e.type == m->ready_event) {
			// audio callback was executed, update audio and screen
			machine_fill_audio_buffer(m);
			
			SDL_RenderClear(renderer);
			
			render_axaka_sequencer(renderer, &m->asq, CHAR_WIDTH, CHAR_HEIGHT*1);
			render_axaka_tracks(renderer, &m->asq, CHAR_WIDTH, CHAR_HEIGHT*8);
			render_gb_sound(renderer, &m->gbs, CHAR_WIDTH, CHAR_HEIGHT*41);
			
			SDL_RenderPresent(renderer);
		}
	}
	
	
	// end
cleanup:
	if (audio_status >= 0) {
		SDL_CloseAudio();
	}
	if (renderer) {
		SDL_DestroyRenderer(renderer);
	}
	if (window) {
		SDL_DestroyWindow(window);
	}
	SDL_Quit();
	if (m) {
		axaka_sequencer_free_instrument_list(&m->asq);
		free(m);
	}
	
	return return_status;
}

