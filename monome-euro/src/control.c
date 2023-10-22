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


#define SPEEDCYCLE 4
#define SPEEDBUTTONCYCLE 10
#define CLOCKOUTWIDTH 10

#define MAXVOLUMELEVEL 7

#define SPEEDTIMER 0
#define SPEEDBUTTONTIMER 1
#define CLOCKTIMER 2
#define CLOCKOUTTIMER 3

// following timers are for each voice
#define NOTEDELAYTIMER 80
#define GATETIMER  90

#define PAGE_PARAM  0
#define PAGE_TRANS  1
#define PAGE_MATRIX 2
#define PAGE_N_DEL  3
#define PAGE_I2C    4

#define PARAM_LEN   0
#define PARAM_ALGOX 1
#define PARAM_ALGOY 2
#define PARAM_SHIFT 3
#define PARAM_SPACE 4
#define PARAM_GATEL 5

#define MATRIXMAXSTATE 1
#define MATRIXGATEWEIGHT 60
#define MATRIXMODEEDIT 0
#define MATRIXMODEPERF 1

#define VOL_DIR_OFF  0
#define VOL_DIR_RAND 1
#define VOL_DIR_FLIP 2
#define VOL_DIR_SLEW 3


// presets and data stored in presets

shared_data_t s;
preset_meta_t meta;
preset_data_t p;
u8 selected_preset;

// local vars

u32 gate_length_mod, speed_button;
s32 matrix_values[MATRIXOUTS];
u8 trans_step, trans_sel, reset_phase;
u8 is_presets, is_preset_saved;
s8 prev_octave;

u16 notes_pitch[NOTECOUNT];
u16 notes_vol[NOTECOUNT];
u8 notes_on[NOTECOUNT];
u8 time_shift_counter;

// prototypes

static void toggle_preset_page(void);
static void save_preset(void);
static void save_preset_and_confirm(void);
static void load_preset(u8 preset);

static void toggle_run_stop(void);

static void set_up_i2c(void);
static void toggle_i2c_device(u8 device);

static void set_vol_dir(u8 dir);
static void toggle_voice_on(u8 voice);
static void set_voice_vol(u8 voice, u8 vol);
static void set_vol_index(u8 index);

static void update_speed_from_knob(void);
static void update_speed_from_buttons(void);
static void update_speed(u32 speed);

static void step(void);
static void update_matrix(void);

static void output_notes(void);
static void output_note(u8 n, u16 pitch, u16 vol, u8 on);
static void stop_note(u8 n);
static u8 note_gen(u8 n);
static u16 note_vol(u8 n);

static void output_mods(void);
static void output_clock(void);

static void set_octave(s8 octave);
static void toggle_octave(void);

static void set_current_scale(u8 scale);
static void toggle_scale(void);
static void toggle_scale_note(u8 scale, u8 note);

static void set_length(u8 length);
static void set_algoX(u8 algoX);
static void set_algoY(u8 algoY);
static void set_shift(u8 shift);
static void set_space(u8 space);

static void set_gate_length(u16 len);
static void set_swing(u8 swing);
static void set_delay_width(u8 delay);
static void set_note_delay(u8 n, u8 delay);

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
static void set_matrix_snapshot(u8 snapshot);
static void toggle_matrix_cell(u8 in, u8 out);

static void update_display(void);

static void process_gate(u8 index, u8 on);
static void process_grid_press(u8 x, u8 y, u8 on);
static void process_grid_trans(u8 x, u8 y, u8 on);
static void process_grid_param(u8 x, u8 y, u8 on);
static void process_grid_matrix(u8 x, u8 y, u8 on);
static void process_grid_note_delay(u8 x, u8 y, u8 on);
static void process_grid_i2c(u8 x, u8 y, u8 on);
static void process_grid_presets(u8 x, u8 y, u8 on);

static void render_trans_page(void);
static void render_param_page(void);
static void render_matrix_page(void);
static void render_note_delay_page(void);
static void render_i2c_page(void);
static void render_presets(void);

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
    for (u8 i = 0; i < MAX_DEVICE_COUNT; i++) s.i2c_device[i] = 0;
    s.i2c_device[VOICE_JF] = 1;
    s.run = 1;
    store_shared_data_to_flash(&s);
    
    p.config.length = 8;
    p.config.algoX = 1;
    p.config.algoY = 1;
    p.config.shift = 0;
    p.config.space = 0;
    
    p.speed = 400;
    p.gate_length = 200;
    
    p.swing = 0;
    p.delay_width = 1;
    for (u8 i = 0; i < NOTECOUNT; i++) p.note_delay[i] = 0;
    
    for (u8 i = 0; i < TRANSSEQLEN; i++) p.transpose[i] = 0;
    p.transpose_seq_on = 0;
    
    for (u8 s = 0; s < SCALECOUNT; s++) {
        for (u8 i = 0; i < SCALELEN; i++) p.scale_buttons[s][i] = 0;
        p.scale_buttons[s][0] = p.scale_buttons[s][3] = p.scale_buttons[s][5] = p.scale_buttons[s][7] = 1;
    }

    p.octave = 0;
    p.current_scale = 0;
    
    for (u8 i = 0; i < MATRIXCOUNT; i++) {
        p.matrix_on[i] = 1;
        p.m_snapshot[i] = 0;
        for (u8 j = 0; j < MATRIXSNAPSHOTS; j++)
            for (u8 k = 0; k < MATRIXINS; k++)
                for (u8 l = 0; l < MATRIXOUTS; l++)
                    p.matrix[i][j][k][l] = 0;
    }
    p.matrix_mode = MATRIXMODEEDIT;
    
    p.vol_index = 0;
    p.vol_dir = VOL_DIR_OFF;
    for (u8 i = 0; i < NOTECOUNT; i++) {
        p.voice_vol[i][0] = p.voice_vol[i][1] = MAXVOLUMELEVEL;
        p.voice_on[i] = 1;
    }

    for (u8 i = 0; i < get_preset_count(); i++)
        store_preset_to_flash(i, &meta, &p);

    store_preset_index(0);
}

void init_control(void) {
    // load shared data
    // load current preset and its meta data
    
    load_shared_data_from_flash(&s);
    load_preset(get_preset_index());
    
    // set up any other initial values and timers

    gate_length_mod = 0;
    
    add_timed_event(CLOCKTIMER, 60000 / (p.speed ? p.speed : 1), 1);
    add_timed_event(SPEEDTIMER, SPEEDCYCLE, 1);
    
    set_as_i2c_leader();
    set_up_i2c();
}

void process_event(u8 event, u8 *data, u8 length) {
    switch (event) {
        case MAIN_CLOCK_RECEIVED:
            step();
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
            if (data[0]) toggle_preset_page();
            break;
    
        case FRONT_BUTTON_HELD:
            save_preset_and_confirm();
            break;
    
        case BUTTON_PRESSED:
            if (data[1]) {
                speed_button = data[0];
                add_timed_event(SPEEDBUTTONTIMER, SPEEDBUTTONCYCLE, 1);
            } else {
                stop_timed_event(SPEEDBUTTONTIMER);
            }
            break;
    
        case I2C_RECEIVED:
            break;
            
        case TIMED_EVENT:
            if (data[0] == SPEEDTIMER) {
                update_speed_from_knob();
            } else if (data[0] == SPEEDBUTTONTIMER) {
                update_speed_from_buttons();
            } else if (data[0] == CLOCKTIMER) {
                if (!is_external_clock_connected() && s.run) step();
            } else if (data[0] == CLOCKOUTTIMER) {
                set_clock_output(0);
            } else if (data[0] >= NOTEDELAYTIMER && data[0] < GATETIMER) {
                u8 n = data[0] - NOTEDELAYTIMER;
                output_note(n, notes_pitch[n], notes_vol[n], notes_on[n]);
            } else if (data[0] >= GATETIMER) {
                stop_note(data[0] - GATETIMER);
            }
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

void toggle_preset_page() {
    if (is_preset_saved) {
        is_preset_saved = is_presets = 0;
        refresh_grid();
        return;
    }
    
    is_presets = !is_presets;
    refresh_grid();
}

void save_preset() {
    store_preset_to_flash(selected_preset, &meta, &p);
    store_shared_data_to_flash(&s);
    store_preset_index(selected_preset);
}

void save_preset_and_confirm() {
    save_preset();
    is_presets = 0;
    is_preset_saved = 1;
    refresh_grid();
}

void load_preset(u8 preset) {
    selected_preset = preset;
    load_preset_from_flash(selected_preset, &p);

    initEngine(&p.config);
    update_timer_interval(CLOCKTIMER, 60000 / (p.speed ? p.speed : 1));
    updateScales(p.scale_buttons);
    setCurrentScale(p.current_scale >= SCALECOUNT ? 0 : p.current_scale);

    refresh_grid();
}

void toggle_run_stop() {
    s.run = !s.run;
    refresh_grid();
}

void set_up_i2c() {
    for (u8 i = 0; i < NOTECOUNT; i++) stop_note(i);
    
    for (u8 i = 0; i < 6; i++) map_voice(i, VOICE_JF, i, 0);
    for (u8 i = 0; i < NOTECOUNT; i++) map_voice(i, VOICE_ER301, i, 0);
    for (u8 i = 0; i < NOTECOUNT; i++) map_voice(i, VOICE_TXO_NOTE, i, 0);
    for (u8 i = 0; i < NOTECOUNT; i++) map_voice(i, VOICE_DISTING_EX, i, 0);
    set_jf_mode(0);

    if (s.i2c_device[VOICE_JF]) {
        set_jf_mode(1);
        for (u8 i = 0; i < 6; i++) map_voice(i, VOICE_JF, i, 1);
    } 
    
    if (s.i2c_device[VOICE_ER301]) {
        for (u8 i = 0; i < NOTECOUNT; i++) map_voice(i, VOICE_ER301, i, 1);
    } 
    
    if (s.i2c_device[VOICE_TXO_NOTE]) {
        for (u8 i = 0; i < NOTECOUNT; i++) {
            set_txo_mode(i, 1);
            map_voice(i, VOICE_TXO_NOTE, i, 1);
        }
    }
    
    if (s.i2c_device[VOICE_DISTING_EX]) {
        for (u8 i = 0; i < NOTECOUNT; i++) map_voice(i, VOICE_DISTING_EX, i, 1);
    }
}

void toggle_i2c_device(u8 device) {
    if (device >= MAX_DEVICE_COUNT) return;
    s.i2c_device[device] = !s.i2c_device[device];
    set_up_i2c();
    refresh_grid();
}

void set_vol_dir(u8 dir) {
    p.vol_dir = dir;
    refresh_grid();
}

void toggle_voice_on(u8 voice) {
    p.voice_on[voice] = !p.voice_on[voice];
    if (!p.voice_on[voice]) stop_note(voice);
    refresh_grid();
}

void set_voice_vol(u8 voice, u8 vol) {
    p.voice_vol[voice][p.vol_index] = vol;
    refresh_grid();
}

void set_vol_index(u8 index) {
    p.vol_index = index;
    refresh_grid();
}

void update_speed_from_knob() {
    if (get_knob_count() == 0) return;
    
    u32 speed = (((get_knob_value(0) * 1980) >> 19) << 3) + 20;
    update_speed(speed);
}

void update_speed_from_buttons() {
    u32 speed = p.speed;
    if (!speed_button && speed > 20)
        speed--;
    else if (speed_button && speed < 2000)
        speed++;
    update_speed(speed);
}

void update_speed(u32 speed) {
    if (speed > 2000) speed = 2000; else if (speed < 20) speed = 20;
    
    if (speed != p.speed) {
        p.speed = speed;
        update_timer_interval(CLOCKTIMER, 60000 / speed);
        update_display();
    }
}

void step() {
    clock();
    transpose_step();
    output_notes();
    output_mods();
    output_clock();
    update_matrix();
    refresh_grid();
}

void transpose_step() {
    if (p.transpose_seq_on && isReset()) {
        trans_step = (trans_step + 1) % TRANSSEQLEN;
        refresh_grid();
    }
}

void output_notes(void) {
    u8 trans = 12 + p.transpose[trans_step];
    if (p.octave > 0) trans += 12;
    else if (p.octave < 0 && trans >= 12) trans -= 12;

    u8 prev_notes[NOTECOUNT];
    u8 found;
    
    u8 gen;
    
    for (u8 n = 0; n < NOTECOUNT; n++) {
        gen = note_gen(n);
        prev_notes[n] = getNote(n, gen);
        
        found = 0;
        for (u8 i = 0; i < n; i++)
            if (prev_notes[n] == prev_notes[i] + 1 || prev_notes[n] == prev_notes[i] - 1) {
                found = 1;
                break;
            }
            
        if (p.voice_on[n] && getGateChanged(n, gen) && !found) {
            notes_pitch[n] = getNote(n, gen) + trans;
            notes_vol[n] = note_vol(n);
            notes_on[n] = getGate(n, gen);
            u32 ndel = (p.delay_width * p.note_delay[n]) % 8;
            if (getCurrentStep() & 1) ndel += p.swing;
            
            if (ndel) {
                u32 delay = (60000 * ndel) / (u32)((p.speed ? p.speed : 1) * 8);
                if (!delay) delay = 1;
                add_timed_event(NOTEDELAYTIMER + n, delay, 0);
            } else {
                output_note(n, notes_pitch[n], notes_vol[n], notes_on[n]);
            }
        }
    }
}

void output_note(u8 n, u16 pitch, u16 vol, u8 on) {
    note(n, pitch, vol, on);
    add_timed_event(GATETIMER + n, gate_length_mod, 0);
}

void stop_note(u8 n) {
    stop_timed_event(NOTEDELAYTIMER + n);
    stop_timed_event(GATETIMER + n);
    note(n, getNote(n, 0), 0, 0);
}

u8 note_gen(u8 n) {
    u8 gen = (p.note_delay[n] * p.delay_width) / 8;
    if (gen >= HISTORYCOUNT) gen = HISTORYCOUNT;
    return gen;
}

u16 note_vol(u8 n) {
    u16 volume;
    
    if (p.vol_dir == VOL_DIR_RAND) {
        u16 min = (min(p.voice_vol[n][0], p.voice_vol[n][1]) + 1) * 1000;
        u16 max = (max(p.voice_vol[n][0], p.voice_vol[n][1]) + 1) * 1000;
        volume = (rand() % (max - min + 1)) + min;
    } else if (p.vol_dir == VOL_DIR_FLIP) {
        volume = 1000 * (p.voice_vol[n][reset_phase] + 1);
    } else if (p.vol_dir == VOL_DIR_SLEW) {
        u16 v1 = p.voice_vol[n][0] * 1000;
        u16 v2 = p.voice_vol[n][1] * 1000;
        u16 len = getLength();
        if (!len) len = 1;
        u16 step = reset_phase ? getCurrentStep() : len - getCurrentStep();
        volume = (v2 - v1) * step / len + v1;
    } else {
        volume =  getModCV(0) * 50 + 1000 * (p.voice_vol[n][p.vol_index] + 1);
    }

    return volume;
}

void output_mods(void) {
    // TODO
}

void output_clock() {
    add_timed_event(CLOCKOUTTIMER, CLOCKOUTWIDTH, 0);
    set_clock_output(1);
}

void update_matrix(void) {
    u8 prevScale = matrix_values[7];
    u8 prevOctave = matrix_values[8];
    
    if (isReset()) {
        reset_phase = !reset_phase;
    }
    
    u8 counts[MATRIXOUTS];
    for (int m = 0; m < MATRIXOUTS; m++) {
        matrix_values[m] = 0;
        counts[m] = 0;
        
        for (u8 i = 0; i < 4; i++) {
            if (p.matrix_on[0]) {
                counts[m] += p.matrix[0][p.m_snapshot[0]][i][m];
                matrix_values[m] += getNote(i, 0) * p.matrix[0][p.m_snapshot[0]][i][m];
            }
            
            if (p.matrix_on[1]) {
                counts[m] += p.matrix[1][p.m_snapshot[1]][i][m];
                matrix_values[m] += getModCV(i) * p.matrix[1][p.m_snapshot[1]][i][m] * 12;
            }
        }

        for (u8 i = 0; i < 2; i++) {
            if (p.matrix_on[0]) {
                counts[m] += p.matrix[0][p.m_snapshot[0]][i + 4][m];
                matrix_values[m] += getGate(i, 0) * p.matrix[0][p.m_snapshot[0]][i + 4][m] * MATRIXGATEWEIGHT;
            }
            
            if (p.matrix_on[1]) {
                counts[m] += p.matrix[1][p.m_snapshot[1]][i + 4][m];
                matrix_values[m] += getModGate(i) * p.matrix[1][p.m_snapshot[1]][i + 4][m] * MATRIXGATEWEIGHT;
            }
        }
        
        if (p.matrix_on[0] & p.matrix[0][p.m_snapshot[0]][6][m]) {
            counts[m]++;
            matrix_values[m] += reset_phase * MATRIXGATEWEIGHT;
        }

        if (p.matrix_on[1] & p.matrix[1][p.m_snapshot[1]][6][m]) {
            counts[m]++;
            matrix_values[m] += reset_phase * MATRIXGATEWEIGHT;
        }
    }
    
    u32 v;
    // value * (max - min) / 120 + param

    // speed_mod = counts[0] ? (matrix_values[0] * 198) / (12 * MATRIXMAXSTATE * counts[0]) : 0;
    
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
    if (gate_length_mod < 20) gate_length_mod = 20; else if (gate_length_mod > 2000) gate_length_mod = 2000;    
    
    if (matrix_values[7] > prevScale && matrix_values[7]) toggle_scale();
    
    if (matrix_values[8] > prevOctave && matrix_values[8]) toggle_octave();
    
    refresh_grid();
}

void toggle_octave() {
    if (p.octave) 
        prev_octave = p.octave;
    else if (!prev_octave)
        prev_octave = 1;
        
    set_octave(p.octave ? 0 : prev_octave);
}

void set_current_scale(u8 scale) {
    if (scale >= SCALECOUNT) return;
    
    setCurrentScale(scale);
    p.current_scale = scale;
    refresh_grid();
}

void set_octave(s8 octave) {
    p.octave = octave;
    refresh_grid();
}

void toggle_scale() {
    u8 newScale = getCurrentScale();
    for (u8 i = 0; i < SCALECOUNT - 1; i++) {
        newScale = (newScale + 1) % SCALECOUNT;
        if (getScaleCount(newScale) != 0) {
            setCurrentScale(newScale);
            p.current_scale = newScale;
            refresh_grid();
            break;
        }
    }
}

void toggle_scale_note(u8 scale, u8 note) {
    p.scale_buttons[scale][note] = !p.scale_buttons[scale][note];
    updateScales(p.scale_buttons);
    refresh_grid();
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
            p.matrix[s.mi][p.m_snapshot[s.mi]][i][o] = 0;
    refresh_grid();
}
    
void randomize_current_matrix() {
    clear_current_matrix();
    for (u8 i = 0; i < 10; i++)
        p.matrix[s.mi][p.m_snapshot[s.mi]][rand() % MATRIXINS][(rand() % (MATRIXOUTS - 1)) + 1] = 1;
    refresh_grid();
}

void set_matrix_snapshot(u8 snapshot) {
    p.m_snapshot[s.mi] = snapshot;
    refresh_grid();
}

void toggle_matrix_cell(u8 in, u8 out) {
    p.matrix[s.mi][p.m_snapshot[s.mi]][in][out] = (p.matrix[s.mi][p.m_snapshot[s.mi]][in][out] + 1) % (MATRIXMAXSTATE + 1);
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

void set_swing(u8 swing) {
    p.swing = swing;
    refresh_grid();
}

void set_delay_width(u8 delay) {
    p.delay_width = delay;
    for (u8 i = 0; i < NOTECOUNT; i++) stop_note(i);
    refresh_grid();
}

void set_note_delay(u8 n, u8 delay) {
    p.note_delay[n] = delay;
    stop_note(n);
    refresh_grid();
}

void toggle_transpose_seq() {
    p.transpose_seq_on = !p.transpose_seq_on;
    refresh_grid();
}

void set_transpose(s8 trans) {
    p.transpose[trans_sel] = trans;
    refresh_grid();
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
    itoa(p.config.space, s, 10);
    draw_str(s, 5, 9, 0);
    
    refresh_screen();
}

void process_gate(u8 index, u8 on) {
    switch (index) {
        case 0:
            reset();
            break;
        case 1:
            toggle_scale();
            break;
        case 2:
            toggle_octave();
            break;
        default:
            break;
    }
}

void process_grid_press(u8 x, u8 y, u8 on) {
    if (is_preset_saved) {
        if (!on) return;
        is_preset_saved = is_presets = 0;
        refresh_grid();
        return;
    }
    
    if (is_presets) {
        process_grid_presets(x, y, on);
        return;
    }
    
    if (y == 0) {
        if (!on) return;
        switch (x) {
            case 0:
                select_matrix(0);
                return;
            case 1:
                select_matrix(1);
                return;
            case 2:
                select_page(PAGE_TRANS);
                break;
            case 14:
                select_page(PAGE_N_DEL);
                return;
            case 15:
                select_page(PAGE_I2C);
                return;
            default:
                break;
        }
    }
    
    if (y == 1 && x == 0 && on) {
        toggle_matrix_mute(0);
        return;
    }

    if (y == 1 && x == 1 && on) {
        toggle_matrix_mute(1);
        return;
    }

    if (y == 1 && x == 15 && on) {
        toggle_transpose_seq();
        return;
    }

    if (s.page == PAGE_I2C) {
        process_grid_i2c(x, y, on);
        return;
    }
    
    if (y == 0) {
        if (!on) return;
        switch (x) {
            case 4:
                select_param(PARAM_LEN);
                break;
            case 5:
                select_param(PARAM_ALGOX);
                break;
            case 6:
                select_param(PARAM_ALGOY);
                break;
            case 7:
                select_param(PARAM_SHIFT);
                break;
            case 8:
                select_param(PARAM_SPACE);
                break;
            case 9:
                select_param(PARAM_GATEL);
                break;
            default:
                break;
        }
        return;
    }
    
    if (s.page == PAGE_TRANS) process_grid_trans(x, y, on);
    else if (s.page == PAGE_PARAM) process_grid_param(x, y, on);
    else if (s.page == PAGE_MATRIX) process_grid_matrix(x, y, on);
    else if (s.page == PAGE_N_DEL) process_grid_note_delay(x, y, on);
}
    
void render_grid() {
    if (!is_grid_connected()) return;
    
    clear_all_grid_leds();
    
    if (is_preset_saved) {
        for (u8 x = 6; x < 10; x++)
            for (u8 y = 2; y < 6; y++)
                set_grid_led(x, y, 10);
        set_grid_led(7, 4, 0);
        set_grid_led(8, 4, 0);
        return;
    }
    
    if (is_presets) {
        render_presets();
        return;
    }
    
    u8 on = 15, off = 7;
    
    set_grid_led(0, 0, s.page == PAGE_MATRIX && s.mi == 0 ? on : off);
    set_grid_led(1, 0, s.page == PAGE_MATRIX && s.mi == 1 ? on : off);
    set_grid_led(0, 1, p.matrix_on[0] ? off : off - 4);
    set_grid_led(1, 1, p.matrix_on[1] ? off : off - 4);
    
    set_grid_led(2, 0, s.page == PAGE_TRANS ? on : off);
    set_grid_led(15, 1, p.transpose_seq_on ? off : off - 4);
    
    set_grid_led(14, 0, s.page == PAGE_N_DEL ? on : off);
    set_grid_led(15, 0, s.page == PAGE_I2C ? on : off);
    
    if (s.page == PAGE_I2C) {
        render_i2c_page();
        return;
    }

    set_grid_led(4, 0, s.page == PAGE_PARAM && s.param == PARAM_LEN ? on : off);
    set_grid_led(5, 0, s.page == PAGE_PARAM && s.param == PARAM_ALGOX ? on : off);
    set_grid_led(6, 0, s.page == PAGE_PARAM && s.param == PARAM_ALGOY ? on : off);
    set_grid_led(7, 0, s.page == PAGE_PARAM && s.param == PARAM_SHIFT ? on : off);
    set_grid_led(8, 0, s.page == PAGE_PARAM && s.param == PARAM_SPACE ? on : off);
    set_grid_led(9, 0, s.page == PAGE_PARAM && s.param == PARAM_GATEL ? on : off);
    
    if (s.page == PAGE_TRANS) render_trans_page();
    else if (s.page == PAGE_PARAM) render_param_page();
    else if (s.page == PAGE_MATRIX) render_matrix_page();
    else if (s.page == PAGE_N_DEL) render_note_delay_page();
}

void process_grid_presets(u8 x, u8 y, u8 on) {
    if (!on) return;
    
    if (y > 0 &&  y < 3 && x > 3 && x < 12) {
        selected_preset = x - 4 + (y - 1) * 8;
        save_preset_and_confirm();
        return;
    }

    if (y > 4 && y < 7 && x > 3 && x < 12) {
        load_preset(x - 4 + (y - 5) * 8);
        return;
    }
}

void render_presets() {
    u8 on = 7;
    
    for (u8 x = 4; x < 12; x++) {
        set_grid_led(x, 1, on);
        set_grid_led(x, 2, on);
    }
        
    for (u8 x = 4; x < 12; x++) {
        set_grid_led(x, 5, on);
        set_grid_led(x, 6, on);
    }

    set_grid_led((selected_preset % 8) + 4, 5 + selected_preset / 8, 15);
}

void process_grid_trans(u8 x, u8 y, u8 on) {
    if (!on) return;

    if (x == 0 && y > 3) {
        set_current_scale(y - 4);
        return;
    }
    
    if (y > 3 && y < 8 && x > 1 && x < 14) {
        toggle_scale_note(y - 4, x - 2);
        return;
    }
    
    if (y == 2 && x == 0) {
        set_octave(p.octave == -1 ? 0 : -1);
        return;
    }

    if (y == 3 && x == 15) {
        set_octave(p.octave == 1 ? 0 : 1);
        return;
    }

    if (y == 1) {
        u8 t = x - (8 - TRANSSEQLEN / 2);
        if (t < TRANSSEQLEN) {
            set_transpose_sel(t);
            if (!p.transpose_seq_on) set_transpose_step(t);
        }
    } else if (y == 2)
        set_transpose(x - 15);
    else if (y == 3)
        set_transpose(x);
}

void render_trans_page() {
    u8 on = 15, mod = 6, off = 3, soff = 1;

    set_grid_led(0, 4, off);
    set_grid_led(0, 5, off);
    set_grid_led(0, 6, off);
    set_grid_led(0, 7, off);
    set_grid_led(0, getCurrentScale() + 4, on);
    
    set_grid_led(15, 4, off);
    set_grid_led(15, 5, off);
    set_grid_led(15, 6, off);
    set_grid_led(15, 7, off);

    for (u8 i = 0; i < SCALECOUNT; i++) {
        for (u8 j = 0; j < SCALELEN; j++)
            set_grid_led(2 + j, i + 4, p.scale_buttons[i][j] ? on : (j == 0 || j == SCALELEN - 1 ? off : soff));
    }
    
    u8 p1 = 8 - TRANSSEQLEN / 2;
    for (u8 i = 0; i < TRANSSEQLEN; i++) set_grid_led(i + p1, 1, off);
    set_grid_led(trans_sel + p1, 1, mod);
    set_grid_led(trans_step + p1, 1, on);

    for (u8 y = 2; y < 4; y++)
        for (u8 x = 0; x < 16; x++)
            set_grid_led(x, y, off);
        
    set_grid_led(0, 2, p.octave == -1 ? on : mod);
    set_grid_led(15, 3, p.octave == 1 ? on : mod);
    
    set_grid_led(15, 2, p.transpose[trans_sel] ? mod : on);
    set_grid_led(0, 3, p.transpose[trans_sel] ? mod : on);
    
    if (p.transpose[trans_step] < 0)
        set_grid_led(15 + p.transpose[trans_step], 2, mod);
    else if (p.transpose[trans_step])
        set_grid_led(p.transpose[trans_step], 3, mod);

    if (p.transpose[trans_sel] < 0)
        set_grid_led(15 + p.transpose[trans_sel], 2, on);
    else if (p.transpose[trans_sel])
        set_grid_led(p.transpose[trans_sel], 3, on);
}

void process_grid_param(u8 x, u8 y, u8 on) {
    if (!on) return;
    
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
            if (y > 2 && y < 5) set_gate_length((((y - 3) << 4) + x) * 64);
            break;
        
        default:
            break;
    }
    
    refresh_grid();
}

void render_param_page() {
    u8 on = 15, mod = 6, off = 3, y, p1, p2, y2;
    
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
            p1 = p.gate_length / 64;
            if (p1 > 16) { y = 4; p1 -= 16; }
            p2 = gate_length_mod / 64;
            if (p2 > 16) { y2 = 4; p2 -= 16; }
            set_grid_led(p2, y2, mod);
            set_grid_led(p1, y, on);
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
    
    if (x == 0 && y == 6 && on) {
        toggle_matrix_mode();
        return;
    }
    
    if (x > 0 && x < 3 && y > 2 && y < 5) {
        set_matrix_snapshot(y - 3 + (x - 1) * 2);
        return;
    }

    // if (x == 4) x = 0;
    if (x > 3 && x < 10) x -= 3;
    else if (x > 10 && x < 13) x -= 4;
    else return;
    
    if (p.matrix_mode == MATRIXMODEPERF || on) toggle_matrix_cell(y - 1, x);
}

void render_matrix_page() {
    set_grid_led(0, 7, 10);
    set_grid_led(1, 7, 10);
    set_grid_led(0, 6, p.matrix_mode == MATRIXMODEEDIT ? 4 : 10);
    
    u8 d = 12 / (MATRIXMAXSTATE + 1);
    u8 a = p.matrix_on[s.mi] ? 3 : 2;
    
    set_grid_led(1, 3, p.m_snapshot[s.mi] == 0 ? 10 : 4);
    set_grid_led(1, 4, p.m_snapshot[s.mi] == 1 ? 10 : 4);
    set_grid_led(2, 3, p.m_snapshot[s.mi] == 2 ? 10 : 4);
    set_grid_led(2, 4, p.m_snapshot[s.mi] == 3 ? 10 : 4);
    
    for (u8 x = 0; x < MATRIXOUTS; x++)
        for (u8 y = 0; y < MATRIXINS; y++) {
            u8 _x;
            if (x == 0) continue; // _x = 4; 
            if (x < 7) _x = x + 3;
            else if (x < 9) _x = x + 4;
            else continue;
            set_grid_led(_x, y + 1, p.matrix[s.mi][p.m_snapshot[s.mi]][y][x] * d + a);
        }
}

void process_grid_note_delay(u8 x, u8 y, u8 on) {
    if (!on) return;
    
    if (y == 2 && x > 3 && x < 12) {
        set_swing(x - 4);
        return;
    }
    
    if (y == 3 && x > 3 && x < 12) {
        set_delay_width(x - 3);
        return;
    }
    
    if (x == 15 && y == 2) {
        toggle_run_stop();
        return;
    }
    
    if (y < 4) return;
    
    u8 n = x > 7 ? y : y - 4;
    if (x > 7) x -= 8;
    set_note_delay(n, x);
}

void render_note_delay_page() {
    u8 off = 3;
    
    set_grid_led(15, 2, s.run ? 15 : 4);
    
    for (u8 x = 4; x < 12; x++) set_grid_led(x, 2, off);
    set_grid_led(4 + p.swing, 2, 15);

    for (u8 x = 4; x < 12; x++) set_grid_led(x, 3, off);
    set_grid_led(3 + p.delay_width, 3, 15);
    
    for (u8 x = 0; x < 16; x++)
        for (u8 y = 4; y < 8; y++)
            set_grid_led(x, y, x == 0 || x == 8 ? 8 : off);

    for (u8 n = 0; n < 4; n++)
        set_grid_led(p.note_delay[n], n + 4, 15);

    for (u8 n = 4; n < 8; n++)
        set_grid_led(p.note_delay[n] + 8, n, 15);
}

void process_grid_i2c(u8 x, u8 y, u8 on) {
    if (!on) return;
    
    if (x == 15) {
        if (y == 2)
            toggle_i2c_device(VOICE_CV_GATE);
        else if (y == 3)
            toggle_i2c_device(VOICE_ER301);
        else if (y == 4)
            toggle_i2c_device(VOICE_JF);
        else if (y == 5)
            toggle_i2c_device(VOICE_TXO_NOTE);
        else if (y == 6)
            toggle_i2c_device(VOICE_DISTING_EX);
    }
    
    if (x == 0 && y > 3) {
        if (y == 4)
            set_vol_dir(VOL_DIR_RAND);
        else if (y == 5)
            set_vol_dir(VOL_DIR_SLEW);
        else if (y == 6)
            set_vol_dir(VOL_DIR_FLIP);
        else
            set_vol_dir(VOL_DIR_OFF);
        return;
    }
    
    if (x == 2 && y > 2 && y < 5) {
        set_vol_index(y - 3);
        return;
    }
    
    if (y == 7) {
        if (x > 3 && x < 12) toggle_voice_on(x - 4);
        return;
    }
    
    if (x > 3 && x < 12) set_voice_vol(x - 4, 7 - y);
}

void render_i2c_page() {
    u8 on = 15, off = 4;

    set_grid_led(2, 3, p.vol_index ? off : on);
    set_grid_led(2, 4, p.vol_index ? on : off);
    
    set_grid_led(0, 4, p.vol_dir == VOL_DIR_RAND ? on : off);
    set_grid_led(0, 5, p.vol_dir == VOL_DIR_SLEW ? on : off);
    set_grid_led(0, 6, p.vol_dir == VOL_DIR_FLIP ? on : off);
    set_grid_led(0, 7, p.vol_dir == VOL_DIR_OFF  ? on : off);
    
    set_grid_led(15, 2, s.i2c_device[VOICE_CV_GATE] ? on : off);
    set_grid_led(15, 3, s.i2c_device[VOICE_ER301] ? on : off);
    set_grid_led(15, 4, s.i2c_device[VOICE_JF] ? on : off);
    set_grid_led(15, 5, s.i2c_device[VOICE_TXO_NOTE] ? on : off);
    set_grid_led(15, 6, s.i2c_device[VOICE_DISTING_EX] ? on : off);
    
    for (u8 i = 0; i < 8; i++) {
        for (u8 y = 0; y < p.voice_vol[i][p.vol_index]; y++)
            set_grid_led(i + 4, 6 - y, p.voice_on[i] ? 4 : 2);
        
        set_grid_led(i + 4, 7 - p.voice_vol[i][p.vol_index], p.voice_on[i] ? 15 : 6);
        set_grid_led(i + 4, 7, p.voice_on[i] ? 6 : 15);
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

-- new

- matrix snapshots
- swing shifted
- 4 scales
- ansible crash fix
- octave up/down buttons
- jf mode should be reset when choosing another i2c device
- minimal gate lenght reduced

-- soon

- parameter sequencer
- copy between matrix and volume snapshots
- undo matrix random/clear
- delays are not calculated properly for ext clock
- make gate len proportional to ext clock
- improve teletype display

-- not tested:

- fix button press on multipass
- fix for front panel button hold

- fix speed not loading from presets on ansible
- selected scale wasn't saved with preset, fixed
- clock output
- additional gate inputs on ansible/teletype
- gate length

-- future

- move mod matrix to engine
- edit scale notes / microtonal scales
- momentary mode for scale notes (or a separate play screen?)
- performance page?
- ability to select any 2 parameters for editing by pressing 2 menu buttons?
- matrix on/off fast forwarded
- i2c parameters in mod matrix
- visualize values for algox/algoy
- switch outputs between notes/mod cvs/clock&reset

- MIDI keyboard support
- toggle between directly mapped voices / first available

*/