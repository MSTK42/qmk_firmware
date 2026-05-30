/*
Copyright 2025 JEEBIS <jeebis.iox@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include QMK_KEYBOARD_H

#define ALT_LAYOUT(name) {name, sizeof(name)/sizeof(name[0])}

// Alternative Layout Definitions

// ====== QWERTY ======
static const alt_mapping_t qwerty[] PROGMEM = {

    {KC_Q,    KC_Q,    KC_Q},
    {KC_W,    KC_W,    KC_W},
    {KC_E,    KC_E,    KC_E},
    {KC_R,    KC_R,    KC_R},
    {KC_T,    KC_T,    KC_T},
    {KC_Y,    KC_Y,    KC_Y},
    {KC_U,    KC_U,    KC_U},
    {KC_I,    KC_I,    KC_I},
    {KC_O,    KC_O,    KC_O},
    {KC_P,    KC_P,    KC_P},
    {KC_MINS, KC_MINS, KC_UNDS},

    {KC_A,    KC_A,    KC_A},
    {KC_S,    KC_S,    KC_S},
    {KC_D,    KC_D,    KC_D},
    {KC_F,    KC_F,    KC_F},
    {KC_G,    KC_G,    KC_G},
    {KC_H,    KC_H,    KC_H},
    {KC_J,    KC_J,    KC_J},
    {KC_K,    KC_K,    KC_K},
    {KC_L,    KC_L,    KC_L},
    {KC_SCLN, KC_SCLN, KC_COLN},
    {KC_QUOT, KC_QUOT, KC_DQUO},

    {KC_Z,    KC_Z,    KC_Z},
    {KC_X,    KC_X,    KC_X},
    {KC_C,    KC_C,    KC_C},
    {KC_V,    KC_V,    KC_V},
    {KC_B,    KC_B,    KC_B},
    {KC_N,    KC_N,    KC_N},
    {KC_M,    KC_M,    KC_M},
    {KC_COMM, KC_COMM, KC_LABK},
    {KC_DOT,  KC_DOT,  KC_RABK},
    {KC_SLSH, KC_SLSH, KC_QUES},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== GRAPHITE ======
static const alt_mapping_t graphite[] PROGMEM = {

    {KC_Q,    KC_B,    KC_B},
    {KC_W,    KC_L,    KC_L},
    {KC_E,    KC_D,    KC_D},
    {KC_R,    KC_W,    KC_W},
    {KC_T,    KC_Z,    KC_Z},
    {KC_Y,    KC_QUOT, KC_UNDS},
    {KC_U,    KC_F,    KC_F},
    {KC_I,    KC_O,    KC_O},
    {KC_O,    KC_U,    KC_U},
    {KC_P,    KC_J,    KC_J},
    {KC_MINS, KC_SCLN, KC_COLN},

    {KC_A,    KC_N,    KC_N},
    {KC_S,    KC_R,    KC_R},
    {KC_D,    KC_T,    KC_T},
    {KC_F,    KC_S,    KC_S},
    {KC_G,    KC_G,    KC_G},
    {KC_H,    KC_Y,    KC_Y},
    {KC_J,    KC_H,    KC_H},
    {KC_K,    KC_A,    KC_A},
    {KC_L,    KC_E,    KC_E},
    {KC_SCLN, KC_I,    KC_I},
    {KC_QUOT, KC_COMM, KC_QUES},

    {KC_Z,    KC_Q,    KC_Q},
    {KC_X,    KC_X,    KC_X},
    {KC_C,    KC_M,    KC_M},
    {KC_V,    KC_C,    KC_C},
    {KC_B,    KC_V,    KC_V},
    {KC_N,    KC_K,    KC_K},
    {KC_M,    KC_P,    KC_P},
    {KC_COMM, KC_DOT,  KC_RABK},
    {KC_DOT,  KC_MINS, KC_DQUO},
    {KC_SLSH, KC_SLSH, KC_LABK},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== COLEMAK ======
static const alt_mapping_t colemak[] PROGMEM = {

    {KC_Q,    KC_Q,    KC_Q},
    {KC_W,    KC_W,    KC_W},
    {KC_E,    KC_F,    KC_F},
    {KC_R,    KC_P,    KC_P},
    {KC_T,    KC_G,    KC_G},
    {KC_Y,    KC_J,    KC_J},
    {KC_U,    KC_L,    KC_L},
    {KC_I,    KC_U,    KC_U},
    {KC_O,    KC_Y,    KC_Y},
    {KC_P,    KC_SCLN, KC_COLN},
    {KC_MINS, KC_MINS, KC_UNDS},

    {KC_A,    KC_A,    KC_A},
    {KC_S,    KC_R,    KC_R},
    {KC_D,    KC_S,    KC_S},
    {KC_F,    KC_T,    KC_T},
    {KC_G,    KC_D,    KC_D},
    {KC_H,    KC_H,    KC_H},
    {KC_J,    KC_N,    KC_N},
    {KC_K,    KC_E,    KC_E},
    {KC_L,    KC_I,    KC_I},
    {KC_SCLN, KC_O,    KC_O},
    {KC_QUOT, KC_QUOT, KC_DQUO},

    {KC_Z,    KC_Z,    KC_Z},
    {KC_X,    KC_X,    KC_X},
    {KC_C,    KC_C,    KC_C},
    {KC_V,    KC_V,    KC_V},
    {KC_B,    KC_B,    KC_B},
    {KC_N,    KC_K,    KC_K},
    {KC_M,    KC_M,    KC_M},
    {KC_COMM, KC_COMM, KC_LABK},
    {KC_DOT,  KC_DOT,  KC_RABK},
    {KC_SLSH, KC_SLSH, KC_QUES},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== COLEMAK_DH ======
static const alt_mapping_t colemak_dh[] PROGMEM = {

    {KC_Q,    KC_Q,    KC_Q},
    {KC_W,    KC_W,    KC_W},
    {KC_E,    KC_F,    KC_F},
    {KC_R,    KC_P,    KC_P},
    {KC_T,    KC_B,    KC_B},
    {KC_Y,    KC_J,    KC_J},
    {KC_U,    KC_L,    KC_L},
    {KC_I,    KC_U,    KC_U},
    {KC_O,    KC_Y,    KC_Y},
    {KC_P,    KC_SCLN, KC_COLN},
    {KC_MINS, KC_MINS, KC_UNDS},

    {KC_A,    KC_A,    KC_A},
    {KC_S,    KC_R,    KC_R},
    {KC_D,    KC_S,    KC_S},
    {KC_F,    KC_T,    KC_T},
    {KC_G,    KC_G,    KC_G},
    {KC_H,    KC_M,    KC_M},
    {KC_J,    KC_N,    KC_N},
    {KC_K,    KC_E,    KC_E},
    {KC_L,    KC_I,    KC_I},
    {KC_SCLN, KC_O,    KC_O},
    {KC_QUOT, KC_QUOT, KC_DQUO},

    {KC_Z,    KC_Z,    KC_Z},
    {KC_X,    KC_X,    KC_X},
    {KC_C,    KC_C,    KC_C},
    {KC_V,    KC_D,    KC_D},
    {KC_B,    KC_V,    KC_V},
    {KC_N,    KC_K,    KC_K},
    {KC_M,    KC_H,    KC_H},
    {KC_COMM, KC_COMM, KC_LABK},
    {KC_DOT,  KC_DOT,  KC_RABK},
    {KC_SLSH, KC_SLSH, KC_QUES},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== DVORAK ======
static const alt_mapping_t dvorak[] PROGMEM = {

    {KC_Q,    KC_QUOT, KC_DQUO},
    {KC_W,    KC_COMM, KC_LABK},
    {KC_E,    KC_DOT,  KC_RABK},
    {KC_R,    KC_P,    KC_P},
    {KC_T,    KC_Y,    KC_Y},
    {KC_Y,    KC_F,    KC_F},
    {KC_U,    KC_G,    KC_G},
    {KC_I,    KC_C,    KC_C},
    {KC_O,    KC_R,    KC_R},
    {KC_P,    KC_L,    KC_L},
    {KC_MINS, KC_SLSH, KC_QUES},

    {KC_A,    KC_A,    KC_A},
    {KC_S,    KC_O,    KC_O},
    {KC_D,    KC_E,    KC_E},
    {KC_F,    KC_U,    KC_U},
    {KC_G,    KC_I,    KC_I},
    {KC_H,    KC_D,    KC_D},
    {KC_J,    KC_H,    KC_H},
    {KC_K,    KC_T,    KC_T},
    {KC_L,    KC_N,    KC_N},
    {KC_SCLN, KC_S,    KC_S},
    {KC_QUOT, KC_MINS, KC_UNDS},

    {KC_Z,    KC_SCLN, KC_COLN},
    {KC_X,    KC_Q,    KC_Q},
    {KC_C,    KC_J,    KC_J},
    {KC_V,    KC_K,    KC_K},
    {KC_B,    KC_X,    KC_X},
    {KC_N,    KC_B,    KC_B},
    {KC_M,    KC_M,    KC_M},
    {KC_COMM, KC_W,    KC_W},
    {KC_DOT,  KC_V,    KC_V},
    {KC_SLSH, KC_Z,    KC_Z},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== WORKMAN ======
static const alt_mapping_t workman[] PROGMEM = {

    {KC_Q,    KC_Q,    KC_Q},
    {KC_W,    KC_D,    KC_D},
    {KC_E,    KC_R,    KC_R},
    {KC_R,    KC_W,    KC_W},
    {KC_T,    KC_B,    KC_B},
    {KC_Y,    KC_J,    KC_J},
    {KC_U,    KC_F,    KC_F},
    {KC_I,    KC_U,    KC_U},
    {KC_O,    KC_P,    KC_P},
    {KC_P,    KC_SCLN, KC_COLN},
    {KC_MINS, KC_MINS, KC_UNDS},

    {KC_A,    KC_A,    KC_A},
    {KC_S,    KC_S,    KC_S},
    {KC_D,    KC_H,    KC_H},
    {KC_F,    KC_T,    KC_T},
    {KC_G,    KC_G,    KC_G},
    {KC_H,    KC_Y,    KC_Y},
    {KC_J,    KC_N,    KC_N},
    {KC_K,    KC_E,    KC_E},
    {KC_L,    KC_O,    KC_O},
    {KC_SCLN, KC_I,    KC_I},
    {KC_QUOT, KC_QUOT, KC_DQUO},

    {KC_Z,    KC_Z,    KC_Z},
    {KC_X,    KC_X,    KC_X},
    {KC_C,    KC_M,    KC_M},
    {KC_V,    KC_C,    KC_C},
    {KC_B,    KC_V,    KC_V},
    {KC_N,    KC_K,    KC_K},
    {KC_M,    KC_L,    KC_L},
    {KC_COMM, KC_COMM, KC_LABK},
    {KC_DOT,  KC_DOT,  KC_RABK},
    {KC_SLSH, KC_SLSH, KC_QUES},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== HANDSDOWN_NEU ======
static const alt_mapping_t handsdown_neu[] PROGMEM = {

    {KC_Q,    KC_W,    KC_W},
    {KC_W,    KC_F,    KC_F},
    {KC_E,    KC_M,    KC_M},
    {KC_R,    KC_P,    KC_P},
    {KC_T,    KC_V,    KC_V},
    {KC_Y,    KC_SLSH, KC_QUES},
    {KC_U,    KC_DOT,  KC_RABK},
    {KC_I,    KC_Q,    KC_Q},
    {KC_O,    KC_SCLN, KC_COLN},
    {KC_P,    KC_QUOT, KC_DQUO},
    {KC_MINS, KC_Z,    KC_Z},

    {KC_A,    KC_R,    KC_R},
    {KC_S,    KC_S,    KC_S},
    {KC_D,    KC_N,    KC_N},
    {KC_F,    KC_T,    KC_T},
    {KC_G,    KC_B,    KC_B},
    {KC_H,    KC_COMM, KC_LABK},
    {KC_J,    KC_A,    KC_A},
    {KC_K,    KC_E,    KC_E},
    {KC_L,    KC_I,    KC_I},
    {KC_SCLN, KC_H,    KC_H},
    {KC_QUOT, KC_J,    KC_J},

    {KC_Z,    KC_X,    KC_X},
    {KC_X,    KC_C,    KC_C},
    {KC_C,    KC_L,    KC_L},
    {KC_V,    KC_D,    KC_D},
    {KC_B,    KC_G,    KC_G},
    {KC_N,    KC_MINS, KC_UNDS},
    {KC_M,    KC_U,    KC_U},
    {KC_COMM, KC_O,    KC_O},
    {KC_DOT,  KC_Y,    KC_Y},
    {KC_SLSH, KC_K,    KC_K},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== STURDY ======
static const alt_mapping_t sturdy[] PROGMEM = {

    {KC_Q,    KC_V,    KC_V},
    {KC_W,    KC_M,    KC_M},
    {KC_E,    KC_L,    KC_L},
    {KC_R,    KC_C,    KC_C},
    {KC_T,    KC_P,    KC_P},
    {KC_Y,    KC_X,    KC_X},
    {KC_U,    KC_F,    KC_F},
    {KC_I,    KC_O,    KC_O},
    {KC_O,    KC_U,    KC_U},
    {KC_P,    KC_J,    KC_J},
    {KC_MINS, KC_MINS, KC_UNDS},

    {KC_A,    KC_S,    KC_S},
    {KC_S,    KC_T,    KC_T},
    {KC_D,    KC_R,    KC_R},
    {KC_F,    KC_D,    KC_D},
    {KC_G,    KC_Y,    KC_Y},
    {KC_H,    KC_DOT,  KC_RABK},
    {KC_J,    KC_N,    KC_N},
    {KC_K,    KC_A,    KC_A},
    {KC_L,    KC_E,    KC_E},
    {KC_SCLN, KC_I,    KC_I},
    {KC_QUOT, KC_SLSH, KC_QUES},

    {KC_Z,    KC_Z,    KC_Z},
    {KC_X,    KC_K,    KC_K},
    {KC_C,    KC_Q,    KC_Q},
    {KC_V,    KC_G,    KC_G},
    {KC_B,    KC_W,    KC_W},
    {KC_N,    KC_B,    KC_B},
    {KC_M,    KC_H,    KC_H},
    {KC_COMM, KC_QUOT, KC_DQUO},
    {KC_DOT,  KC_SCLN, KC_COLN},
    {KC_SLSH, KC_COMM, KC_LABK},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== ENGRAM ======
static const alt_mapping_t engram[] PROGMEM = {

    {KC_Q,    KC_B,    KC_B},
    {KC_W,    KC_Y,    KC_Y},
    {KC_E,    KC_O,    KC_O},
    {KC_R,    KC_U,    KC_U},
    {KC_T,    KC_QUOT, KC_LPRN},
    {KC_Y,    KC_DQUO, KC_RPRN},
    {KC_U,    KC_L,    KC_L},
    {KC_I,    KC_D,    KC_D},
    {KC_O,    KC_W,    KC_W},
    {KC_P,    KC_V,    KC_V},
    {KC_MINS, KC_Z,    KC_Z},

    {KC_A,    KC_C,    KC_C},
    {KC_S,    KC_I,    KC_I},
    {KC_D,    KC_E,    KC_E},
    {KC_F,    KC_A,    KC_A},
    {KC_G,    KC_COMM, KC_SCLN},
    {KC_H,    KC_DOT,  KC_COLN},
    {KC_J,    KC_H,    KC_H},
    {KC_K,    KC_T,    KC_T},
    {KC_L,    KC_S,    KC_S},
    {KC_SCLN, KC_N,    KC_N},
    {KC_QUOT, KC_Q,    KC_Q},

    {KC_Z,    KC_G,    KC_G},
    {KC_X,    KC_X,    KC_X},
    {KC_C,    KC_J,    KC_J},
    {KC_V,    KC_K,    KC_K},
    {KC_B,    KC_MINS, KC_UNDS},
    {KC_N,    KC_QUES, KC_EXLM},
    {KC_M,    KC_R,    KC_R},
    {KC_COMM, KC_M,    KC_M},
    {KC_DOT,  KC_F,    KC_F},
    {KC_SLSH, KC_P,    KC_P},
    {KC_BSLS, KC_SLSH, KC_BSLS},

};

// ====== GALLIUM ======
static const alt_mapping_t gallium[] PROGMEM = {

    {KC_Q,    KC_B,    KC_B},
    {KC_W,    KC_L,    KC_L},
    {KC_E,    KC_D,    KC_D},
    {KC_R,    KC_C,    KC_C},
    {KC_T,    KC_V,    KC_V},
    {KC_Y,    KC_J,    KC_J},
    {KC_U,    KC_Y,    KC_Y},
    {KC_I,    KC_O,    KC_O},
    {KC_O,    KC_U,    KC_U},
    {KC_P,    KC_COMM, KC_LABK},
    {KC_MINS, KC_SLSH, KC_QUES},

    {KC_A,    KC_N,    KC_N},
    {KC_S,    KC_R,    KC_R},
    {KC_D,    KC_T,    KC_T},
    {KC_F,    KC_S,    KC_S},
    {KC_G,    KC_G,    KC_G},
    {KC_H,    KC_P,    KC_P},
    {KC_J,    KC_H,    KC_H},
    {KC_K,    KC_A,    KC_A},
    {KC_L,    KC_E,    KC_E},
    {KC_SCLN, KC_I,    KC_I},
    {KC_QUOT, KC_MINS, KC_UNDS},

    {KC_Z,    KC_X,    KC_X},
    {KC_X,    KC_Q,    KC_Q},
    {KC_C,    KC_M,    KC_M},
    {KC_V,    KC_W,    KC_W},
    {KC_B,    KC_Z,    KC_Z},
    {KC_N,    KC_K,    KC_K},
    {KC_M,    KC_F,    KC_F},
    {KC_COMM, KC_QUOT, KC_DQUO},
    {KC_DOT,  KC_SCLN, KC_COLN},
    {KC_SLSH, KC_DOT,  KC_RABK},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== CANARY ======
static const alt_mapping_t canary[] PROGMEM = {

    {KC_Q,    KC_W,    KC_W},
    {KC_W,    KC_L,    KC_L},
    {KC_E,    KC_Y,    KC_Y},
    {KC_R,    KC_P,    KC_P},
    {KC_T,    KC_B,    KC_B},
    {KC_Y,    KC_Z,    KC_Z},
    {KC_U,    KC_F,    KC_F},
    {KC_I,    KC_O,    KC_O},
    {KC_O,    KC_U,    KC_U},
    {KC_P,    KC_QUOT, KC_DQUO},
    {KC_MINS, KC_MINS, KC_UNDS},

    {KC_A,    KC_C,    KC_C},
    {KC_S,    KC_R,    KC_R},
    {KC_D,    KC_S,    KC_S},
    {KC_F,    KC_T,    KC_T},
    {KC_G,    KC_G,    KC_G},
    {KC_H,    KC_M,    KC_M},
    {KC_J,    KC_N,    KC_N},
    {KC_K,    KC_E,    KC_E},
    {KC_L,    KC_I,    KC_I},
    {KC_SCLN, KC_A,    KC_A},
    {KC_QUOT, KC_SCLN, KC_COLN},

    {KC_Z,    KC_Q,    KC_Q},
    {KC_X,    KC_J,    KC_J},
    {KC_C,    KC_V,    KC_V},
    {KC_V,    KC_D,    KC_D},
    {KC_B,    KC_K,    KC_K},
    {KC_N,    KC_X,    KC_X},
    {KC_M,    KC_H,    KC_H},
    {KC_COMM, KC_SLSH, KC_QUES},
    {KC_DOT,  KC_COMM, KC_LABK},
    {KC_SLSH, KC_DOT,  KC_RABK},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== ASTARTE ======
static const alt_mapping_t astarte[] PROGMEM = {

    {KC_Q,    KC_Q,    KC_Q},
    {KC_W,    KC_P,    KC_P},
    {KC_E,    KC_U,    KC_U},
    {KC_R,    KC_Y,    KC_Y},
    {KC_T,    KC_COMM, KC_LABK},
    {KC_Y,    KC_J,    KC_J},
    {KC_U,    KC_D,    KC_D},
    {KC_I,    KC_H,    KC_H},
    {KC_O,    KC_G,    KC_G},
    {KC_P,    KC_W,    KC_W},
    {KC_MINS, KC_MINS, KC_UNDS},

    {KC_A,    KC_I,    KC_I},
    {KC_S,    KC_O,    KC_O},
    {KC_D,    KC_E,    KC_E},
    {KC_F,    KC_A,    KC_A},
    {KC_G,    KC_DOT,  KC_RABK},
    {KC_H,    KC_K,    KC_K},
    {KC_J,    KC_T,    KC_T},
    {KC_K,    KC_N,    KC_N},
    {KC_L,    KC_S,    KC_S},
    {KC_SCLN, KC_R,    KC_R},
    {KC_QUOT, KC_QUOT, KC_DQUO},

    {KC_Z,    KC_Z,    KC_Z},
    {KC_X,    KC_X,    KC_X},
    {KC_C,    KC_SLSH, KC_QUES},
    {KC_V,    KC_C,    KC_C},
    {KC_B,    KC_SCLN, KC_COLN},
    {KC_N,    KC_M,    KC_M},
    {KC_M,    KC_L,    KC_L},
    {KC_COMM, KC_F,    KC_F},
    {KC_DOT,  KC_B,    KC_B},
    {KC_SLSH, KC_V,    KC_V},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== BOO ======
static const alt_mapping_t boo[] PROGMEM = {

    {KC_Q,    KC_COMM, KC_LABK},
    {KC_W,    KC_DOT,  KC_RABK},
    {KC_E,    KC_U,    KC_U},
    {KC_R,    KC_C,    KC_C},
    {KC_T,    KC_V,    KC_V},
    {KC_Y,    KC_Q,    KC_Q},
    {KC_U,    KC_F,    KC_F},
    {KC_I,    KC_D,    KC_D},
    {KC_O,    KC_L,    KC_L},
    {KC_P,    KC_Y,    KC_Y},
    {KC_MINS, KC_SLSH, KC_QUES},

    {KC_A,    KC_A,    KC_A},
    {KC_S,    KC_O,    KC_O},
    {KC_D,    KC_E,    KC_E},
    {KC_F,    KC_S,    KC_S},
    {KC_G,    KC_G,    KC_G},
    {KC_H,    KC_B,    KC_B},
    {KC_J,    KC_N,    KC_N},
    {KC_K,    KC_T,    KC_T},
    {KC_L,    KC_R,    KC_R},
    {KC_SCLN, KC_I,    KC_I},
    {KC_QUOT, KC_MINS, KC_UNDS},

    {KC_Z,    KC_SCLN, KC_COLN},
    {KC_X,    KC_X,    KC_X},
    {KC_C,    KC_QUOT, KC_DQUO},
    {KC_V,    KC_W,    KC_W},
    {KC_B,    KC_Z,    KC_Z},
    {KC_N,    KC_P,    KC_P},
    {KC_M,    KC_H,    KC_H},
    {KC_COMM, KC_M,    KC_M},
    {KC_DOT,  KC_K,    KC_K},
    {KC_SLSH, KC_J,    KC_J},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== EUCALYN ======
static const alt_mapping_t eucalyn[] PROGMEM = {

    {KC_Q,    KC_Q,    KC_Q},
    {KC_W,    KC_W,    KC_W},
    {KC_E,    KC_COMM, KC_LABK},
    {KC_R,    KC_DOT,  KC_RABK},
    {KC_T,    KC_SCLN, KC_COLN},
    {KC_Y,    KC_M,    KC_M},
    {KC_U,    KC_R,    KC_R},
    {KC_I,    KC_D,    KC_D},
    {KC_O,    KC_Y,    KC_Y},
    {KC_P,    KC_P,    KC_P},
    {KC_MINS, KC_MINS, KC_UNDS},

    {KC_A,    KC_A,    KC_A},
    {KC_S,    KC_O,    KC_O},
    {KC_D,    KC_E,    KC_E},
    {KC_F,    KC_I,    KC_I},
    {KC_G,    KC_U,    KC_U},
    {KC_H,    KC_G,    KC_G},
    {KC_J,    KC_T,    KC_T},
    {KC_K,    KC_K,    KC_K},
    {KC_L,    KC_S,    KC_S},
    {KC_SCLN, KC_N,    KC_N},
    {KC_QUOT, KC_QUOT, KC_DQUO},

    {KC_Z,    KC_Z,    KC_Z},
    {KC_X,    KC_X,    KC_X},
    {KC_C,    KC_C,    KC_C},
    {KC_V,    KC_V,    KC_V},
    {KC_B,    KC_F,    KC_F},
    {KC_N,    KC_B,    KC_B},
    {KC_M,    KC_H,    KC_H},
    {KC_COMM, KC_J,    KC_J},
    {KC_DOT,  KC_L,    KC_L},
    {KC_SLSH, KC_SLSH, KC_QUES},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== EUCALYN_BIACCO ======
static const alt_mapping_t eucalyn_biacco[] PROGMEM = {

    {KC_Q,    KC_SCLN, KC_COLN},
    {KC_W,    KC_COMM, KC_LABK},
    {KC_E,    KC_DOT,  KC_RABK},
    {KC_R,    KC_P,    KC_P},
    {KC_T,    KC_Q,    KC_Q},
    {KC_Y,    KC_Y,    KC_Y},
    {KC_U,    KC_G,    KC_G},
    {KC_I,    KC_D,    KC_D},
    {KC_O,    KC_M,    KC_M},
    {KC_P,    KC_F,    KC_F},
    {KC_MINS, KC_MINS, KC_UNDS},

    {KC_A,    KC_A,    KC_A},
    {KC_S,    KC_O,    KC_O},
    {KC_D,    KC_E,    KC_E},
    {KC_F,    KC_I,    KC_I},
    {KC_G,    KC_U,    KC_U},
    {KC_H,    KC_B,    KC_B},
    {KC_J,    KC_N,    KC_N},
    {KC_K,    KC_T,    KC_T},
    {KC_L,    KC_R,    KC_R},
    {KC_SCLN, KC_S,    KC_S},
    {KC_QUOT, KC_QUOT, KC_DQUO},

    {KC_Z,    KC_Z,    KC_Z},
    {KC_X,    KC_X,    KC_X},
    {KC_C,    KC_C,    KC_C},
    {KC_V,    KC_V,    KC_V},
    {KC_B,    KC_W,    KC_W},
    {KC_N,    KC_H,    KC_H},
    {KC_M,    KC_J,    KC_J},
    {KC_COMM, KC_K,    KC_K},
    {KC_DOT,  KC_L,    KC_L},
    {KC_SLSH, KC_SLSH, KC_QUES},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== MERLIN ======
static const alt_mapping_t merlin[] PROGMEM = {

    {KC_Q,    KC_G,    KC_G},
    {KC_W,    KC_M,    KC_M},
    {KC_E,    KC_H,    KC_H},
    {KC_R,    KC_R,    KC_R},
    {KC_T,    KC_W,    KC_W},
    {KC_Y,    KC_Q,    KC_Q},
    {KC_U,    KC_Y,    KC_Y},
    {KC_I,    KC_V,    KC_V},
    {KC_O,    KC_L,    KC_L},
    {KC_P,    KC_SCLN, KC_COLN},
    {KC_MINS, KC_QUOT, KC_DQUO},

    {KC_A,    KC_S,    KC_S},
    {KC_S,    KC_K,    KC_K},
    {KC_D,    KC_N,    KC_N},
    {KC_F,    KC_T,    KC_T},
    {KC_G,    KC_D,    KC_D},
    {KC_H,    KC_U,    KC_U},
    {KC_J,    KC_I,    KC_I},
    {KC_K,    KC_A,    KC_A},
    {KC_L,    KC_O,    KC_O},
    {KC_SCLN, KC_E,    KC_E},
    {KC_QUOT, KC_MINS, KC_UNDS},

    {KC_Z,    KC_Z,    KC_Z},
    {KC_X,    KC_P,    KC_P},
    {KC_C,    KC_J,    KC_J},
    {KC_V,    KC_B,    KC_B},
    {KC_B,    KC_F,    KC_F},
    {KC_N,    KC_C,    KC_C},
    {KC_M,    KC_X,    KC_X},
    {KC_COMM, KC_COMM, KC_LABK},
    {KC_DOT,  KC_DOT,  KC_RABK},
    {KC_SLSH, KC_SLSH, KC_QUES},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== O24 ======
static const alt_mapping_t o24[] PROGMEM = {

    {KC_Q,    KC_Q,    KC_Q},
    {KC_W,    KC_L,    KC_L},
    {KC_E,    KC_U,    KC_U},
    {KC_R,    KC_F,    KC_F},
    {KC_T,    KC_MINS, KC_UNDS},
    {KC_Y,    KC_K,    KC_K},
    {KC_U,    KC_W,    KC_W},
    {KC_I,    KC_R,    KC_R},
    {KC_O,    KC_Y,    KC_Y},
    {KC_P,    KC_P,    KC_P},
    {KC_MINS, KC_MINS, KC_UNDS},

    {KC_A,    KC_E,    KC_E},
    {KC_S,    KC_I,    KC_I},
    {KC_D,    KC_A,    KC_A},
    {KC_F,    KC_O,    KC_O},
    {KC_G,    KC_G,    KC_G},
    {KC_H,    KC_H,    KC_H},
    {KC_J,    KC_T,    KC_T},
    {KC_K,    KC_N,    KC_N},
    {KC_L,    KC_S,    KC_S},
    {KC_SCLN, KC_H,    KC_H},
    {KC_QUOT, KC_QUOT, KC_DQUO},

    {KC_Z,    KC_Z,    KC_Z},
    {KC_X,    KC_X,    KC_X},
    {KC_C,    KC_C,    KC_C},
    {KC_V,    KC_V,    KC_V},
    {KC_B,    KC_COMM, KC_LABK},
    {KC_N,    KC_G,    KC_G},
    {KC_M,    KC_D,    KC_D},
    {KC_COMM, KC_M,    KC_M},
    {KC_DOT,  KC_J,    KC_J},
    {KC_SLSH, KC_B,    KC_B},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};

// ====== TOMISUKE ======
static const alt_mapping_t tomisuke[] PROGMEM = {

    {KC_Q,    KC_QUOT, KC_DQUO},
    {KC_W,    KC_COMM, KC_LABK},
    {KC_E,    KC_DOT,  KC_RABK},
    {KC_R,    KC_MINS, KC_UNDS},
    {KC_T,    KC_SCLN, KC_COLN},
    {KC_Y,    KC_L,    KC_L},
    {KC_U,    KC_R,    KC_R},
    {KC_I,    KC_D,    KC_D},
    {KC_O,    KC_Y,    KC_Y},
    {KC_P,    KC_P,    KC_P},
    {KC_MINS, KC_SLSH, KC_QUES},

    {KC_A,    KC_A,    KC_A},
    {KC_S,    KC_O,    KC_O},
    {KC_D,    KC_E,    KC_E},
    {KC_F,    KC_I,    KC_I},
    {KC_G,    KC_U,    KC_U},
    {KC_H,    KC_G,    KC_G},
    {KC_J,    KC_N,    KC_N},
    {KC_K,    KC_T,    KC_T},
    {KC_L,    KC_S,    KC_S},
    {KC_SCLN, KC_K,    KC_K},
    {KC_QUOT, KC_F,    KC_F},

    {KC_Z,    KC_X,    KC_X},
    {KC_X,    KC_C,    KC_C},
    {KC_C,    KC_V,    KC_V},
    {KC_V,    KC_W,    KC_W},
    {KC_B,    KC_Q,    KC_Q},
    {KC_N,    KC_J,    KC_J},
    {KC_M,    KC_H,    KC_H},
    {KC_COMM, KC_M,    KC_M},
    {KC_DOT,  KC_B,    KC_B},
    {KC_SLSH, KC_Z,    KC_Z},
    {KC_BSLS, KC_BSLS, KC_PIPE},

};
