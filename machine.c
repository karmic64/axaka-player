
#include <string.h>
#include <limits.h>

#include "machine.h"



// init machine
void machine_init(machine_t * m, unsigned ready_event) {
	axaka_sound_init(&m->as);
	gb_sound_init(&m->gbs);
	
	axaka_sequencer_init(&m->asq, &m->as, &m->gbs);
	
	m->frame = 0;
	m->cyc = 0;
	m->frame_cyc = CYCLES_PER_FRAME;
	m->axaka_sample_cyc = AXAKA_CLOCK_DIVIDER;
	
	memset(&m->audio_buffer_left, 0, sizeof(m->audio_buffer_left));
	memset(&m->audio_buffer_right, 0, sizeof(m->audio_buffer_right));
	m->audio_buffer_read_index = 0;
	m->audio_buffer_write_index = 0;
	m->audio_buffer_queue_size = 0;
	
	m->ready_event = ready_event;
}



// run machine for one clock, possibly returning a sample
int machine_clock(machine_t * m, int * left, int * right) {
	// run sequencer if needed
	if (++m->frame_cyc >= CYCLES_PER_FRAME) {
		m->frame_cyc = 0;
		
		if (m->gbs.pulse_1.retrig_time != -1u)
			m->gbs.pulse_1.retrig_time++;
		if (m->gbs.pulse_2.retrig_time != -1u)
			m->gbs.pulse_2.retrig_time++;
		if (m->gbs.wave.retrig_time != -1u)
			m->gbs.wave.retrig_time++;
		if (m->gbs.noise.retrig_time != -1u)
			m->gbs.noise.retrig_time++;
		
		axaka_sequencer_clock(&m->asq);
		
		m->frame++;
	}
	
	// clock sound units
	if (!(m->cyc % GB_SOUND_CLOCK_DIVIDER)) {
		gb_sound_clock(&m->gbs);
	}
	
	if (++m->axaka_sample_cyc >= AXAKA_CLOCK_DIVIDER) {
		m->axaka_sample_cyc = 0;
		axaka_sound_clock(&m->as);
	}
	
	// get sample (if it's time)
	int returned_sample = 0;;
	if (!(m->cyc % SAMPLE_CLOCK_DIVIDER)) {
		// get samples direct from sound units
		int gb_left;
		int gb_right;
		int gb_sample = gb_sound_get_sample(&m->gbs, &gb_left, &gb_right);
		int axaka_sample = axaka_sound_get_sample(&m->as);
		
		// on GBA:
		//	the full possible signed output range is ±$200
		//	one GB channel ranges ±$80, there are 4, therefore total GB sound ranges ±$200
		//	one FIFO channel ranges ±$200  (not sure how the original does its mixing)
		// for us:
		//	the full possible signed output range is ±$8000
		//	one GB channel ranges ±$08, total GB sound ranges ±$20
		//	one axaka channel ranges ±$80*$100=$8000, there are 6, therefore total ranges ±$30000
		//		(samples themselves range ±80, there are $100 normal volume levels)
		
		// this is a guess that sounds about right
		gb_sample *= (0x2000 / 0x20);
		gb_left *= (0x2000 / 0x20);
		gb_right *= (0x2000 / 0x20);
		axaka_sample /= (0x30000 / 0x10000);
		
		if (left)
			*left = gb_left + axaka_sample;
		if (right)
			*right = gb_right + axaka_sample;
		
		returned_sample = 1;
	}
	
	//
	m->cyc++;
	
	return returned_sample;
}



// queue more audio until it's full again
void machine_fill_audio_buffer(machine_t * m) {
	while (m->audio_buffer_queue_size < AUDIO_BUFFER_SIZE) {
		int l;
		int r;
		if (machine_clock(m, &l, &r)) {
			m->audio_buffer_left[m->audio_buffer_write_index] = l;
			m->audio_buffer_right[m->audio_buffer_write_index] = r;
			m->audio_buffer_write_index++;
			m->audio_buffer_write_index %= AUDIO_BUFFER_SIZE;
			
			m->audio_buffer_queue_size++;
		}
	}
}

// dump all audio to the requested buffer
void machine_dump_audio_buffer(machine_t * m, int16_t * dest, unsigned samples) {
	for (unsigned i = 0; i < samples; i++) {
		int l;
		int r;
		if (m->audio_buffer_queue_size) {
			// write data
			l = m->audio_buffer_left[m->audio_buffer_read_index];
			r = m->audio_buffer_right[m->audio_buffer_read_index];
			m->audio_buffer_read_index++;
			m->audio_buffer_read_index %= AUDIO_BUFFER_SIZE;
			
			m->audio_buffer_queue_size--;
		} else {
			// we have no more data
			l = m->audio_buffer_left[(m->audio_buffer_read_index - 1) % AUDIO_BUFFER_SIZE];
			r = m->audio_buffer_right[(m->audio_buffer_read_index - 1) % AUDIO_BUFFER_SIZE];
		}
		dest[i*2] = l;
		dest[i*2 + 1] = r;
	}
}


