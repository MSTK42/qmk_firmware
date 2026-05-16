#include "mejiro_fifo.h"
#include "mejiro_commands.h"
#include "mejiro_abbreviations.h"
#include "mejiro_transform.h"
#include "jis_transform.h"
#include <stdlib.h>
#include <string.h>

#define MEJIRO_MAX_KEYS 32

static uint16_t chord[MEJIRO_MAX_KEYS];
static uint16_t held_stn_keys[MEJIRO_MAX_KEYS];
static uint8_t  chord_len = 0;
static uint8_t  held_stn_len = 0;
static uint8_t  down_count = 0;
static uint8_t  prev_down_count = 0;
static bool     chord_active = false;
static bool     chord_has_new_press = false;
static bool     should_send_passthrough = false;  // 変換失敗時のパススルーフラグ
static bool     last_output_was_space = false;    // 直近に出力した文字がスペースかを記録

bool            mejiro_first_up_chord_send = true;

#define HISTORY_SIZE 20
static char     history_outputs[HISTORY_SIZE][64];
static uint8_t  history_lengths[HISTORY_SIZE];
static uint8_t  history_count = 0;

#define MACRO_VALUE_SIZE 512
#define MACRO_KEY_COUNT 7
static const char *macro_keys[MACRO_KEY_COUNT] = {"n", "t", "k", "nt", "nk", "tk", "ntk"};
static char macro_values[MACRO_KEY_COUNT][MACRO_VALUE_SIZE];
static bool active_recording_macros[MACRO_KEY_COUNT];
static uint8_t recording_macro_order[MACRO_KEY_COUNT];
static uint8_t recording_macro_order_len = 0;

static bool contains(uint16_t kc) {
    for (uint8_t i = 0; i < chord_len; i++) {
        if (chord[i] == kc) return true;
    }
    return false;
}

static bool held_contains(uint16_t kc) {
    for (uint8_t i = 0; i < held_stn_len; i++) {
        if (held_stn_keys[i] == kc) return true;
    }
    return false;
}

static void append_held_stn(uint16_t kc) {
    if (held_stn_len >= MEJIRO_MAX_KEYS) return;
    if (held_contains(kc)) return;
    held_stn_keys[held_stn_len++] = kc;
}

static void remove_held_stn(uint16_t kc) {
    for (uint8_t i = 0; i < held_stn_len; i++) {
        if (held_stn_keys[i] == kc) {
            for (uint8_t j = i; j + 1 < held_stn_len; j++) {
                held_stn_keys[j] = held_stn_keys[j + 1];
            }
            held_stn_len--;
            return;
        }
    }
}

static void seed_chord_from_held_stn(void) {
    if (held_stn_len == 0) return;
    chord_active = true;
    chord_has_new_press = false;
    for (uint8_t i = 0; i < held_stn_len; i++) {
        if (chord_len >= MEJIRO_MAX_KEYS) return;
        if (contains(held_stn_keys[i])) continue;
        chord[chord_len++] = held_stn_keys[i];
    }
}

bool is_stn_key(uint16_t kc) {
    switch (kc) {
        case STN_N1: case STN_S1: case STN_TL: case STN_PL: case STN_HL: case STN_ST1: case STN_ST3:
        case STN_FR: case STN_PR: case STN_LR: case STN_TR: case STN_DR:
        case STN_N2: case STN_S2: case STN_KL: case STN_WL: case STN_RL: case STN_ST2: case STN_ST4:
        case STN_RR: case STN_BR: case STN_GR: case STN_SR: case STN_ZR:
        case STN_N3: case STN_N4:
        case STN_A:  case STN_O:  case STN_E:  case STN_U:
            return true;
        default:
            return false;
    }
}

static void reset_chord(void) {
    chord_len = 0;
    chord_active = false;
    chord_has_new_press = false;
    should_send_passthrough = false;
}

static void append_kc(uint16_t kc) {
    if (chord_len >= MEJIRO_MAX_KEYS) return;
    if (contains(kc)) return;
    chord[chord_len++] = kc;
}

static void send_backspace_times(uint8_t cnt) {
    for (uint8_t i = 0; i < cnt; i++) {
        tap_code(KC_BSPC);
    }
    last_output_was_space = false;
}

// 文字を対応するキーコードへ簡易マッピング（必要な記号のみ）
static uint16_t map_char_to_kc(char c) {
    switch (c) {
        case '"': return KC_DQUO;
        case '\'': return KC_QUOT;
        case '|':  return KC_PIPE;
        case ':':  return KC_COLN;
        case '/':  return KC_SLSH;
        case '*':  return KC_ASTR;
        case '~':  return KC_TILD;
        case '^':  return KC_CIRC;
        case '(':  return KC_LPRN;
        case ')':  return KC_RPRN;
        case '[':  return KC_LBRC;
        case ']':  return KC_RBRC;
        case '{':  return KC_LCBR;
        case '}':  return KC_RCBR;
        case '<':  return KC_LABK;
        case '>':  return KC_RABK;
        case '.':  return KC_DOT;
        case ',':  return KC_COMM;
        case '?':  return KC_QUES;
        case '!':  return KC_EXLM;
        case ' ':  return KC_SPC;
        default:   return 0;  // 未対応
    }
}

static void send_string_jis_aware(const char *s) {
    if (!is_jis_mode) {
        send_string(s);
        return;
    }
    // JISモード: 記号はキーコードに変換して送信
    for (const char *p = s; *p; p++) {
        uint16_t kc = map_char_to_kc(*p);
        if (kc != 0) {
            uint16_t jis_kc = jis_transform(kc, false);
            tap_code16(jis_kc);
        } else {
            // 未対応文字はそのまま送る（英数は概ね問題ない）
            char buf[2] = {*p, '\0'};
            send_string(buf);
        }
    }
}

static bool ends_with_space(const char *s) {
    size_t len = strlen(s);
    return (len > 0 && s[len - 1] == ' ');
}

static bool is_user_abbreviation_pattern(const char *pattern) {
    if (pattern == NULL || pattern[0] == '\0') {
        return false;
    }

    char stroke[64] = {0};
    strncpy(stroke, pattern, sizeof(stroke) - 1);

    char *asterisk_pos = strchr(stroke, '*');
    bool has_asterisk = (asterisk_pos != NULL);
    if (asterisk_pos != NULL) {
        *asterisk_pos = '\0';
    }

    abbreviation_result_t result = mejiro_user_abbreviation(stroke, has_asterisk);
    return result.success;
}

static int macro_key_to_index(const char *key) {
    for (uint8_t i = 0; i < MACRO_KEY_COUNT; i++) {
        if (strcmp(key, macro_keys[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static void push_macro_order(uint8_t idx) {
    for (uint8_t i = 0; i < recording_macro_order_len; i++) {
        if (recording_macro_order[i] == idx) {
            for (uint8_t j = i; j + 1 < recording_macro_order_len; j++) {
                recording_macro_order[j] = recording_macro_order[j + 1];
            }
            recording_macro_order_len--;
            break;
        }
    }
    if (recording_macro_order_len < MACRO_KEY_COUNT) {
        recording_macro_order[recording_macro_order_len++] = idx;
    }
}

static void remove_macro_order(uint8_t idx) {
    for (uint8_t i = 0; i < recording_macro_order_len; i++) {
        if (recording_macro_order[i] == idx) {
            for (uint8_t j = i; j + 1 < recording_macro_order_len; j++) {
                recording_macro_order[j] = recording_macro_order[j + 1];
            }
            recording_macro_order_len--;
            break;
        }
    }
}

static void append_to_active_macros(const char *s) {
    if (s == NULL || s[0] == '\0') {
        return;
    }
    for (uint8_t i = 0; i < MACRO_KEY_COUNT; i++) {
        if (!active_recording_macros[i]) {
            continue;
        }
        size_t current_len = strlen(macro_values[i]);
        if (current_len >= MACRO_VALUE_SIZE - 1) {
            continue;
        }
        size_t remain = MACRO_VALUE_SIZE - current_len - 1;
        strncat(macro_values[i], s, remain);
    }
}

static void push_history(const char *output, uint8_t length) {
    if (output == NULL || output[0] == '\0') {
        return;
    }
    if (history_count < HISTORY_SIZE) {
        strncpy(history_outputs[history_count], output, sizeof(history_outputs[0]) - 1);
        history_outputs[history_count][sizeof(history_outputs[0]) - 1] = '\0';
        history_lengths[history_count] = length;
        history_count++;
        return;
    }
    for (uint8_t i = 0; i < HISTORY_SIZE - 1; i++) {
        strcpy(history_outputs[i], history_outputs[i + 1]);
        history_lengths[i] = history_lengths[i + 1];
    }
    strncpy(history_outputs[HISTORY_SIZE - 1], output, sizeof(history_outputs[0]) - 1);
    history_outputs[HISTORY_SIZE - 1][sizeof(history_outputs[0]) - 1] = '\0';
    history_lengths[HISTORY_SIZE - 1] = length;
}

// メジロIDのビット順: #,S,T,K,N,Y,I,A,U,n,t,k | S,T,K,N,Y,I,A,U,n,t,k,*
// ビットインデックスへ変換（押されていると1に立てる）
static uint8_t stn_to_bit(uint16_t kc) {
    switch (kc) {
        // 左手
        case STN_N1: case STN_N2: return 0; // #
        case STN_S1: case STN_S2: return 1;  // S-
        case STN_TL:              return 2;  // T-
        case STN_KL:              return 3;  // K-
        case STN_WL:              return 4;  // N-
        case STN_PL:              return 5;  // Y-
        case STN_HL:              return 6;  // I-
        case STN_RL:              return 7;  // A-
        case STN_ST1: case STN_ST2: return 8;  // U- (*1,*2)
        case STN_N3:              return 9;  // n- (#3)
        case STN_A:               return 10;  // t- (A-)
        case STN_O:               return 11; // k- (O-)
        // 右手
        case STN_SR: case STN_TR: return 12; // -S (-T,-S)
        case STN_LR:              return 13; // -T (-L)
        case STN_GR:              return 14; // -K (-G)
        case STN_BR:              return 15; // -N (-B)
        case STN_PR:              return 16; // -Y (-P)
        case STN_FR:              return 17; // -I (-F)
        case STN_RR:              return 18; // -A (-R)
        case STN_ST3: case STN_ST4: return 19; // -U (*3,*4)
        case STN_N4:              return 20; // -n (#4)
        case STN_U:               return 21; // -t (-U)
        case STN_E:               return 22; // -k (-E)
        case STN_DR: case STN_ZR: return 23; // * (-D,-Z)
        default:
            return 0xFF;
    }
}

// メジロID列に変換して送信
// 両手: 左→"-"→右
// 左のみ: 左→"-"（末尾ハイフンで左手を示す）
// 右のみ: "-"→右（先頭ハイフンで右手を示す）
// 変換表（例）:
//   "#"  : repeat last output
//   "-U" : undo last output (同じ長さだけBackspace)
//   "-AU": Backspace 1回
//   "-IU": Delete 1回
//   "-S" : Escape
static void convert_and_send(void) {
    if (chord_len == 0) return;

    uint32_t bits = 0;
    for (uint8_t i = 0; i < chord_len; i++) {
        uint8_t idx = stn_to_bit(chord[i]);
        if (idx != 0xFF) {
            bits |= (1UL << idx);
        }
    }

    static const char *labels_left[12] = {"#","S","T","K","N","Y","I","A","U","n","t","k"};
    static const char *labels_right[12] = {"S","T","K","N","Y","I","A","U","n","t","k","*"};

    char out[64];
    uint8_t pos = 0;
    bool left_has = (bits & 0xFFF) != 0;
    bool right_has = (bits & 0xFFF000) != 0;

    // 左側
    for (uint8_t i = 0; i < 12; i++) {
        if (bits & (1UL << i)) {
            const char *p = labels_left[i];
            while (*p && pos < sizeof(out) - 1) out[pos++] = *p++;
        }
    }

    // ハイフン配置
    if (left_has && right_has) {
        if (pos < sizeof(out) - 1) out[pos++] = '-';
    } else if (left_has && !right_has) {
        if (pos < sizeof(out) - 1) out[pos++] = '-';
    } else if (!left_has && right_has) {
        if (pos < sizeof(out) - 1) out[pos++] = '-';
    }

    // 右側
    for (uint8_t i = 0; i < 12; i++) {
        uint8_t bit = 12 + i;
        if (bits & (1UL << bit)) {
            const char *p = labels_right[i];
            while (*p && pos < sizeof(out) - 1) out[pos++] = *p++;
        }
    }

    out[pos] = '\0';
    if (pos == 0) return;

    // # を含むパターンの処理：# を除いたパターンを処理して出力を繰り返す
    // ただし、# で始まるコマンドテーブル登録パターン（#-S など）は繰り返し対象外
    bool has_hash = strchr(out, '#') != NULL;
    bool has_asterisk = strchr(out, '*') != NULL;
    char pattern_without_hash[64] = {0};

    if (has_hash) {
        // # を除いたパターンを生成
        uint8_t j = 0;
        for (uint8_t i = 0; i < pos && j < sizeof(pattern_without_hash) - 1; i++) {
            if (out[i] != '#') {
                pattern_without_hash[j++] = out[i];
            }
        }
        pattern_without_hash[j] = '\0';
    }

    // マクロ処理（Plover互換）
    // 左手は bit0 が '#' なので、子音/母音判定では除外する
    bool left_has_conso_or_vowel = (bits & (0xFFUL << 1)) != 0;
    // 左粒子キーは n/t/k (= bit9/10/11)
    bool left_has_particle = (bits & ((1UL << 9) | (1UL << 10) | (1UL << 11))) != 0;
    bool right_has_conso_or_vowel = (bits & (0xFFUL << 12)) != 0;
    bool right_has_particle = (bits & ((1UL << 20) | (1UL << 21) | (1UL << 22))) != 0;
    bool pure_left_particle = (!left_has_conso_or_vowel && left_has_particle && !right_has_conso_or_vowel && !right_has_particle);

    char left_particle_key[8] = {0};
    uint8_t lp = 0;
    if (bits & (1UL << 9)) left_particle_key[lp++] = 'n';
    if (bits & (1UL << 10)) left_particle_key[lp++] = 't';
    if (bits & (1UL << 11)) left_particle_key[lp++] = 'k';
    left_particle_key[lp] = '\0';

    int macro_idx = macro_key_to_index(left_particle_key);
    bool is_macro_target = pure_left_particle && (macro_idx >= 0);
    bool is_macro_record_toggle = has_hash && has_asterisk && is_macro_target;
    bool is_macro_record_stop_generic = has_hash && has_asterisk &&
                                        !left_has_conso_or_vowel && !left_has_particle &&
                                        !right_has_conso_or_vowel && !right_has_particle;
    bool is_macro_replay = has_hash && !has_asterisk && is_macro_target;

    if (is_macro_record_toggle) {
        if (active_recording_macros[macro_idx]) {
            active_recording_macros[macro_idx] = false;
            remove_macro_order((uint8_t)macro_idx);
        } else {
            macro_values[macro_idx][0] = '\0';
            active_recording_macros[macro_idx] = true;
            push_macro_order((uint8_t)macro_idx);
        }
        return;
    }

    if (is_macro_record_stop_generic) {
        if (recording_macro_order_len > 0) {
            uint8_t idx = recording_macro_order[recording_macro_order_len - 1];
            recording_macro_order_len--;
            active_recording_macros[idx] = false;
        }
        return;
    }

    if (is_macro_replay) {
        const char *macro_output = macro_values[macro_idx];
        if (macro_output[0] != '\0') {
            send_string(macro_output);
            append_to_active_macros(macro_output);
            push_history(macro_output, (uint8_t)strlen(macro_output));
        }
        return;
    }

    // 変換テーブル検索：まず元のパターンで検索（#- などの特殊コマンドを優先）
    for (uint8_t i = 0; i < mejiro_command_count; i++) {
        if (strcmp(out, mejiro_commands[i].pattern) == 0) {
            switch (mejiro_commands[i].type) {
                case CMD_REPEAT:
                    if (history_count > 0) {
                        uint8_t idx = history_count - 1;
                        send_string(history_outputs[idx]);
                        append_to_active_macros(history_outputs[idx]);
                        last_output_was_space = ends_with_space(history_outputs[idx]);
                        // 繰り返した出力を新しい履歴として追加
                        if (history_count < HISTORY_SIZE) {
                            memmove(history_outputs[history_count], history_outputs[idx], sizeof(history_outputs[0]));
                            history_lengths[history_count] = history_lengths[idx];
                            history_count++;
                        } else {
                            // 履歴が満杯の場合は古いものをシフト
                            for (uint8_t j = 0; j < HISTORY_SIZE - 1; j++) {
                                memmove(history_outputs[j], history_outputs[j + 1], sizeof(history_outputs[0]));
                                history_lengths[j] = history_lengths[j + 1];
                            }
                            memmove(history_outputs[HISTORY_SIZE - 1], history_outputs[idx], sizeof(history_outputs[0]));
                            history_lengths[HISTORY_SIZE - 1] = history_lengths[idx];
                        }
                    }
                    return;

                case CMD_UNDO:
                    if (history_count > 0) {
                        uint8_t idx = history_count - 1;
                        send_backspace_times(history_lengths[idx]);
                        history_count--;
                    } else {
                        // 履歴がない場合はデフォルトで2文字削除
                        send_backspace_times(2);
                    }
                    // 「っ」の持ち越し状態をクリア
                    mejiro_clear_pending_tsu();
                    return;

                case CMD_KEYCODE: {
                    uint16_t kc = mejiro_commands[i].action.keycode;
                    // JISモード時はキーコードを変換
                    if (is_jis_mode) {
                        // 記号キーの場合は変換が必要
                        kc = jis_transform(kc, false);
                    }
                    tap_code16(kc);
                    // BackspaceまたはDeleteの場合は「っ」の持ち越し状態をクリア
                    if (kc == KC_BSPC || kc == KC_DEL) {
                        mejiro_clear_pending_tsu();
                    }
                    if (kc == KC_SPC) {
                        last_output_was_space = true;
                    }
                    // バックスペース（-AU）の場合は直前の履歴長を1減らす
                    if (kc == KC_BSPC) {
                        if (last_output_was_space) {
                            // 直前の出力がスペースなら履歴は変更しない
                            last_output_was_space = false;
                            return;
                        }
                        if (history_count > 0) {
                            uint8_t idx = history_count - 1;
                            if (history_lengths[idx] > 0) {
                                history_lengths[idx] -= 1;
                                // 0 になったら履歴を1つ削除（ポップ）
                                if (history_lengths[idx] == 0) {
                                    history_count -= 1;
                                }
                            }
                        }
                        last_output_was_space = false;
                    }
                    if (kc != KC_SPC) {
                        last_output_was_space = false;
                    }
                    // コマンドテーブル登録パターンは1回だけ実行
                    return;
                }

                case CMD_STRING: {
                    const char *str = mejiro_commands[i].action.string;

                    // {#Left} を含む場合はプレースホルダを除去して送信し、カーソルを左に移動
                    const char *left_token = strstr(str, "{#Left}");
                    const char *output_str = str;
                    char expanded[64];

                    if (left_token) {
                        size_t prefix_len = (size_t)(left_token - str);
                        size_t suffix_len = strlen(left_token + 7); // 7 = strlen("{#Left}")

                        size_t copy_len = prefix_len;
                        if (copy_len > sizeof(expanded) - 1) {
                            copy_len = sizeof(expanded) - 1;
                        }
                        memcpy(expanded, str, copy_len);

                        size_t remain = sizeof(expanded) - copy_len - 1;
                        size_t suffix_copy = suffix_len > remain ? remain : suffix_len;
                        memcpy(expanded + copy_len, left_token + 7, suffix_copy);
                        expanded[copy_len + suffix_copy] = '\0';

                        output_str = expanded;
                        send_string_jis_aware(expanded);
                        append_to_active_macros(expanded);
                        tap_code(KC_LEFT);
                    } else {
                        send_string_jis_aware(str);
                        append_to_active_macros(str);
                    }

                    last_output_was_space = ends_with_space(output_str);

                    // コマンドテーブル登録パターンは1回だけ実行

                    // 出力した文字列（プレースホルダ除去後）を履歴に追加
                    const char *store_str = output_str;
                    if (history_count < HISTORY_SIZE) {
                        strncpy(history_outputs[history_count], store_str, sizeof(history_outputs[0]) - 1);
                        history_outputs[history_count][sizeof(history_outputs[0]) - 1] = '\0';
                        history_lengths[history_count] = (uint8_t)strlen(history_outputs[history_count]);
                        history_count++;
                    } else {
                        // 履歴が満杯の場合は古いものをシフト
                        for (uint8_t j = 0; j < HISTORY_SIZE - 1; j++) {
                            strcpy(history_outputs[j], history_outputs[j + 1]);
                            history_lengths[j] = history_lengths[j + 1];
                        }
                        strncpy(history_outputs[HISTORY_SIZE - 1], store_str, sizeof(history_outputs[0]) - 1);
                        history_outputs[HISTORY_SIZE - 1][sizeof(history_outputs[0]) - 1] = '\0';
                        history_lengths[HISTORY_SIZE - 1] = (uint8_t)strlen(history_outputs[HISTORY_SIZE - 1]);
                    }
                    return;
                }
            }
        }
    }

    // # を含む場合、二次検索：# を除いたパターンでコマンドテーブルを再検索
    if (has_hash && pattern_without_hash[0] != '\0') {
        for (uint8_t i = 0; i < mejiro_command_count; i++) {
            if (strcmp(pattern_without_hash, mejiro_commands[i].pattern) == 0) {
                switch (mejiro_commands[i].type) {
                    case CMD_REPEAT:
                        if (history_count > 0) {
                            uint8_t idx = history_count - 1;
                            send_string(history_outputs[idx]);
                            append_to_active_macros(history_outputs[idx]);
                            // # を含む場合は同じ出力をもう一度送信
                            send_string(history_outputs[idx]);
                            append_to_active_macros(history_outputs[idx]);
                            last_output_was_space = ends_with_space(history_outputs[idx]);

                            // 繰り返した出力（2倍の長さ）を新しい履歴として追加
                            uint8_t doubled_length = history_lengths[idx] * 2;
                            if (history_count < HISTORY_SIZE) {
                                memmove(history_outputs[history_count], history_outputs[idx], sizeof(history_outputs[0]));
                                history_lengths[history_count] = doubled_length;
                                history_count++;
                            } else {
                                // 履歴が満杯の場合は古いものをシフト
                                for (uint8_t j = 0; j < HISTORY_SIZE - 1; j++) {
                                    memmove(history_outputs[j], history_outputs[j + 1], sizeof(history_outputs[0]));
                                    history_lengths[j] = history_lengths[j + 1];
                                }
                                memmove(history_outputs[HISTORY_SIZE - 1], history_outputs[idx], sizeof(history_outputs[0]));
                                history_lengths[HISTORY_SIZE - 1] = doubled_length;
                            }
                        }
                        return;

                    case CMD_UNDO:
                        if (history_count > 0) {
                            uint8_t idx = history_count - 1;
                            send_backspace_times(history_lengths[idx]);
                            history_count--;
                        } else {
                            send_backspace_times(2);
                        }
                        last_output_was_space = false;
                        return;

                    case CMD_KEYCODE: {
                        uint16_t kc = mejiro_commands[i].action.keycode;
                        if (is_jis_mode) {
                            kc = jis_transform(kc, false);
                        }
                        tap_code16(kc);
                        if (kc == KC_SPC) {
                            last_output_was_space = true;
                        } else {
                            last_output_was_space = false;
                        }
                        // # を含む場合は同じキーコードをもう一度送信
                        tap_code16(kc);
                        if (kc == KC_SPC) {
                            last_output_was_space = true;
                        }
                        return;
                    }

                    case CMD_STRING: {
                        const char *str = mejiro_commands[i].action.string;
                        const char *left_token = strstr(str, "{#Left}");
                        const char *output_str = str;
                        char expanded[64];
                        bool has_left_placeholder = false;

                        if (left_token) {
                            has_left_placeholder = true;
                            size_t prefix_len = (size_t)(left_token - str);
                            size_t suffix_len = strlen(left_token + 7);

                            size_t copy_len = prefix_len;
                            if (copy_len > sizeof(expanded) - 1) {
                                copy_len = sizeof(expanded) - 1;
                            }
                            memcpy(expanded, str, copy_len);

                            size_t remain = sizeof(expanded) - copy_len - 1;
                            size_t suffix_copy = suffix_len > remain ? remain : suffix_len;
                            memcpy(expanded + copy_len, left_token + 7, suffix_copy);
                            expanded[copy_len + suffix_copy] = '\0';

                            output_str = expanded;
                            send_string_jis_aware(expanded);
                            append_to_active_macros(expanded);
                            tap_code(KC_LEFT);
                        } else {
                            send_string_jis_aware(str);
                            append_to_active_macros(str);
                        }

                        // # を含む場合は同じ出力をもう一度送信
                        send_string_jis_aware(output_str);
                        append_to_active_macros(output_str);
                        if (has_left_placeholder) {
                            tap_code(KC_LEFT);
                        }

                        const char *store_str = output_str;
                        last_output_was_space = ends_with_space(store_str);
                        if (history_count < HISTORY_SIZE) {
                            strncpy(history_outputs[history_count], store_str, sizeof(history_outputs[0]) - 1);
                            history_outputs[history_count][sizeof(history_outputs[0]) - 1] = '\0';
                            history_lengths[history_count] = (uint8_t)strlen(history_outputs[history_count]);
                            history_count++;
                        } else {
                            for (uint8_t j = 0; j < HISTORY_SIZE - 1; j++) {
                                strcpy(history_outputs[j], history_outputs[j + 1]);
                                history_lengths[j] = history_lengths[j + 1];
                            }
                            strncpy(history_outputs[HISTORY_SIZE - 1], store_str, sizeof(history_outputs[0]) - 1);
                            history_outputs[HISTORY_SIZE - 1][sizeof(history_outputs[0]) - 1] = '\0';
                            history_lengths[HISTORY_SIZE - 1] = (uint8_t)strlen(history_outputs[HISTORY_SIZE - 1]);
                        }
                        return;
                    }
                }
            }
        }
    }

    // 右手のみ入力も含め、未コマンド入力は一度すべて mejiro_transform に渡す。
    // 変換できなかった場合のみ下の失敗分岐でパススルーする。

    // ユーザー略語では # を含めたまま変換し、# による繰り返しを行わない
    bool is_user_abbr = is_user_abbreviation_pattern(out);

    // mejiro_transform: # を含む場合は # を除いたパターンで変換（ユーザー略語は除外）
    const char *transform_pattern =
        (has_hash && !is_user_abbr && pattern_without_hash[0] != '\0') ? pattern_without_hash : out;
    mejiro_result_t transformed = mejiro_transform(transform_pattern);

    // 変換成功時は変換結果を出力、失敗時は元のSTN_キーコードをそのまま送信
    if (transformed.success) {
        const char *final_output = transformed.output;
        uint8_t final_length = (uint8_t)transformed.kana_length;
        send_string(final_output);
        append_to_active_macros(final_output);

        // # を含む場合は同じ出力をもう一度送信（ユーザー略語は除外）
        if (has_hash && !is_user_abbr) {
            send_string(final_output);
            append_to_active_macros(final_output);
            // 履歴の長さも2倍にする
            final_length *= 2;
        }

        push_history(final_output, final_length);
    } else {
        // メジロ変換失敗時はパススルーフラグをセット
        should_send_passthrough = true;
    }
}

void mejiro_on_press(uint16_t kc) {
    append_held_stn(kc);
    down_count++;
    if (down_count > prev_down_count) {
        if (!chord_active) chord_active = true;
        append_kc(kc);
        chord_has_new_press = true;
    }
    prev_down_count = down_count;
}

void mejiro_on_release(uint16_t kc) {
    remove_held_stn(kc);
    if (down_count > 0) down_count--;

    if (mejiro_first_up_chord_send && !chord_has_new_press) {
        reset_chord();
        seed_chord_from_held_stn();
    }

    bool should_commit = mejiro_first_up_chord_send
                           ? (chord_has_new_press && down_count < prev_down_count)
                           : (down_count == 0 && prev_down_count > 0);

    if (chord_active && should_commit && chord_len > 0) {
        convert_and_send();
        reset_chord();
        if (mejiro_first_up_chord_send && down_count > 0) {
            seed_chord_from_held_stn();
        }
    }
    prev_down_count = down_count;
}

bool mejiro_should_send_passthrough(void) {
    return should_send_passthrough;
}

void mejiro_send_passthrough_keys(void) {
    // GeminiPRプロトコルの場合、STN_キーを正しいパケット形式で送信
    // chord配列にあるすべてのSTN_キーコードをGeminiPRプロトコル経由で送信

    // STN_キーコードを登録（GeminiPRプロトコルはQMKで自動的に処理される）
    for (uint8_t i = 0; i < chord_len; i++) {
        uint16_t kc = chord[i];
        if (is_stn_key(kc)) {
            // register_code/unregister_codeを使用してキープレスイベントをシミュレート
            register_code(kc);
        }
    }

    // すべてのキーをリリース
    for (uint8_t i = 0; i < chord_len; i++) {
        uint16_t kc = chord[i];
        if (is_stn_key(kc)) {
            unregister_code(kc);
        }
    }
}

void mejiro_reset_state(void) {
    reset_chord();
    held_stn_len = 0;
    down_count = 0;
    prev_down_count = 0;
    last_output_was_space = false;

    // Keep saved macros across steno mode exits, but stop any in-progress recording.
    recording_macro_order_len = 0;
    for (uint8_t i = 0; i < MACRO_KEY_COUNT; i++) {
        active_recording_macros[i] = false;
    }
}
