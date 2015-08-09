#ifndef NOTEFREQ_H
#define NOTEFREQ_H


#include <avr/pgmspace.h>
#include "common.h"

#define N_NOTES (12+1)
#define N_OCTAVES 10

typedef enum {
	NOTE_NONE,
	NOTE_C,
	NOTE_Cs,
	NOTE_D,
	NOTE_Ds,
	NOTE_E,
	NOTE_F,
	NOTE_Fs,
	NOTE_G,
	NOTE_Gs,
	NOTE_A,
	NOTE_As,
	NOTE_B
} note_t;

u16 notefreq(note_t note, u8 octave);

extern const u16 NOTEFREQ[N_NOTES][N_OCTAVES] PROGMEM;


#endif