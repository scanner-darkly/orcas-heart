// ----------------------------------------------------------------------------
// defines engine actions available to control
// ----------------------------------------------------------------------------

#pragma once
#include "types.h"

#define TRACKCOUNT 6

#define SCALELEN 12
#define SCALECOUNT 2

#define NOTECOUNT 6
#define MODCOUNT 4


typedef struct {
    uint8_t length;
    uint8_t algoX;
    uint8_t algoY;
    uint8_t shift;
    uint8_t space;
} engine_config_t;


typedef struct {
    engine_config_t config;
    uint16_t globalCounter;
    uint16_t spaceCounter;

    uint8_t counter[TRACKCOUNT];
    uint8_t divisor[TRACKCOUNT];
    uint8_t phase[TRACKCOUNT];

    uint8_t trackOn[TRACKCOUNT];
    uint8_t weightOn[TRACKCOUNT];
    uint16_t totalWeight;
    
    uint8_t shifts[NOTECOUNT];

    uint8_t scales[SCALECOUNT][SCALELEN];
    uint8_t scaleCount[SCALECOUNT];
    uint8_t scale;
    
    uint8_t notes[NOTECOUNT];
    uint8_t gateOn[NOTECOUNT];
    uint8_t gateChanged[NOTECOUNT];
    
    uint16_t modCvs[MODCOUNT];
    uint8_t modGateOn[MODCOUNT];
    uint8_t modGateChanged[MODCOUNT];
} engine_t;


void initEngine(engine_config_t *config);
void updateScales(uint8_t scales[SCALECOUNT][SCALELEN]);

uint8_t getLength(void);
uint8_t getAlgoX(void);
uint8_t getAlgoY(void);
uint8_t getShift(void);
uint8_t getSpace(void);

void updateLength(uint8_t length);
void updateAlgoX(uint8_t algoX);
void updateAlgoY(uint8_t algoY);
void updateShift(uint8_t shift);
void updateSpace(uint8_t space);

void reset(void);
void clock(void);

void setCurrentScale(uint8_t scale);
uint8_t getCurrentScale(void);
uint8_t getScaleCount(uint8_t scale);

uint8_t getNote(uint8_t index);
uint8_t getGate(uint8_t index);
uint8_t getGateChanged(uint8_t index);
uint16_t getModCV(uint8_t index);
uint8_t getModGate(uint8_t index);
