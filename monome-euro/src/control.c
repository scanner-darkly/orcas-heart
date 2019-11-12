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
#define GATETIMER 2

#define PAGE_PARAM   0
#define PAGE_MATRIX 1

#define PARAM_LEN   0
#define PARAM_ALGOX 1
#define PARAM_ALGOY 2
#define PARAM_SHIFT 3
#define PARAM_SPACE 4
#define PARAM_TRANS 5
#define PARAM_GATEL 6

#define MATRIXMAXSTATE 1
#define MATRIXGATEWEIGHT 60
#define MATRIXMODEEDIT 0
#define MATRIXMODEPERF 1

// presets and data stored in presets

shared_data_t s;
preset_meta_t meta;
preset_data_t p;
u8 selected_preset;

// local vars

u32 speed_mod, gate_length_mod;
s32 matrix_values[MATRIXOUTS];
u8 trans_step, trans_sel, reset_phase;

// prototypes

static void load_preset(u8 preset);
static void save_preset(void);

static void step(void);

static void update_parameters(void);
static void update_matrix(void);

static void output_notes(void);
static void stop_note(u8 n);
static void output_mods(void);

static void toggle_scale(u8 manual);
static void toggle_scaleA_octave(void);
static void toggle_scaleB_octave(void);
void toggle_scale_note(u8 scale, u8 note);

static void set_length(u8 length);
static void set_algoX(u8 algoX);
static void set_algoY(u8 algoY);
static void set_shift(u8 shift);
static void set_space(u8 space);

static void set_gate_length(u16 len);
static void transpose_step(void);
static void toggle_transpose_seq(void);
static void set_transpose(s8 trans);
static void set_transpose_sel(u8 sel);
static void set_transpose_step(u8 step);

static void select_page(u8 p);
static void select_param(u8 p);
static void select_matrix(u8 m);
static void toggle_matrix_mute(u8 m);
static void toggle_matrix_mode(void);
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
    
    s.page = PAGE_PARAM;
    s.param = PARAM_LEN;
    s.mi = 0;
    store_shared_data_to_flash(&s);
    
    p.config.length = 8;
    p.config.algoX = 0;
    p.config.algoY = 0;
    p.config.shift = 0;
    p.config.space = 0;
    p.speed = 400;
    p.gate_length = 1000;
    for (u8 i = 0; i < TRANSSEQLEN; i++) p.transpose[i] = 0;
    p.transpose_seq_on = 0;
    
    for (u8 s = 0; s < SCALECOUNT; s++)
        for (u8 i = 0; i < SCALELEN; i++)
            p.scale_buttons[s][i] = 0;
        
    p.scaleA_octave = 0;
    p.scaleB_octave = 0;
    
    for (u8 i = 0; i < MATRIXCOUNT; i++) {
        p.matrix_on[i] = 1;
        for (u8 j = 0; j < MATRIXINS; j++)
            for (u8 k = 0; k < MATRIXOUTS; k++)
                p.matrix[i][j][k] = 0;
    }
    p.matrix_mode = MATRIXMODEEDIT;

    for (u8 i = 0; i < get_preset_count(); i++)
        store_preset_to_flash(i, &meta, &p);

    store_preset_index(0);
}

void init_control(void) {
    // load shared data
    // load current preset and its meta data
    
    add_timed_event(CLOCKTIMER, 100, 1);

    load_shared_data_from_flash(&s);
    load_preset(get_preset_index());
    
    // set up any other initial values and timers
    
    speed_mod = gate_length_mod = 0;
    add_timed_event(PARAMTIMER, PARAMCYCLE, 1);
    
    set_as_i2c_leader();
    set_jf_mode(1);
    for (u8 i = 0; i < NOTECOUNT; i++) map_voice(i, VOICE_JF, i, 1);
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
            if (data[0] == PARAMTIMER)
                update_parameters();
            else if (data[0] == CLOCKTIMER)
                step();
            else if (data[0] >= GATETIMER)
                stop_note(data[0] - GATETIMER);
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

void save_preset() {
    store_preset_to_flash(selected_preset, &meta, &p);
    store_shared_data_to_flash(&s);
    store_preset_index(0);
}

void load_preset(u8 preset) {
    selected_preset = preset;
    load_preset_from_flash(selected_preset, &p);

    initEngine(&p.config);
    update_timer_interval(CLOCKTIMER, p.speed);
    updateScales(p.scale_buttons);

    refresh_grid();
}

void step() {
    clock();
    transpose_step();
    output_notes();
    output_mods();
    update_matrix();
    refresh_grid();
}

void transpose_step() {
    if (p.transpose_seq_on && isReset()) {
        trans_step = (trans_step + 1) % TRANSSEQLEN;
        if (s.page == PAGE_PARAM && s.param == PARAM_TRANS) refresh_grid();
    }
}

void update_parameters() {
    u32 sp = ((get_knob_value(0) * 1980) >> 16) + 20 + speed_mod;
    if (sp > 2000) sp = 2000; else if (sp < 20) sp = 20;
    u32 newSpeed = 60000 / sp;
    
    if (newSpeed != p.speed) {
        p.speed = newSpeed;
        update_timer_interval(CLOCKTIMER, p.speed);
        update_display();
    }
}

void output_notes(void) {
    u8 trans = 24 + p.transpose[trans_step];
    u8 scale = getCurrentScale();
    if (!scale && p.scaleA_octave) trans += 12;
    else if (scale && p.scaleB_octave) trans += 12;

    u8 prev_notes[NOTECOUNT];
    u8 found;
    
    for (u8 n = 0; n < NOTECOUNT; n++) {
        prev_notes[n] = getNote(n);
        found = 0;
        for (u8 i = 0; i < n; i++)
            if (prev_notes[n] == prev_notes[i] + 1 || prev_notes[n] == prev_notes[i] - 1) {
                found = 1;
                break;
            }
        if (getGateChanged(n) && !found) {
            note(n, getNote(n) + trans, getModCV(0) * 100 + 6000, getGate(n));
            add_timed_event(GATETIMER + n, gate_length_mod, 0);
        }
    }
}

void stop_note(u8 n) {
    note(n, 0, 0, 0);
}

void output_mods(void) {
    // TODO
}

void update_matrix(void) {
    u8 prevScale = matrix_values[7];
    u8 prevOctaveA = matrix_values[8];
    u8 prevOctaveB = matrix_values[9];
    
    if (isReset()) {
        reset_phase = !reset_phase;
    }
    
    u8 counts[MATRIXOUTS];
    for (int m = 0; m < MATRIXOUTS; m++) {
        matrix_values[m] = 0;
        counts[m] = 0;
        
        for (u8 i = 0; i < 4; i++) {
            if (p.matrix_on[0]) {
                counts[m] += p.matrix[0][i][m];
                matrix_values[m] += getNote(i) * p.matrix[0][i][m];
            }
            
            if (p.matrix_on[1]) {
                counts[m] += p.matrix[1][i][m];
                matrix_values[m] += getModCV(i) * p.matrix[1][i][m] * 12;
            }
        }

        for (u8 i = 0; i < 2; i++) {
            if (p.matrix_on[0]) {
                counts[m] += p.matrix[0][i + 4][m];
                matrix_values[m] += getGate(i) * p.matrix[0][i + 4][m] * MATRIXGATEWEIGHT;
            }
            
            if (p.matrix_on[1]) {
                counts[m] += p.matrix[1][i + 4][m];
                matrix_values[m] += getModGate(i) * p.matrix[1][i + 4][m] * MATRIXGATEWEIGHT;
            }
        }
        
        if (p.matrix_on[0] & p.matrix[0][6][m]) {
            counts[m]++;
            matrix_values[m] += reset_phase * MATRIXGATEWEIGHT;
        }

        if (p.matrix_on[1] & p.matrix[1][6][m]) {
            counts[m]++;
            matrix_values[m] += reset_phase * MATRIXGATEWEIGHT;
        }
    }
    
    u32 v;
    // value * (max - min) / 120 + param

    speed_mod = counts[0] ? (matrix_values[0] * 198) / (12 * MATRIXMAXSTATE * counts[0]) : 0;
    
    v = p.config.length;
    if (counts[1]) {
        v += (matrix_values[1] * 31) / (120 * MATRIXMAXSTATE * counts[1]);
        if (v > 32) v = 32; else if (v < 1) v = 1;
    }
    updateLength(v);
    
    v = p.config.algoX;
    if (counts[2]) {
        v += (matrix_values[2] * 127) / (120 * MATRIXMAXSTATE * counts[2]);
        if (v > 127) v = 127; else if (v < 0) v = 0;
    }
    updateAlgoX(v);

    v = p.config.algoY;
    if (counts[3]) {
        v += (matrix_values[3] * 127) / (120 * MATRIXMAXSTATE * counts[3]);
        if (v > 127) v = 127; else if (v < 0) v = 0;
    }
    updateAlgoY(v);
        
    v = p.config.shift;
    if (counts[4]) {
        v += matrix_values[4] / (10 * MATRIXMAXSTATE * counts[4]);
        if (v > 12) v = 12; else if (v < 0) v = 0;
    }
    updateShift(v);

    v = p.config.space;
    if (counts[5]) {
        v += (matrix_values[5] * 15) / (120 * MATRIXMAXSTATE * counts[5]);
        if (v > 12) v = 12; else if (v < 0) v = 0;
    }
    updateSpace(v);
    
    gate_length_mod = counts[6] ? (matrix_values[6] * 390) / (12 * MATRIXMAXSTATE * counts[6]) : 0;
    gate_length_mod += p.gate_length;
    if (gate_length_mod < 100) gate_length_mod = 100; else if (gate_length_mod > 4000) gate_length_mod = 4000;    
    
    if (matrix_values[7] > prevScale) toggle_scale(0);
    
    if (matrix_values[8] > prevOctaveA) toggle_scaleA_octave();

    if (matrix_values[9] > prevOctaveB) toggle_scaleB_octave();
    
    refresh_grid();
}

void toggle_scale(u8 manual) {
    u8 newScale = (getCurrentScale() + 1) % SCALECOUNT;
    if (getScaleCount(newScale) == 0 && !manual) return;
    setCurrentScale(newScale);
    if (s.page == PAGE_PARAM) refresh_grid();
}

void toggle_scaleA_octave() {
    p.scaleA_octave = !p.scaleA_octave;
    if (s.page == PAGE_PARAM) refresh_grid();
}

void toggle_scaleB_octave() {
    p.scaleB_octave = !p.scaleB_octave;
    if (s.page == PAGE_PARAM) refresh_grid();
}

void toggle_scale_note(u8 scale, u8 note) {
    p.scale_buttons[scale][note] = !p.scale_buttons[scale][note];
    updateScales(p.scale_buttons);
    if (s.page == PAGE_PARAM) refresh_grid();
}

void select_page(u8 p) {
    s.page = p;
    refresh_grid();
}

void select_param(u8 p) {
    s.param = p;
    select_page(PAGE_PARAM);
}

void select_matrix(u8 m) {
    s.mi = m;
    select_page(PAGE_MATRIX);
}

void toggle_matrix_mute(u8 m) {
    p.matrix_on[m] = !p.matrix_on[m];
    refresh_grid();
}

void toggle_matrix_mode() {
    p.matrix_mode = p.matrix_mode == MATRIXMODEEDIT ? MATRIXMODEPERF : MATRIXMODEEDIT;
    if (s.page == PAGE_MATRIX) refresh_grid();
}

void clear_current_matrix() {
    for (int i = 0; i < MATRIXINS; i++)
        for (int o = 0; o < MATRIXOUTS; o++)
            p.matrix[s.mi][i][o] = 0;
    refresh_grid();
}
    
void randomize_current_matrix() {
    clear_current_matrix();
    for (u8 i = 0; i < 10; i++)
        p.matrix[s.mi][rand() % MATRIXINS][(rand() % (MATRIXOUTS - 1)) + 1] = 1;
    refresh_grid();
}

void toggle_matrix_cell(u8 in, u8 out) {
    p.matrix[s.mi][in][out] = (p.matrix[s.mi][in][out] + 1) % (MATRIXMAXSTATE + 1);
    update_matrix();
    refresh_grid();
}

void set_length(u8 length) {
    p.config.length = length;
    updateLength(p.config.length);
    if (s.page == PAGE_PARAM && s.param == PARAM_LEN) refresh_grid();
}

void set_algoX(u8 algoX) {
    p.config.algoX = algoX;
    updateAlgoX(p.config.algoX);
    if (s.page == PAGE_PARAM && s.param == PARAM_ALGOX) refresh_grid();
}

void set_algoY(u8 algoY) {
    p.config.algoY = algoY;
    updateAlgoY(p.config.algoY);
    if (s.page == PAGE_PARAM && s.param == PARAM_ALGOY) refresh_grid();
}

void set_shift(u8 shift) {
    p.config.shift = shift;
    updateShift(p.config.shift);
    if (s.page == PAGE_PARAM && s.param == PARAM_SHIFT) refresh_grid();
}

void set_space(u8 space) {
    p.config.space = space;
    updateSpace(p.config.space);
    if (s.page == PAGE_PARAM && s.param == PARAM_SPACE) refresh_grid();
}

void set_gate_length(u16 len) {
    p.gate_length = len;
    if (s.page == PAGE_PARAM && s.param == PARAM_GATEL) refresh_grid();
}

void toggle_transpose_seq() {
    p.transpose_seq_on = !p.transpose_seq_on;
    refresh_grid();
}

void set_transpose(s8 trans) {
    p.transpose[trans_sel] = trans;
    if (s.page == PAGE_PARAM && s.param == PARAM_TRANS) refresh_grid();
}

void set_transpose_sel(u8 sel) {
    trans_sel = sel;
    refresh_grid();
}

void set_transpose_step(u8 step) {
    trans_step = step;
    refresh_grid();
}


// ----------------------------------------------------------------------------
// controller

void update_display() {
    clear_screen();
    draw_str("ORCA'S HEART", 0, 15, 0);
    
    // TODO format better

    char s[8];

    itoa(p.config.length, s, 10);
    draw_str(s, 2, 9, 0);
    itoa(p.config.algoX, s, 10);
    draw_str(s, 3, 9, 0);
    itoa(p.config.algoY, s, 10);
    draw_str(s, 4, 9, 0);
    itoa(p.config.shift, s, 10);
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
            case 15:
                toggle_transpose_seq();
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

    if (s.page == PAGE_PARAM) process_grid_param(x, y, on);
    else if (s.page == PAGE_MATRIX) process_grid_matrix(x, y, on);
}
    
void render_grid() {
    if (!is_grid_connected()) return;
    
    clear_all_grid_leds();
    
    u8 on = 15, off = 7;
    set_grid_led(0, 0, s.page == PAGE_MATRIX && s.mi == 0 ? on : off);
    set_grid_led(1, 0, s.page == PAGE_MATRIX && s.mi == 1 ? on : off);
    set_grid_led(0, 1, p.matrix_on[0] ? off : off - 4);
    set_grid_led(1, 1, p.matrix_on[1] ? off : off - 4);
    set_grid_led(6, 0, s.page == PAGE_PARAM && s.param == PARAM_LEN ? on : off);
    set_grid_led(7, 0, s.page == PAGE_PARAM && s.param == PARAM_ALGOX ? on : off);
    set_grid_led(8, 0, s.page == PAGE_PARAM && s.param == PARAM_ALGOY ? on : off);
    set_grid_led(9, 0, s.page == PAGE_PARAM && s.param == PARAM_SHIFT ? on : off);
    set_grid_led(10, 0, s.page == PAGE_PARAM && s.param == PARAM_SPACE ? on : off);
    set_grid_led(11, 0, s.page == PAGE_PARAM && s.param == PARAM_GATEL ? on : off);
    set_grid_led(13, 0, s.page == PAGE_PARAM && s.param == PARAM_TRANS ? on : off);
    set_grid_led(15, 0, p.transpose_seq_on ? on : off);
    
    if (s.page == PAGE_PARAM) render_param_page();
    else if (s.page == PAGE_MATRIX) render_matrix_page();
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
    
    switch (s.param) {

        case PARAM_LEN:
            if (y > 2 && y < 5) set_length(((y - 3) << 4) + x + 1);
            break;
        
        case PARAM_ALGOX:
            if (y == 3 && x > 3 && x < 12)
                set_algoX (((x - 4) << 4) + (p.config.algoX & 15));
            else if (y == 4)
                set_algoX((p.config.algoX & 0b1110000) + x);
            break;
        
        case PARAM_ALGOY:
            if (y == 3 && x > 3 && x < 12)
                set_algoY (((x - 4) << 4) + (p.config.algoY & 15));
            else if (y == 4)
                set_algoY((p.config.algoY & 0b1110000) + x);
            break;
        
        case PARAM_SHIFT:
            if (y == 3 && x > 1 && x < 15) set_shift(x - 2);
            break;
        
        case PARAM_SPACE:
            if (y == 3) set_space(x);
            break;
        
        case PARAM_GATEL:
            if (y > 2 && y < 5) set_gate_length((((y - 3) << 4) + x) * 129);
            break;
        
        case PARAM_TRANS:
            if (y == 2) {
                u8 t = x - (8 - TRANSSEQLEN / 2);
                if (t < TRANSSEQLEN) {
                    set_transpose_sel(t);
                    if (!p.transpose_seq_on) set_transpose_step(t);
                }
            } else if (y == 3)
                set_transpose(x - 15);
            else if (y == 4)
                set_transpose(x);
            break;
        
        default:
            break;
    }
    
    refresh_grid();
}

void render_param_page() {
    u8 on = 15, mod = 6, off = 3, y, p1, p2, y2;
    
    set_grid_led(0, 6, off);
    set_grid_led(0, 7, off);
    set_grid_led(0, getCurrentScale() ? 7 : 6, on);
    
    set_grid_led(15, 6, p.scaleA_octave ? on : off);
    set_grid_led(15, 7, p.scaleB_octave ? on : off);

    for (u8 i = 0; i < SCALECOUNT; i++) {
        y = 8 - SCALECOUNT + i;
        for (u8 j = 0; j < SCALELEN; j++) set_grid_led(2 + j, y, p.scale_buttons[i][j] ? on : off);
    }
    
    switch (s.param) {
        
        case PARAM_LEN:
            for (u8 x = 0; x < 16; x++) for (u8 y = 3; y < 5; y++) set_grid_led(x, y, off);
            
            y = y2 = 3;
            p1 = p.config.length - 1;
            if (p1 > 16) { y = 4; p1 -= 16; }
            p2 = getLength() - 1;
            if (p2 > 16) { y2 = 4; p2 -= 16; }
            set_grid_led(p2, y2, mod);
            set_grid_led(p1, y, on);
            break;
        
        case PARAM_ALGOX:
            p1 = (p.config.algoX >> 4);
            p2 = (getAlgoX() >> 4);
            for (u8 i = 0; i < 8; i++) set_grid_led(i + 4, 3, i == p1 ? on : (i == p2 ? mod : off));
            
            p1 = (p.config.algoX & 15);
            p2 = (getAlgoX() & 15);
            for (u8 i = 0; i < 16; i++) set_grid_led(i, 4, i == p1 ? on : (i == p2 ? mod : off));
            break;
        
        case PARAM_ALGOY:
            p1 = p.config.algoY >> 4;
            p2 = (getAlgoY() >> 4);
            for (u8 i = 0; i < 8; i++) set_grid_led(i + 4, 3, i == p1 ? on : (i == p2 ? mod : off));
            
            p1 = (p.config.algoY & 15);
            p2 = (getAlgoY() & 15);
            for (u8 i = 0; i < 16; i++) set_grid_led(i, 4, i == p1 ? on : (i == p2 ? mod : off));
            break;
        
        case PARAM_SHIFT:
            for (u8 x = 0; x < 13; x++) set_grid_led(x + 2, 3, x == p.config.shift ? on : (x == getShift() ? mod : off));
            break;
        
        case PARAM_SPACE:
            for (u8 x = 0; x < 16; x++) set_grid_led(x, 3, x == p.config.space ? on : (x == getSpace() ? mod : off));
            break;
        
        case PARAM_GATEL:
            for (u8 x = 0; x < 16; x++) for (u8 y = 3; y < 5; y++) set_grid_led(x, y, off);
            
            y = y2 = 3;
            p1 = p.gate_length / 129;
            if (p1 > 16) { y = 4; p1 -= 16; }
            p2 = gate_length_mod / 129;
            if (p2 > 16) { y2 = 4; p2 -= 16; }
            set_grid_led(p2, y2, mod);
            set_grid_led(p1, y, on);
            break;

        case PARAM_TRANS:
            p1 = 8 - TRANSSEQLEN / 2;
            for (u8 i = 0; i < TRANSSEQLEN; i++) set_grid_led(i + p1, 2, off);
            set_grid_led(trans_sel + p1, 2, mod);
            set_grid_led(trans_step + p1, 2, on);
        
            for (u8 y = 3; y < 5; y++)
                for (u8 x = 0; x < 16; x++)
                    set_grid_led(x, y, off);
                
            set_grid_led(15, 3, p.transpose[trans_sel] ? mod : on);
            set_grid_led(0, 4, p.transpose[trans_sel] ? mod : on);
            
            if (p.transpose[trans_sel] < 0)
                set_grid_led(15 + p.transpose[trans_sel], 3, on);
            else if (p.transpose[trans_sel])
                set_grid_led(p.transpose[trans_sel], 4, on);
        
            break;
        
        default:
            break;
    }
}

void process_grid_matrix(u8 x, u8 y, u8 on) {
    if (x == 0 && y == 7 && on) {
        clear_current_matrix();
        return;
    }

    if (x == 1 && y == 7 && on) {
        randomize_current_matrix();
        return;
    }
    
    if (x == 0 && y == 4 && on) {
        toggle_matrix_mode();
        return;
    }

    if (x == 4) x = 0;
    else if (x > 5 && x < 12) x -= 5;
    else if (x > 12) x -= 6;
    else return;
    
    if (p.matrix_mode == MATRIXMODEPERF || on) toggle_matrix_cell(y - 1, x);
}

void render_matrix_page() {
    set_grid_led(0, 7, 10);
    set_grid_led(1, 7, 10);
    set_grid_led(0, 4, p.matrix_mode == MATRIXMODEEDIT ? 10 : 4);
    
    u8 d = 12 / (MATRIXMAXSTATE + 1);
    u8 a = p.matrix_on[s.mi] ? 3 : 2;
    
    for (u8 x = 0; x < MATRIXOUTS; x++)
        for (u8 y = 0; y < MATRIXINS; y++) {
            u8 _x;
            if (x == 0) _x = 4;
            else if (x < 7) _x = x + 5;
            else _x = x + 6;
            set_grid_led(_x, y + 1, p.matrix[s.mi][y][x] * d + a);
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

presets
way to mute voices

i2c config
switch outputs between notes/mod cvs

clock input / make gate len proportional
clock / reset outputs
speed for ansible

MIDI keyboard support
visualize values for algox/algoy
toggle between directly mapped voices / first available
transfer matrix stuff to engine?
    
-- not tested:

- additional gate inputs on ansible/teletype
- visualize value changes from matrix
- matrix perf mode
- transpose
- gate length
- changed 7th mod source to reset
  
*/