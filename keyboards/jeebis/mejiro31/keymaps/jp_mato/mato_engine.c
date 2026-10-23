// SPDX-License-Identifier: GPL-2.0-or-later
// This engine is shared by QMK and the executable host tests.
#include "mato_engine.h"
#include "keycodes.h"
#include <string.h>

static uint16_t elapsed(uint16_t now, uint16_t then) { return (uint16_t)(now - then); }
static bool dual(uint16_t c) { return c >= MATO_DUAL2 && c <= MATO_SHIFT_ENTER; }
static uint8_t layer_for(uint16_t c) {
    if (c == MATO_MO1) return 1;
    if (c >= MATO_DUAL2 && c <= MATO_DUAL4) return (uint8_t)(c - MATO_DUAL2 + 2);
    return 0;
}
static uint16_t tap_for(uint16_t c) {
    switch (c) {
        case MATO_DUAL2: return KC_GRV;
        case MATO_DUAL3: case MATO_SHIFT_SPACE: return KC_SPC;
        case MATO_DUAL4: case MATO_SHIFT_ENTER: return KC_ENT;
        default: return KC_NO;
    }
}
static void emit(mato_engine_t *s, uint16_t code, bool pressed) {
    if (code == KC_NO || code == KC_TRNS) return;
    // Merge encoded shortcut modifiers with held combos/thumb modifiers.
    if (code >= 0x0100 && code <= 0x1FFF) {
        uint8_t mods = (code >> 8) & 0x0F;
        uint16_t base = (code & 0x1000) ? KC_RCTL : KC_LCTL;
        if (!pressed) emit(s, code & 0xFF, false);
        for (uint8_t i = 0; i < 4; i++) if (mods & (1 << i)) emit(s, base + i, pressed);
        if (pressed) emit(s, code & 0xFF, true);
        return;
    }
    if (code >= KC_LCTL && code <= KC_RGUI) {
        uint8_t *ref = &s->modifier_refs[code - KC_LCTL];
        if (pressed) { if ((*ref)++ != 0) return; }
        else { if (*ref == 0 || --(*ref) != 0) return; }
    }
    s->emit(code, pressed);
}
void mato_init(mato_engine_t *s, mato_emit_fn output) { memset(s, 0, sizeof(*s)); s->emit = output; }
uint8_t mato_layer(const mato_engine_t *s) {
    uint8_t layer = 0;
    for (uint8_t i = 0; i < MATO_KEYS; i++) if (s->keys[i].down && s->keys[i].held) {
        uint8_t candidate = layer_for(s->keys[i].code);
        if (candidate > layer) layer = candidate;
    }
    return layer;
}
uint16_t mato_lookup(const mato_engine_t *s, uint8_t position) {
    if (position >= MATO_KEYS) return KC_NO;
    // Only active held layers participate; transparent cells fall back to base.
    for (int layer = MATO_LAYERS - 1; layer > 0; layer--) {
        bool active = false;
        for (uint8_t i = 0; i < MATO_KEYS; i++) if (s->keys[i].down && s->keys[i].held && layer_for(s->keys[i].code) == layer) active = true;
        if (active && mato_maps[layer][position] != KC_TRNS) return mato_maps[layer][position];
    }
    return mato_maps[0][position];
}
static void send_pending(mato_engine_t *s, uint8_t i) {
    mato_key_state_t *k = &s->keys[i];
    if (!k->pending || dual(k->code)) return;
    k->pending = false; k->sent = true; emit(s, k->code, true);
}
static void flush_singles(mato_engine_t *s, uint16_t now, int except) {
    for (;;) {
        int oldest = -1; uint16_t age = 0;
        for (uint8_t i = 0; i < MATO_KEYS; i++) if (i != except && s->keys[i].pending && !dual(s->keys[i].code)) {
            uint16_t a = elapsed(now, s->keys[i].time);
            if (oldest < 0 || a > age) { oldest = i; age = a; }
        }
        if (oldest < 0) break;
        send_pending(s, oldest);
    }
}
static void promote(mato_engine_t *s, uint8_t i) {
    mato_key_state_t *k = &s->keys[i];
    if (!k->down || !k->pending || !dual(k->code)) return;
    k->pending = false; k->held = true;
    if (k->code == MATO_SHIFT_SPACE || k->code == MATO_SHIFT_ENTER) emit(s, KC_LSFT, true);
}
static int combo_for(uint8_t p) {
    for (uint8_t i = 0; i < MATO_COMBOS; i++) if (mato_combo_keys[i][0] == p || mato_combo_keys[i][1] == p) return i;
    return -1;
}
void mato_tick(mato_engine_t *s, uint16_t now) {
    // Flush ordinary keys in press order, not position order.
    for (;;) {
        int oldest = -1; uint16_t age = 0;
        for (uint8_t i = 0; i < MATO_KEYS; i++) if (s->keys[i].down && s->keys[i].pending) {
            uint16_t a = elapsed(now, s->keys[i].time);
            uint16_t limit = dual(s->keys[i].code) ? MATO_TAPPING_TERM : MATO_COMBO_TERM;
            if (a >= limit && (oldest < 0 || a > age)) { oldest = i; age = a; }
        }
        if (oldest < 0) break;
        if (dual(s->keys[oldest].code)) promote(s, oldest); else send_pending(s, oldest);
    }
}
void mato_event(mato_engine_t *s, uint8_t p, bool pressed, uint16_t now) {
    if (p >= MATO_KEYS) return;
    mato_tick(s, now);
    mato_key_state_t *k = &s->keys[p];
    if (pressed) {
        if (k->down) return;
        // Interrupting a dual-role thumb chooses its held action before lookup.
        for (uint8_t i = 0; i < MATO_KEYS; i++) if (i != p) promote(s, i);
        uint8_t layer = mato_layer(s);
        int ci = layer <= 1 ? combo_for(p) : -1;
        int partner = ci < 0 ? -1 : mato_combo_keys[ci][0] == p ? mato_combo_keys[ci][1] : mato_combo_keys[ci][0];
        bool can_combine = partner >= 0 && s->keys[partner].down && s->keys[partner].pending &&
            !s->keys[partner].suppressed && s->keys[partner].layer == layer &&
            elapsed(now, s->keys[partner].time) < MATO_COMBO_TERM;
        // A different next key commits earlier singles in chronological order.
        flush_singles(s, now, can_combine ? partner : -1);
        *k = (mato_key_state_t){.code = mato_lookup(s, p), .time = now, .layer = layer, .down = true};
        if (k->code == MATO_MO1) { k->held = true; return; }
        if (dual(k->code)) { k->pending = true; return; }
        if (ci >= 0) {
            mato_key_state_t *other = &s->keys[partner];
            if (other->down && other->pending && !other->suppressed && other->layer == layer && elapsed(now, other->time) < MATO_COMBO_TERM) {
                other->pending = false; other->suppressed = true; k->suppressed = true;
                s->combos[ci] = true; emit(s, mato_combo_mods[ci], true); return;
            }
            k->pending = true;
        } else { k->sent = true; emit(s, k->code, true); }
    } else {
        if (!k->down) return;
        int ci = combo_for(p);
        bool releasing_modifier = (ci >= 0 && s->combos[ci]) || (k->code >= KC_LCTL && k->code <= KC_RGUI) ||
            (k->sent && k->code >= 0x0100 && k->code <= 0x1FFF) ||
            (k->held && (k->code == MATO_SHIFT_SPACE || k->code == MATO_SHIFT_ENTER));
        if (releasing_modifier) flush_singles(s, now, -1);
        if (ci >= 0 && s->combos[ci]) { emit(s, mato_combo_mods[ci], false); s->combos[ci] = false; }
        if (k->pending) {
            uint16_t code = dual(k->code) ? tap_for(k->code) : k->code;
            emit(s, code, true); emit(s, code, false);
        } else if (k->held && (k->code == MATO_SHIFT_SPACE || k->code == MATO_SHIFT_ENTER)) emit(s, KC_LSFT, false);
        else if (k->sent) emit(s, k->code, false);
        memset(k, 0, sizeof(*k));
    }
}
void mato_reset(mato_engine_t *s) {
    // Cancel pending text; release exactly the reports this engine owns.
    for (uint8_t i = 0; i < MATO_KEYS; i++) {
        if (s->keys[i].sent) emit(s, s->keys[i].code, false);
        if (s->keys[i].held && (s->keys[i].code == MATO_SHIFT_SPACE || s->keys[i].code == MATO_SHIFT_ENTER)) emit(s, KC_LSFT, false);
    }
    for (uint8_t i = 0; i < MATO_COMBOS; i++) if (s->combos[i]) emit(s, mato_combo_mods[i], false);
    mato_emit_fn output = s->emit; mato_init(s, output);
}
