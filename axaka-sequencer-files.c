
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "axaka-sequencer-files.h"



int fget32(FILE * f) {
	int v = 0;
	for (unsigned b = 0; b < 32; b += 8) {
		v |= fgetc(f) << b;
	}
	return v;
}




int strendcmp(const char * str, const char * end) {
	size_t str_len = strlen(str);
	size_t end_len = strlen(end);
	
	if (str_len < end_len) {
		return 1;
	}
	
	return memcmp(str + str_len - end_len, end, end_len);
}





const char * axaka_sequencer_load_instrument_list(axaka_sequencer_t * asq, const char * filename) {
	FILE * f = fopen(filename, "rb");
	if (!f) {
		return strerror(errno);
	}
	
	// first check the file contents are sane
	fseek(f, 0, SEEK_END);
	size_t actual_size = ftell(f);
	rewind(f);
	size_t reported_size = fget32(f);
	if (actual_size != reported_size + 4) {
		fclose(f);
		return "Incorrect file size";
	}
	
	unsigned toc[AXAKA_MAX_INSTRUMENTS][4];
	toc[0][0] = fget32(f);
	unsigned max = toc[0][0] / 0x10;
	if (toc[0][0] == 0 || toc[0][0] % 0x10 || max > AXAKA_MAX_INSTRUMENTS) {
		fclose(f);
		return "Invalid index to first sample";
	}
	
	for (unsigned i = 0; i < max; i++) {
		for (unsigned j = i ? 0 : 1; j < 4; j++) {
			toc[i][j] = fget32(f);
		}
	}
	
	for (unsigned i = 0; i < max; i++) {
		unsigned offs = toc[i][0];
		unsigned size = toc[i][1];
		unsigned loop_size = toc[i][2];
		unsigned next_offs = i < max-1 ? toc[i+1][0] : reported_size;
		unsigned max_size = next_offs - offs;
		
		if (offs >= reported_size || next_offs > reported_size || size > max_size || size < loop_size) {
			fclose(f);
			return "Bad sample definition";
		}
	}
	
	// file seems ok, commit to setting the instrument list
	axaka_sequencer_free_instrument_list(asq);
	
	axaka_instrument_t * list = malloc(max * sizeof(*list));
	for (unsigned i = 0; i < max; i++) {
		unsigned offs = toc[i][0];
		unsigned size = toc[i][1];
		unsigned loop_size = toc[i][2];
		unsigned rate = toc[i][3];
		
		// there are lots of zero-size samples in cross channel
		list[i].data = size ? malloc(size) : NULL;
		list[i].length = size;
		list[i].loop_length = loop_size;
		list[i].rate = rate;
		
		fseek(f, offs + 4, SEEK_SET);
		fread(list[i].data, 1, size, f);
	}
	
	fclose(f);
	asq->max_instrument = max;
	asq->inst = list;
	
	size_t name_len = strlen(filename);
	char * name = malloc(name_len + 1);
	memcpy(name, filename, name_len);
	name[name_len] = '\0';
	asq->inst_name = name;
	
	return NULL;
}




const char * axaka_sequencer_load_music(axaka_sequencer_t * asq, const char * filename) {
	// don't check this to make hajirusu music playable
	//if (!asq->inst) {
	//	return "No instrument list loaded";
	//}
	
	FILE * f = fopen(filename, "rb");
	if (!f) {
		return strerror(errno);
	}
	
	// check the first 4 bytes (they are the same on all known .mus)
	if (fget32(f) != 0x0060000c) {
		fclose(f);
		return "Bad header";
	}
	
	// load the file
	axaka_sequencer_free_song_data(asq);
	
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	rewind(f);
	asq->data_start = malloc(size);
	fread(asq->data_start, 1, size, f);
	fclose(f);
	
	size_t name_len = strlen(filename);
	char * name = malloc(name_len + 1);
	memcpy(name, filename, name_len);
	name[name_len] = '\0';
	asq->song_name = name;
	
	axaka_sequencer_play_music(asq);
	
	return NULL;
}




const char * axaka_sequencer_load_file(axaka_sequencer_t * asq, const char * filename) {
	if (!strendcmp(filename, ".inst"))
		return axaka_sequencer_load_instrument_list(asq, filename);
	else if (!strendcmp(filename, ".mus"))
		return axaka_sequencer_load_music(asq, filename);
	
	return "Unrecognized file extension (must be .inst or .mus)";
}



