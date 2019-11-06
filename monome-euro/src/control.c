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

#define MATRIXOUTS 12
#define MATRIXINS 8
#define MATRIXCOUNT 2
#define MATRIXMAXSTATE 1
#define MATRIXGATEWEIGHT 60

u16 speed, gateLength, transpose;
u32 speedMod;
u8 scaleButtons[SCALECOUNT][SCALELEN] = {};
u8 scaleAOctave, scaleBOctave;

u8 page = PAGE_MAIN;
u8 param = PARAM_LEN;
u8 matrix[MATRIXCOUNT][MATRIXINS][MATRIXOUTS];
s32 matrix_values[MATRIXOUTS];
u8 matrix_on[MATRIXCOUNT];
u8 matrix_inv[MATRIXCOUNT];
u8 mi;

engine_config_t config;

static void update_parameters(void);
static void step(void);

static void update_notes(void);
static void update_mods(void);

static void update_matrix(void);
static void update_display(void);

static void process_gate(u8 index, u8 on);
static void process_grid_press(u8 x, u8 y, u8 on);

static void process_grid_main(u8 x, u8 y, u8 on);
static void process_grid_matrix(u8 x, u8 y, u8 on);

static void render_main_page(void);
static void render_matrix_page(void);
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
    // TODO load matrix from preset
    for (u8 i = 0; i < MATRIXCOUNT; i++) matrix_on[i] = 1;
    
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
            process_gate(data[0], data[1]);
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
            if (data[0] == PARAMTIMER) update_parameters();
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

void update_parameters() {
    s32 sp = ((get_knob_value(0) * 1980) >> 16) + 20 + speedMod;
    if (sp > 2000) sp = 2000; else if (sp < 20) sp = 20;
    u32 newSpeed = 60000 / sp;
    
    if (newSpeed != speed) {
        speed = newSpeed;
        update_timer_interval(CLOCKTIMER, speed);
    }

    update_display();
}

void step() {
    clock();
    update_notes();
    update_mods();
    update_matrix();
    refresh_grid();
}

void update_notes(void) {
    // TODO implement transpose
    u8 trans = 8;
    u8 scale = getCurrentScale();
    if (!scale && scaleAOctave) trans += 12;
    else if (scale && scaleBOctave) trans += 12;

    for (u8 n = 0; n < NOTECOUNT; n++) {
        // TODO implement gate length
        note(n, getNote(n) + trans, 10000, getGate(n));
    }
}

void update_mods(void) {
    // TODO
}

void update_matrix(void) {
    u8 prevScale = matrix_values[7];
    u8 prevInv1 = matrix_values[10];
    u8 prevInv2 = matrix_values[11];
    
    s32 v;
    
    for (int m = 0; m < MATRIXOUTS; m++) {
        matrix_values[m] = 0;
        for (u8 i = 0; i < 4; i++) {
            if (matrix_on[0]) {
                v = getNote(i) * matrix[0][i][m] + getGate(i) * matrix[0][i + 4][m] * MATRIXGATEWEIGHT;
                if (matrix_inv[0]) v *= -1;
                matrix_values[m] += v;
            }
            if (matrix_on[1]) {
                v = getModCV(i) * matrix[1][i][m] * 12 + getModGate(i) * matrix[1][i + 4][m] * MATRIXGATEWEIGHT;
                if (matrix_inv[1]) v *= -1;
                matrix_values[m] += v;
            }
        }
    }
    
    // value * (max - min) / 240 + param
    
    v = (matrix_values[0] * 198) / (24 * MATRIXMAXSTATE);
    speedMod = v;
    
    v = (matrix_values[1] * 31) / (240 * MATRIXMAXSTATE) + config.length;
    if (v > 32) v = 32; else if (v < 1) v = 1;
    updateLength(v);
    
    v = (matrix_values[2] * 127) / (240 * MATRIXMAXSTATE) + config.algoX;
    if (v > 127) v = 127; else if (v < 0) v = 0;
    updateAlgoX(v);
    
    v = (matrix_values[3] * 127) / (240 * MATRIXMAXSTATE) + config.algoY;
    if (v > 127) v = 127; else if (v < 0) v = 0;
    updateAlgoY(v);
    
    v = matrix_values[4] / (20 * MATRIXMAXSTATE) + config.shift;
    if (v > 12) v = 12; else if (v < 0) v = 0;
    updateShift(v);
    
    v = (matrix_values[5] * 15) / (240 * MATRIXMAXSTATE) + config.space;
    if (v > 12) v = 12; else if (v < 0) v = 0;
    updateSpace(v);
    
    if (matrix_values[7] && !prevScale) {
        setCurrentScale((getCurrentScale() + 1) % SCALECOUNT);
        refresh_grid();
    }
    if (matrix_values[8] > 0) {
        scaleAOctave = 1;
        refresh_grid();
    }
    if (matrix_values[9] > 0) {
        scaleBOctave = 1;
        refresh_grid();
    }
    if (matrix_values[10] && !prevInv1) {
        matrix_inv[0] = !matrix_inv[0];
        refresh_grid();
    }
    if (matrix_values[11] && !prevInv2) {
        matrix_inv[1] = !matrix_inv[1];
        refresh_grid();
    }
}

void update_display() {
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

void process_gate(u8 index, u8 on) {
    switch (index) {
        case 0:
            reset();
            refresh_grid();
            break;
            
        case 1:
            setCurrentScale(getCurrentScale() ? 0 : 1);
            refresh_grid();
            break;
            
        case 2:
            scaleAOctave = !scaleAOctave;
            refresh_grid();
            break;
            
        case 3:
            scaleBOctave = !scaleBOctave;
            refresh_grid();
            break;
            
        default:
            break;
    }
}

void process_grid_press(u8 x, u8 y, u8 on) {
    if (x == 0 && y == 0) {
        if (on) page = page == PAGE_MAIN ? PAGE_MATRIX : PAGE_MAIN;
        refresh_grid();
        return;
    }
    
    if (page == PAGE_MAIN) process_grid_main(x, y, on);
    else if (page == PAGE_MATRIX) process_grid_matrix(x, y, on);
}
    
void render_grid() {
    if (!is_grid_connected()) return;
    
    clear_all_grid_leds();
    if (page == PAGE_MAIN) render_main_page();
    else if (page == PAGE_MATRIX) render_matrix_page();
}

void process_grid_main(u8 x, u8 y, u8 on) {
    if (!on) return;
    
    if (x == 0 && y == 6) setCurrentScale(0);
    else if (x == 0 && y == 7) setCurrentScale(1);
    else if (x == 15 && y == 6) scaleAOctave = !scaleAOctave;
    else if (x == 15 && y == 7) scaleBOctave = !scaleBOctave;
    else if (y > 5 && y < 8 && x > 1 && x < 14) {
        scaleButtons[y - 6][x - 2] = !scaleButtons[y - 6][x - 2];
        updateScales(scaleButtons);
    }
    else if (x == 0 && y == 1) param = PARAM_LEN;
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

void render_main_page() {
    u8 on = 15, prm = 8, off = 3, y, l;
    
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
    
    set_grid_led(0, 0, prm);
    set_grid_led(0, 1, prm);
    set_grid_led(1, 0, prm);
    set_grid_led(1, 1, prm);
    set_grid_led(14, 0, prm);
    set_grid_led(15, 0, prm);
    set_grid_led(14, 1, prm);
    set_grid_led(15, 1, prm);
    
    switch (param) {
        
        case PARAM_LEN:
            set_grid_led(0, 1, on);
            for (u8 x = 0; x < 16; x++) for (u8 y = 3; y < 5; y++) set_grid_led(x, y, off);
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
            for (u8 x = 0; x < 13; x++) set_grid_led(x + 2, 3, x == config.shift ? on : (x == getShift() ? off + 3 : off));
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

void process_grid_matrix(u8 x, u8 y, u8 on) {
    if (!on) return;
    
    if (x == 0 && y > 2 && y < 5) {
        mi = y - 3;
        refresh_grid();
        return;
    }
    
    if (y > 2 && y < 5 && x == 1) {
        matrix_on[y - 3] = !matrix_on[y - 3];
        refresh_grid();
    }
    
    if (y > 2 && y < 5 && x == 2) {
        matrix_inv[y - 3] = !matrix_inv[y - 3];
        refresh_grid();
    }

    if (x == 2 && y == 0) {
        for (int i = 0; i < MATRIXINS; i++)
            for (int o = 0; o < MATRIXOUTS; o++)
                matrix[mi][i][o] = 0;
        refresh_grid();
    }

    if (x == 2 && y == 1) {
        for (int i = 0; i < MATRIXINS; i++)
            for (int o = 0; o < MATRIXOUTS; o++)
                matrix[mi][i][o] = rand() & 1;
        refresh_grid();
    }

    if (x < 4) return;
    
    x -= 4;
    matrix[mi][y][x] = (matrix[mi][y][x] + 1) % (MATRIXMAXSTATE + 1);
    update_matrix();
    refresh_grid();
}

void render_matrix_page() {
    u8 on = 15, off = 3;

    set_grid_led(0, 0, on);
    
    set_grid_led(2, 0, on);
    set_grid_led(2, 1, on);
    
    set_grid_led(0, 3, mi ? off : on);
    set_grid_led(0, 4, mi ? on : off);
    
    set_grid_led(1, 3, matrix_on[0] ? on : off);
    set_grid_led(1, 4, matrix_on[1] ? on : off);

    set_grid_led(2, 3, matrix_inv[0] ? on : off);
    set_grid_led(2, 4, matrix_inv[1] ? on : off);

    u8 d = 12 / (MATRIXMAXSTATE + 1);
    if (matrix_inv[mi]) d -= 2;
    u8 a = matrix_on[mi] ? 4 : 1;
    
    for (u8 x = 0; x < MATRIXOUTS; x++)
        for (u8 y = 0; y < MATRIXINS; y++)
            set_grid_led(x + 4, y, matrix[mi][y][x] * d + a);
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
visualize value changes from matrix
visualize values
way to mute voices
toggle between directly mapped voices / first available
MIDI keyboard support

    clear matrix
    randomize matrix
    mute matrix
mute scales
    invert matrix

speed for ansible
    additional gate inputs on ansible/teletype
*/