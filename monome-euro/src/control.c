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
int selected_preset;

// ----------------------------------------------------------------------------
// firmware dependent stuff starts here

#define PARAMCYCLE 4
#define PARAMTIMER 0

int gatePresets[GATEPRESETCOUNT][NOTECOUNT] = {
    {0b0001, 0b0010, 0b0100, 0b1000},
    {0b0011, 0b0010, 0b0101, 0b1000},
    {0b0011, 0b0110, 0b1101, 0b1000},
    {0b0111, 0b0110, 0b1101, 0b1001},

    {0b0111, 0b0101, 0b1101, 0b1010},
    {0b1111, 0b0101, 0b1110, 0b1010},
    {0b1101, 0b1101, 0b1010, 0b1011},
    {0b1101, 0b1000, 0b0110, 0b1101},

    {0b1001, 0b1100, 0b1110, 0b0111},
    {0b1100, 0b0101, 0b0110, 0b0111},
    {0b1100, 0b0110, 0b0110, 0b1100},
    {0b0101, 0b1010, 0b0110, 0b1101},

    {0b0101, 0b1001, 0b0110, 0b0101},
    {0b0110, 0b0101, 0b0110, 0b1101},
    {0b1100, 0b0011, 0b0110, 0b1100},
    {0b1001, 0b0010, 0b0101, 0b1000}
};

int spacePresets[SPACEPRESETCOUNT] = {
    0b0000, 0b0001, 0b0010, 0b0100,
    0b1000, 0b0011, 0b0101, 0b1001,
    0b0110, 0b1010, 0b1100, 0b0111,
    0b1011, 0b1101, 0b1110, 0b1111
};

u16 speed, gateLength, transpose, length, algoX, algoY, shift, space;
u8 scales[SCALECOUNT][SCALELEN] = {};
u8 scaleButtons[SCALECOUNT][SCALELEN] = {};
u16 scaleCount[SCALECOUNT] = {};
u8 scale = 0;

u16 globalCounter = 0;
u16 spaceCounter = 0;

u8 counter[TRACKCOUNT] = {0, 0, 0, 0};
u8 divisor[TRACKCOUNT] = {2, 3, 5, 4};
u8 phase[TRACKCOUNT]   = {2, 3, 5, 4};
u8 weights[TRACKCOUNT] = {1, 2, 4, 7};
u8 shifts[TRACKCOUNT] = {0, 0, 0, 0};

u8 trackOn[TRACKCOUNT] = {0, 0, 0, 0};
u8 weightOn[TRACKCOUNT] = {0, 0, 0, 0};
u8 totalWeight = 0;

u8 notes[NOTECOUNT];
u8 gateOn[NOTECOUNT] = {0, 0, 0, 0};
u8 gateChanged[NOTECOUNT] = {1, 1, 1, 1};

u8 modCvs[MODCOUNT];
u8 modGateOn[MODCOUNT] = {0, 0, 0, 0};
u8 modGateChanged[MODCOUNT] = {1, 1, 1, 1};


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

    // set up any other initial values and timers
    clear_screen();
    add_timed_event(PARAMTIMER, PARAMCYCLE, 1);
    set_as_i2c_leader();
    
    clear_screen();
    draw_str("ORCA'S HEART", 0, 15, 0);
    refresh_screen();
}

static void process_grid_press(u8 x, u8 y, u8 on) {
    if (!on) return;
    
         if (x == 0 && y == 1) scaleButtons[0][0] = !scaleButtons[0][0];
    else if (x == 1 && y == 0) scaleButtons[0][1] = !scaleButtons[0][1];
    else if (x == 1 && y == 1) scaleButtons[0][2] = !scaleButtons[0][2];
    else if (x == 2 && y == 0) scaleButtons[0][3] = !scaleButtons[0][3];
    else if (x == 2 && y == 1) scaleButtons[0][4] = !scaleButtons[0][4];
    else if (x == 4 && y == 1) scaleButtons[0][5] = !scaleButtons[0][5];
    else if (x == 5 && y == 0) scaleButtons[0][6] = !scaleButtons[0][6];
    else if (x == 5 && y == 1) scaleButtons[0][7] = !scaleButtons[0][7];
    else if (x == 6 && y == 0) scaleButtons[0][8] = !scaleButtons[0][8];
    else if (x == 6 && y == 1) scaleButtons[0][9] = !scaleButtons[0][9];
    else if (x == 7 && y == 0) scaleButtons[0][10] = !scaleButtons[0][10];
    else if (x == 7 && y == 1) scaleButtons[0][11] = !scaleButtons[0][11];
    
    else if (x == 0 && y == 4) scaleButtons[1][0] = !scaleButtons[1][0];
    else if (x == 1 && y == 3) scaleButtons[1][1] = !scaleButtons[1][1];
    else if (x == 1 && y == 4) scaleButtons[1][2] = !scaleButtons[1][2];
    else if (x == 2 && y == 3) scaleButtons[1][3] = !scaleButtons[1][3];
    else if (x == 2 && y == 4) scaleButtons[1][4] = !scaleButtons[1][4];
    else if (x == 4 && y == 4) scaleButtons[1][5] = !scaleButtons[1][5];
    else if (x == 5 && y == 3) scaleButtons[1][6] = !scaleButtons[1][6];
    else if (x == 5 && y == 4) scaleButtons[1][7] = !scaleButtons[1][7];
    else if (x == 6 && y == 3) scaleButtons[1][8] = !scaleButtons[1][8];
    else if (x == 6 && y == 4) scaleButtons[1][9] = !scaleButtons[1][9];
    else if (x == 7 && y == 3) scaleButtons[1][10] = !scaleButtons[1][10];
    else if (x == 7 && y == 4) scaleButtons[1][11] = !scaleButtons[1][11];
    
    else if (x == 3 && y == 7) scale = 0;
    else if (x == 4 && y == 7) scale = 1;
    
    refresh_grid();
}

static void updateScales(void) {
    for (u8 s = 0; s < SCALECOUNT; s++) {
        scaleCount[s] = 0;
        for (int i = 0; i < SCALELEN; i++) {
            if (scaleButtons[s][i]) {
                scales[s][scaleCount[s]++] = i;
            }
        }
        if (scaleCount[s] == 0) {
            scaleCount[s] = 1;
            scales[s][0] = 0;
        }
    }
}

static void updateParameters(void) {
    // TODO
    length = 16;
    speed = 400;
    algoX = 12;
    algoY = 15;
    shift = 0;
    space = 0;
    transpose = 0;
    gateLength = 5000;
}

void updateTrackParameters() {
    divisor[0] = (algoX & 3) + 1;
    phase[0] = algoX >> 5;
   
    for (int i = 1; i < TRACKCOUNT; i++) {
        if (algoX & (1 << (i + 2))) divisor[i] = divisor[i-1] + 1; else divisor[i] = divisor[i-1] - 1;
        if (divisor[i] < 0) divisor[i] = 1 - divisor[i];
        if (divisor[i] == 0) divisor[i] = i + 2;
        phase[i] = ((algoX & (0b11 << i)) + i) % divisor[i];
    }
   
    for (int i = 0; i < TRACKCOUNT; i++) shifts[i] = shift;
}

static void updateCounters(void) {
    if (++spaceCounter >= 16) spaceCounter = 0;

    if (++globalCounter >= length) {
        globalCounter = 0;
        for (u8 i = 0; i < TRACKCOUNT; i++) counter[i] = 0;
        // TODO output reset trigger
    } else {
        for (u8 i = 0; i < TRACKCOUNT; i++) counter[i]++;
    }
   
    totalWeight = 0;
    for (int i = 0; i < TRACKCOUNT; i++) {
        trackOn[i] = ((counter[i] + phase[i]) / divisor[i]) & 1;
        weightOn[i] = trackOn[i] ? weights[i] : 0;
        totalWeight += weightOn[i];
    }
   
    for (int i = 0; i < NOTECOUNT; i++) {
        shifts[i] = shift;
        if (shift > SCALELEN / 2) shifts[i] += i;
    }
}

static void reset(void) {
    spaceCounter = globalCounter = 0;
    for (int i = 0; i < TRACKCOUNT; i++) counter[i] = 0;
    // TODO trigger to reset output
}

static void process_gate(u8 index, u8 on) {
    if (!on) return;
    
    switch (index) {
        case 0:
            // clock;
            break;
        case 1:
            reset();
            break;
        case 4:
            scale = !scale;
            refresh_grid();
            break;
        default:
            break;
    }
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
            if (data[0] == PARAMTIMER) updateParameters();
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

void render_grid(void) {
    if (!is_grid_connected()) return;
    
    clear_all_grid_leds();
    set_grid_led(3, 7, 4);
    set_grid_led(4, 7, 4);
    if (scale) set_grid_led(scale ? 4 : 3, 7, 15);
    
    u8 on = 15, off = 8, y;
    for (u8 i = 0; i < SCALECOUNT; i++) {
        y = i * 3;
        set_grid_led(0, 1 + y, scaleButtons[i][0] ? on : off);
        set_grid_led(1, 0 + y, scaleButtons[i][1] ? on : off);
        set_grid_led(1, 1 + y, scaleButtons[i][2] ? on : off);
        set_grid_led(2, 0 + y, scaleButtons[i][3] ? on : off);
        set_grid_led(2, 1 + y, scaleButtons[i][4] ? on : off);
        set_grid_led(4, 1 + y, scaleButtons[i][5] ? on : off);
        set_grid_led(5, 0 + y, scaleButtons[i][6] ? on : off);
        set_grid_led(5, 1 + y, scaleButtons[i][7] ? on : off);
        set_grid_led(6, 0 + y, scaleButtons[i][8] ? on : off);
        set_grid_led(6, 1 + y, scaleButtons[i][9] ? on : off);
        set_grid_led(7, 0 + y, scaleButtons[i][10] ? on : off);
        set_grid_led(7, 1 + y, scaleButtons[i][11] ? on : off);
    }
}

void render_arc(void) { }

