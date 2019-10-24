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


// ----------------------------------------------------------------------------
// firmware dependent stuff starts here

#define SCALELEN 12
#define SCALECOUNT 2
#define NOTECOUNT 4
#define MODCOUNT 4
#define TRACKCOUNT 4
#define GATEPRESETCOUNT 16
#define SPACEPRESETCOUNT 16


// ----------------------------------------------------------------------------
// shared types

typedef struct {
} preset_meta_t;

typedef struct {
} shared_data_t;

typedef struct {
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
