#ifndef SINE_H
#define SINE_H


#include <avr/pgmspace.h>
#include "common.h"

#define SINE_QUART1 10000
#define SINE_QUART2 20000
#define SINE_QUART3 30000
#define SINE_QUART4 40000

#define SINE_TABLE_SIZE SINE_QUART1
#define SINE_N_SAMPLES  SINE_QUART4

#define SINE_BASELINE 2048
#define SINE_AMPLITUDE 2047

u16 sine(u16 x, u8 downshift);

extern const u16 SINE[SINE_TABLE_SIZE] PROGMEM;


#endif