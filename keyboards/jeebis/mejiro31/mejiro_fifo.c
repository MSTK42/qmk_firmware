#include "mejiro_fifo.h"
#include "mejiro_commands.h"
#include "mejiro_transform.h"
#include "jis_transform.h"
#include <stdlib.h>
#include <string.h>

#define MEJIRO_MAX_KEYS 32

static uint16_t chord[MEJIRO_MAX_KEYS];
static uint8_t  chord_len = 0;
static uint8_t  down_count = 0;
static uint8_t  prev_down_count = 0;
static bool     chord_active = false;
static bool     should_send_passthrough = false;  // 変換失敗時のパススルーフラグ
static bool     last_output_was_space = false;    // 直近に出力した文字がスペースかを記録
bool            mejiro_first_up_chord_send = false;

#define HISTORY_SIZE 20
static char     history_outputs[HISTORY_SIZE][64];
static uint8_t  history_lengths[HISTORY_SIZE];
static uint8_t  history_count = 0;

static bool contains(uint16_t kc) {
    for (uint8_t i = 0; i < chord_len; i++) {
        if (chord[i] == kc) return true;
    }
    return false;
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

// メジロIDのビット順: S,T,K,N,Y,I,A,U,n,t,k,# | S,T,K,N,Y,I,A,U,n,t,k,*
// ビットインデックスへ変換（押されていると1に立てる）
static uint8_t stn_to_bit(uint16_t kc) {
    switch (kc) {
        // 左手
        case STN_S1: case STN_S2: return 0;  // S-
        case STN_TL:              return 1;  // T-
        case STN_KL:              return 2;  // K-
        case STN_WL:              return 3;  // N-
        case STN_PL:              return 4;  // Y-
        case STN_HL:              return 5;  // I-
        case STN_RL:              return 6;  // A-
        case STN_ST1: case STN_ST2: return 7;  // U- (*1,*2)
        case STN_N3:              return 8;  // n- (#3)
        case STN_A:               return 9;  // t- (A-)
        case STN_O:               return 10; // k- (O-)
        case STN_N1: case STN_N2: return 11; // #
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

    static const char *labels_left[12] = {"S","T","K","N","Y","I","A","U","n","t","k","#"};
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

    // 変換テーブル検索：まず元のパターンで検索（#- などの特殊コマンドを優先）
    for (uint8_t i = 0; i < mejiro_command_count; i++) {
        if (strcmp(out, mejiro_commands[i].pattern) == 0) {
            switch (mejiro_commands[i].type) {
                case CMD_REPEAT:
                    if (history_count > 0) {
                        uint8_t idx = history_count - 1;
                        send_string(history_outputs[idx]);
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
                        tap_code(KC_LEFT);
                    } else {
                        send_string_jis_aware(str);
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
                            // # を含む場合は同じ出力をもう一度送信
                            send_string(history_outputs[idx]);
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
                            tap_code(KC_LEFT);
                        } else {
                            send_string_jis_aware(str);
                        }

                        // # を含む場合は同じ出力をもう一度送信
                        send_string_jis_aware(output_str);
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

    // 右側だけの入力で、コマンドテーブルに登録されていない場合はSTN_コード出力
    // ただし、* を含む場合や、右手だけの助詞キー（k,t,n,tk,nt,nk,ntk）はメジロ変換を試みる
    if (!left_has && right_has && strchr(out, '*') == NULL) {
        const char *right_only = out[0] == '-' ? out + 1 : out;  // 先頭のハイフンを除去
        bool is_particle =
            (strcmp(right_only, "k") == 0) || (strcmp(right_only, "t") == 0) || (strcmp(right_only, "n") == 0) ||
            (strcmp(right_only, "tk") == 0) || (strcmp(right_only, "nt") == 0) || (strcmp(right_only, "nk") == 0) ||
            (strcmp(right_only, "ntk") == 0);

        if (!is_particle) {
            should_send_passthrough = true;
            return;
        }
    }

    // mejiro_transform: # を含む場合は # を除いたパターンで変換
    const char *transform_pattern = has_hash && pattern_without_hash[0] != '\0' ? pattern_without_hash : out;
    mejiro_result_t transformed = mejiro_transform(transform_pattern);

    // 変換成功時は変換結果を出力、失敗時は元のSTN_キーコードをそのまま送信
    if (transformed.success) {
        const char *final_output = transformed.output;
        uint8_t final_length = (uint8_t)transformed.kana_length;
        send_string(final_output);

        // # を含む場合は同じ出力をもう一度送信
        if (has_hash) {
            send_string(final_output);
            // 履歴の長さも2倍にする
            final_length *= 2;
        }

        if (history_count < HISTORY_SIZE) {
            strncpy(history_outputs[history_count], final_output, sizeof(history_outputs[0]) - 1);
            history_outputs[history_count][sizeof(history_outputs[0]) - 1] = '\0';
            history_lengths[history_count] = final_length;
            history_count++;
        } else {
            // 履歴が満杯の場合は古いものをシフトして最新を追加
            for (uint8_t i = 0; i < HISTORY_SIZE - 1; i++) {
                strcpy(history_outputs[i], history_outputs[i + 1]);
                history_lengths[i] = history_lengths[i + 1];
            }
            strncpy(history_outputs[HISTORY_SIZE - 1], final_output, sizeof(history_outputs[0]) - 1);
            history_outputs[HISTORY_SIZE - 1][sizeof(history_outputs[0]) - 1] = '\0';
            history_lengths[HISTORY_SIZE - 1] = final_length;
        }
    } else {
        // メジロ変換失敗時はパススルーフラグをセット
        should_send_passthrough = true;
    }
}

void mejiro_on_press(uint16_t kc) {
    down_count++;
    if (down_count > prev_down_count) {
        if (!chord_active) chord_active = true;
        append_kc(kc);
    }
    prev_down_count = down_count;
}

void mejiro_on_release(uint16_t kc) {
    (void)kc;
    if (down_count > 0) down_count--;

    bool should_commit = mejiro_first_up_chord_send
                           ? (down_count < prev_down_count)
                           : (down_count == 0 && prev_down_count > 0);

    if (chord_active && should_commit && chord_len > 0) {
        convert_and_send();
        reset_chord();
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
    last_output_was_space = false;
}
