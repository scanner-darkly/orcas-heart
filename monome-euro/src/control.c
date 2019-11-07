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


#define PARAMCYCLE 4

#define PARAMTIMER 0
#define CLOCKTIMER 1

#define PAGE_PARAM   0
#define PAGE_MATRIX 1

#define PARAM_LEN   0
#define PARAM_ALGOX 1
#define PARAM_ALGOY 2
#define PARAM_SHIFT 3
#define PARAM_SPACE 4
#define PARAM_TRANS 5
#define PARAM_GATEL 6

#define MATRIXOUTS 10
#define MATRIXINS   7
#define MATRIXCOUNT 2
#define MATRIXMAXSTATE 1
#define MATRIXGATEWEIGHT 60

// presets and data stored in presets

shared_data_t shared;
preset_meta_t meta;
preset_data_t preset;
u8 selected_preset;

engine_config_t config;
u16 speed, gateLength, transpose;
u8 matrix[MATRIXCOUNT][MATRIXINS][MATRIXOUTS];
u8 matrix_on[MATRIXCOUNT];

u8 page;
u8 param;
u8 mi;

// UI and local vars

u8 scaleButtons[SCALECOUNT][SCALELEN] = {};
u8 scaleAOctave, scaleBOctave;

u32 speedMod;
s32 matrix_values[MATRIXOUTS];

// prototypes

static void step(void);

static void update_parameters(void);
static void update_matrix(void);

static void output_notes(void);
static void output_mods(void);

static void toggle_scale(u8 manual);
static void toggle_scaleA_octave(void);
static void toggle_scaleB_octave(void);
void toggle_scale_note(u8 scale, u8 note);

void set_length(u8 length);
void set_algoX(u8 algoX);
void set_algoY(u8 algoY);
void set_shift(u8 shift);
void set_space(u8 space);

static void select_page(u8 p);
static void select_param(u8 p);
static void select_matrix(u8 m);
static void toggle_matrix_mute(u8 m);
static void clear_current_matrix(void);
static void randomize_current_matrix(void);
static void toggle_matrix_cell(u8 in, u8 out);

static void update_display(void);

static void process_gate(u8 index, u8 on);
static void process_grid_press(u8 x, u8 y, u8 on);
static void process_grid_param(u8 x, u8 y, u8 on);
static void process_grid_matrix(u8 x, u8 y, u8 on);

static void render_param_page(void);
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
    
    speed = 400;
    gateLength = 1;
    transpose = 0;
    
    // TODO update scale buttons from preset
    updateScales(scaleButtons);
    
    // TODO load matrix from preset
    for (u8 i = 0; i < MATRIXCOUNT; i++) matrix_on[i] = 1;
    
    // TODO load page / param from preset
    page = PAGE_PARAM;
    param = PARAM_LEN;
    mi = 0;
    
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
// actions

void step() {
    clock();
    output_notes();
    output_mods();
    update_matrix();
    refresh_grid();
}

void update_parameters() {
    u32 sp = ((get_knob_value(0) * 1980) >> 16) + 20 + speedMod;
    if (sp > 2000) sp = 2000; else if (sp < 20) sp = 20;
    u32 newSpeed = 60000 / sp;
    
    if (newSpeed != speed) {
        speed = newSpeed;
        update_timer_interval(CLOCKTIMER, speed);
        update_display();
    }
}

void output_notes(void) {
    // TODO implement transpose
    u8 trans = 8;
    u8 scale = getCurrentScale();
    if (!scale && scaleAOctave) trans += 12;
    else if (scale && scaleBOctave) trans += 12;

    for (u8 n = 0; n < NOTECOUNT; n++) {
        // TODO implement gate length
        note(n, getNote(n) + trans, getVolume(n) * getModCV(0) * 500 + 5000, getGate(n));
    }
}

void output_mods(void) {
    // TODO
}

void update_matrix(void) {
    u8 prevScale = matrix_values[7];
    u8 prevOctaveA = matrix_values[8];
    u8 prevOctaveB = matrix_values[9];
    
    u8 counts[MATRIXOUTS];
    for (int m = 0; m < MATRIXOUTS; m++) {
        matrix_values[m] = 0;
        counts[m] = 0;
        for (u8 i = 0; i < 4; i++) {
            if (matrix_on[0]) {
                counts[m] += matrix[0][i][m];
                matrix_values[m] += getNote(i) * matrix[0][i][m];
                if (i < 3) {
                    counts[m] += matrix[0][i + 4][m];
                    matrix_values[m] += getGate(i) * matrix[0][i + 4][m] * MATRIXGATEWEIGHT;
                }
            }
            if (matrix_on[1]) {
                counts[m] += matrix[1][i][m];
                matrix_values[m] += getModCV(i) * matrix[1][i][m] * 12;
                if (i < 3) {
                    matrix_values[m] += getModGate(i) * matrix[1][i + 4][m] * MATRIXGATEWEIGHT;
                    counts[m] += matrix[1][i + 4][m];
                }
            }
        }
    }
    
    s32 v;
    // value * (max - min) / 120 + param
    
    v = (matrix_values[0] * 198) / (12 * MATRIXMAXSTATE * counts[0]);
    speedMod = v;
    
    v = (matrix_values[1] * 31) / (120 * MATRIXMAXSTATE * counts[1]) + config.length;
    if (v > 32) v = 32; else if (v < 1) v = 1;
    updateLength(v);
    
    v = (matrix_values[2] * 127) / (120 * MATRIXMAXSTATE * counts[2]) + config.algoX;
    if (v > 127) v = 127; else if (v < 0) v = 0;
    updateAlgoX(v);
    
    v = (matrix_values[3] * 127) / (120 * MATRIXMAXSTATE * counts[3]) + config.algoY;
    if (v > 127) v = 127; else if (v < 0) v = 0;
    updateAlgoY(v);
    
    v = matrix_values[4] / (10 * MATRIXMAXSTATE * counts[4]) + config.shift;
    if (v > 12) v = 12; else if (v < 0) v = 0;
    updateShift(v);
    
    v = (matrix_values[5] * 15) / (120 * MATRIXMAXSTATE * counts[5]) + config.space;
    if (v > 12) v = 12; else if (v < 0) v = 0;
    updateSpace(v);
    
    if (matrix_values[7] > prevScale) toggle_scale(0);
    
    if (matrix_values[8] > prevOctaveA) toggle_scaleA_octave();

    if (matrix_values[9] > prevOctaveB) toggle_scaleB_octave();
}

void toggle_scale(u8 manual) {
    u8 newScale = (getCurrentScale() + 1) % SCALECOUNT;
    if (getScaleCount(newScale) == 0 && !manual) return;
    setCurrentScale(newScale);
    if (page == PAGE_PARAM) refresh_grid();
}

void toggle_scaleA_octave() {
    scaleAOctave = !scaleAOctave;
    if (page == PAGE_PARAM) refresh_grid();
}

void toggle_scaleB_octave() {
    scaleBOctave = !scaleBOctave;
    if (page == PAGE_PARAM) refresh_grid();
}

void toggle_scale_note(u8 scale, u8 note) {
    scaleButtons[scale][note] = !scaleButtons[scale][note];
    updateScales(scaleButtons);
    if (page == PAGE_PARAM) refresh_grid();
}

void select_page(u8 p) {
    page = p;
    refresh_grid();
}

void select_param(u8 p) {
    param = p;
    select_page(PAGE_PARAM);
}

void select_matrix(u8 m) {
    mi = m;
    select_page(PAGE_MATRIX);
}

void toggle_matrix_mute(u8 m) {
    matrix_on[m] = !matrix_on[m];
    refresh_grid();
}

void clear_current_matrix() {
    for (int i = 0; i < MATRIXINS; i++)
        for (int o = 0; o < MATRIXOUTS; o++)
            matrix[mi][i][o] = 0;
    refresh_grid();
}
    
void randomize_current_matrix() {
    clear_current_matrix();
    for (u8 i = 0; i < 10; i++)
        matrix[mi][rand() % MATRIXINS][rand() % MATRIXOUTS] = 1;
    refresh_grid();
}

void toggle_matrix_cell(u8 in, u8 out) {
    matrix[mi][in][out] = (matrix[mi][in][out] + 1) % (MATRIXMAXSTATE + 1);
    update_matrix();
    refresh_grid();
}

void set_length(u8 length) {
    config.length = length;
    updateLength(config.length);
}

void set_algoX(u8 algoX) {
    config.algoX = algoX;
    updateAlgoX(config.algoX);
}

void set_algoY(u8 algoY) {
    config.algoY = algoY;
    updateAlgoY(config.algoY);
}

void set_shift(u8 shift) {
    config.shift = shift;
    updateShift(config.shift);
}

void set_space(u8 space) {
    config.space = space;
    updateSpace(config.space);
}



// ----------------------------------------------------------------------------
// controller

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
            break;
        case 1:
            toggle_scale(1);
            break;
        case 2:
            toggle_scaleA_octave();
            break;
        case 3:
            toggle_scaleB_octave();
            break;
        default:
            break;
    }
}

void process_grid_press(u8 x, u8 y, u8 on) {
    if (y == 0) {
        if (!on) return;
        switch (x) {
            case 0:
                select_matrix(0);
                break;
            case 1:
                select_matrix(1);
                break;
            case 6:
                select_param(PARAM_LEN);
                break;
            case 7:
                select_param(PARAM_ALGOX);
                break;
            case 8:
                select_param(PARAM_ALGOY);
                break;
            case 9:
                select_param(PARAM_SHIFT);
                break;
            case 10:
                select_param(PARAM_SPACE);
                break;
            case 11:
                select_param(PARAM_GATEL);
                break;
            case 13:
                select_param(PARAM_TRANS);
                break;
            default:
                break;
        }
        return;
    }
    
    if (y == 1 && x == 0 && on) {
        toggle_matrix_mute(0);
        return;
    }
    if (y == 1 && x == 1 && on) {
        toggle_matrix_mute(1);
        return;
    }

    if (page == PAGE_PARAM) process_grid_param(x, y, on);
    else if (page == PAGE_MATRIX) process_grid_matrix(x, y, on);
}
    
void render_grid() {
    if (!is_grid_connected()) return;
    
    clear_all_grid_leds();
    
    u8 on = 15, off = 7;
    set_grid_led(0, 0, page == PAGE_MATRIX && mi == 0 ? on : off);
    set_grid_led(1, 0, page == PAGE_MATRIX && mi == 1 ? on : off);
    set_grid_led(0, 1, matrix_on[0] ? off : off - 4);
    set_grid_led(1, 1, matrix_on[1] ? off : off - 4);
    set_grid_led(6, 0, page == PAGE_PARAM && param == PARAM_LEN ? on : off);
    set_grid_led(7, 0, page == PAGE_PARAM && param == PARAM_ALGOX ? on : off);
    set_grid_led(8, 0, page == PAGE_PARAM && param == PARAM_ALGOY ? on : off);
    set_grid_led(9, 0, page == PAGE_PARAM && param == PARAM_SHIFT ? on : off);
    set_grid_led(10, 0, page == PAGE_PARAM && param == PARAM_SPACE ? on : off);
    set_grid_led(11, 0, page == PAGE_PARAM && param == PARAM_GATEL ? on : off);
    set_grid_led(13, 0, page == PAGE_PARAM && param == PARAM_TRANS ? on : off);
    
    if (page == PAGE_PARAM) render_param_page();
    else if (page == PAGE_MATRIX) render_matrix_page();
}

void process_grid_param(u8 x, u8 y, u8 on) {
    if (!on) return;
    
    if (x == 0 && y == 6)
        setCurrentScale(0);
    
    else if (x == 0 && y == 7)
        setCurrentScale(1);
    
    else if (x == 15 && y == 6)
        toggle_scaleA_octave();
    
    else if (x == 15 && y == 7)
        toggle_scaleB_octave();
    
    else if (y > 5 && y < 8 && x > 1 && x < 14)
        toggle_scale_note(y - 6, x - 2);
    
    if (y < 3 || y > 4) return;
    
    switch (param) {

        case PARAM_LEN:
            set_length(((y - 3) << 4) + x + 1);
            break;
        
        case PARAM_ALGOX:
            if (y == 3 && x > 3 && x < 12)
                set_algoX (((x - 4) << 4) + (config.algoX & 15));
            else if (y == 4)
                set_algoX((config.algoX & 0b1110000) + x);
            break;
        
        case PARAM_ALGOY:
            if (y == 3 && x > 3 && x < 12)
                set_algoY (((x - 4) << 4) + (config.algoX & 15));
            else if (y == 4)
                set_algoY((config.algoX & 0b1110000) + x);
            break;
        
        case PARAM_SHIFT:
            if (y == 3 && x > 1 && x < 15) set_shift(x - 2);
            break;
        
        case PARAM_SPACE:
            if (y == 3) set_space(x);
            break;
        
        case PARAM_TRANS:
            break;
        
        case PARAM_GATEL:
            break;
        
        default:
            break;
    }
    
    refresh_grid();
}

void render_param_page() {
    u8 on = 15, off = 3, y, l;
    
    set_grid_led(0, 6, off);
    set_grid_led(0, 7, off);
    set_grid_led(0, getCurrentScale() ? 7 : 6, on);
    
    set_grid_led(15, 6, scaleAOctave ? on : off);
    set_grid_led(15, 7, scaleBOctave ? on : off);

    for (u8 i = 0; i < SCALECOUNT; i++) {
        y = 8 - SCALECOUNT + i;
        for (u8 j = 0; j < SCALELEN; j++) set_grid_led(2 + j, y, scaleButtons[i][j] ? on : off);
    }
    
    switch (param) {
        
        case PARAM_LEN:
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
            l = (config.algoX >> 4);
            for (u8 i = 0; i < 8; i++) set_grid_led(i + 4, 3, i == l ? on : off);
            l = (config.algoX & 15);
            for (u8 i = 0; i < 16; i++) set_grid_led(i, 4, i == l ? on : off);
            break;
        
        case PARAM_ALGOY:
            l = config.algoY >> 4;
            for (u8 i = 0; i < 8; i++) set_grid_led(i + 4, 3, i == l ? on : off);
            l = (config.algoY & 15);
            for (u8 i = 0; i < 16; i++) set_grid_led(i, 4, i == l ? on : off);
            break;
        
        case PARAM_SHIFT:
            for (u8 x = 0; x < 13; x++) set_grid_led(x + 2, 3, x == config.shift ? on : (x == getShift() ? off + 3 : off));
            break;
        
        case PARAM_SPACE:
            for (u8 x = 0; x < 16; x++) set_grid_led(x, 3, x == config.space ? on : off);
            break;
        
        case PARAM_TRANS:
            break;
        
        case PARAM_GATEL:
            break;
        
        default:
            break;
    }
}

void process_grid_matrix(u8 x, u8 y, u8 on) {
    if (!on) return;
    
    if (x == 0 && y == 7) {
        clear_current_matrix();
        return;
    }

    if (x == 1 && y == 7) {
        randomize_current_matrix();
        return;
    }

    if (x == 4) x = 0;
    else if (x > 5 && x < 12) x -= 5;
    else if (x > 12) x -= 6;
    else return;
    
    toggle_matrix_cell(y - 1, x);
}

void render_matrix_page() {
    set_grid_led(0, 7, 10);
    set_grid_led(1, 7, 10);
    
    u8 d = 12 / (MATRIXMAXSTATE + 1);
    u8 a = matrix_on[mi] ? 4 : 2;
    
    for (u8 x = 0; x < MATRIXOUTS; x++)
        for (u8 y = 0; y < MATRIXINS; y++) {
            u8 _x;
            if (x == 0) _x = 4;
            else if (x < 7) _x = x + 5;
            else _x = x + 6;
            set_grid_led(_x, y + 1, matrix[mi][y][x] * d + a);
        }
}

void render_arc() { }


// ----------------------------------------------------------------------------
// helper functions

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
visualize values for algox/algoy
way to mute voices
toggle between directly mapped voices / first available
MIDI keyboard support
mute scales
speed for ansible
transfer matrix stuff to engine?
    
--
    
space should just mute a note instead of not producing a gate (or tweak it)
    weight modulations by the number of mod sources
    turn off scale switching from mod if the scale to switch to is empty
    
-- not tested:

- additional gate inputs on ansible/teletype
    
*/