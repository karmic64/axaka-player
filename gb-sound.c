
#include <stdio.h>
#include <string.h>

#include "gb-sound.h"




////////////////////////////////////////////////////////////////////////
// length counter

void gb_sound_retrig_length_counter(gb_sound_length_counter_t * lc) {
	lc->current = lc->initial;
}

void gb_sound_reload_length_counter(gb_sound_length_counter_t * lc, unsigned nrx1) {
	lc->initial = nrx1;
}

void gb_sound_init_length_counter(gb_sound_length_counter_t * lc, unsigned maximum) {
	lc->enable = 0;
	lc->initial = maximum;
	lc->current = maximum;
	lc->maximum = maximum;
}

void gb_sound_clock_length_counter(gb_sound_length_counter_t * lc) {
	if (lc->enable && lc->current < lc->maximum) {
		lc->current++;
	}
}


int gb_sound_length_counter_is_over(gb_sound_length_counter_t * lc) {
	return lc->enable && lc->current == lc->maximum;
}





/////////////////////////////////////////////////////////////////////////
// period sweep


void gb_sound_retrig_sweep(gb_sound_sweep_t * sw) {
	sw->count = sw->time;
}

void gb_sound_reload_sweep(gb_sound_sweep_t * sw, unsigned nrx0) {
	sw->shift = nrx0 & 7;
	sw->direction = nrx0 & 8;
	sw->time = (nrx0 & 0x70) >> 4;
}

void gb_sound_init_sweep(gb_sound_sweep_t * sw, uint16_t * period_ptr) {
	sw->period_ptr = period_ptr;
	gb_sound_reload_sweep(sw, 0);
	gb_sound_retrig_sweep(sw);
}

void gb_sound_clock_sweep(gb_sound_sweep_t * sw) {
	if (sw->time && !(--sw->count)) {
		sw->count = sw->time;
		unsigned period_diff = *sw->period_ptr >> sw->shift;
		if (sw->direction) {
			*sw->period_ptr -= period_diff;
		} else {
			*sw->period_ptr += period_diff;
		}
		
		if (*sw->period_ptr >= 0x800) {
			*sw->period_ptr = 0x800;
		}
	}
}




/////////////////////////////////////////////////////////////////////////
// volume envelope


void gb_sound_retrig_envelope(gb_sound_envelope_t * env) {
	env->time = env->nrx2 & 7;
	env->direction = env->nrx2 & 8;
	env->initial_volume = (env->nrx2 & 0xf0) >> 4;
	
	env->count = env->time;
	*env->volume_ptr = env->initial_volume;
}

void gb_sound_reload_envelope(gb_sound_envelope_t * env, unsigned nrx2) {
	env->nrx2 = nrx2;
	if ((nrx2 & 0xf8) == 0)
		gb_sound_retrig_envelope(env);
}

void gb_sound_init_envelope(gb_sound_envelope_t * env, uint8_t * volume_ptr) {
	env->volume_ptr = volume_ptr;
	gb_sound_reload_envelope(env, 0);
}

void gb_sound_clock_envelope(gb_sound_envelope_t * env) {
	if (env->time && !(--env->count)) {
		env->count = env->time;
		if (env->direction && *env->volume_ptr != 0x0f) {
			(*env->volume_ptr)++;
		} else if (!env->direction && *env->volume_ptr) {
			(*env->volume_ptr)--;
		}
	}
}




/////////////////////////////////////////////////////////////////////////
// pulse channel


void gb_sound_init_pulse(gb_sound_pulse_t * pul) {
	memset(pul, 0, sizeof(*pul));
	pul->retrig_time = -1u;
}

void gb_sound_clock_pulse(gb_sound_pulse_t * pul) {
	if (pul->period < 0x800 && ++pul->count == 0x800) {
		pul->count = pul->period;
		pul->sample = (pul->sample + 1) & 7;
	}
}

int gb_sound_get_pulse_sample(gb_sound_pulse_t * pul) {
	int is_low = 0;
	switch (pul->duty) {
		case 0:
			is_low = pul->sample < 1;
			break;
		case 1:
			is_low = pul->sample < 2;
			break;
		case 2:
			is_low = pul->sample < 4;
			break;
		case 3:
			is_low = pul->sample < 6;
			break;
	}
	return is_low ? 0 : pul->volume;
}







/////////////////////////////////////////////////////////////////////////
// wave channel


void gb_sound_init_wave(gb_sound_wave_t * wav) {
	memset(wav, 0, sizeof(*wav));
	wav->retrig_time = -1u;
}

void gb_sound_clock_wave(gb_sound_wave_t * wav) {
	if (++wav->count == 0x800) {
		wav->count = wav->period;
		wav->index = (wav->index + 1) & 63;
	}
}

int gb_sound_get_wave_sample(gb_sound_wave_t * wav) {
	unsigned w = wav->wave_ram[wav->index];
	switch (wav->volume) {
		case 0:
			w = 0;
			break;
		case 2:
			w >>= 1;
			break;
		case 3:
			w >>= 2;
			break;
		case 4:
		case 5:
		case 6:
		case 7:
			w = (w * 3) / 4;
			break;
	}
	return w;
}






//////////////////////////////////////////////////////////////////
// noise channel


void gb_sound_init_noise(gb_sound_noise_t * noi) {
	memset(noi, 0, sizeof(*noi));
	noi->time = 1;
	noi->count = 1;
	noi->lfsr = 0x4000;
	noi->retrig_time = -1u;
}

void gb_sound_clock_noise(gb_sound_noise_t * noi) {
	if (!(--noi->count)) {
		noi->count = noi->time;
		unsigned b = noi->lfsr & 1;
		noi->lfsr >>= 1;
		if (b) {
			noi->lfsr ^= noi->mode ? 0x60 : 0x6000;
		}
	}
}

int gb_sound_get_noise_sample(gb_sound_noise_t * noi) {
	return (noi->lfsr & 1) ? noi->volume : 0;
}







///////////////////////////////////////////////////////////////////////////////
// global control


void gb_sound_init(gb_sound_t * gbs) {
	gbs->cyc = 0;
	gbs->panning = 0xff;
	
	gb_sound_init_pulse(&gbs->pulse_1);
	gb_sound_init_sweep(&gbs->pulse_1_sweep, &gbs->pulse_1.period);
	gb_sound_init_envelope(&gbs->pulse_1_envelope, &gbs->pulse_1.volume);
	gb_sound_init_length_counter(&gbs->pulse_1_length_counter, 64);
	
	gb_sound_init_pulse(&gbs->pulse_2);
	gb_sound_init_envelope(&gbs->pulse_2_envelope, &gbs->pulse_2.volume);
	gb_sound_init_length_counter(&gbs->pulse_2_length_counter, 64);
	
	gb_sound_init_wave(&gbs->wave);
	gb_sound_init_length_counter(&gbs->wave_length_counter, 256);
	
	gb_sound_init_noise(&gbs->noise);
	gb_sound_init_envelope(&gbs->noise_envelope, &gbs->noise.volume);
	gb_sound_init_length_counter(&gbs->noise_length_counter, 64);
}



void gb_sound_clock(gb_sound_t * gbs) {
	if (!(gbs->cyc % GB_SWEEP_CLOCK_DIVIDER)) {
		gb_sound_clock_sweep(&gbs->pulse_1_sweep);
	}
	
	if (!(gbs->cyc % GB_ENVELOPE_CLOCK_DIVIDER)) {
		gb_sound_clock_envelope(&gbs->pulse_1_envelope);
		gb_sound_clock_envelope(&gbs->pulse_2_envelope);
		gb_sound_clock_envelope(&gbs->noise_envelope);
	}
	
	if (!(gbs->cyc % GB_LENGTH_COUNTER_CLOCK_DIVIDER)) {
		gb_sound_clock_length_counter(&gbs->pulse_1_length_counter);
		gb_sound_clock_length_counter(&gbs->pulse_2_length_counter);
		gb_sound_clock_length_counter(&gbs->wave_length_counter);
		gb_sound_clock_length_counter(&gbs->noise_length_counter);
	}
	
	if (!(gbs->cyc % GB_PULSE_CLOCK_DIVIDER)) {
		gb_sound_clock_pulse(&gbs->pulse_1);
		gb_sound_clock_pulse(&gbs->pulse_2);
	}
	
	if (!(gbs->cyc % GB_WAVE_CLOCK_DIVIDER)) {
		gb_sound_clock_wave(&gbs->wave);
	}
	
	if (!(gbs->cyc % GB_NOISE_CLOCK_DIVIDER)) {
		gb_sound_clock_noise(&gbs->noise);
	}
	
	gbs->cyc++;
}





int gb_sound_pulse_1_is_enabled(gb_sound_t * gbs) {
	return (gbs->pulse_1_envelope.nrx2 & 0xf8) && !gb_sound_length_counter_is_over(&gbs->pulse_1_length_counter) && gbs->pulse_1.period < 0x800;
}

int gb_sound_pulse_2_is_enabled(gb_sound_t * gbs) {
	return (gbs->pulse_2_envelope.nrx2 & 0xf8) && !gb_sound_length_counter_is_over(&gbs->pulse_2_length_counter);
}

int gb_sound_wave_is_enabled(gb_sound_t * gbs) {
	return (gbs->wave.enable) && !gb_sound_length_counter_is_over(&gbs->wave_length_counter);
}

int gb_sound_noise_is_enabled(gb_sound_t * gbs) {
	return (gbs->noise_envelope.nrx2 & 0xf8) && !gb_sound_length_counter_is_over(&gbs->noise_length_counter);
}





int gb_sound_get_sample(gb_sound_t * gbs, int * left, int * right) {
	int sample = -0x20;
	if (left)
		*left = -0x20;
	if (right)
		*right = -0x20;
	
	if (gb_sound_pulse_1_is_enabled(gbs)) {
		int s = gb_sound_get_pulse_sample(&gbs->pulse_1);
		if (gbs->panning & 0x11)
			sample += s;
		if (left && (gbs->panning & 0x10))
			*left += s;
		if (right && (gbs->panning & 0x01))
			*right += s;
	} else {
		
	}
	
	if (gb_sound_pulse_2_is_enabled(gbs)) {
		int s = gb_sound_get_pulse_sample(&gbs->pulse_2);
		if (gbs->panning & 0x22)
			sample += s;
		if (left && (gbs->panning & 0x20))
			*left += s;
		if (right && (gbs->panning & 0x02))
			*right += s;
	} else {
		
	}
	
	if (gb_sound_wave_is_enabled(gbs)) {
		int s = gb_sound_get_wave_sample(&gbs->wave);
		if (gbs->panning & 0x44)
			sample += s;
		if (left && (gbs->panning & 0x40))
			*left += s;
		if (right && (gbs->panning & 0x04))
			*right += s;
	} else {
		
	}
	
	if (gb_sound_noise_is_enabled(gbs)) {
		int s = gb_sound_get_noise_sample(&gbs->noise);
		if (gbs->panning & 0x88)
			sample += s;
		if (left && (gbs->panning & 0x80))
			*left += s;
		if (right && (gbs->panning & 0x08))
			*right += s;
	} else {
		
	}
	
	return sample;
}




void gb_sound_write(gb_sound_t * gbs, unsigned reg, unsigned v) {
	switch (reg) {
		// pulse 1
		case 0x60:
			gb_sound_reload_sweep(&gbs->pulse_1_sweep, v & 0xff);
			break;
		case 0x62:
			gb_sound_reload_length_counter(&gbs->pulse_1_length_counter, v & 0x3f);
			gbs->pulse_1.duty = (v & 0xc0) >> 6;
			gb_sound_reload_envelope(&gbs->pulse_1_envelope, (v & 0xff00) >> 8);
			break;
		case 0x64:
			gbs->pulse_1.period = v & 0x7ff;
			gbs->pulse_1_length_counter.enable = (v & 0x4000) >> 14;
			if (v & 0x8000) {
				gb_sound_retrig_sweep(&gbs->pulse_1_sweep);
				gb_sound_retrig_envelope(&gbs->pulse_1_envelope);
				gb_sound_retrig_length_counter(&gbs->pulse_1_length_counter);
				gbs->pulse_1.retrig_time = 0;
			}
			break;
			
		// pulse 2
		case 0x68:
			gb_sound_reload_length_counter(&gbs->pulse_2_length_counter, v & 0x3f);
			gbs->pulse_2.duty = (v & 0xc0) >> 6;
			gb_sound_reload_envelope(&gbs->pulse_2_envelope, (v & 0xff00) >> 8);
			break;
		case 0x6c:
			gbs->pulse_2.period = v & 0x7ff;
			gbs->pulse_2_length_counter.enable = (v & 0x4000) >> 14;
			if (v & 0x8000) {
				gb_sound_retrig_envelope(&gbs->pulse_2_envelope);
				gb_sound_retrig_length_counter(&gbs->pulse_2_length_counter);
				gbs->pulse_2.retrig_time = 0;
			}
			break;
			
		// wave
		case 0x70:
			gbs->wave.enable = v & 0x80;
			break;
		case 0x72:
			gb_sound_reload_length_counter(&gbs->wave_length_counter, v & 0xff);
			gbs->wave.volume = (v & 0xe000) >> 13;
			break;
		case 0x74:
			gbs->wave.period = v & 0x7ff;
			gbs->wave_length_counter.enable = (v & 0x4000) >> 14;
			if (v & 0x8000) {
				gb_sound_retrig_length_counter(&gbs->wave_length_counter);
				gbs->wave.retrig_time = 0;
			}
			break;
			
		// noise
		case 0x78:
			gb_sound_reload_length_counter(&gbs->noise_length_counter, v & 0x3f);
			gb_sound_reload_envelope(&gbs->noise_envelope, (v & 0xff00) >> 8);
			break;
		case 0x7c:
		{
			unsigned divider = v & 7;
			unsigned shift = (v & 0xf0) >> 4;
			gbs->noise.time = (divider == 0) ? (1u << shift) : (divider << (shift + 1));
			gbs->noise.mode = v & 8;
			gbs->noise_length_counter.enable = (v & 0x4000) >> 14;
			if (v & 0x8000) {
				gb_sound_retrig_envelope(&gbs->noise_envelope);
				gb_sound_retrig_length_counter(&gbs->noise_length_counter);
				gbs->noise.lfsr = gbs->noise.mode ? 0x40 : 0x4000;
				gbs->noise.retrig_time = 0;
			}
			break;
		}
			
			// invalid
		default:
			printf("invalid GB sound write $%04X to register $%02X\n", v, reg);
			break;
	}
}

