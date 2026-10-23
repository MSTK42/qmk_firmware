// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define MATO_KEYS 24
#define MATO_LAYERS 5
#define MATO_COMBOS 8
#define MATO_COMBO_TERM 50
#define MATO_TAPPING_TERM 175
enum { MATO_MO1 = 0x7F01, MATO_DUAL2, MATO_DUAL3, MATO_DUAL4, MATO_SHIFT_SPACE, MATO_SHIFT_ENTER };
typedef void (*mato_emit_fn)(uint16_t code, bool pressed);
typedef struct { uint16_t code, time; uint8_t layer; bool down, pending, sent, suppressed, held; } mato_key_state_t;
typedef struct {
    mato_key_state_t keys[MATO_KEYS];
    bool combos[MATO_COMBOS];
    uint8_t modifier_refs[8];
    mato_emit_fn emit;
} mato_engine_t;
extern const uint16_t mato_maps[MATO_LAYERS][MATO_KEYS];
extern const uint8_t mato_combo_keys[MATO_COMBOS][2];
extern const uint16_t mato_combo_mods[MATO_COMBOS];
void mato_init(mato_engine_t *state, mato_emit_fn emit);
void mato_reset(mato_engine_t *state);
void mato_event(mato_engine_t *state, uint8_t position, bool pressed, uint16_t now);
void mato_tick(mato_engine_t *state, uint16_t now);
uint8_t mato_layer(const mato_engine_t *state);
uint16_t mato_lookup(const mato_engine_t *state, uint8_t position);
