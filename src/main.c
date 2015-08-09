#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

#include "common.h"
#include "sine.h"
#include "notefreq.h"

#define LED_PORT PORTE
#define SW_PORT  PORTD

#define N_SWITCHES 8

#define UI_HEARTBEAT_PERIOD 10

#define VOLUME_MAX 12
#define VOLUME_MIN  9
#define VOLUME_DEFAULT VOLUME_MAX

#define NOTE_MIN 0
#define NOTE_MAX (N_NOTES-1)
#define NOTE_DEFAULT NOTE_NONE

#define OCTAVE_MIN 0
#define OCTAVE_MAX (N_OCTAVES-1)
#define OCTAVE_DEFAULT 5

typedef struct {
	uint8_t min, off, on, max, accum;
	bool is_on;
} hyst_t;

typedef enum {
	UI_IDLE,
	UI_USER_RESET_SW_DOWN,
	UI_START_SW_DOWN, UI_STOP_SW_DOWN,
	UI_SCORE_GAP, UI_SCORE_NOTE,
	UI_VOLUME_DECR_SW_DOWN, UI_VOLUME_INCR_SW_DOWN,
	UI_OCTAVE_DECR_SW_DOWN, UI_OCTAVE_INCR_SW_DOWN,
	UI_NOTE_PREV_SW_DOWN, UI_NOTE_NEXT_SW_DOWN
} ui_state_t;
	
volatile hyst_t sw[N_SWITCHES] = {
	{ 0, 0, 2, 4, 0, false },
	{ 0, 0, 2, 4, 0, false },
	{ 0, 0, 2, 4, 0, false },
	{ 0, 0, 2, 4, 0, false },
	{ 0, 0, 2, 4, 0, false },
	{ 0, 0, 2, 4, 0, false },
	{ 0, 0, 2, 4, 0, false },
	{ 0, 0, 2, 4, 0, false }
};

const hyst_t SW_UNBOUND = { 0, 0, 1, 1, 0, false };

#define user_reset_sw  SW_UNBOUND
#define note_prev_sw   SW_UNBOUND
#define note_next_sw   SW_UNBOUND
#define octave_decr_sw sw[2]
#define octave_incr_sw sw[3]
#define volume_decr_sw sw[4]
#define volume_incr_sw sw[5]
#define startstop_sw   sw[0]

volatile bool flag_ui = false;

volatile ui_state_t ui_state = UI_IDLE;
volatile u16 ui_state_time = 0;

volatile u8 curr_volume = VOLUME_DEFAULT;
volatile u8 curr_octave = OCTAVE_DEFAULT;
volatile note_t curr_note = NOTE_DEFAULT;

volatile u16 curr_freq = 1;
volatile u16 curr_sample = 0;


// Hot Cross Buns:

#define SCORE_LEN 32
#define SCORE_NOTE_PERIOD   250
#define SCORE_NOTE_DURATION (SCORE_NOTE_PERIOD-50)

const note_t score[SCORE_LEN] = {
	NOTE_B, NOTE_NONE, NOTE_A, NOTE_NONE, NOTE_G, NOTE_NONE, NOTE_NONE, NOTE_NONE,
	NOTE_B, NOTE_NONE, NOTE_A, NOTE_NONE, NOTE_G, NOTE_NONE, NOTE_NONE, NOTE_NONE,
	NOTE_G, NOTE_G, NOTE_G, NOTE_G, NOTE_A, NOTE_A, NOTE_A, NOTE_A,
	NOTE_B, NOTE_NONE, NOTE_A, NOTE_NONE, NOTE_G, NOTE_NONE, NOTE_NONE, NOTE_NONE
};

volatile u8 score_pos = 0;

//-- FUNCTION PROTOTYPES ------------------------------------------------------

void sample_inputs(void);
void ui_transition(ui_state_t next_state);
void update_volume_display(void);
void update_curr_freq(void);
void user_reset(void);
void clear_note(void);
bool load_score_note(void);
void volume_decr(void);
void volume_incr(void);
void octave_decr(void);
void octave_incr(void);
void note_prev(void);
void note_next(void);

//-- FUNCTION DEFINITIONS -----------------------------------------------------

void sample_inputs() {
	u8 pins = SW_PORT.IN;
	for (u8 i = 0; i < N_SWITCHES; i++) {
		if (pins & (1<<i)) {
			if (sw[i].accum > sw[i].min)  sw[i].accum--;
			if (sw[i].accum == sw[i].off) sw[i].is_on = false;
		} else {
			if (sw[i].accum < sw[i].max)  sw[i].accum++;
			if (sw[i].accum == sw[i].on)  sw[i].is_on = true;
		}
	}
}

void ui_transition(ui_state_t next_state) {
	ui_state = next_state;
	ui_state_time = 0;
}

void update_volume_display() {
	LED_PORT.OUT = 0xFF << (curr_volume - VOLUME_MIN + 1);
}

void update_curr_freq() {
	curr_freq = notefreq(curr_note, curr_octave);
}

void user_reset() {
	cli();
	curr_note = NOTE_DEFAULT;
	curr_octave = OCTAVE_DEFAULT;
	curr_volume = VOLUME_DEFAULT;
	curr_sample = 0;
	update_curr_freq();
	update_volume_display();
	sei();
}

void clear_note() {
	curr_note = NOTE_NONE;
	update_curr_freq();
}

bool load_score_note() {
	if (score_pos < SCORE_LEN) {
		curr_note = score[score_pos];
		score_pos++;
		return true;
	} else {
		curr_note = NOTE_NONE;
		return false;
	}
}

void volume_decr() {
	if (curr_volume > VOLUME_MIN) curr_volume--;
	update_volume_display();
}

void volume_incr() {
	if (curr_volume < VOLUME_MAX) curr_volume++;
	update_volume_display();
}

void octave_decr() {
	if (curr_octave > OCTAVE_MIN) curr_octave--;
	update_curr_freq();
}

void octave_incr() {
	if (curr_octave < OCTAVE_MAX) curr_octave++;
	update_curr_freq();
}

void note_prev() {
	if (curr_note > NOTE_MIN) curr_note--;
	update_curr_freq();
}

void note_next() {
	if (curr_note < NOTE_MAX) curr_note++;
	update_curr_freq();
}

//-- SIGNALS & INTERRUPT SERVICE ROUTINES -------------------------------------

ISR(TCC0_OVF_vect) {
	DACB.CH0DATA = sine(curr_sample, VOLUME_MAX - curr_volume);
	curr_sample += curr_freq;
	curr_sample %= SINE_N_SAMPLES;
}

ISR(TCC1_OVF_vect) {
	flag_ui = true;
	ui_state_time += UI_HEARTBEAT_PERIOD;
}

//-----------------------------------------------------------------------------

int main(void) {
	// configure system clock for 32MHz
	// -- enable 32MHz oscillator
	CCP = CCP_IOREG_gc;
	OSC.CTRL |= OSC_RC32MEN_bm;
	// -- wait for oscillator to stabilize
	while ( ! (OSC.STATUS & OSC_RC32MRDY_bm) )
		;
	// -- switch system clock
	CCP = CCP_IOREG_gc;
	CLK.CTRL = CLK_SCLKSEL_RC32M_gc;
	
	// configure Timer0
	// -- select CLK/1
	TCC0.CTRLA |= TC0_CLKSEL0_bm;
	// -- enable overflow int, priority: HIGH
	TCC0.INTCTRLA |= TC0_OVFINTLVL1_bm | TC0_OVFINTLVL0_bm;
	// -- set period to 800: 40KHz for 32MHz CLK
	TCC0.PER = 800-1;
	
	// configure Timer1
	// -- select CLK/64
	TCC1.CTRLA |= TC1_CLKSEL2_bm | TC1_CLKSEL0_bm;
	// -- enable overflow int, priority: LOW
	TCC1.INTCTRLA |= TC1_OVFINTLVL0_bm;
	// -- set period to 5000: 100Hz for 32MHz CLK/64
	TCC1.PER = 5000-1;
	
	// configure SW_PORT for input
	SW_PORT.DIRCLR = 0xFF;
	SW_PORT.PIN0CTRL |= PORT_OPC_WIREDANDPULL_gc;
	SW_PORT.PIN1CTRL |= PORT_OPC_WIREDANDPULL_gc;
	SW_PORT.PIN2CTRL |= PORT_OPC_WIREDANDPULL_gc;
	SW_PORT.PIN3CTRL |= PORT_OPC_WIREDANDPULL_gc;
	SW_PORT.PIN4CTRL |= PORT_OPC_WIREDANDPULL_gc;
	SW_PORT.PIN5CTRL |= PORT_OPC_WIREDANDPULL_gc;
	SW_PORT.PIN6CTRL |= PORT_OPC_WIREDANDPULL_gc;
	SW_PORT.PIN7CTRL |= PORT_OPC_WIREDANDPULL_gc;
	
	// configure LED_PORT for output (LEDs)
	LED_PORT.DIRSET = 0xFF;
	LED_PORT.OUTSET = 0xFF;
	update_volume_display();
	
	// configure DAC & speaker
	// -- enable channel 0 output
	DACB.CTRLA |= DAC_CH0EN_bm;
	// -- load the factory calibration values
	DACB.OFFSETCAL = 0x28;
	DACB.GAINCAL = 0x1A;
	// -- enable the DAC
	DACB.CTRLA |= DAC_ENABLE_bm;
	// -- configure PB2 for output (amp & speaker)
	PORTB.DIRSET = PIN2_bm;
	// -- enable speaker
	PORTQ.DIRSET = PIN3_bm;
	PORTQ.OUTSET = PIN3_bm;

	// enable all interrupt priorities (HIGH/MED/LOW)
	PMIC.CTRL |= PMIC_HILVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm;
	
	set_sleep_mode(SLEEP_MODE_IDLE);
	
	sei();
	update_curr_freq();
	for (;;) {
		sleep_mode();
		
		if (flag_ui) {
			flag_ui = false;
			
			sample_inputs();
			
			switch (ui_state) {
			case UI_IDLE:
				if (user_reset_sw.is_on) {
					ui_transition(UI_USER_RESET_SW_DOWN);
				} else if (startstop_sw.is_on) {
					ui_transition(UI_START_SW_DOWN);
				} else if (volume_decr_sw.is_on) {
					ui_transition(UI_VOLUME_DECR_SW_DOWN);
				} else if (volume_incr_sw.is_on) {
					ui_transition(UI_VOLUME_INCR_SW_DOWN);
				} else if (octave_decr_sw.is_on) {
					ui_transition(UI_OCTAVE_DECR_SW_DOWN);
				} else if (octave_incr_sw.is_on) {
					ui_transition(UI_OCTAVE_INCR_SW_DOWN);
				} else if (note_prev_sw.is_on) {
					ui_transition(UI_NOTE_PREV_SW_DOWN);
				} else if (note_next_sw.is_on) {
					ui_transition(UI_NOTE_NEXT_SW_DOWN);
				}
				break;
			case UI_USER_RESET_SW_DOWN:
				if (!user_reset_sw.is_on) {
					user_reset();
					ui_transition(UI_IDLE);
				}
				break;
			case UI_START_SW_DOWN:
				if (!startstop_sw.is_on) {
					score_pos = 0;
					ui_transition(UI_SCORE_GAP);
				}
				break;
			case UI_STOP_SW_DOWN:
				ui_transition(UI_IDLE);
				break;
			case UI_SCORE_GAP:
				if (ui_state_time > SCORE_NOTE_PERIOD - SCORE_NOTE_DURATION) {
					if (load_score_note()) {
						ui_transition(UI_SCORE_NOTE);
					} else {
						ui_transition(UI_IDLE);
					}
					update_curr_freq();
				}
				break;
			case UI_SCORE_NOTE:
				if (ui_state_time > SCORE_NOTE_DURATION) {
					clear_note();
					ui_transition(UI_SCORE_GAP);
				}
				break;
			case UI_VOLUME_DECR_SW_DOWN:
				if (!volume_decr_sw.is_on) {
					volume_decr();
					ui_transition(UI_IDLE);
				}
				break;
			case UI_VOLUME_INCR_SW_DOWN:
				if (!volume_incr_sw.is_on) {
					volume_incr();
					ui_transition(UI_IDLE);
				}
				break;
			case UI_OCTAVE_DECR_SW_DOWN:
				if (!octave_decr_sw.is_on) {
					octave_decr();
					ui_transition(UI_IDLE);
				}
				break;
			case UI_OCTAVE_INCR_SW_DOWN:
				if (!octave_incr_sw.is_on) {
					octave_incr();
					ui_transition(UI_IDLE);
				}
				break;
			case UI_NOTE_PREV_SW_DOWN:
				if (!note_prev_sw.is_on) {
					note_prev();
					ui_transition(UI_IDLE);
				}
				break;
			case UI_NOTE_NEXT_SW_DOWN:
				if (!note_next_sw.is_on) {
					note_next();
					ui_transition(UI_IDLE);
				}
				break;
			}
		}
	}
}