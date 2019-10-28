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

u8 gatePresets[GATEPRESETCOUNT][NOTECOUNT] = {
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

u8 spacePresets[SPACEPRESETCOUNT] = {
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
u16 weights[TRACKCOUNT] = {1, 2, 4, 7};
u8 shifts[TRACKCOUNT] = {0, 0, 0, 0};

u8 trackOn[TRACKCOUNT] = {0, 0, 0, 0};
u8 weightOn[TRACKCOUNT] = {0, 0, 0, 0};
u16 totalWeight = 0;

u8 notes[NOTECOUNT];
u8 gateOn[NOTECOUNT] = {0, 0, 0, 0};
u8 gateChanged[NOTECOUNT] = {1, 1, 1, 1};

u16 modCvs[MODCOUNT];
u8 modGateOn[MODCOUNT] = {0, 0, 0, 0};
u8 modGateChanged[MODCOUNT] = {1, 1, 1, 1};


static char* itoa(int value, char* result, int base);
static void process(void);

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
    
    // TODO
    load_shared_data_from_flash(&shared);
    selected_preset = get_preset_index();
    load_preset_from_flash(selected_preset, &preset);
    load_preset_meta_from_flash(selected_preset, &meta);

    // set up any other initial values and timers
    add_timed_event(PARAMTIMER, PARAMCYCLE, 1);
    add_timed_event(CLOCKTIMER, 50, 1);
    set_as_i2c_leader();
    
    set_jf_mode(1);
    for (u8 i = 0; i < NOTECOUNT; i++) map_voice(i, VOICE_JF, i + 1, 1);
    
    refresh_grid();
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
        for (u8 i = 0; i < SCALELEN; i++) {
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

static void renderScreen(void) {
    clear_screen();
    draw_str("ORCA'S HEART", 0, 15, 0);
    
    char s[8];

    itoa(length, s, 10);
    draw_str(s, 2, 9, 0);
    itoa(algoX, s, 10);
    draw_str(s, 3, 9, 0);
    itoa(algoY, s, 10);
    draw_str(s, 4, 9, 0);
    itoa(shift, s, 10);
    draw_str(s, 5, 9, 0);
    
    refresh_screen();
}

static void updateTrackParameters(void) {
    divisor[0] = (algoX & 3) + 1;
    phase[0] = algoX >> 5;
   
    for (u8 i = 1; i < TRACKCOUNT; i++) {
        if (algoX & (1 << (i + 2))) divisor[i] = divisor[i-1] + 1; else divisor[i] = divisor[i-1] - 1;
        if (divisor[i] < 0) divisor[i] = 1 - divisor[i];
        if (divisor[i] == 0) divisor[i] = i + 2;
        phase[i] = ((algoX & (0b11 << i)) + i) % divisor[i];
    }
   
    for (u8 i = 0; i < TRACKCOUNT; i++) shifts[i] = shift;
}

static void updateParameters(void) {
    // TODO
    length = get_txi_param(0) >> 11; if (length == 0) length = 1;
    speed = 400;
    algoX = get_txi_param(2) >> 9;
    algoY = get_txi_param(3) >> 9;
    shift = get_txi_param(4) >> 10;
    space = 0; // get_txi_param(5) >> 10;
    transpose = get_txi_param(6) >> 10;
    gateLength = get_txi_param(7) >> 10;
    
    updateTrackParameters();
    updateScales();
    renderScreen();
}

static void reset(void) {
    spaceCounter = globalCounter = 0;
    for (u8 i = 0; i < TRACKCOUNT; i++) counter[i] = 0;
    // TODO trigger to reset output
}

static void updateCounters(void) {
    if (++spaceCounter >= 16) spaceCounter = 0;

    if (++globalCounter >= length) {
        reset();
    } else {
        for (u8 i = 0; i < TRACKCOUNT; i++) counter[i]++;
    }
   
    totalWeight = 0;
    for (u8 i = 0; i < TRACKCOUNT; i++) {
        trackOn[i] = ((counter[i] + phase[i]) / divisor[i]) & 1;
        weightOn[i] = trackOn[i] ? weights[i] : 0;
        totalWeight += weightOn[i];
    }
   
    for (u8 i = 0; i < NOTECOUNT; i++) {
        shifts[i] = shift;
        if (shift > SCALELEN / 2) shifts[i] += i;
    }
}

static void process_gate(u8 index, u8 on) {
    if (!on) return;
    
    switch (index) {
        case 0:
            // clock;
            break;
        case 1:
            // reset
            reset();
            break;
        case 2:
            // switch scale
            scale = !scale;
            refresh_grid();
            break;
        default:
            break;
    }
}

static void updateMod(void) {
    for (u8 i = 0; i < MODCOUNT; i++) modGateOn[i] = trackOn[i % TRACKCOUNT];

    modCvs[0] = totalWeight + weightOn[0];
    modCvs[1] = weights[1] * (trackOn[3] + trackOn[2]) + weights[2] * (trackOn[0] + trackOn[2]);
    modCvs[2] = weights[0] * (trackOn[2] + trackOn[1]) + weights[3] * (trackOn[0] + trackOn[3]);
    modCvs[3] = weights[1] * (trackOn[1] + trackOn[2]) + weights[2] * (trackOn[2]  + trackOn[3]) + weights[3] * (trackOn[3] + trackOn[2]);
   
    for (u8 i = 0; i < MODCOUNT; i++) modCvs[i] %= 10;
}

static void calculateNote(int n) {
    u8 mask = algoY >> 3;

    notes[n] = 0;
    for (u8 j = 0; j < TRACKCOUNT; j++) {
        if (trackOn[j] && (mask & (1 << j))) notes[n] += weightOn[j];
    }

    if (algoY & 1) notes[n] += weightOn[(n + 1) % TRACKCOUNT];
    if (algoY & 2) notes[n] += weightOn[(n + 2) % TRACKCOUNT];
    if (algoY & 4) notes[n] += weightOn[(n + 3) % TRACKCOUNT];
   
    notes[n] += shifts[n];
}
   
static void calculateNextNote(int n) {
    u8 mask = gatePresets[algoY >> 3][n];
    if (mask == 0) mask = 0b0101;
    for (u8 i = 0; i < n; i++) mask = ((mask & 1) << 3) | (mask >> 1);
   
    u8 gate = 0;
    for (u8 j = 0; j < TRACKCOUNT; j++) {
        if (trackOn[j] && (mask & (1 << j))) gate = 1;
        // if (mask & (1 << j)) gate = 1;
    }

    if (algoY & 1) gate ^= trackOn[n % TRACKCOUNT] << 1;
    if (algoY & 2) gate ^= trackOn[(n + 2) % TRACKCOUNT] << 2;
    if (algoY & 4) gate ^= trackOn[(n + 3) % TRACKCOUNT] << 3;

    bool previousGatesOn = 1;
    for (u8 i = 0; i < NOTECOUNT - 1; i++) previousGatesOn &= gateChanged[i] & gateOn[i];
    if (n == NOTECOUNT - 1 && previousGatesOn) gate = 0;
   
    gateChanged[n] = gateOn[n] != gate;
    gateOn[n] = gate;
    if (gateChanged[n]) {
        // TODO gateTimer[n] = speed * gateLength;
        calculateNote(n);
    }
}

void process() {
    updateCounters();
    updateMod();
    for (u8 n = 0; n < NOTECOUNT; n++) calculateNextNote(n);
    // TODO clockOut.trigger(1e-3);
   
    // TODO float trans = transpose;
    // TODO if ((scale == 0 && getValue(SCALE_A_PARAM) > 0) || (scale == 1 && getValue(SCALE_B_PARAM) > 0)) trans += 1.f;
   
    for (u8 n = 0; n < NOTECOUNT; n++) {
        note(n, scales[scale][notes[n] % scaleCount[scale]] + min(2, notes[n] / 12) * 12 + 20, 3000, gateOn[n]);
        
        // TODO
        /*
        int sp = spacePresets[(space | n) % SPACEPRESETCOUNT];
        if (sp & spaceCounter) {
            outputs[GATE_1_OUTPUT + n].setVoltage(0);
        } else {
            if (gateTimer[n] > 0) gateTimer[n] -= args.sampleTime;
            if (gateTimer[n] < 0) gateTimer[n] = 0;
            float g = 10.0; // (float)(modCvs[n] % 8) / 4.0 + 5.0;
            outputs[GATE_1_OUTPUT + n].setVoltage(gateTimer[n] > 0 ? g : 0);
        }
        */
    }
   
    // TODO
    /*
    for (int i = 0; i < MODCOUNT; i++) {
        outputs[MOD_GATE_1_OUTPUT + i].setVoltage(modGateOn[i] ? 10.f : 0.f);
        outputs[MOD_CV_1_OUTPUT + i].setVoltage((float)(modCvs[i] % 8) / 7.f * 10.f);
    }
    */
   
    // TODO outputs[CLOCK_OUTPUT].setVoltage(clockOut.process(args.sampleTime) ? 10.f : 0.f);
    // TODO outputs[RESET_OUTPUT].setVoltage(resetOut.process(args.sampleTime) ? 10.f : 0.f);
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
            else if (data[0] == CLOCKTIMER) process();
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
    set_grid_led(scale ? 4 : 3, 7, 15);
    
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
