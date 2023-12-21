
#include <string.h>

#include "axaka-sound.h"


// init axaka sound unit
void axaka_sound_init(axaka_sound_t * as) {
	memset(as, 0, sizeof(*as));
	for (unsigned i = 0; i < AXAKA_CHANNELS; i++) {
		axaka_channel_t * ch = &as->ch[i];
		ch->sample_offset = -1u;
	}
}


// run sound unit for one clock
void axaka_sound_clock(axaka_sound_t * as) {
	for (unsigned i = 0; i < AXAKA_CHANNELS; i++) {
		axaka_channel_t * ch = &as->ch[i];
		if (ch->instrument && ch->sample_offset != -1u) {
			unsigned o = ch->offset_accum;
			o += ch->rate;
			ch->sample_offset += (o >> 16);
			ch->offset_accum += ch->rate;
			
			unsigned l = ch->instrument->length;
			unsigned ll = ch->instrument->loop_length;
			
			if (ch->sample_offset >= l) {
				if (ll) {
					ch->sample_offset -= ll;
				} else {
					ch->sample_offset = -1u;
				}
			}
		}
	}
}


// get currently output sample
int axaka_sound_get_sample(axaka_sound_t * as) {
	int o = 0;
	for (unsigned i = 0; i < AXAKA_CHANNELS; i++) {
		axaka_channel_t * ch = &as->ch[i];
		if (ch->instrument && ch->sample_offset != -1u) {
			o += ch->instrument->data[ch->sample_offset] * ch->volume;
		}
	}
	return o;
}

