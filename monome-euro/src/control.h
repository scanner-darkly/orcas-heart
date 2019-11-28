// ----------------------------------------------------------------------------
// defines functions for multipass to send events to the controller (grid 
// presses etc)
//
// defines functions for engine to send updates (note on etc)
//
// defines data structures for multipass preset management
// ----------------------------------------------------------------------------

#pragma once
#include "types.h"
#include "engine.h"


// ----------------------------------------------------------------------------
// firmware dependent stuff starts here

#define MATRIXOUTS 11
#define MATRIXINS   7
#define MATRIXCOUNT 2
#define TRANSSEQLEN 8


// ----------------------------------------------------------------------------
// shared types

typedef struct {
    u8 page;
    u8 param;
    u8 mi;
    u8 i2c_device;
    u8 run;
} shared_data_t;

typedef struct {
} preset_meta_t;

typedef struct {
    engine_config_t config;
    
    u16 speed;
    u16 gate_length;
    u8 delay_width;
    u8 note_delay[NOTECOUNT];
    
    s8 transpose[TRANSSEQLEN];
    u8 transpose_seq_on;

    u8 scale_buttons[SCALECOUNT][SCALELEN];
    u8 scaleA_octave, scaleB_octave;
    u8 current_scale;

    u8 matrix[MATRIXCOUNT][MATRIXINS][MATRIXOUTS];
    u8 matrix_on[MATRIXCOUNT];
    u8 matrix_mode;
    
    u8 vol_index;
    u8 vol_dir;
    u8 voice_vol[NOTECOUNT][2];
    u8 voice_on[NOTECOUNT];
} preset_data_t;


// ----------------------------------------------------------------------------
// firmware settings/variables main.c needs to know


// ----------------------------------------------------------------------------
// functions control.c needs to implement (will be called from main.c)

void init_presets(void);
void init_control(void);
void process_event(u8 event, u8 *data, u8 length);
void render_grid(void);
void render_arc(void);


// ----------------------------------------------------------------------------
// functions engine needs to call
