// SPDX-License-Identifier: GPL-2.0-or-later
// Reuse the maintained legacy keymap unchanged. Only this opt-in keymap exports wrappers.
#define process_record_user legacy_process_record_user
#define matrix_scan_user legacy_matrix_scan_user
#define keyboard_post_init_user legacy_keyboard_post_init_user
#include "../jp_default/keymap.c"
#undef process_record_user
#undef matrix_scan_user
#undef keyboard_post_init_user
#include "mato_engine.h"

static mato_engine_t mato;
static bool mato_enabled = false;
static uint8_t mato_lang = 2;
static uint8_t mato_saved_o24_lang = 1;
enum { MATO_REQUEST_NONE, MATO_REQUEST_ON, MATO_REQUEST_OFF, MATO_REQUEST_MEJIRO };
static uint8_t mato_request = MATO_REQUEST_NONE;
static uint32_t mato_owned = 0;
static uint64_t mato_control_owned = 0;

static int8_t mato_position(uint8_t row, uint8_t col) {
    if (row <= 1 && col >= 1 && col <= 10) return (int8_t)(row * 10 + col - 1);
    if (row == 3 && col >= 4 && col <= 7) return (int8_t)(20 + col - 4);
    return -1;
}
static bool mato_all_released(void) {
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) if (matrix_get_row(r)) return false;
    return true;
}
static void mato_emit(uint16_t code, bool pressed) {
    // Tables already contain JIS keycodes: do not apply O24/JIS transformations twice.
    if (pressed) register_code16(code); else unregister_code16(code);
}
static void mato_apply_request(void) {
    if (!mato_request || !mato_all_released() || combo_fifo_len || get_mods() || get_weak_mods() || get_oneshot_mods()) return;
    mato_reset(&mato); mato_owned = 0; mato_control_owned = 0;
    if (mato_request == MATO_REQUEST_ON) {
        mato_saved_o24_lang = kbd_lang;
        kbd_lang = mato_lang;
        mato_enabled = true;
        if (default_layer != _QWERTY) toggle_keyboard_mejiro_layer();
        update_lang(mato_lang);
    } else if (mato_request == MATO_REQUEST_OFF) {
        mato_enabled = false;
        kbd_lang = mato_saved_o24_lang;
        if (default_layer != _QWERTY) toggle_keyboard_mejiro_layer();
        else update_lang(kbd_lang);
    } else if (default_layer == _QWERTY) {
        toggle_keyboard_mejiro_layer();
    }
    mato_request = MATO_REQUEST_NONE;
}
void keyboard_post_init_user(void) {
    alt_en_layout = (alt_layout_def_t)ALT_LAYOUT(o24);
    alt_jp_layout = (alt_layout_def_t)ALT_LAYOUT(o24);
    legacy_keyboard_post_init_user();
    mato_init(&mato, mato_emit);
    // Boot in O24; no new EEPROM schema or mato persistence.
    mato_enabled = false;
}
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    uint8_t row = record->event.key.row, col = record->event.key.col;
    uint64_t bit = (uint64_t)1 << (row * MATRIX_COLS + col);
    int8_t position = mato_position(row, col);
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);

    // Release ownership survives virtual-layer changes, mode requests and legacy edge toggles.
    if (!record->event.pressed && (mato_control_owned & bit)) {
        update_o24_edge_toggle_other(keycode, record);
        mato_control_owned &= ~bit; return false;
    }
    if (!record->event.pressed && position >= 0 && (mato_owned & ((uint32_t)1 << position))) {
        update_o24_edge_toggle_other(keycode, record);
        mato_event(&mato, position, false, timer_read());
        mato_owned &= ~((uint32_t)1 << position); return false;
    }
    // New entry uses an unassigned SYSTEM position; all existing bindings remain unchanged.
    if (record->event.pressed && !mato_enabled && layer == _O24_SYSTEM && row == 0 && col == 6) {
        update_o24_edge_toggle_other(keycode, record);
        mato_request = MATO_REQUEST_ON; mato_control_owned |= bit; return false;
    }
    bool mato_base = mato_enabled && default_layer == _QWERTY && !force_qwerty_active &&
        (layer == _QWERTY || layer == _QWERTY_SHIFT);
    // Keep the user's O24 language separate, and let the legacy edge toggle
    // restore mato's latest language through its existing kbd_lang path.
    if (mato_base && (keycode == KC_LNG1 || keycode == KC_LNG2)) {
        update_o24_edge_toggle_other(keycode, record);
        if (record->event.pressed) {
            mato_lang = keycode == KC_LNG1 ? 2 : 1;
            kbd_lang = mato_lang;
            update_lang(mato_lang);
        }
        return false;
    }
    if (mato_base && col == 0 && row <= 1) {
        if (record->event.pressed) {
            update_o24_edge_toggle_other(keycode, record);
            mato_reset(&mato);
            mato_request = row == 0 ? MATO_REQUEST_OFF : MATO_REQUEST_MEJIRO;
            mato_control_owned |= bit;
        }
        return false;
    }
    if (mato_base && position >= 0) {
        update_o24_edge_toggle_other(keycode, record);
        if (record->event.pressed) mato_owned |= (uint32_t)1 << position;
        if (!mato_request) mato_event(&mato, position, record->event.pressed, timer_read());
        return false;
    }
    // Includes the unchanged far-right upper/lower Mejiro toggle and all steno keys.
    bool result = legacy_process_record_user(keycode, record);
    if (get_highest_layer(layer_state | default_layer_state) == _GEMINI) mato_reset(&mato);
    return result;
}
void matrix_scan_user(void) {
    legacy_matrix_scan_user();
    if (mato_enabled && default_layer == _QWERTY) mato_tick(&mato, timer_read());
    mato_apply_request();
}
