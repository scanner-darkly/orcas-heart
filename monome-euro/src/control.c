// -----------------------------------------------------------------------------
// controller - the glue between the engine and the hardware
//
// reacts to events (grid press, clock etc) and translates them into appropriate
// engine actions. reacts to engine updates and translates them into user 
// interface and hardware updates (grid LEDs, CV outputs etc)
//
// should talk to hardware via what's defined in interface.h only
// should talk to the engine via what's defined in engine.h only
// ----------------------------------------------------------------------------

#include "compiler.h"
#include "string.h"

#include "control.h"
#include "interface.h"
#include "engine.h"

preset_meta_t meta;
preset_data_t preset;
shared_data_t shared;
u8 selected_preset;

// ----------------------------------------------------------------------------
// firmware dependent stuff starts here

#define PARAMCYCLE 4

#define PARAMTIMER 0
#define CLOCKTIMER 1

#define PAGE_MAIN 0
#define PAGE_MATRIX 1

#define PARAM_LEN 0
#define PARAM_ALGOX 1
#define PARAM_ALGOY 2
#define PARAM_SHIFT 3
#define PARAM_SPACE 4
#define PARAM_TRANS 5
#define PARAM_GATEL 6


u16 speed, gateLength, transpose;
u8 scaleButtons[SCALECOUNT][SCALELEN] = {};
u8 scaleAOctave, scaleBOctave;

u8 page = PAGE_MAIN;
u8 param = PARAM_LEN;
u8 matrix;

engine_config_t config;

static void updateParameters(void);
static void step(void);
static void updateNotes(void);
static void updateMods(void);
static void updateDisplay(void);
static void process_grid_press(u8 x, u8 y, u8 on);
static void render_main_page(void);
static char* itoa(int value, char* result, int base);


// ----------------------------------------------------------------------------
// functions for main.c

void init_presets(void) {
    // called by main.c if there are no presets saved to flash yet
    // initialize meta - some meta data to be associated with a preset, like a glyph
    // initialize shared (any data that should be shared by all presets) with default values
    // initialize preset with default values 
    // store them to flash
    
    for (u8 i = 0; i < get_preset_count(); i++) {
        store_preset_to_flash(i, &meta, &preset);
    }

    store_shared_data_to_flash(&shared);
    store_preset_index(0);
}

void init_control(void) {
    // load shared data
    // load current preset and its meta data
    
    load_shared_data_from_flash(&shared);
    selected_preset = get_preset_index();
    load_preset_from_flash(selected_preset, &preset);
    load_preset_meta_from_flash(selected_preset, &meta);

    // TODO read values from preset
    config.length = 8;
    config.algoX = 12;
    config.algoY = 12;
    config.shift = 0;
    config.space = 0;
    initEngine(&config);
    // TODO update scale buttons from preset
    updateScales(scaleButtons);
    
    // set up any other initial values and timers
    set_as_i2c_leader();
    set_jf_mode(1);
    for (u8 i = 0; i < NOTECOUNT; i++) map_voice(i, VOICE_JF, i, 1);
    
    refresh_grid();

    add_timed_event(PARAMTIMER, PARAMCYCLE, 1);
    add_timed_event(CLOCKTIMER, 100, 1);
}

void process_event(u8 event, u8 *data, u8 length) {
    switch (event) {
        case MAIN_CLOCK_RECEIVED:
            break;
        
        case MAIN_CLOCK_SWITCHED:
            break;
    
        case GATE_RECEIVED:
            break;
        
        case GRID_CONNECTED:
            break;
        
        case GRID_KEY_PRESSED:
            process_grid_press(data[0], data[1], data[2]);
            break;
    
        case GRID_KEY_HELD:
            break;
            
        case ARC_ENCODER_COARSE:
            break;
    
        case FRONT_BUTTON_PRESSED:
            break;
    
        case FRONT_BUTTON_HELD:
            break;
    
        case BUTTON_PRESSED:
            break;
    
        case I2C_RECEIVED:
            break;
            
        case TIMED_EVENT:
            if (data[0] == PARAMTIMER) updateParameters();
            else if (data[0] == CLOCKTIMER) step();
            break;
        
        case MIDI_CONNECTED:
            break;
        
        case MIDI_NOTE:
            break;
        
        case MIDI_CC:
            break;
            
        case MIDI_AFTERTOUCH:
            break;
            
        case SHNTH_BAR:
            break;
            
        case SHNTH_ANTENNA:
            break;
            
        case SHNTH_BUTTON:
            break;
            
        default:
            break;
    }
}


// ----------------------------------------------------------------------------
// controller

void updateParameters() {
    u32 knob = get_knob_value(0);
    u32 newSpeed = 60000 / (((knob * 1980) >> 16) + 20);
    
    if (newSpeed != speed) {
        speed = newSpeed;
        update_timer_interval(CLOCKTIMER, speed);
    }

    updateDisplay();
}

void step() {
    clock();
    updateNotes();
    updateMods();
    refresh_grid();
}

void updateNotes(void) {
    // TODO implement transpose
    u8 trans = 20;
    u8 scale = getCurrentScale();
    if (!scale && scaleAOctave) trans += 12;
    else if (scale && scaleBOctave) trans += 12;

    for (u8 n = 0; n < NOTECOUNT; n++) {
        // TODO implement gate length
        note(n, getNote(n) + trans, 10000, getGate(n));
    }
}

void updateMods(void) {
    // TODO
}

void updateDisplay() {
    clear_screen();
    draw_str("ORCA'S HEART", 0, 15, 0);
    
    // TODO format better

    char s[8];

    itoa(config.length, s, 10);
    draw_str(s, 2, 9, 0);
    itoa(config.algoX, s, 10);
    draw_str(s, 3, 9, 0);
    itoa(config.algoY, s, 10);
    draw_str(s, 4, 9, 0);
    itoa(config.shift, s, 10);
    draw_str(s, 5, 9, 0);
    
    refresh_screen();
}

void process_grid_press(u8 x, u8 y, u8 on) {
    if (!on) return;
    
    if (x == 0 && y == 6) setCurrentScale(0);
    else if (x == 0 && y == 7) setCurrentScale(1);
    else if (x == 15 && y == 6) scaleAOctave = !scaleAOctave;
    else if (x == 15 && y == 7) scaleBOctave = !scaleBOctave;
    else if (y > 5 && y < 8 && x > 1 && x < 14) {
        scaleButtons[y - 6][x - 2] = !scaleButtons[y - 6][x - 2];
        updateScales(scaleButtons);
    }
    else if (x == 0 && y == 0) param = PARAM_LEN;
    else if (x == 1 && y == 0) param = PARAM_ALGOX;
    else if (x == 1 && y == 1) param = PARAM_ALGOY;
    else if (x == 14 && y == 0) param = PARAM_SHIFT;
    else if (x == 14 && y == 1) param = PARAM_SPACE;
    else if (x == 15 && y == 0) param = PARAM_TRANS;
    else if (x == 15 && y == 1) param = PARAM_GATEL;
    else if (y > 2 && y < 5) {
        switch (param) {

            case PARAM_LEN:
                config.length = (y - 3) * 16 + x + 1;
                updateLength(config.length);
                break;
            
            case PARAM_ALGOX:
                if (y == 3 && x > 3 && x < 12) {
                    config.algoX = ((x - 4) << 4) + (config.algoX & 15);
                    updateAlgoX(config.algoX);
                } else if (y == 4) {
                    config.algoX = (config.algoX & 0b1110000) + x;
                    updateAlgoX(config.algoX);
                }
                break;
            
            case PARAM_ALGOY:
                if (y == 3 && x > 3 && x < 12) {
                    config.algoY = (x - 4) * 16 + (config.algoY & 15);
                    updateAlgoY(config.algoY);
                } else if (y == 4) {
                    config.algoY = (config.algoY & 0b1110000) + x;
                    updateAlgoY(config.algoY);
                }
                break;
            
            case PARAM_SHIFT:
                if (y == 3 && x > 1 && x < 15) {
                    config.shift = x - 2;
                    updateShift(config.shift);
                }
                break;
            
            case PARAM_SPACE:
                if (y == 3) {
                    config.space = x;
                    updateSpace(config.space);
                }
                break;
            
            case PARAM_TRANS:
                break;
            
            case PARAM_GATEL:
                break;
            
            default:
                break;
        }
    }
    
    refresh_grid();
}

void render_grid() {
    if (!is_grid_connected()) return;
    
    clear_all_grid_leds();
    if (page == PAGE_MAIN) render_main_page();
}

void render_main_page() {
    u8 on = 12, off = 5, y, l;
    
    set_grid_led(0, 6, off);
    set_grid_led(0, 7, off);
    set_grid_led(0, getCurrentScale() ? 7 : 6, on);
    
    set_grid_led(15, 6, scaleAOctave ? on : off);
    set_grid_led(15, 7, scaleBOctave ? on : off);

    set_grid_led(15, 0, getScaleCount(0) > 1 ? 15 : 8);
    set_grid_led(15, 1, getScaleCount(1) > 1 ? 15 : 8);
    
    for (u8 i = 0; i < SCALECOUNT; i++) {
        y = 8 - SCALECOUNT + i;
        for (u8 j = 0; j < SCALELEN; j++) set_grid_led(2 + j, y, scaleButtons[i][j] ? on : off);
    }
    
    set_grid_led(0, 0, off);
    set_grid_led(1, 0, off);
    set_grid_led(0, 1, off);
    set_grid_led(1, 1, off);
    set_grid_led(14, 0, off);
    set_grid_led(15, 0, off);
    set_grid_led(14, 1, off);
    set_grid_led(15, 1, off);
    
    switch (param) {
        
        case PARAM_LEN:
            set_grid_led(0, 0, on);
            for (u8 x = 0; x < 16; x++) for (u8 y = 3; y < 4; y++) set_grid_led(x, y, off);
            y = 3;
            l = config.length;
            if (config.length > 16) {
                for (u8 i = 0; i < 16; i++) set_grid_led(i, 3, on);
                y = 4;
                l -=16;
            }
            for (u8 i = 0; i < l; i++) set_grid_led(i, y, on);
            break;
        
        case PARAM_ALGOX:
            set_grid_led(1, 0, on);
            l = (config.algoX >> 4);
            for (u8 i = 0; i < 8; i++) set_grid_led(i + 4, 3, i == l ? on : off);
            l = (config.algoX & 15);
            for (u8 i = 0; i < 16; i++) set_grid_led(i, 4, i == l ? on : off);
            break;
        
        case PARAM_ALGOY:
            set_grid_led(1, 1, on);
            l = config.algoY >> 4;
            for (u8 i = 0; i < 8; i++) set_grid_led(i + 4, 3, i == l ? on : off);
            l = (config.algoY & 15);
            for (u8 i = 0; i < 16; i++) set_grid_led(i, 4, i == l ? on : off);
            break;
        
        case PARAM_SHIFT:
            set_grid_led(14, 0, on);
            for (u8 x = 0; x < 13; x++) set_grid_led(x + 2, 3, x == config.shift ? on : off);
            break;
        
        case PARAM_SPACE:
            set_grid_led(14, 1, on);
            for (u8 x = 0; x < 16; x++) set_grid_led(x, 3, x == config.space ? on : off);
            break;
        
        case PARAM_TRANS:
            set_grid_led(15, 0, on);
            break;
        
        case PARAM_GATEL:
            set_grid_led(15, 1, on);
            break;
        
        default:
            break;
    }
}

void render_arc() { }

// http://www.jb.man.ac.uk/~slowe/cpp/itoa.html
// http://embeddedgurus.com/stack-overflow/2009/06/division-of-integers-by-constants/
// http://codereview.blogspot.com/2009/06/division-of-integers-by-constants.html
// http://homepage.cs.uiowa.edu/~jones/bcd/divide.html
/**
	 * C++ version 0.4 char* style "itoa":
	 * Written by Lukás Chmela
	 * Released under GPLv3.
	 */
char* itoa(int value, char* result, int base) {
	// check that the base if valid
	// removed for optimization
	// if (base < 2 || base > 36) { *result = '\0'; return result; }

	char* ptr = result, *ptr1 = result, tmp_char;
	int tmp_value;
	uint8_t inv = 0;

	// add: opt crashes on negatives
	if(value<0) {
		value = -value;
		inv++;
	}

	do {
		tmp_value = value;
		// opt-hack for base 10 assumed
		// value = (((uint16_t)value * (uint16_t)0xCD) >> 8) >> 3;
		value = (((uint32_t)value * (uint32_t)0xCCCD) >> 16) >> 3;
		// value /= base;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
	} while ( value );

	// Apply negative sign
	if(inv) *ptr++ = '-';
	*ptr-- = '\0';
	while(ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr--= *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}

/*
transpose
gate length
presets
i2c config
switch outputs notes/mod cvs
clock input
clock / reset outputs
visualize values

speed for ansible
txi / teletype cv input
additional gate inputs on ansible/teletype
*/