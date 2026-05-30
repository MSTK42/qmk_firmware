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

/*---------------------------------------------------------------------------------------------------*/
/*----------------------------------------------初期設定----------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/

#define KEYMAP_JP_DEFAULT

#include QMK_KEYBOARD_H
#include "os_detection.h"
#include "keymap_japanese.h"
#include "jis_transform.h"
#include "combo_fifo.h"
#include "alt_layout.h"
#include "mejiro_fifo.h"

enum layer_names {
    _GEMINI = 0,
    _QWERTY,
    _QWERTY_SHIFT,
    _NUMBER,
    _NUMBER_SHIFT,
    _FUNCTION,
    _O24_NUMBER,
    _O24_NAV,
    _O24_SYSTEM,
};

enum custom_keycodes {
    KC_DZ = SAFE_RANGE,  // 00キー
    KC_TZ,               // 000キー
    TG_JIS,              // JISモード切替キー
    TG_ALT,              // Alternative Layout切替キー
    TG_MJR,              // Mejiro（メジロ式）モード切替キー
};

#define MT_SPC KC_LSFT
#define MT_ENT KC_RSFT
#define MT_TGL LT(_NUMBER, KC_F24)
#define MO_FUN MO(_FUNCTION)
#define MY_QUES RSFT(KC_SLSH)
#define O24_KEY1 LT(_O24_NUMBER, KC_BSPC)
#define O24_KEY2 LCTL_T(KC_ENT)
#define O24_KEY3 LT(_O24_NAV, KC_SPC)
#define O24_KEY4 LCTL_T(KC_TAB)

/*---------------------------------------------------------------------------------------------------*/
/*-----------------------------------------ユーザーカスタマイズ----------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/

// ============================================================
// Alternative Layout 設定（TG_ALTキーで切り替え）
// ============================================================
// 使用可能なレイアウト:
//   qwerty, graphite, colemak, colemak_dh, dvorak, workman,
//   handsdown_neu, sturdy, engram, gallium, canary,
//   astarte, boo, eucalyn, eucalyn_biacco, merlin, o24, tomisuke
//
// 使い方: ALT_LAYOUT(レイアウト名)

typedef struct {
    const alt_mapping_t* mappings;
    uint8_t count;
} alt_layout_def_t;

static alt_layout_def_t alt_en_layout = ALT_LAYOUT(qwerty);  // 英語モード時のレイアウト
static alt_layout_def_t alt_jp_layout = ALT_LAYOUT(qwerty);  // 日本語モード時のレイアウト

static inline bool layout_def_is_o24(alt_layout_def_t layout) {
    return layout.mappings == o24;
}

static inline bool keyboard_uses_o24_layout(void) {
    return layout_def_is_o24(alt_en_layout) || layout_def_is_o24(alt_jp_layout);
}

static inline bool is_o24_keyboard_layer(uint8_t layer) {
    return keyboard_uses_o24_layout() && (
        layer == _QWERTY ||
        layer == _QWERTY_SHIFT ||
        layer == _O24_NUMBER ||
        layer == _O24_NAV ||
        layer == _O24_SYSTEM
    );
}

// ============================================================
// 言語設定
// ============================================================
// 0:未使用, 1:英語, 2:日本語, 3:日英両方

static int stn_lang = 3;  // ステノモード時の言語
static int kbd_lang = 1;  // キーボードモード時の言語

/*---------------------------------------------------------------------------------------------------*/
/*----------------------------------------------内部変数----------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/

bool is_jis_mode = true;
bool is_mejiro_mode = true;
static bool is_mac = false;
static bool os_detected = false;
static bool mt_tgl_pressed = false;
static uint16_t dz_timer = 0;
static bool dz_delayed = false;
static uint8_t dz_fifo_len_at_press = 0;  // DZ キー押下時のコンボ FIFO 長
uint16_t lshift_timer = 0;      // L_shiftが押される時間
uint16_t rshift_timer = 0;      // R_shiftが押される時間
static bool lshift_has_key = false;    // L_shift押下中に新しいキーが押されたか
static bool rshift_has_key = false;    // R_shift押下中に新しいキーが押されたか
typedef struct {
    bool pressed;
    uint16_t timer;
} toggle_hold_state_t;
static toggle_hold_state_t tg_jis_state = {false, 0};
static toggle_hold_state_t tg_alt_state = {false, 0};
static toggle_hold_state_t tg_mjr_state = {false, 0};

static inline bool is_modifier_keycode(uint16_t keycode) {
    switch (keycode) {
        case KC_LCTL: case KC_RCTL:
        case KC_LALT: case KC_RALT:
        case KC_LGUI: case KC_RGUI:
        case KC_LSFT: case KC_RSFT:
            return true;
        default:
            return false;
    }
}

static inline bool mt_tgl_can_toggle(const keyrecord_t *record) {
    if (combo_fifo_len != 0) return false;
    if (get_mods() != 0 || get_weak_mods() != 0 || get_oneshot_mods() != 0) return false;
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        matrix_row_t row_state = matrix_get_row(row);
        if (row == record->event.key.row) {
            matrix_row_t mask = ((matrix_row_t)1) << record->event.key.col;
            row_state &= ~mask;
        }
        if (row_state != 0) return false;
    }
    return true;
}

typedef union {
    uint32_t raw;
    struct {
        bool jis_mode : 1;
        bool alt_mode : 1;
        bool mejiro_mode : 1;
    };
} user_config_t;

static user_config_t user_config;

void eeconfig_init_user(void) {
    user_config.raw = 0;
    user_config.jis_mode = false;
    user_config.alt_mode = true;
    user_config.mejiro_mode = true;
    eeconfig_update_user(user_config.raw);
    steno_set_mode(STENO_MODE_GEMINI);
}

void keyboard_post_init_user(void) {
    os_variant_t os = detected_host_os();
    is_mac = (os == OS_MACOS || os == OS_IOS);
    user_config.raw = eeconfig_read_user();
    if (keyboard_uses_o24_layout() && !user_config.alt_mode) {
        user_config.alt_mode = true;
        eeconfig_update_user(user_config.raw);
    }
    is_jis_mode = (user_config.jis_mode);
    is_alt_mode = (user_config.alt_mode);
    is_mejiro_mode = (user_config.mejiro_mode);
    default_layer_set((layer_state_t)1UL << _QWERTY);
    layer_clear();
    layer_move(_QWERTY);
    default_layer = _QWERTY;
}

/**
 * @param kc
 */
void tap_code16_with_shift(uint16_t kc) {
    uint8_t saved_mods      = get_mods();
    uint8_t saved_weak_mods = get_weak_mods();
    uint8_t saved_osm       = get_oneshot_mods();

    add_mods(MOD_LSFT);
    send_keyboard_report();

    tap_code16(kc);

    set_mods(saved_mods);
    set_weak_mods(saved_weak_mods);
    set_oneshot_mods(saved_osm);
    send_keyboard_report();
}

/**
 * @param kc
 */
void register_code16_with_shift(uint16_t kc) {
    add_mods(MOD_LSFT);
    send_keyboard_report();
    register_code16(kc);
}

/**
 * @param kc
 */
void unregister_code16_with_shift(uint16_t kc) {
    unregister_code16(kc);
    del_mods(MOD_LSFT);
    send_keyboard_report();
}

/**
 * Shift が押されている状態でも、一時的に解除してキーをレジスタする
 * @param kc キーコード
 */
void register_code16_without_shift(uint16_t kc) {
    uint8_t saved_mods      = get_mods();
    uint8_t saved_weak_mods = get_weak_mods();
    uint8_t saved_osm       = get_oneshot_mods();

    del_mods(MOD_MASK_SHIFT);
    del_weak_mods(MOD_MASK_SHIFT);
    clear_oneshot_mods();
    send_keyboard_report();

    register_code16(kc);

    set_mods(saved_mods);
    set_weak_mods(saved_weak_mods);
    set_oneshot_mods(saved_osm);
    send_keyboard_report();
}

/**
 * Shift が押されている状態でも、一時的に解除してキーをアンレジスタする
 * @param kc キーコード
 */
void unregister_code16_without_shift(uint16_t kc) {
    uint8_t saved_mods      = get_mods();
    uint8_t saved_weak_mods = get_weak_mods();
    uint8_t saved_osm       = get_oneshot_mods();

    del_mods(MOD_MASK_SHIFT);
    del_weak_mods(MOD_MASK_SHIFT);
    clear_oneshot_mods();
    send_keyboard_report();

    unregister_code16(kc);

    set_mods(saved_mods);
    set_weak_mods(saved_weak_mods);
    set_oneshot_mods(saved_osm);
    send_keyboard_report();
}

uint16_t alt_transform(uint16_t kc, bool shifted, uint8_t layer) {
    (void)shifted;
    if (!is_alt_mode || force_qwerty_active) return kc;

    if (layer != _QWERTY && layer != _QWERTY_SHIFT) return kc;

    // _QWERTY_SHIFT の記号キーは、対応するベースキーに正規化して
    // shifted 側マッピングとして解釈する。
    uint16_t normalized_kc = kc;
    bool normalized_shifted = (layer == _QWERTY_SHIFT);
    switch (kc) {
        case KC_UNDS: normalized_kc = KC_MINS; normalized_shifted = true; break;
        case KC_LABK: normalized_kc = KC_COMM; normalized_shifted = true; break;
        case KC_RABK: normalized_kc = KC_DOT;  normalized_shifted = true; break;
        case KC_QUES: normalized_kc = KC_SLSH; normalized_shifted = true; break;
        case KC_PIPE: normalized_kc = KC_BSLS; normalized_shifted = true; break;
        case KC_COLN: normalized_kc = KC_SCLN; normalized_shifted = true; break;
        case KC_DQUO: normalized_kc = KC_QUOT; normalized_shifted = true; break;
        default: break;
    }

    // 現在の言語に応じたマッピングを選択
    const alt_mapping_t* mappings;
    uint8_t count;

    if (kbd_lang == 1) {
        // 英語モード
        mappings = alt_en_layout.mappings;
        count = alt_en_layout.count;
    } else {
        // 日本語モード
        mappings = alt_jp_layout.mappings;
        count = alt_jp_layout.count;
    }

    // マッピングが設定されていない場合は変換しない
    if (mappings == NULL || count == 0) return kc;

    for (uint8_t i = 0; i < count; i++) {
        alt_mapping_t mapping;
        memcpy_P(&mapping, &mappings[i], sizeof(mapping));
        if (mapping.base == normalized_kc) {
            return normalized_shifted ? mapping.shifted : mapping.unshifted;
        }
    }

    return kc;
}

static inline transformed_key_t transform_key_extended(uint16_t kc, bool shifted, uint8_t layer) {
    uint16_t alt_kc = alt_transform(kc, shifted, layer);

    bool needs_unshift = is_jis_shift_target(alt_kc);
    bool transform_shifted = needs_unshift ? false : shifted;

    transformed_key_t result = {
        .keycode = jis_transform(alt_kc, transform_shifted),
        .needs_unshift = needs_unshift,
    };
    return result;
}

static void refresh_force_qwerty_state(void) {
    uint8_t current_layer = get_highest_layer(layer_state | default_layer_state);
    bool is_number_or_function = (current_layer == _NUMBER || current_layer == _FUNCTION);
    bool keep_o24_layout = is_o24_keyboard_layer(current_layer);

    bool should_force = mods_except_shift_active() && !is_number_or_function && !keep_o24_layout;
    layer_state_t qwerty_default = (layer_state_t)1UL << _QWERTY;

    if (should_force) {
        if (current_layer == _GEMINI) {
            mejiro_reset_state();
        }
        if (!force_qwerty_active || default_layer_state != qwerty_default) {
            default_layer_set(qwerty_default);
            layer_move(_QWERTY);
        }
        force_qwerty_active = true;
    } else if (force_qwerty_active) {
        layer_state_t target_default = (layer_state_t)1UL << (default_layer == 1 ? _QWERTY : _GEMINI);
        default_layer_set(target_default);
        layer_move(default_layer == 1 ? _QWERTY : _GEMINI);
        force_qwerty_active = false;
    }
}

static void refresh_shift_layer_state(void) {
    uint8_t active_layer = get_highest_layer(layer_state | default_layer_state);
    bool shift_held = lshift_timer > 0 || rshift_timer > 0;
    bool number_shift_shortcut = lshift_timer > 0 && rshift_timer > 0;

    if (number_shift_shortcut) {
      layer_move(_NUMBER_SHIFT);
    } else if (shift_held) {
        if (!mt_tgl_pressed) {
            layer_move(_QWERTY_SHIFT);
            default_layer_set((layer_state_t)1UL << _QWERTY);
            default_layer = _QWERTY;
        }
    } else {
        if (active_layer == _QWERTY_SHIFT) {
            layer_move(_QWERTY);
        } else if (active_layer == _NUMBER_SHIFT) {
            layer_move(_NUMBER);
        }
    }
}

static bool has_alt_layout_for_lang(uint8_t lang) {
    if (lang == 1) {
        return alt_en_layout.mappings != NULL && alt_en_layout.count > 0;
    }
    if (lang == 2) {
        return alt_jp_layout.mappings != NULL && alt_jp_layout.count > 0;
    }
    return false;
}

static uint8_t get_active_lang(void) {
    return (default_layer == _QWERTY) ? (uint8_t)kbd_lang : (uint8_t)stn_lang;
}

static void apply_alt_mode_for_lang(uint8_t lang) {
    is_alt_mode = user_config.alt_mode && has_alt_layout_for_lang(lang);
}

// lang : 0=なし, 1=英語, 2=日本語
static void update_lang(uint8_t lang) {
    apply_alt_mode_for_lang(lang);

    switch (lang) {
        case 1:
            tap_code16(KC_LNG2);
            break;
        case 2:
            tap_code16(KC_LNG1);
            break;
        default:
            break;
    }
}

bool combo_fifo_custom_action(uint16_t keycode, bool shifted, bool needs_unshift, bool is_hold) {
    (void)shifted;
    (void)needs_unshift;

    // MacOS用のキー変換
    if (is_mac) {
        uint8_t mods = get_mods();
        bool command_held = (mods & MOD_MASK_GUI);

        switch (keycode) {
            case KC_HOME:
                // Home → ⌘+← (Ctrl+Home → ⌘+↑)
                {
                    uint8_t saved_mods = mods;
                    add_mods(MOD_LGUI);
                    if (!command_held) {
                        tap_code16(KC_LEFT);
                    } else {
                        del_mods(MOD_LCTL);
                        tap_code16(KC_UP);
                    }
                    set_mods(saved_mods);
                    send_keyboard_report();
                }
                return true;

            case KC_END:
                // End → ⌘+→ (Ctrl+End → ⌘+↓)
                {
                    uint8_t saved_mods = mods;
                    add_mods(MOD_LGUI);
                    if (!command_held) {
                        tap_code16(KC_RIGHT);
                    } else {
                        del_mods(MOD_LCTL);
                        tap_code16(KC_DOWN);
                    }
                    set_mods(saved_mods);
                    send_keyboard_report();
                }
                return true;
        }
    }

    // 言語切り替え
    switch (keycode) {
        case KC_LNG1:
            if (default_layer == _QWERTY) {
                kbd_lang = 2;
            }
            update_lang(2);
            return true;
        case KC_LNG2:
            if (default_layer == _QWERTY) {
                kbd_lang = 1;
            }
            update_lang(1);
            return true;
        default:
            return false;
    }
}

static bool combo_pair_matches(combo_pair_t pair, uint16_t a, uint16_t b, uint8_t layer) {
    return pair.layer == layer &&
           ((pair.a == a && pair.b == b) || (pair.a == b && pair.b == a));
}

static uint16_t o24_combo_hold_modifier(combo_pair_t pair) {
    if (combo_pair_matches(pair, KC_R, KC_V, _QWERTY)) return KC_LSFT;    // F + V
    if (combo_pair_matches(pair, KC_U, KC_M, _QWERTY)) return KC_LSFT;    // W + D
    if (combo_pair_matches(pair, KC_E, KC_C, _QWERTY)) return KC_LALT;    // U + C
    if (combo_pair_matches(pair, KC_I, KC_COMM, _QWERTY)) return KC_LALT; // R + M
    if (combo_pair_matches(pair, KC_W, KC_X, _QWERTY)) return KC_LGUI;    // L + X
    if (combo_pair_matches(pair, KC_O, KC_DOT, _QWERTY)) return KC_LGUI;  // Y + J
    return KC_NO;
}

bool combo_fifo_custom_combo_wait_for_hold(combo_pair_t pair) {
    return o24_combo_hold_modifier(pair) != KC_NO;
}

bool combo_fifo_custom_combo_action(combo_pair_t pair, uint16_t keycode, bool shifted, bool needs_unshift, bool is_hold) {
    (void)keycode;
    (void)shifted;
    (void)needs_unshift;

    if (!is_hold) return false;

    uint16_t modifier = o24_combo_hold_modifier(pair);
    if (modifier == KC_NO) return false;

    hold_state.keycode = modifier;
    hold_state.time_confirmed = timer_read();
    hold_state.is_held = true;
    hold_state.source_key_a = pair.a;
    hold_state.source_key_b = pair.b;
    hold_state.source_a_pressed = true;
    hold_state.source_b_pressed = true;
    hold_state.shift_held = false;
    hold_state.shift_injected = false;
    hold_state.modifier_hold = true;
    register_code16(modifier);
    return true;
}

static void toggle_jis_mode(void) {
    is_jis_mode = !is_jis_mode;
    user_config.jis_mode = is_jis_mode;
    eeconfig_update_user(user_config.raw);
}

static void toggle_alt_mode(void) {
    if (keyboard_uses_o24_layout()) {
        user_config.alt_mode = true;
        apply_alt_mode_for_lang(get_active_lang());
        eeconfig_update_user(user_config.raw);
        return;
    }
    user_config.alt_mode = !user_config.alt_mode;
    apply_alt_mode_for_lang(get_active_lang());
    eeconfig_update_user(user_config.raw);
}

static void toggle_mejiro_mode(void) {
    if (is_mejiro_mode) {
        mejiro_reset_state();
    }
    is_mejiro_mode = !is_mejiro_mode;
    user_config.mejiro_mode = is_mejiro_mode;
    eeconfig_update_user(user_config.raw);
}

static bool handle_toggle_on_hold(keyrecord_t *record, toggle_hold_state_t *state, void (*toggle_fn)(void)) {
    if (record->event.pressed) {
        state->pressed = true;
        state->timer = timer_read();
        return false;
    }

    if (!state->pressed) return false;

    state->pressed = false;
    if (timer_elapsed(state->timer) >= COMBO_TIMEOUT_MS) {
        toggle_fn();
    }
    return false;
}

/*---------------------------------------------------------------------------------------------------*/
/*--------------------------------------------FIFOコンボ----------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/

// コンボ定義（順不同）
const combo_pair_t combo_pairs[] PROGMEM = {

    {KC_Q,    KC_Z,    KC_A,    _QWERTY},
    {KC_W,    KC_X,    KC_S,    _QWERTY},
    {KC_E,    KC_C,    KC_D,    _QWERTY},
    {KC_R,    KC_V,    KC_F,    _QWERTY},
    {KC_U,    KC_M,    KC_J,    _QWERTY},
    {KC_I,    KC_COMM, KC_K,    _QWERTY},
    {KC_O,    KC_DOT,  KC_L,    _QWERTY},
    {KC_P,    KC_SLSH, KC_SCLN, _QWERTY},

    {KC_1,    KC_7,    KC_4,     _NUMBER},
    {KC_2,    KC_8,    KC_5,     _NUMBER},
    {KC_3,    KC_9,    KC_6,     _NUMBER},
    {KC_0,    KC_DZ,   KC_TZ,    _NUMBER},
    {KC_DOT,  KC_MINS, KC_COMMA, _NUMBER},
    {KC_9,    KC_DOT,  KC_TAB,   _NUMBER},
    {KC_3,    KC_MINS, KC_ESC,   _NUMBER},
    {KC_PGDN, KC_LEFT, KC_BSPC,  _NUMBER},
    {KC_PGUP, KC_HOME, KC_DEL,   _NUMBER},
    {KC_AMPR, KC_PIPE, KC_HASH,  _NUMBER},
    {KC_PERC, KC_DLR,  KC_SLSH,  _NUMBER_SHIFT},
    {KC_LCBR, KC_RCBR, KC_ASTR,  _NUMBER_SHIFT},
    {KC_LBRC, KC_RBRC, KC_EQL,   _NUMBER_SHIFT},
    {KC_LPRN, KC_RPRN, KC_PLUS,  _NUMBER_SHIFT},
    {KC_LABK, KC_RABK, KC_CIRC,  _NUMBER_SHIFT},
    {KC_RPRN, KC_RABK, KC_TAB,   _NUMBER_SHIFT},
    {KC_LPRN, KC_LABK, KC_ESC,   _NUMBER_SHIFT},
    {KC_PGDN, KC_LEFT, KC_BSPC,  _NUMBER_SHIFT},
    {KC_PGUP, KC_HOME, KC_DEL,   _NUMBER_SHIFT},
};
uint8_t combo_pair_count = sizeof(combo_pairs) / sizeof(combo_pairs[0]);

bool is_combo_candidate(uint16_t keycode) {
    uint8_t mods = get_mods();
    bool shifted = (mods & MOD_MASK_SHIFT);
    if (keycode == KC_GRV) return true;
    if (keycode == KC_DZ) return shifted;
    if (keycode == KC_TZ) return shifted;
    if (keycode == KC_DOWN) return true;
    if (keycode == KC_UP) return true;
    if (keycode == KC_RIGHT) return true;
    if (keycode == KC_END) return true;
    if (keycode == KC_CAPS) return true;
    if (keyboard_uses_o24_layout()) {
        switch (keycode) {
            case KC_Q: case KC_W: case KC_E: case KC_R: case KC_T:
            case KC_Y: case KC_U: case KC_I: case KC_O: case KC_P:
            case KC_Z: case KC_X: case KC_C: case KC_V: case KC_B:
            case KC_N: case KC_M: case KC_COMM: case KC_DOT: case KC_SLSH:
                return true;
        }
    }
    return is_combo_candidate_default(keycode, 0);
}

/*---------------------------------------------------------------------------------------------------*/
/*--------------------------------------------キーマップ----------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // GEMINI
    // ┌─────┬─────┬─────┬─────┬─────┬─────┐             ┌─────┬─────┬─────┬─────┬─────┬─────┐
    // │  #  │  S  │  T  │  P  │  H  │  *  │             │  *  │  F  │  P  │  L  │  T  │  D  │
    // ├─────┼─────┼─────┼─────┼─────┼─────┤             ├─────┼─────┼─────┼─────┼─────┼─────┤
    // │  #  │  S  │  K  │  W  │  R  │  *  │             │  *  │  R  │  B  │  G  │  S  │  Z  │
    // └─────┴─────┴─────┴─────┴─────┴─────┘             └─────┴─────┴─────┴─────┴─────┴─────┘
    //                         ┌───────────┐             ┌───────────┐
    //                         │     #     │             │     #     │
    //                         ├─────┬─────┤   ┌─────┐   ├─────┬─────┤
    //                         │  A  │  O  │   │Layer│   │  E  │  U  │
    //                         └─────┴─────┘   └─────┘   └─────┴─────┘
    // GEMINI
    [_GEMINI] = LAYOUT(
        STN_N1, STN_S1, STN_TL, STN_PL, STN_HL, STN_ST1,         STN_ST3, STN_FR, STN_PR, STN_LR, STN_TR, STN_DR,
        STN_N2, STN_S2, STN_KL, STN_WL, STN_RL, STN_ST2,         STN_ST4, STN_RR, STN_BR, STN_GR, STN_SR, STN_ZR,
                                        STN_N3,          MT_TGL, STN_N4,
                                        STN_A,  STN_O,           STN_E,   STN_U
    ),
    // QWERTY
    // ┌─────┬─────┬─────┬─────┬─────┬─────┐             ┌─────┬─────┬─────┬─────┬─────┬─────┐
    // │  `  │  q  │  w  │  e  │  r ESC t  │             │  y DEL u  │  i  │  o  │  p  │  -  │
    // ├─────┼──a──┼──s──┼──d──┼──f──┼──g──┤             ├──h──┼──j──┼──k──┼──l──┼──;──┼──'──┤
    // │ WIN │  z  │  x  │  c  │  v TAB b  │             │  n BSP m  │  ,  │  .  │  /  │  \  │
    // └─────┴─────┴─────┴─────┴─────┴─────┘             └─────┴─────┴─────┴─────┴─────┴─────┘
    //                         ┌───────────┐             ┌───────────┐
    //                         │Space/Shift│             │Enter/Shift│
    //                         ├─────┬─────┤   ┌─────┐   ├─────┬─────┤
    //                         │ ALT │ CTL │   │Layer│   │Eisu @ Kana│
    //                         └─────┴─────┘   └─────┘   └─────┴─────┘
    // QWERTY Shifted
    // ┌─────┬─────┬─────┬─────┬─────┬─────┐             ┌─────┬─────┬─────┬─────┬─────┬─────┐
    // │  ~  │  Q  │  W  │  E  │  R ESC T  │             │  Y DEL U  │  I  │  O  │  P  │  _  │
    // ├─────┼──A──┼──S──┼──D──┼──F──┼──G──┤             ├──H──┼──J──┼──K──┼──L──┼──:──┼──"──┤
    // │ WIN │  Z  │  X  │  C  │  V TAB B  │             │  N BSP M  │  <  │  >  │  ?  │  |  │
    // └─────┴─────┴─────┴─────┴─────┴─────┘             └─────┴─────┴─────┴─────┴─────┴─────┘
    //                         ┌───────────┐             ┌───────────┐
    //                         │Space/Shift│             │Enter/Shift│
    //                         ├─────┬─────┤   ┌─────┐   ├─────┬─────┤
    //                         │ ALT │ CTL │   │Layer│   │  !  │  ?  │
    //                         └─────┴─────┘   └─────┘   └─────┴─────┘
    [_QWERTY] = LAYOUT(
        KC_NO,  KC_Q, KC_W, KC_E, KC_R, KC_T,               KC_Y, KC_U, KC_I,    KC_O,   KC_P,    KC_NO,
        KC_NO,  KC_Z, KC_X, KC_C, KC_V, KC_B,               KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_NO,
                                  KC_NO  ,          MT_TGL, KC_NO,
                                  O24_KEY1, O24_KEY2,       O24_KEY3, O24_KEY4
    ),
    [_QWERTY_SHIFT] = LAYOUT(
        KC_NO,  KC_Q, KC_W, KC_E, KC_R, KC_T,               KC_Y, KC_U, KC_I,    KC_O,   KC_P,    KC_NO,
        KC_NO,  KC_Z, KC_X, KC_C, KC_V, KC_B,               KC_N, KC_M, KC_LABK, KC_RABK,KC_QUES, KC_NO,
                                  KC_NO  ,          MT_TGL, KC_NO,
                                  O24_KEY1, O24_KEY2,       O24_KEY3, O24_KEY4
    ),
    // NUMBER
    // ┌─────┬─────┬─────┬─────┬─────┬─────┐             ┌─────┬─────┬─────┬─────┬─────┬─────┐
    // │ MJR │ 00  │  1  │  2  │  3  │  -  │             │ PGU │ HOM │  ↑  │ END │ CAP │ ALT │
    // ├─────┼─────┼──4──┼──5──┼──6──┼──,──┤             ├─────┼─────┼─────┼─────┼─────┼─────┤
    // │ WIN │  0  │  7  │  8  │  9  │  .  │             │ PGD │  ←  │  ↓  │  →  │ WIN │MO_FN│
    // └─────┴─────┴─────┴─────┴─────┴─────┘             └─────┴─────┴─────┴─────┴─────┴─────┘
    //                         ┌───────────┐             ┌───────────┐
    //                         │Space/Shift│             │Enter/Shift│
    //                         ├─────┬─────┤   ┌─────┐   ├─────┬─────┤
    //                         │ ALT │ CTL │   │Layer│   │  &  #  |  │
    //                         └─────┴─────┘   └─────┘   └─────┴─────┘
    // NUMBER Shifted
    // ┌─────┬─────┬─────┬─────┬─────┬─────┐             ┌─────┬─────┬─────┬─────┬─────┬─────┐
    // │ MJR │  %  │  [  │  {  │  (  │  <  │             │ PGU │ HOM │  ↑  │ END │ CAP │ ALT │
    // ├─────┼──/──┼──*──┼──=──┼──+──┼──^──┤             ├─────┼─────┼─────┼─────┼─────┼─────┤
    // │ WIN │  $  │  ]  │  }  │  )  │  >  │             │ PGD │  ←  │  ↓  │  →  │ WIN │MO_FN│
    // └─────┴─────┴─────┴─────┴─────┴─────┘             └─────┴─────┴─────┴─────┴─────┴─────┘
    //                         ┌───────────┐             ┌───────────┐
    //                         │Space/Shift│             │Enter/Shift│
    //                         ├─────┬─────┤   ┌─────┐   ├─────┬─────┤
    //                         │ ALT │ CTL │   │Layer│   │ CTL │ ALT │
    //                         └─────┴─────┘   └─────┘   └─────┴─────┘
    [_NUMBER] = LAYOUT(
        KC_NO,  KC_DZ,   KC_1,    KC_2,    KC_3,    KC_MINS,          KC_PGUP, KC_HOME, KC_UP,   KC_END,   KC_CAPS, KC_NO,
        KC_LGUI,KC_0,    KC_7,    KC_8,    KC_9,    KC_DOT,           KC_PGDN, KC_LEFT, KC_DOWN, KC_RIGHT, KC_LGUI, MO_FUN,
                                           MT_SPC,           KC_TRNS, MT_ENT,
                                           KC_LALT, KC_LCTL,          KC_AMPR, KC_PIPE
    ),
    [_NUMBER_SHIFT] = LAYOUT(
        KC_NO,  KC_PERC, KC_LCBR, KC_LBRC, KC_LPRN, KC_LABK,          KC_PGUP, KC_HOME, KC_UP,   KC_END,   KC_CAPS, KC_NO,
        KC_LGUI,KC_DLR,  KC_RCBR, KC_RBRC, KC_RPRN, KC_RABK,          KC_PGDN, KC_LEFT, KC_DOWN, KC_RIGHT, KC_LGUI, MO_FUN,
                                           MT_SPC,           KC_TRNS, MT_ENT,
                                           KC_LALT, KC_LCTL,          KC_LCTL, KC_LALT
    ),
    // FUNCTION
    // ┌─────┬─────┬─────┬─────┬─────┬─────┐             ┌─────┬─────┬─────┬─────┬─────┬─────┐
    // │ JIS │ F1  │ F2  │ F3  │ F4  │ F5  │             │ BRU │ VL0 │ VL- │ VL+ │ PSC │ xxx │
    // ├─────┼─────┼─────┼─────┼─────┼─────┤             ├─────┼─────┼─────┼─────┼─────┼─────┤
    // │ WIN │ F6  │ F7  │ F8  │ F9  │ F10 │             │ BRD │ |<< │ >|| │ >>| │ WIN │MO_FN│
    // └─────┴─────┴─────┴─────┴─────┴─────┘             └─────┴─────┴─────┴─────┴─────┴─────┘
    //                         ┌───────────┐             ┌───────────┐
    //                         │Space/Shift│             │Enter/Shift│
    //                         ├─────┬─────┤   ┌─────┐   ├─────┬─────┤
    //                         │ ALT │ CTL │   │Layer│   │ F11 │ F12 │
    //                         └─────┴─────┘   └─────┘   └─────┴─────┘
    // FUNCTION
    [_FUNCTION] = LAYOUT(
        KC_NO,  KC_F1, KC_F2, KC_F3, KC_F4,   KC_F5,           KC_BRIU, KC_MUTE, KC_VOLD, KC_VOLU, KC_PSCR, KC_TRNS,
        KC_LGUI,KC_F6, KC_F7, KC_F8, KC_F9,   KC_F10,          KC_BRID, KC_MPRV, KC_MPLY, KC_MNXT, KC_LGUI, KC_TRNS,
                                     KC_TRNS,         KC_TRNS, KC_TRNS,
                                     KC_TRNS, KC_TRNS,         KC_F11,  KC_F12
    ),
    [_O24_NUMBER] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                                      KC_TRNS,         KC_TRNS, KC_TRNS,
                                      KC_TRNS, KC_TRNS,         KC_TRNS, KC_TRNS
    ),
    [_O24_NAV] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                                      KC_TRNS,         KC_TRNS, KC_TRNS,
                                      KC_TRNS, KC_TRNS,         KC_TRNS, KC_TRNS
    ),
    [_O24_SYSTEM] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                                      KC_TRNS,         KC_TRNS, KC_TRNS,
                                      KC_TRNS, KC_TRNS,         KC_TRNS, KC_TRNS
    ),
};

/*---------------------------------------------------------------------------------------------------*/
/*--------------------------------------------メイン処理----------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/


bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    uint8_t mods = get_mods();
    bool shifted = (mods & MOD_MASK_SHIFT);

    if (record->event.pressed) {
        if (lshift_timer != 0 && !is_modifier_keycode(keycode)) {
            lshift_has_key = true;
        }
        if (rshift_timer != 0 && !is_modifier_keycode(keycode)) {
            rshift_has_key = true;
        }
    }

    if (is_mejiro_mode && get_highest_layer(layer_state | default_layer_state) == _GEMINI && is_stn_key(keycode)) {
        if (record->event.pressed) {
            mejiro_on_press(keycode);
        } else {
            mejiro_on_release(keycode);
            if (mejiro_should_send_passthrough()) {
                mejiro_send_passthrough_keys();
            }
        }
        return false;
    }

    if (is_combo_candidate(keycode)) {
        if (keycode == KC_GRV) {
            if (is_jis_mode && (mods & MOD_MASK_ALT)) {
                if (record->event.pressed) {
                    tap_code16(KC_GRV); // 全角半角
                }
                return false;
            }
        }

        if (record->event.pressed) {
            if (combo_fifo_len < COMBO_FIFO_LEN) {
                uint16_t base = keycode;
                if (hold_state.is_held && !hold_state.modifier_hold && combo_fifo_len > 0) {
                    clear_hold_state();
                }
                combo_fifo[combo_fifo_len].keycode = base;
                combo_fifo[combo_fifo_len].layer   = get_highest_layer(layer_state | default_layer_state);
                combo_fifo[combo_fifo_len].mods    = mods;
                combo_fifo[combo_fifo_len].time_pressed = timer_read();
                combo_fifo[combo_fifo_len].released = false;
                if ((mods & MOD_LSFT) && lshift_timer != 0) {
                    lshift_has_key = true;
                }
                if ((mods & MOD_RSFT) && rshift_timer != 0) {
                    rshift_has_key = true;
                }
                combo_fifo_len++;
            } else {
                uint8_t current_layer = get_highest_layer(layer_state | default_layer_state);
                transformed_key_t transformed = transform_key_extended(keycode, shifted, current_layer);
                if (transformed.needs_unshift) {
                    tap_code16_unshifted(transformed.keycode);
                } else {
                    tap_code16(transformed.keycode);
                }
            }
        } else {
            uint16_t base = keycode;
            if (hold_state.is_held) {
                if (base == hold_state.source_key_a) {
                    hold_state.source_a_pressed = false;
                }
                if (base == hold_state.source_key_b) {
                    hold_state.source_b_pressed = false;
                }
                if (!hold_state.source_a_pressed || !hold_state.source_b_pressed) {
                    clear_hold_state();
                }
            }

            bool fifo_updated = false;
            for (uint8_t i = 0; i < combo_fifo_len; i++) {
                if (combo_fifo[i].keycode == base && !combo_fifo[i].released) {
                    combo_fifo[i].released = true;
                    fifo_updated = true;
                    break;
                }
            }
            if (fifo_updated) {
                combo_fifo_service_extended(transform_key_extended);
            }
        }
        return false;
    }

    switch (keycode) {
        case MT_TGL:
            if (record->event.pressed) {
                mt_tgl_pressed = true;
                if (record->tap.count > 0 && mt_tgl_can_toggle(record)) {
                    if (default_layer == _QWERTY) {
                        default_layer_set((layer_state_t)1UL << _GEMINI);
                        layer_move(_GEMINI);
                        default_layer = _GEMINI;
                        update_lang(is_mejiro_mode ? 2 : stn_lang);
                    } else {
                        mejiro_reset_state();
                        default_layer_set((layer_state_t)1UL << _QWERTY);
                        layer_move(_QWERTY);
                        default_layer = _QWERTY;
                        update_lang(kbd_lang);
                    }
                }
            } else {
                mt_tgl_pressed = false;
            }
            return record->tap.count == 0;
        case TG_JIS:
            return handle_toggle_on_hold(record, &tg_jis_state, toggle_jis_mode);
        case TG_ALT:
            return handle_toggle_on_hold(record, &tg_alt_state, toggle_alt_mode);
        case TG_MJR:
            return handle_toggle_on_hold(record, &tg_mjr_state, toggle_mejiro_mode);
        case KC_DZ:
            if (record->event.pressed) {
                dz_timer = timer_read();
                dz_delayed = true;
                dz_fifo_len_at_press = combo_fifo_len;
            }
            return false;
        case KC_LSFT:
            if (record->event.pressed) {
                lshift_timer = timer_read();
                lshift_has_key = false;

                // FIFO に KC_LSFT を追加
                if (combo_fifo_len < COMBO_FIFO_LEN) {
                    combo_fifo[combo_fifo_len].keycode = KC_LSFT;
                    combo_fifo[combo_fifo_len].layer = get_highest_layer(layer_state | default_layer_state);
                    combo_fifo[combo_fifo_len].mods = get_mods();
                    combo_fifo[combo_fifo_len].time_pressed = timer_read();
                    combo_fifo[combo_fifo_len].released = false;
                    combo_fifo_len++;
                }
            } else {
                lshift_timer = 0;

                // FIFO 内の KC_LSFT に released=true を設定
                bool lshift_found = false;
                for (uint8_t i = 0; i < combo_fifo_len; i++) {
                    if (combo_fifo[i].keycode == KC_LSFT && !combo_fifo[i].released) {
                        combo_fifo[i].released = true;
                        lshift_found = true;
                        break;
                    }
                }

                // コンボ処理を実行
                if (lshift_found) {
                    combo_fifo_service_extended(transform_key_extended);
                }
            }
            return true;
        case KC_RSFT:
            if (record->event.pressed) {
                rshift_timer = timer_read();
                rshift_has_key = false;

                // FIFO に KC_RSFT を追加
                if (combo_fifo_len < COMBO_FIFO_LEN) {
                    combo_fifo[combo_fifo_len].keycode = KC_RSFT;
                    combo_fifo[combo_fifo_len].layer = get_highest_layer(layer_state | default_layer_state);
                    combo_fifo[combo_fifo_len].mods = get_mods();
                    combo_fifo[combo_fifo_len].time_pressed = timer_read();
                    combo_fifo[combo_fifo_len].released = false;
                    combo_fifo_len++;
                }
            } else {
                rshift_timer = 0;

                // FIFO 内の KC_RSFT に released=true を設定
                bool rshift_found = false;
                for (uint8_t i = 0; i < combo_fifo_len; i++) {
                    if (combo_fifo[i].keycode == KC_RSFT && !combo_fifo[i].released) {
                        combo_fifo[i].released = true;
                        rshift_found = true;
                        break;
                    }
                }

                // コンボ処理を実行
                if (rshift_found) {
                    combo_fifo_service_extended(transform_key_extended);
                }
            }
            return true;
        case KC_LCTL:
            if (is_mac) {
                if (record->event.pressed) {
                    register_mods(MOD_LGUI);
                } else {
                    unregister_mods(MOD_LGUI);
                }
                return false;
            }
            return true;
        case KC_LGUI:
            if (is_mac) {
                if (record->event.pressed) {
                    register_mods(MOD_LCTL);
                } else {
                    unregister_mods(MOD_LCTL);
                }
                return false;
            }
            return true;
        case KC_RCTL:
            if (is_mac) {
                if (record->event.pressed) {
                    register_mods(MOD_RGUI);
                } else {
                    unregister_mods(MOD_RGUI);
                }
                return false;
            }
            return true;
        case KC_RGUI:
            if (is_mac) {
                if (record->event.pressed) {
                    register_mods(MOD_RCTL);
                } else {
                    unregister_mods(MOD_RCTL);
                }
                return false;
            }
            return true;
        case KC_LNG1:
            update_lang(2);  // 日本語へ切り替え
            return false;
        case KC_LNG2:
            update_lang(1);  // 英語へ切り替え
            return false;
        default:
            break;
    }
    return true;
}

void matrix_scan_user(void) {
    if (!os_detected) {
        os_variant_t os = detected_host_os();
        if (os == OS_MACOS || os == OS_IOS) {
            is_mac = true;
            os_detected = true;
        }
    }
    refresh_force_qwerty_state();
    refresh_shift_layer_state();
    combo_fifo_service_extended(transform_key_extended);

    if (dz_delayed) {
        if (dz_fifo_len_at_press == 0) {
            SEND_STRING("00");
            dz_delayed = false;
        } else if (combo_fifo_len < dz_fifo_len_at_press) {
            SEND_STRING("00");
            dz_delayed = false;
        } else if (timer_elapsed(dz_timer) >= COMBO_TIMEOUT_MS) {
            SEND_STRING("00");
            dz_delayed = false;
        }
    }
}
