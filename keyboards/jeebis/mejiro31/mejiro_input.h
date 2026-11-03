// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Portable input state machine. No QMK/RMK types or host keycode encoding.
#include <stdbool.h>
#include <stdint.h>
#include "mejiro_layout.h"

#define MJI_PHYSICAL_COUNT 48
#define MJI_TOKEN_COUNT 96
#define MJI_EVENT_COUNT 192
#define MJI_LAYER_COUNT 32
#define MJI_NO_LAYER UINT8_MAX
#define MJI_STICKY_SHIFT UINT16_C(0xfffe)
#define MJI_SHIFT_MASK UINT8_C(0x22)

typedef struct {
    uint16_t tap;
    uint16_t hold_code;
    uint8_t  hold_mods;
    uint8_t  hold_layer;
    bool     skip_alt; // Position is outside the alternative-layout typing area.
    uint8_t  layer; // Layer on which this complete Tap/Hold binding was found.
} mji_binding_t;

typedef struct {
    uint8_t  a, b;
    uint16_t logical;
    uint8_t  priority; // Smaller wins: vertical = 0, horizontal = 1.
} mji_pair_t;

typedef struct {
    uint16_t combo_ms, continuation_ms, long_hold_ms;
} mji_timing_t;

typedef struct {
    uint8_t mods;
    uint8_t layers[MJI_LAYER_COUNT]; // Newest first (mato: highest first); base last.
    uint8_t layer_count;
} mji_context_t;

typedef struct {
    mji_binding_t (*lookup)(void *user, uint16_t logical, const mji_context_t *context);
    void (*key)(void *user, uint8_t source, uint16_t code, bool pressed, uint8_t mods);
    void (*mods)(void *user, uint8_t mods);
    void (*clear)(void *user);
    bool (*decode)(void *user, uint16_t code, mji_key_t *key);
    mji_stroke_t (*encode)(void *user, mji_stroke_t stroke, uint8_t mods);
} mji_callbacks_t;

typedef enum { MJI_FREE, MJI_WAIT, MJI_READY, MJI_PREPARED, MJI_ACTIVE, MJI_ENDED, MJI_CANCELLED } mji_state_t;
typedef struct {
    uint64_t      sources;
    uint32_t      born, ended, activated, order;
    uint16_t      logical;
    uint8_t       pressed_layer;
    mji_binding_t binding;
    mji_stroke_t  tap_stroke, hold_stroke;
    mji_state_t   state;
    bool          released, used, emitted, residual, mods_exposed, binding_resolved;
} mji_token_t;
typedef struct {
    uint8_t token;
    bool    pressed;
} mji_event_t;

typedef struct {
    const mji_pair_t   *pairs;
    uint8_t             pair_count, base_layer;
    mji_timing_t        timing;
    mji_layout_config_t layout;
    mji_callbacks_t     callbacks;
    void               *user;
    mji_token_t         tokens[MJI_TOKEN_COUNT];
    mji_event_t         events[MJI_EVENT_COUNT];
    uint16_t            event_count;
    uint8_t             owner[MJI_PHYSICAL_COUNT]; // Token + 1, or zero.
    uint64_t            physical, ignored;
    uint32_t            now, serial, epoch, next_order;
    uint16_t            overflows;
    int16_t             sticky_target;
    int16_t             output_target;
    bool                output_is_hold;
    bool                sticky_waiting;
    // Opt-in mato policy. Set after mji_init; mji_reset preserves it.
    // Ordinary profiles keep the existing compression and layer semantics.
    bool                mato_mode;
} mji_engine_t;

bool mji_init(mji_engine_t *engine, const mji_pair_t *pairs, uint8_t pair_count, mji_timing_t timing, uint8_t base_layer, mji_callbacks_t callbacks, void *user);
void mji_event(mji_engine_t *engine, uint8_t physical, bool pressed, uint32_t now);
void mji_tick(mji_engine_t *engine, uint32_t now);
// Clears all output, suppresses currently held physical keys until release.
void mji_reset(mji_engine_t *engine, uint8_t base_layer);

// Config selection is an input-context change: release and suppress held keys.
// Map storage belongs to the caller and must outlive the selected config.
bool mji_set_layout(mji_engine_t *engine, const mji_layout_config_t *config);
