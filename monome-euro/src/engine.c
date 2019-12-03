// ----------------------------------------------------------------------------
// app engine
//
// hardware agnostic application logic
// sends updates to control and provides actions for control to call
// ----------------------------------------------------------------------------

#include "engine.h"

#define GATEPRESETCOUNT 16
#define SPACEPRESETCOUNT 16

uint8_t gatePresets[GATEPRESETCOUNT][NOTECOUNT] = {
    {0b1000, 0b0010, 0b0100, 0b1000, 0b0000, 0b0001, 0b0101, 0b1010},
    {0b0011, 0b0010, 0b0101, 0b1000, 0b0001, 0b0010, 0b0100, 0b0100},
    {0b0011, 0b0110, 0b1101, 0b1000, 0b0010, 0b0100, 0b0100, 0b0001},
    {0b0111, 0b0110, 0b1101, 0b1001, 0b0100, 0b1000, 0b0010, 0b0001},

    {0b0111, 0b0101, 0b1101, 0b1010, 0b1001, 0b0101, 0b0010, 0b1001},
    {0b1111, 0b0101, 0b1110, 0b1010, 0b0110, 0b1010, 0b0011, 0b1001},
    {0b1101, 0b1101, 0b1010, 0b1011, 0b1010, 0b0110, 0b0011, 0b1100},
    {0b1101, 0b1000, 0b0110, 0b1101, 0b1100, 0b0011, 0b0010, 0b0100},

    {0b1001, 0b1100, 0b1110, 0b0111, 0b1000, 0b0001, 0b0100, 0b0010},
    {0b1100, 0b0101, 0b0110, 0b0111, 0b0100, 0b1000, 0b1010, 0b0110},
    {0b1100, 0b0110, 0b0110, 0b1100, 0b0010, 0b0100, 0b1001, 0b1110},
    {0b0101, 0b1010, 0b0110, 0b1101, 0b0001, 0b0010, 0b0110, 0b1001},

    {0b0101, 0b1001, 0b0110, 0b0101, 0b1101, 0b1011, 0b0010, 0b0001},
    {0b0110, 0b0101, 0b0110, 0b1101, 0b1100, 0b0011, 0b0001, 0b0010},
    {0b1100, 0b0011, 0b0110, 0b1100, 0b0110, 0b0110, 0b1000, 0b0010},
    {0b1001, 0b0010, 0b0101, 0b1000, 0b0010, 0b0100, 0b1010, 0b0001}
};

uint8_t spacePresets[SPACEPRESETCOUNT] = {
    0b0000, 0b0001, 0b0010, 0b0100,
    0b1000, 0b0011, 0b0101, 0b1001,
    0b0110, 0b1010, 0b1100, 0b0111,
    0b1011, 0b1101, 0b1110, 0b1111
};

uint16_t weights[TRACKCOUNT] = {1, 2, 4, 7, 5, 3, 4, 2};

engine_t engine;

static void updateCounters(void);
static void updateTrackParameters(void);
static void updateTrackValues(void);
static void calculateNotes(void);
static void calculateMods(void);
static void calculateNote(int n);
static void calculateNextNote(int n);
static void initHistory(void);
static void pushHistory(void);


// ----------------------------------------------------------------------------
// functions for control

void initEngine(engine_config_t *config) {
    updateLength(config->length);
    updateAlgoX(config->algoX);
    updateAlgoY(config->algoY);
    updateShift(config->shift);
    updateSpace(config->space);
    
    reset();
    updateTrackParameters();
    updateTrackValues();
    initHistory();
    calculateNotes();
    calculateMods();
}

void updateScales(uint8_t scales[SCALECOUNT][SCALELEN]) {
    for (uint8_t s = 0; s < SCALECOUNT; s++) {
        engine.scaleCount[s] = 0;
        for (uint8_t i = 0; i < SCALELEN; i++) {
            if (scales[s][i]) {
                engine.scales[s][engine.scaleCount[s]++] = i;
            }
        }
    }
}

uint8_t getLength() {
    return engine.config.length;
}

uint8_t getAlgoX(void) {
    return engine.config.algoX;
}

uint8_t getAlgoY(void) {
    return engine.config.algoY;
}

uint8_t getShift() {
    return engine.config.shift;
}

uint8_t getSpace() {
    return engine.config.space;
}

void updateLength(uint8_t length) {
    engine.config.length = length;
}

void updateAlgoX(uint8_t algoX) {
    engine.config.algoX = algoX;
}

void updateAlgoY(uint8_t algoY) {
    engine.config.algoY = algoY;
}

void updateShift(uint8_t shift) {
    engine.config.shift = shift;
    for (uint8_t i = 0; i < NOTECOUNT; i++) { 
        engine.shifts[i] = shift;
        if (shift > SCALELEN / 2) engine.shifts[i] += i;
    }
}

void updateSpace(uint8_t space) {
    engine.config.space = space;
}

void clock() {
    updateCounters();
    updateTrackParameters();
    updateTrackValues();
    pushHistory();
    calculateNotes();
    calculateMods();
}

void reset() {
    engine.globalCounter = engine.spaceCounter = 0;
    for (uint8_t i = 0; i < TRACKCOUNT; i++) engine.counter[i] = 0;
}

uint8_t isReset() {
    return engine.globalCounter == 0;
}

uint8_t getCurrentStep() {
    return engine.globalCounter;
}

void setCurrentScale(uint8_t scale) {
    if (scale >= SCALECOUNT) return;
    engine.scale = scale;
}

uint8_t getCurrentScale() {
    return engine.scale;
}

uint8_t getScaleCount(uint8_t scale) {
    return engine.scaleCount[scale];
}

uint8_t getNote(uint8_t index, u8 generation) {
    return engine.notes[index][generation];
}

uint8_t getGate(uint8_t index, u8 generation) {
    return engine.gateOn[index][generation];
}

uint8_t getGateChanged(uint8_t index, u8 generation) {
    return engine.gateChanged[index][generation];
}

uint16_t getModCV(uint8_t index) {
    return engine.modCvs[index];
}

uint8_t getModGate(uint8_t index) {
    return engine.modGateOn[index];
}


// ----------------------------------------------------------------------------
// internal functions

void updateCounters() {
    if (++engine.spaceCounter >= 16) engine.spaceCounter = 0;
    
    if (++engine.globalCounter >= engine.config.length) {
        reset();
    } else {
        for (uint8_t i = 0; i < TRACKCOUNT; i++) engine.counter[i]++;
    }
}

void updateTrackParameters() {
    engine.divisor[0] = (engine.config.algoX & 3) + 1;
    engine.phase[0] = engine.config.algoX >> 5;
   
    for (uint8_t i = 1; i < TRACKCOUNT; i++) {
        if (engine.config.algoX & (1 << ((i & 3) + 2))) 
            engine.divisor[i] = engine.divisor[i-1] + 1; 
        else 
            engine.divisor[i] = engine.divisor[i-1] - 1;
        if (engine.divisor[i] < 0) engine.divisor[i] = 1 - engine.divisor[i];
        if (engine.divisor[i] == 0) engine.divisor[i] = i + 2;
        engine.phase[i] = ((engine.config.algoX & (0b11 << i)) + i) % engine.divisor[i];
    }
}

void updateTrackValues() {
    engine.totalWeight = 0;
    for (uint8_t i = 0; i < TRACKCOUNT; i++) {
        engine.trackOn[i] = ((engine.counter[i] + engine.phase[i]) / engine.divisor[i]) & 1;
        engine.weightOn[i] = engine.trackOn[i] ? weights[i] : 0;
        engine.totalWeight += engine.weightOn[i];
    }
}

void initHistory(void) {
    for (uint8_t n = 0; n < NOTECOUNT; n++)
        for (uint8_t h = 1; h < HISTORYCOUNT; h++) {
            engine.notes[n][h] = 0;
            engine.gateOn[n][h] = 0;
            engine.gateChanged[n][h] = 0;
        }
}

void pushHistory(void) {
    for (uint8_t n = 0; n < NOTECOUNT; n++)
        for (int8_t h = HISTORYCOUNT - 1; h > 0; h--) {
            engine.notes[n][h] = engine.notes[n][h-1];
            engine.gateOn[n][h] = engine.gateOn[n][h-1];
            engine.gateChanged[n][h] = engine.gateChanged[n][h-1];
        }
}

void calculateNotes(void) {
    for (uint8_t i = 0; i < NOTECOUNT; i++) calculateNextNote(i);
}

void calculateMods() {
    for (uint8_t i = 0; i < MODCOUNT; i++) engine.modGateOn[i] = engine.trackOn[i % TRACKCOUNT];

    engine.modCvs[0] = engine.totalWeight + engine.weightOn[0];
    engine.modCvs[1] = weights[1] * (engine.trackOn[3] + engine.trackOn[2]) + weights[2] * (engine.trackOn[0] + engine.trackOn[2]);
    engine.modCvs[2] = weights[0] * (engine.trackOn[2] + engine.trackOn[1]) + weights[3] * (engine.trackOn[0] + engine.trackOn[3]);
    engine.modCvs[3] = weights[1] * (engine.trackOn[1] + engine.trackOn[2]) + weights[2] * (engine.trackOn[2]  + engine.trackOn[3]) + weights[3] * (engine.trackOn[3] + engine.trackOn[2]);
   
    for (uint8_t i = 0; i < MODCOUNT; i++) engine.modCvs[i] %= 10;
}

void calculateNote(int n) {
    uint16_t note = 0;
    uint8_t mask = engine.config.algoY >> 3;

    for (uint8_t j = 0; j < TRACKCOUNT; j++) {
        if (engine.trackOn[j] && (mask & (1 << (j & 3)))) note += engine.weightOn[j];
    }

    if (engine.config.algoY & 1) note += engine.weightOn[(n + 1) % TRACKCOUNT];
    if (engine.config.algoY & 2) note += engine.weightOn[(n + 2) % TRACKCOUNT];
    if (engine.config.algoY & 4) note += engine.weightOn[(n + 3) % TRACKCOUNT];
   
    note += engine.shifts[n];
    
    uint8_t octave = (note / 12 < 2 ? note / 12 : 2) * 12;
    engine.notes[n][0] = engine.scaleCount[engine.scale] ? engine.scales[engine.scale][note % engine.scaleCount[engine.scale]] + octave : 0;
}
   
void calculateNextNote(int n) {
    uint8_t mask = gatePresets[engine.config.algoY >> 3][n];
    if (mask == 0) mask = 0b1111;
    for (uint8_t i = 0; i < n; i++) mask = ((mask & 1) << 3) | (mask >> 1);
   
    uint8_t gate = 0;
    for (uint8_t j = 0; j < TRACKCOUNT; j++) {
        if (engine.trackOn[j] && (mask & (1 << (j & 3)))) gate = 1;
        // if (mask & (1 << j)) gate = 1;
    }

    if (engine.config.algoY & 1) gate ^= engine.trackOn[n % TRACKCOUNT] << 1;
    if (engine.config.algoY & 2) gate ^= engine.trackOn[(n + 2) % TRACKCOUNT] << 2;
    if (engine.config.algoY & 4) gate ^= engine.trackOn[(n + 3) % TRACKCOUNT] << 3;
    
    uint8_t previousGatesOn = 1;
    for (uint8_t i = 0; i < NOTECOUNT - 1; i++) previousGatesOn &= engine.gateChanged[i][0] & engine.gateOn[i][0];
    if (n == NOTECOUNT - 1 && previousGatesOn) gate = 0;
    
    u8 space = spacePresets[(engine.config.space | n) % SPACEPRESETCOUNT];
    space |= space << 4;
    if (spacePresets[(engine.config.space | n) % SPACEPRESETCOUNT] & engine.spaceCounter) gate = 0;
   
    if (!engine.scaleCount[engine.scale]) gate = 0;
   
    engine.gateChanged[n][0] = engine.gateOn[n][0] != gate;
    engine.gateOn[n][0] = gate;
    if (engine.gateChanged[n][0]) calculateNote(n);
}
