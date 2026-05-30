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
#include "jis_transform.h"

#define COMBO_FIFO_LEN       30  // FIFOの長さ
#define COMBO_TIMEOUT_MS     200 // コンボ待機のタイムアウト時間(ms) ※ QMKコンボでいうところのCOMBO_TERM

typedef struct {
    uint16_t keycode;
    uint16_t time_pressed;
    uint8_t  layer;
    uint8_t  mods;       // キーが押された瞬間のmod状態
    bool     released;
} combo_event_t;

typedef struct {
    uint16_t a;
    uint16_t b;
    uint16_t out;
    uint8_t  layer;
} combo_pair_t;

typedef struct {
    uint16_t keycode;
    uint16_t time_confirmed;
    bool     is_held;
    uint16_t source_key_a;
    uint16_t source_key_b;
    bool     source_a_pressed;
    bool     source_b_pressed;
    bool     shift_held;  // shift を含めてホールドしているかどうか
    bool     shift_injected;  // 物理Shiftなしで一時的に追加したか
    bool     modifier_hold;  // 通常キーではなく修飾キーをホールドしているか
} hold_state_t;

// グローバル変数の宣言（combo_fifo.cで実装）
extern combo_event_t combo_fifo[COMBO_FIFO_LEN];
extern uint8_t combo_fifo_len;
extern hold_state_t hold_state;

// コンボペア定義（各キーマップで定義）
extern const combo_pair_t combo_pairs[];
extern uint8_t combo_pair_count;

// コンボ候補判定関数の宣言（オプション：各キーマップでオーバーライド可能）
extern bool is_combo_candidate(uint16_t keycode);

/**
 * デフォルトのコンボ候補判定（オーバーライド不要な場合はこれを使う）
 * @param keycode キーコード
 * @param exclude_kc 除外するキーコード（0の場合は除外なし）
 * @return コンボ候補の場合はtrue
 */
static inline bool is_combo_candidate_default(uint16_t keycode, uint16_t exclude_kc) {
    if (exclude_kc != 0 && keycode == exclude_kc) return false;
    // モディファイア類はコンボから除外
    switch (keycode) {
        case KC_LCTL: case KC_RCTL:
        case KC_LGUI: case KC_RGUI:
        case KC_LALT: case KC_RALT:
        case KC_LSFT: case KC_RSFT:
            return false;
    }

    uint16_t base = keycode;
    for (uint8_t i = 0; i < combo_pair_count; i++) {
        combo_pair_t pair;
        memcpy_P(&pair, &combo_pairs[i], sizeof(pair));
        if (pair.a == base || pair.b == base) {
            return true;
        }
    }
    return false;
}

// 拡張キー変換時に一時的にShift を有効にしてキーを送信する関数の宣言（各キーマップファイルで定義）
void tap_code16_with_shift(uint16_t kc);

// 拡張キー変換時に一時的にShift を有効にしてキーをレジスタする関数の宣言（各キーマップファイルで定義）
void register_code16_with_shift(uint16_t kc);

// Shift を有効にしてレジスタされたキーを解除する関数の宣言（各キーマップファイルで定義）
void unregister_code16_with_shift(uint16_t kc);

// Shift が押されている状態でも、一時的に解除してキーをレジスタする関数の宣言（各キーマップファイルで定義）
void register_code16_without_shift(uint16_t kc);

// Shift が押されている状態でも、一時的に解除してキーをアンレジスタする関数の宣言（各キーマップファイルで定義）
void unregister_code16_without_shift(uint16_t kc);

// キー変換関数の宣言（各キーマップファイルで定義）
// @param kc キーコード
// @return 変換後のキーコード
typedef uint16_t (*key_transform_fn_t)(uint16_t kc);

// 拡張キー変換関数の宣言（jp_graphiteなど、Shift状態を考慮する場合）
typedef struct {
    uint16_t keycode;
    bool     needs_unshift;
} transformed_key_t;

typedef transformed_key_t (*key_transform_extended_fn_t)(uint16_t kc, bool shifted, uint8_t layer);

bool combo_fifo_custom_action(uint16_t keycode, bool shifted, bool needs_unshift, bool is_hold);
bool combo_fifo_custom_combo_wait_for_hold(combo_pair_t pair);
bool combo_fifo_custom_combo_action(combo_pair_t pair, uint16_t keycode, bool shifted, bool needs_unshift, bool is_hold);

/**
 * コンボ定義を検索する関数
 * @param a キーコードA
 * @param b キーコードB
 * @param layer レイヤー
 * @return コンボペアへのポインタ、見つからない場合はNULL
 */
static inline const combo_pair_t *find_combo(uint16_t a, uint16_t b, uint8_t layer) {
    for (uint8_t i = 0; i < combo_pair_count; i++) {
        combo_pair_t pair;
        memcpy_P(&pair, &combo_pairs[i], sizeof(pair));
        if (pair.layer != layer) continue;
        if ((pair.a == a && pair.b == b) || (pair.a == b && pair.b == a)) {
            return &combo_pairs[i];
        }
    }
    return NULL;
}

/**
 * 指定インデックスの要素をFIFOから削除する関数
 * @param idx インデックス
 */
static inline void fifo_remove(uint8_t idx) {
    if (idx >= combo_fifo_len) return;
    for (uint8_t i = idx; i + 1 < combo_fifo_len; i++) {
        combo_fifo[i] = combo_fifo[i + 1];
    }
    combo_fifo_len--;
}

/**
 * ホールド状態をクリアする関数
 */
static inline void clear_hold_state(void) {
    if (hold_state.is_held) {
        if (hold_state.modifier_hold) {
            unregister_code16(hold_state.keycode);
            hold_state.modifier_hold = false;
        } else if (hold_state.shift_held) {
            if (hold_state.shift_injected) {
                unregister_code16_with_shift(hold_state.keycode);
            } else {
                unregister_code16_without_shift(hold_state.keycode);
            }
            hold_state.shift_held = false;
            hold_state.shift_injected = false;
        } else {
            unregister_code16_without_shift(hold_state.keycode);
        }
        hold_state.is_held = false;
        hold_state.keycode = 0;
        hold_state.source_key_a = 0;
        hold_state.source_key_b = 0;
        hold_state.source_a_pressed = false;
        hold_state.source_b_pressed = false;
    }
}

/**
 * 基本的なコンボ解決処理（シンプルなキー変換用）
 * @param transform_fn キー変換関数
 * @return コンボが解決された場合はtrue
 */
static inline bool resolve_combo_head_basic(key_transform_fn_t transform_fn) {
    if (combo_fifo_len < 2) return false;

    uint16_t head_kc    = combo_fifo[0].keycode;
    uint8_t  head_layer = combo_fifo[0].layer;

    for (uint8_t i = 1; i < combo_fifo_len; i++) {
        uint16_t other_kc    = combo_fifo[i].keycode;
        uint8_t  other_layer = combo_fifo[i].layer;

        if (other_layer != head_layer) continue;

        const combo_pair_t *hit = find_combo(head_kc, other_kc, head_layer);
        if (hit) {
            combo_pair_t pair;
            memcpy_P(&pair, hit, sizeof(pair));

            uint16_t out_kc = transform_fn(pair.out);
            bool head_pressed  = !combo_fifo[0].released;
            bool other_pressed = !combo_fifo[i].released;

            if (!head_pressed || !other_pressed) {
                clear_hold_state();
                tap_code16(out_kc);
                fifo_remove(i);
                fifo_remove(0);
                return true;
            }

            clear_hold_state();
            hold_state.keycode = out_kc;
            hold_state.time_confirmed = timer_read();
            hold_state.is_held = true;
            hold_state.source_key_a = head_kc;
            hold_state.source_key_b = other_kc;
            hold_state.source_a_pressed = head_pressed;
            hold_state.source_b_pressed = other_pressed;
            hold_state.shift_held = false;
            hold_state.shift_injected = false;

            register_code16_without_shift(out_kc);
            fifo_remove(i);
            fifo_remove(0);
            return true;
        }
    }
    return false;
}

/**
 * @param transform_fn 拡張キー変換関数
 * @return コンボが解決された場合はtrue
 */
static inline bool resolve_combo_head_extended(key_transform_extended_fn_t transform_fn) {
    if (combo_fifo_len < 2) return false;

    uint16_t head_kc    = combo_fifo[0].keycode;
    uint8_t  head_layer = combo_fifo[0].layer;
    uint8_t  head_mods  = combo_fifo[0].mods;  // 先頭キーのmodを使用

    for (uint8_t i = 1; i < combo_fifo_len; i++) {
        uint16_t other_kc    = combo_fifo[i].keycode;
        uint8_t  other_layer = combo_fifo[i].layer;

        if (other_layer != head_layer) continue;

        const combo_pair_t *hit = find_combo(head_kc, other_kc, head_layer);
        if (hit) {
            combo_pair_t pair;
            memcpy_P(&pair, hit, sizeof(pair));

            // コンボペアの場合は先頭キーが押された瞬間のmodを使用
            bool shifted = (head_mods & MOD_MASK_SHIFT);
            transformed_key_t transformed = transform_fn(pair.out, shifted, head_layer);

            bool head_pressed  = !combo_fifo[0].released;
            bool other_pressed = !combo_fifo[i].released;

            if (!head_pressed || !other_pressed) {
                if (!hold_state.modifier_hold) {
                    clear_hold_state();
                }
                if (combo_fifo_custom_combo_action(pair, transformed.keycode, shifted, transformed.needs_unshift, false)) {
                    fifo_remove(i);
                    fifo_remove(0);
                    return true;
                }
                if (combo_fifo_custom_action(transformed.keycode, shifted, transformed.needs_unshift, false)) {
                    fifo_remove(i);
                    fifo_remove(0);
                    return true;
                }
                if (transformed.needs_unshift) {
                    tap_code16_unshifted(transformed.keycode);
                } else if (shifted) {
                    tap_code16_with_shift(transformed.keycode);
                } else {
                    tap_code16_unshifted(transformed.keycode);
                }
                fifo_remove(i);
                fifo_remove(0);
                return true;
            }

            if (combo_fifo_custom_combo_wait_for_hold(pair) && timer_elapsed(combo_fifo[i].time_pressed) <= COMBO_TIMEOUT_MS) {
                return false;
            }

            clear_hold_state();
            if (combo_fifo_custom_combo_action(pair, transformed.keycode, shifted, transformed.needs_unshift, true)) {
                fifo_remove(i);
                fifo_remove(0);
                return true;
            }
            if (combo_fifo_custom_action(transformed.keycode, shifted, transformed.needs_unshift, true)) {
                fifo_remove(i);
                fifo_remove(0);
                return true;
            }
            hold_state.keycode = transformed.keycode;
            hold_state.time_confirmed = timer_read();
            hold_state.is_held = true;
            hold_state.source_key_a = head_kc;
            hold_state.source_key_b = other_kc;
            hold_state.source_a_pressed = head_pressed;
            hold_state.source_b_pressed = other_pressed;
            hold_state.modifier_hold = false;

            if (transformed.needs_unshift) {
                tap_code16_unshifted(transformed.keycode);
                hold_state.is_held = false;
            } else {
                if (shifted) {
                    bool physical_lshift = (get_mods() & MOD_LSFT);
                    register_code16_with_shift(transformed.keycode);
                    hold_state.shift_held = true;
                    hold_state.shift_injected = !physical_lshift;
                } else {
                    register_code16_without_shift(transformed.keycode);
                    hold_state.shift_held = false;
                    hold_state.shift_injected = false;
                }
            }
            fifo_remove(i);
            fifo_remove(0);
            return true;
        }
    }
    return false;
}

/**
 * @param transform_fn キー変換関数
 */
static inline void combo_fifo_service_basic(key_transform_fn_t transform_fn) {
    while (combo_fifo_len > 0) {
        if (combo_fifo_len == 1) {
            if (combo_fifo[0].released) {
                uint16_t base_kc = combo_fifo[0].keycode;
                uint16_t out_kc = transform_fn(base_kc);
                tap_code16(out_kc);
                fifo_remove(0);
                continue;
            }
            if (timer_elapsed(combo_fifo[0].time_pressed) > COMBO_TIMEOUT_MS) {
                uint16_t base_kc = combo_fifo[0].keycode;
                uint16_t out_kc = transform_fn(base_kc);
                clear_hold_state();
                // 単要素時は長押し対応のためホールド
                hold_state.keycode = out_kc;
                hold_state.time_confirmed = timer_read();
                hold_state.is_held = true;
                hold_state.source_key_a = base_kc;
                hold_state.source_key_b = 0;
                hold_state.source_a_pressed = true;
                hold_state.source_b_pressed = false;
                hold_state.shift_held = false;
                hold_state.shift_injected = false;
                register_code16_without_shift(out_kc);
                fifo_remove(0);
                continue;
            }
            break;
        }
        if (combo_fifo_len >= 2) {
            if (resolve_combo_head_basic(transform_fn)) {
                continue;
            }
            if (combo_fifo[0].released) {
                clear_hold_state();
                uint16_t base_kc = combo_fifo[0].keycode;
                uint16_t out_kc = transform_fn(base_kc);
                fifo_remove(0);
                tap_code16(out_kc);
                continue;
            } else {
                // 先頭キーの個別タイムアウトをチェック
                if (timer_elapsed(combo_fifo[0].time_pressed) > COMBO_TIMEOUT_MS) {
                    uint16_t base_kc = combo_fifo[0].keycode;
                    uint16_t out_kc = transform_fn(base_kc);
                    tap_code16(out_kc);
                    fifo_remove(0);
                    continue;
                }
                break;
            }
        }
        break;
    }
}

/**
 * @param transform_fn 拡張キー変換関数
 */
static inline void combo_fifo_service_extended(key_transform_extended_fn_t transform_fn) {
    while (combo_fifo_len > 0) {
        if (combo_fifo_len == 1) {
            if (combo_fifo[0].released) {
                uint16_t base_kc = combo_fifo[0].keycode;

                // KC_LSFT/KC_RSFT の特別な処理
                if (base_kc == KC_LSFT) {
                    extern uint16_t rshift_timer;
                    if (rshift_timer != 0) {
                        tap_code16(KC_SPC);
                    } else {
                        tap_code16_unshifted(KC_SPC);
                    }
                    fifo_remove(0);
                    continue;
                } else if (base_kc == KC_RSFT) {
                    extern uint16_t lshift_timer;
                    if (lshift_timer != 0) {
                        tap_code16(KC_ENT);
                    } else {
                        tap_code16_unshifted(KC_ENT);
                    }
                    fifo_remove(0);
                    continue;
                }

                uint8_t layer = combo_fifo[0].layer;  // 保存されたレイヤーを使用
                uint8_t mods = combo_fifo[0].mods;  // 保存されたmodを使用
                bool shifted = (mods & MOD_MASK_SHIFT);
                transformed_key_t transformed = transform_fn(base_kc, shifted, layer);
                if (combo_fifo_custom_action(transformed.keycode, shifted, transformed.needs_unshift, false)) {
                    fifo_remove(0);
                    continue;
                }
                if (transformed.needs_unshift) {
                    tap_code16_unshifted(transformed.keycode);
                } else if (shifted) {
                    tap_code16_with_shift(transformed.keycode);
                } else {
                    tap_code16_unshifted(transformed.keycode);
                }
                fifo_remove(0);
                continue;
            }
            if (timer_elapsed(combo_fifo[0].time_pressed) > COMBO_TIMEOUT_MS) {
                uint16_t base_kc = combo_fifo[0].keycode;

                if (base_kc == KC_LSFT || base_kc == KC_RSFT) {
                    fifo_remove(0);
                    continue;
                }

                uint8_t layer = combo_fifo[0].layer;  // 保存されたレイヤーを使用
                uint8_t mods = combo_fifo[0].mods;  // 保存されたmodを使用
                bool shifted = (mods & MOD_MASK_SHIFT);
                transformed_key_t transformed = transform_fn(base_kc, shifted, layer);
                if (combo_fifo_custom_action(transformed.keycode, shifted, transformed.needs_unshift, true)) {
                    clear_hold_state();
                    fifo_remove(0);
                    continue;
                }
                clear_hold_state();
                // 単要素時は長押し対応のためホールド（needs_unshiftの場合はtap）
                hold_state.keycode = transformed.keycode;
                hold_state.time_confirmed = timer_read();
                hold_state.is_held = !transformed.needs_unshift && !shifted;  // shiftなしの場合のみホールド
                hold_state.source_key_a = base_kc;
                hold_state.source_key_b = 0;
                hold_state.source_a_pressed = true;
                hold_state.source_b_pressed = false;
                hold_state.shift_held = false;  // デフォルトは shift を含まない
                hold_state.shift_injected = false;
                if (transformed.needs_unshift) {
                    tap_code16_unshifted(transformed.keycode);
                } else if (shifted) {
                    bool physical_lshift = (get_mods() & MOD_LSFT);
                    register_code16_with_shift(transformed.keycode);
                    hold_state.shift_held = true;
                    hold_state.shift_injected = !physical_lshift;
                    hold_state.is_held = true;
                } else {
                    register_code16_without_shift(transformed.keycode);
                    hold_state.shift_held = false;
                    hold_state.shift_injected = false;
                    hold_state.is_held = true;
                }
                fifo_remove(0);
                continue;
            }
            break;
        }
        if (combo_fifo_len >= 2) {
            if (resolve_combo_head_extended(transform_fn)) {
                continue;
            }
            if (combo_fifo[0].released) {
                clear_hold_state();
                uint16_t base_kc = combo_fifo[0].keycode;
                uint8_t layer = combo_fifo[0].layer;  // 保存されたレイヤーを使用
                uint8_t mods = combo_fifo[0].mods;  // 保存されたmodを使用
                bool shifted = (mods & MOD_MASK_SHIFT);
                transformed_key_t transformed = transform_fn(base_kc, shifted, layer);
                fifo_remove(0);
                if (transformed.needs_unshift) {
                    tap_code16_unshifted(transformed.keycode);
                } else if (shifted) {
                    tap_code16_with_shift(transformed.keycode);
                } else {
                    tap_code16_unshifted(transformed.keycode);
                }
                continue;
            } else {
                // 先頭キーの個別タイムアウトをチェック
                if (timer_elapsed(combo_fifo[0].time_pressed) > COMBO_TIMEOUT_MS) {
                    uint16_t base_kc = combo_fifo[0].keycode;
                    uint8_t layer = combo_fifo[0].layer;  // 保存されたレイヤーを使用
                    uint8_t mods = combo_fifo[0].mods;  // 保存されたmodを使用
                    bool shifted = (mods & MOD_MASK_SHIFT);
                    transformed_key_t transformed = transform_fn(base_kc, shifted, layer);
                    if (transformed.needs_unshift) {
                        tap_code16_unshifted(transformed.keycode);
                    } else if (shifted) {
                        tap_code16_with_shift(transformed.keycode);
                    } else {
                        tap_code16_unshifted(transformed.keycode);
                    }
                    fifo_remove(0);
                    continue;
                }
                break;
            }
        }
        break;
    }
}
