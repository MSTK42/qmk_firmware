// SPDX-License-Identifier: GPL-2.0-or-later
// Generated from mato-source.json. Source: https://note.com/mato_kb/n/nf57efe5d6ca0
#include "mato_engine.h"
#include "quantum_keycodes.h"
#include "keymap_japanese.h"
const uint16_t mato_maps[MATO_LAYERS][MATO_KEYS] = {
    {KC_L, KC_W, KC_K, KC_R, KC_P, KC_TAB, KC_U, KC_O, KC_Y, JP_MINS, KC_H, KC_S, KC_T, KC_N, KC_M, KC_BSPC, KC_A, KC_E, KC_I, JP_COMM, MATO_DUAL2, MATO_DUAL3, MATO_DUAL4, MATO_MO1},
    {KC_X, KC_V, KC_G, KC_F, KC_Q, KC_ESC, KC_U, KC_O, KC_Y, JP_SLSH, KC_B, KC_Z, KC_D, KC_C, KC_J, KC_DEL, KC_A, KC_E, KC_I, JP_DOT, KC_LSFT, MATO_SHIFT_SPACE, KC_NO, KC_TRNS},
    {JP_BSLS, JP_YEN, JP_COLN, JP_SCLN, KC_NO, KC_NO, JP_AT, JP_LBRC, JP_RBRC, JP_CIRC, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_TRNS, KC_NO, MATO_SHIFT_ENTER, KC_LSFT},
    {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_UP, KC_NO, KC_NO, LCTL(KC_Z), LCTL(KC_X), LCTL(KC_C), LCTL(KC_V), KC_NO, KC_NO, KC_LEFT, KC_DOWN, KC_RIGHT, KC_NO, KC_NO, KC_TRNS, KC_NO, KC_LSFT},
    {KC_F11, KC_F12, KC_F13, KC_F14, KC_F15, KC_F16, KC_F17, KC_F18, KC_F19, KC_F20, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10, KC_NO, KC_NO, KC_TRNS, KC_NO},
};
const uint8_t mato_combo_keys[MATO_COMBOS][2] = {
    {0, 10},
    {1, 11},
    {2, 12},
    {3, 13},
    {6, 16},
    {7, 17},
    {8, 18},
    {9, 19},
};
const uint16_t mato_combo_mods[MATO_COMBOS] = {
    KC_LSFT, KC_LGUI, KC_LALT, KC_LCTL, KC_LCTL, KC_LALT, KC_LGUI, KC_LSFT
};
