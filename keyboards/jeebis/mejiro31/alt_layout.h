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
#include "combo_fifo.h"

// グローバル変数の宣言
extern bool is_alt_mode;
extern bool force_qwerty_active;
extern uint16_t default_layer;

// 列挙型の宣言（各キーマップで定義）
// enum layer_names の _QWERTY, _GEMINI が必要

typedef struct {
    uint16_t base;
    uint16_t unshifted;
    uint16_t shifted;
} alt_mapping_t;

/**
 * Shift以外のモディファイアが押されているか判定
 * @return アクティブな場合はtrue
 */
static inline bool mods_except_shift_active(void) {
    uint8_t mods = get_mods() | get_weak_mods() | get_oneshot_mods();
    mods &= ~MOD_MASK_SHIFT;
    return mods != 0;
}

// Include layout config (which includes layout definitions)
#include "alt_layout_config.h"

/**
 * Helper function to get current layout's mapping array
 * @return Pointer to the current layout's alt_mapping_t array
 */
static inline const alt_mapping_t* alt_get_current_mappings(void) {
    return ALT_LAYOUT_MAPPINGS;
}

/**
 * Helper function to get current layout ID
 * @return The current layout ID
 */
static inline alt_layout_t alt_get_layout_id(void) {
    return ALT_LAYOUT_ID;
}

/**
 * Helper function to get the number of mappings for the current layout
 * @return Number of mappings in the current layout
 */
static inline uint8_t alt_get_mappings_count(void) {
    return sizeof(ALT_LAYOUT_MAPPINGS) / sizeof(ALT_LAYOUT_MAPPINGS[0]);
}
