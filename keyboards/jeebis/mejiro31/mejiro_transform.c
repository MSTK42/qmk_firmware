/*
 * Mejiro transformation logic implementation
 */
#include "mejiro_transform.h"
#include "mejiro_commands.h"
#include "mejiro_abbreviations.h"
#include "mejiro_verb.h"
#include <string.h>

// 前回の母音を保存（省略時に使用）
static char last_vowel_stroke[8] = "A";  // デフォルトは"A"
static char prev_joshi[32] = "";

// 「っ」の持ち越し状態
static bool pending_tsu = false;

// 「っ」の持ち越し状態をクリア（外部から呼び出し可能）
void mejiro_clear_pending_tsu(void) {
    pending_tsu = false;
}

// ひらがな→ヘボン式ローマ字変換テーブル
typedef struct {
    const char *kana;
    const char *roma;
} kana_roma_t;

static const kana_roma_t kana_roma_table[] = {
    // 基本五十音
    {"あ", "a"},  {"い", "i"},   {"う", "u"},   {"え", "e"},  {"お", "o"},
    {"か", "ka"}, {"き", "ki"},  {"く", "ku"},  {"け", "ke"}, {"こ", "ko"},
    {"さ", "sa"}, {"し", "shi"}, {"す", "su"},  {"せ", "se"}, {"そ", "so"},
    {"た", "ta"}, {"ち", "chi"}, {"つ", "tsu"}, {"て", "te"}, {"と", "to"},
    {"な", "na"}, {"に", "ni"},  {"ぬ", "nu"},  {"ね", "ne"}, {"の", "no"},
    {"は", "ha"}, {"ひ", "hi"},  {"ふ", "fu"},  {"へ", "he"}, {"ほ", "ho"},
    {"ま", "ma"}, {"み", "mi"},  {"む", "mu"},  {"め", "me"}, {"も", "mo"},
    {"や", "ya"}, {"ゆ", "yu"},  {"よ", "yo"},
    {"ら", "ra"}, {"り", "ri"},  {"る", "ru"},  {"れ", "re"}, {"ろ", "ro"},
    {"わ", "wa"}, {"ゐ", "wyi"}, {"ゑ", "wye"}, {"を", "wo"}, {"ん", "nn"},

    // 濁音
    {"が", "ga"}, {"ぎ", "gi"}, {"ぐ", "gu"}, {"げ", "ge"}, {"ご", "go"},
    {"ざ", "za"}, {"じ", "ji"}, {"ず", "zu"}, {"ぜ", "ze"}, {"ぞ", "zo"},
    {"だ", "da"}, {"ぢ", "di"}, {"づ", "du"}, {"で", "de"}, {"ど", "do"},
    {"ば", "ba"}, {"び", "bi"}, {"ぶ", "bu"}, {"べ", "be"}, {"ぼ", "bo"},

    // 半濁音
    {"ぱ", "pa"}, {"ぴ", "pi"}, {"ぷ", "pu"}, {"ぺ", "pe"}, {"ぽ", "po"},

    // 拗音(きゃ系)
    {"きゃ", "kya"}, {"きゅ", "kyu"}, {"きょ", "kyo"},
    {"しゃ", "sha"}, {"しゅ", "shu"}, {"しょ", "sho"},
    {"ちゃ", "cha"}, {"ちゅ", "chu"}, {"ちょ", "cho"},
    {"にゃ", "nya"}, {"にゅ", "nyu"}, {"にょ", "nyo"},
    {"ひゃ", "hya"}, {"ひゅ", "hyu"}, {"ひょ", "hyo"},
    {"みゃ", "mya"}, {"みゅ", "myu"}, {"みょ", "myo"},
    {"りゃ", "rya"}, {"りゅ", "ryu"}, {"りょ", "ryo"},
    {"ぎゃ", "gya"}, {"ぎゅ", "gyu"}, {"ぎょ", "gyo"},
    {"じゃ", "ja"},  {"じゅ", "ju"},  {"じょ", "jo"},
    {"ぢゃ", "dya"}, {"ぢゅ", "dyu"}, {"ぢょ", "dyo"},
    {"びゃ", "bya"}, {"びゅ", "byu"}, {"びょ", "byo"},
    {"ぴゃ", "pya"}, {"ぴゅ", "pyu"}, {"ぴょ", "pyo"},

    // 特殊音
    {"ゔ", "vu"},    {"ゔぁ", "va"},  {"ゔぃ", "vi"}, {"ゔぇ", "ve"}, {"ゔぉ", "vo"},
    {"ゔゅ", "vyu"},
    {"うぁ", "wha"}, {"うぃ", "wi"},  {"うぇ", "we"}, {"うぉ", "who"},
    {"ふぁ", "fa"},  {"ふぃ", "fi"},  {"ふぇ", "fe"}, {"ふぉ", "fo"},
    {"ふゃ", "fya"}, {"ふゅ", "fyu"}, {"ふょ", "fyo"},
    {"くぁ", "kwa"},  {"くぃ", "kwi"},  {"くぇ", "kwe"}, {"くぉ", "kwo"},
    {"いぇ", "ye"}, {"しぇ", "she"}, {"じぇ", "je"}, {"ちぇ", "che"},
    {"てぃ", "thi"}, {"でぃ", "dhi"}, {"でゅ", "dhu"},
    {"とぅ", "twu"}, {"どぅ", "dwu"},

    // 小書き文字
    {"ぁ", "xa"},  {"ぃ", "xi"},  {"ぅ", "xu"}, {"ぇ", "xe"}, {"ぉ", "xo"},
    {"ゃ", "xya"}, {"ゅ", "xyu"}, {"ょ", "xyo"},
    {"っ", "xtu"}, {"ゎ", "xwa"},

    // 長音符
    {"ー", "-"},

    // 句読点
    {"、", ","}, {"。", "."}, {"!", "!"}, {"?", "?"},

    {NULL, NULL}
};

static size_t utf8_char_count(const char *s) {
    size_t count = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if ((c & 0xC0) != 0x80) {
            count++;
        }
        s++;
    }
    return count;
}

// ひらがなをヘボン式ローマ字に変換（最長一致 + 促音対応）
void kana_to_roma(const char *kana_input, char *roma_output, size_t output_size) {
    roma_output[0] = '\0';
    const char *p = kana_input;

    while (*p && strlen(roma_output) < output_size - 10) {
        // 促音（小さい「っ」）の処理：次の音の頭子音を重ねる
        if (strncmp(p, "っ", 3) == 0) {
            // 次のエントリを最長一致で先読み
            const kana_roma_t *best = NULL;
            size_t best_len = 0;
            for (const kana_roma_t *entry = kana_roma_table; entry->kana != NULL; entry++) {
                size_t kana_len = strlen(entry->kana);
                if (kana_len > best_len && strncmp(p + 3, entry->kana, kana_len) == 0) {
                    best = entry;
                    best_len = kana_len;
                }
            }
            if (best && best->roma && best->roma[0] != '\0') {
                char c = best->roma[0];
                // 母音頭の場合は重ねられないので「xtu」を出力
                if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
                    strcat(roma_output, "xtu");
                } else {
                    // 子音頭の場合は子音を重ねる
                    size_t len = strlen(roma_output);
                    if (len < output_size - 2) {
                        roma_output[len] = c;
                        roma_output[len + 1] = '\0';
                    }
                }
            } else {
                // 次の文字がない場合も「xtu」を出力
                strcat(roma_output, "xtu");
            }
            p += 3; // 「っ」を消費して続行
            continue;
        }

        // 最長一致検索（拗音などを優先）
        const kana_roma_t *best = NULL;
        size_t best_len = 0;
        for (const kana_roma_t *entry = kana_roma_table; entry->kana != NULL; entry++) {
            size_t kana_len = strlen(entry->kana);
            if (kana_len > best_len && strncmp(p, entry->kana, kana_len) == 0) {
                best = entry;
                best_len = kana_len;
            }
        }

        if (best) {
            strcat(roma_output, best->roma);
            p += best_len;
            continue;
        }

        if ((unsigned char)*p < 128) {
            size_t len = strlen(roma_output);
            roma_output[len] = *p;
            roma_output[len + 1] = '\0';
            p++;
        } else {
            p++;
        }
    }
}

// 子音ストローク→ローマ字マッピング
typedef struct {
    const char *stroke;
    const char *roma;
} conso_map_t;

static const conso_map_t conso_table[] = {
    {"", ""}, {"S", "s"}, {"T", "t"}, {"K", "k"}, {"N", "n"},
    {"ST", "r"}, {"SK", "w"}, {"TK", "h"},
    {"SN", "z"}, {"TN", "d"}, {"KN", "g"}, {"TKN", "b"},
    {"STK", "p"}, {"STN", "l"}, {"SKN", "m"}, {"STKN", "f"}
};

// 母音ストローク→ローマ字マッピング
typedef struct {
    const char *stroke;
    const char *roma;
    uint8_t index;
} vowel_map_t;

static const vowel_map_t vowel_table[] = {
    {"A", "a", 0}, {"I", "i", 1}, {"U", "u", 2}, {"IA", "e", 3}, {"AU", "o", 4},
    {"YA", "ya", 5}, {"YU", "yu", 6}, {"YAU", "yo", 7}
};

// 行段→ひらがなマッピング
static const char *kana_table[][8] = {
    {"あ", "い", "う", "え", "お", "や", "ゆ", "よ"},     // ""
    {"か", "き", "く", "け", "こ", "きゃ", "きゅ", "きょ"}, // k
    {"さ", "し", "す", "せ", "そ", "しゃ", "しゅ", "しょ"}, // s
    {"た", "ち", "つ", "て", "と", "ちゃ", "ちゅ", "ちょ"}, // t
    {"な", "に", "ぬ", "ね", "の", "にゃ", "にゅ", "にょ"}, // n
    {"は", "ひ", "ふ", "へ", "ほ", "ひゃ", "ひゅ", "ひょ"}, // h
    {"ま", "み", "む", "め", "も", "みゃ", "みゅ", "みょ"}, // m
    {"ら", "り", "る", "れ", "ろ", "りゃ", "りゅ", "りょ"}, // r
    {"わ", "うぃ", "う", "うぇ", "うぉ", "うぁ", "う", "を"}, // w
    {"が", "ぎ", "ぐ", "げ", "ご", "ぎゃ", "ぎゅ", "ぎょ"}, // g
    {"ざ", "じ", "ず", "ぜ", "ぞ", "じゃ", "じゅ", "じょ"}, // z
    {"だ", "ぢ", "づ", "で", "ど", "ぢゃ", "ぢゅ", "ぢょ"}, // d
    {"ば", "び", "ぶ", "べ", "ぼ", "びゃ", "びゅ", "びょ"}, // b
    {"ぱ", "ぴ", "ぷ", "ぺ", "ぽ", "ぴゃ", "ぴゅ", "ぴょ"}, // p
    {"ふぁ", "ふぃ", "ふ", "ふぇ", "ふぉ", "ふゃ", "ふゅ", "ふょ"}, // f
    {"ぁ", "ぃ", "ぅ", "ぇ", "ぉ", "ゃ", "ゅ", "ょ"}      // l
};

static const char *roma_order[] = {"", "k", "s", "t", "n", "h", "m", "r", "w", "g", "z", "d", "b", "p", "f", "l"};

// 二重母音マッピング
typedef struct {
    const char *stroke;
    const char *first_vowel;
    const char *suffix;
} diphthong_map_t;

static const diphthong_map_t diphthong_table[] = {
    {"Y", "a", "い"},
    {"YI", "yo", "う"},
    {"YIA", "e", "い"},
    {"YIU", "yu", "う"},
    {"YIAU", "u", "う"},
    {"IU", "u", "い"},
    {"IAU", "o", "う"}
};

// マイナー二重母音マッピング（親指を使う外来音）
typedef struct {
    const char *stroke;  // 母音+助詞ストローク
    const char *first_vowel;
    const char *suffix;
} minor_diphthong_map_t;

static const minor_diphthong_map_t minor_diphthong_table[] = {
    {"IAUtk", "a", "u"},   // ~あう
    {"YItk", "i", "i"},    // ~いい
    {"YIUtk", "o", "i"},   // ~おい
    {"YIAtk", "a", "e"},   // ~あえ
    {"YIAUtk", "o", "o"},  // ~おお
};

// 英語音マッピング（親指を使う外来音）
typedef struct {
    const char *stroke;  // 母音+助詞ストローク
    const char *first_vowel;
    const char *suffix;
} english_diphthong_map_t;

static const english_diphthong_map_t english_diphthong_table[] = {

// |V/C | t | k |  nt   | nk | ntk|
// |----|---|---|-------|----|----|
// |IAU |~as|~al|~ation |~ind|~arn|
// |YI  |~is|~il|~ition |~ing|~een|
// |YIU |~us|~ul|~usion |~and|~oom|
// |YIA |~es|~el|~ention|~end|~ain|
// |YIAU|~os|~ol|~otion |~ong|~orn|

    // t
    {"IAUt", "a", "す"},   // ~as
    {"YIt", "i", "す"},    // ~is
    {"YIUt", "u", "す"},   // ~us
    {"YIAt", "e", "す"},   // ~es
    {"YIAUt", "o", "す"},  // ~os

    // k
    {"IAUk", "a", "る"},   // ~al
    {"YIk", "i", "る"},    // ~il
    {"YIUk", "u", "る"},   // ~ul
    {"YIAk", "e", "る"},   // ~el
    {"YIAUk", "o", "る"},  // ~ol

    // nt
    {"IAUnt", "e", "ーしょん"},  // ~ation
    {"YInt", "i", "しょん"},     // ~ition
    {"YIUnt", "u", "しょん"},    // ~usion
    {"YIAnt", "e", "んしょん"},  // ~ention
    {"YIAUnt", "o", "ーしょん"}, // ~otion

    // nk
    {"IAUnk", "a", "いんど"},  // ~ind
    {"YInk", "i", "んぐ"},   // ~ing
    {"YIUnk", "a", "んど"},  // ~and
    {"YIAnk", "e", "んど"},  // ~end
    {"YIAUnk", "o", "んぐ"}, // ~ong

    // ntk
    {"IAUntk", "a", "ーん"},  // ~arn
    {"YIntk", "i", "ーん"},   // ~een
    {"YIUntk", "u", "ーん"},  // ~oom
    {"YIAntk", "e", "ーん"},  // ~ain
    {"YIAUntk", "o", "ーん"}, // ~orn
};

// 例外的なかなのマッピング
typedef struct {
    const char *stroke;  // 子音+母音ストローク
    const char *kana;
} exception_kana_t;

static const exception_kana_t exception_kana_table[] = {

//  |U  |YA |YI |YU  |YIU|YAU|YIAU|IAU|IU  |Y   |YIA
// -|---|---|---|----|---|---|----|---|----|----|----
// F|vu |va |vi |fyu |ve |vo |jei |je |vyu |----|----
// W|xwa|wha|wyi|ui  |wye|wo |chei|che|yui |----|----
// D|---|thi|twu|dhu |dwu|dhi|ye  |---|thu |----|----
// X|---|sta|sti|sthi|ste|sto|shei|she|kusu|stai|stei

    // F
    {"STKNU", "ゔ"},
    {"STKNYA", "ゔぁ"},
    {"STKNYI", "ゔぃ"},
    {"STKNYU", "ふゅ"},
    {"STKNYIU", "ゔぇ"},
    {"STKNYAU", "ゔぉ"},
    {"STKNYIAU", "じぇい"},
    {"STKNIAU", "じぇ"},
    {"STKNIU", "ゔゅ"},

    // W
    {"SKU", "ゎ"},
    {"SKYA", "うぁ"},
    {"SKYI", "ゐ"},
    {"SKYU", "いう"},
    {"SKYIU", "ゑ"},
    {"SKYAU", "を"},
    {"SKYIAU", "ちぇい"},
    {"SKIAU", "ちぇ"},
    {"SKIU", "ゆい"},

    // D
    {"TNYA", "てぃ"},
    {"TNYI", "とぅ"},
    {"TNYU", "でゅ"},
    {"TNYIU", "どぅ"},
    {"TNYAU", "でぃ"},
    {"TNYIAU", "いぇ"},
    {"TNIU", "てゅ"},

    // X
    {"STNYA", "すた"},
    {"STNYI", "すち"},
    {"STNYU", "すてぃ"},
    {"STNYIU", "すて"},
    {"STNYAU", "すと"},
    {"STNYIAU", "しぇい"},
    {"STNIAU", "しぇ"},
    {"STNIU", "くす"},
    {"STNY", "すたい"},
    {"STNYIA", "すてい"},
    {NULL, NULL}
};

// 追加音リスト
static const char *second_sound_list[] = {"", "ん", "つ", "く", "っ", "ち", "き", "ー"};
static const char *particle_key_list[] = {"", "n", "t", "k", "tk", "nt", "nk", "ntk"};

// 左側助詞
static const char *l_particle[] = {"", "、", "に", "の", "で", "と", "を", "へ"};
// 右側助詞
static const char *r_particle[] = {"", "、", "は", "が", "も", "は、", "が、", "や"};

static void replace_he_with_ka(char *text) {
    char *p = text;
    while ((p = strstr(p, "へ")) != NULL) {
        memcpy(p, "か", strlen("か"));
        p += strlen("か");
    }
}

static void replace_nana_with_nanoga(char *text, size_t text_size) {
    char *p = text;
    const size_t from_len = strlen("なな");
    const size_t to_len = strlen("なのが");
    while ((p = strstr(p, "なな")) != NULL) {
        size_t tail_len = strlen(p + from_len);
        size_t used_len = strlen(text);
        if (used_len + (to_len - from_len) >= text_size) {
            break;
        }
        memmove(p + to_len, p + from_len, tail_len + 1);
        memcpy(p, "なのが", to_len);
        p += to_len;
    }
}

// メジロIDをパース（左側・右側に分離）
static void parse_mejiro_id(const char *id, char *left, char *right) {
    const char *hyphen = strchr(id, '-');
    if (hyphen == NULL) {
        strcpy(left, id);
        right[0] = '\0';
    } else {
        size_t left_len = hyphen - id;
        strncpy(left, id, left_len);
        left[left_len] = '\0';
        strcpy(right, hyphen + 1);
    }
}

// 子音ストロークからローマ字を取得
static const char *get_conso_roma(const char *stroke) {
    for (uint8_t i = 0; i < sizeof(conso_table)/sizeof(conso_table[0]); i++) {
        if (strcmp(stroke, conso_table[i].stroke) == 0) {
            return conso_table[i].roma;
        }
    }
    return NULL;
}

// 母音ストロークから母音インデックスを取得
static int get_vowel_index(const char *stroke) {
    // 二重母音チェック
    for (uint8_t i = 0; i < sizeof(diphthong_table)/sizeof(diphthong_table[0]); i++) {
        if (strcmp(stroke, diphthong_table[i].stroke) == 0) {
            // first_vowelから基本母音インデックスを取得
            for (uint8_t j = 0; j < sizeof(vowel_table)/sizeof(vowel_table[0]); j++) {
                if (strcmp(vowel_table[j].roma, diphthong_table[i].first_vowel) == 0) {
                    return vowel_table[j].index;
                }
            }
        }
    }

    // 基本母音チェック
    for (uint8_t i = 0; i < sizeof(vowel_table)/sizeof(vowel_table[0]); i++) {
        if (strcmp(stroke, vowel_table[i].stroke) == 0) {
            return vowel_table[i].index;
        }
    }
    return -1;
}

// ローマ字からkana_tableのインデックスを取得
static int get_roma_index(const char *roma) {
    for (uint8_t i = 0; i < sizeof(roma_order)/sizeof(roma_order[0]); i++) {
        if (strcmp(roma, roma_order[i]) == 0) {
            return i;
        }
    }
    return -1;
}

// 二重母音のsuffixを取得
static const char *get_diphthong_suffix(const char *stroke) {
    for (uint8_t i = 0; i < sizeof(diphthong_table)/sizeof(diphthong_table[0]); i++) {
        if (strcmp(stroke, diphthong_table[i].stroke) == 0) {
            return diphthong_table[i].suffix;
        }
    }
    return "";
}

// 英語音をチェック（母音+助詞ストローク）
static bool check_english_diphthong(const char *vowel_particle, const char **first_vowel, const char **suffix) {
    for (uint8_t i = 0; i < sizeof(english_diphthong_table)/sizeof(english_diphthong_table[0]); i++) {
        if (strcmp(vowel_particle, english_diphthong_table[i].stroke) == 0) {
            *first_vowel = english_diphthong_table[i].first_vowel;
            *suffix = english_diphthong_table[i].suffix;
            return true;
        }
    }
    return false;
}

// マイナー二重母音をチェック（母音+助詞ストローク）
static bool check_minor_diphthong(const char *vowel_particle, const char **first_vowel, const char **suffix) {
    for (uint8_t i = 0; i < sizeof(minor_diphthong_table)/sizeof(minor_diphthong_table[0]); i++) {
        if (strcmp(vowel_particle, minor_diphthong_table[i].stroke) == 0) {
            *first_vowel = minor_diphthong_table[i].first_vowel;
            *suffix = minor_diphthong_table[i].suffix;
            return true;
        }
    }
    return false;
}

// 例外的なかなをチェック（子音+母音ストローク）
static const char *check_exception_kana(const char *conso_vowel) {
    for (uint8_t i = 0; exception_kana_table[i].stroke != NULL; i++) {
        if (strcmp(conso_vowel, exception_kana_table[i].stroke) == 0) {
            return exception_kana_table[i].kana;
        }
    }
    return NULL;
}

// 追加音を取得
static const char *get_second_sound(const char *particle) {
    for (uint8_t i = 0; i < sizeof(particle_key_list)/sizeof(particle_key_list[0]); i++) {
        if (strcmp(particle, particle_key_list[i]) == 0) {
            return second_sound_list[i];
        }
    }
    return "";
}

static bool is_extra_sound_only_stroke(const char *conso, const char *vowel) {
    return (strlen(conso) == 0 && strlen(vowel) == 0) ||
           (strcmp(conso, "STN") == 0 && strlen(vowel) == 0);
}


// 子音・母音・助詞から、かな文字列を生成する共通関数
// include_extra_sound: 追加音を含めるかどうか（左+助詞パターンではfalse）
static void convert_to_kana(const char *conso, const char *vowel, const char *particle_str,
                           bool include_extra_sound, char *out) {
    out[0] = '\0';  // 初期化

    // STN+母音なしは「何も出力しない」特例（追加音のみ必要なケース）
    if (is_extra_sound_only_stroke(conso, vowel) && strcmp(conso, "STN") == 0) {
        if (include_extra_sound) {
            const char *extra = get_second_sound(particle_str);
            strcpy(out, extra);
        } else {
            out[0] = '\0';
        }
        return;
    }

    // 例外かなを先に確認（特定の助詞では最優先）
    char conso_vowel[32];
    strcpy(conso_vowel, conso);
    strcat(conso_vowel, vowel);
    const char *exception_kana = check_exception_kana(conso_vowel);
    bool prefer_exception = (strcmp(particle_str, "n") == 0 ||
                             strcmp(particle_str, "tk") == 0 ||
                             strcmp(particle_str, "ntk") == 0);
    if (prefer_exception && exception_kana != NULL) {
        strcpy(out, exception_kana);

        // 入力ストロークで判定（「っ」持ち越し処理に対応）
        if (strcmp(particle_str, "tk") == 0) {
            if (strcmp(conso_vowel, "SKIAU") == 0) {
                strcpy(out, "ちぇ");
            } else if (strcmp(conso_vowel, "STKNIAU") == 0) {
                strcpy(out, "じぇ");
            } else if (strcmp(conso_vowel, "STNIAU") == 0) {
                strcpy(out, "しぇ");
            } else if (strcmp(conso_vowel, "TNYIAU") == 0) {
                strcpy(out, "いぇ");
            }
        }

        if (include_extra_sound) {
            const char *extra = get_second_sound(particle_str);
            strcat(out, extra);
        }
        return;
    }

    // 英語音をチェック（母音+助詞）- 優先度高
    char vowel_particle[32];
    strcpy(vowel_particle, vowel);
    strcat(vowel_particle, particle_str);
    const char *first_vowel = NULL;
    const char *suffix = NULL;
    bool is_english = check_english_diphthong(vowel_particle, &first_vowel, &suffix);

    if (is_english) {
        // 英語音の場合、first_vowelからインデックスを取得
        const char *c_roma = get_conso_roma(conso);
        if (c_roma == NULL) c_roma = "";

        int v_idx = -1;
        for (uint8_t j = 0; j < sizeof(vowel_table)/sizeof(vowel_table[0]); j++) {
            if (strcmp(vowel_table[j].roma, first_vowel) == 0) {
                v_idx = vowel_table[j].index;
                break;
            }
        }
        if (v_idx < 0) v_idx = 0;

        int c_idx = get_roma_index(c_roma);
        if (c_idx < 0) c_idx = 0;

        const char *base_kana = kana_table[c_idx][v_idx];

        // 英語モードの場合、ち→てぃ、ぢ→でぃ、づ→どぅに置き換え
        if (strcmp(base_kana, "ち") == 0) {
            base_kana = "てぃ";
        } else if (strcmp(base_kana, "ぢ") == 0) {
            base_kana = "でぃ";
        } else if (strcmp(base_kana, "づ") == 0) {
            base_kana = "どぅ";
        } else if (strcmp(base_kana, "ぁ") == 0) {
            base_kana = "すた";
        } else if (strcmp(base_kana, "ぃ") == 0) {
            base_kana = "すち";
        } else if (strcmp(base_kana, "ぅ") == 0) {
            base_kana = "すてぃ";
        } else if (strcmp(base_kana, "ぇ") == 0) {
            base_kana = "すて";
        } else if (strcmp(base_kana, "ぉ") == 0) {
            base_kana = "すと";
        }

        strcpy(out, base_kana);
        strcat(out, suffix);

        // 連結後の組み合わせ置き換え
        if (strcmp(out, "るしょん") == 0) {
            strcpy(out, "りゅーしょん");
        } else if (strcmp(out, "ふしょん") == 0) {
            strcpy(out, "ふゅーじょん");
        }
        // 英語音は追加音なし
        return;
    }

    // マイナー二重母音をチェック（母音+助詞）
    bool is_minor = check_minor_diphthong(vowel_particle, &first_vowel, &suffix);

    if (is_minor) {
        const char *c_roma = get_conso_roma(conso);
        if (c_roma == NULL) c_roma = "";

        int v_idx = -1;
        for (uint8_t j = 0; j < sizeof(vowel_table)/sizeof(vowel_table[0]); j++) {
            if (strcmp(vowel_table[j].roma, first_vowel) == 0) {
                v_idx = vowel_table[j].index;
                break;
            }
        }
        if (v_idx < 0) v_idx = 0;

        int c_idx = get_roma_index(c_roma);
        if (c_idx < 0) c_idx = 0;

        const char *base_kana = kana_table[c_idx][v_idx];

        strcpy(out, base_kana);
        strcat(out, suffix);
        // 追加音なし
        return;
    }

    // 例外的なかなのマッピングをチェック
    if (exception_kana != NULL) {
        // 例外かなを使用
        strcpy(out, exception_kana);

        // 入力ストロークで判定（「っ」持ち越し処理に対応）
        if (strcmp(particle_str, "tk") == 0) {
            if (strcmp(conso_vowel, "SKIAU") == 0) {
                strcpy(out, "ちぇ");
            } else if (strcmp(conso_vowel, "STKNIAU") == 0) {
                strcpy(out, "じぇ");
            } else if (strcmp(conso_vowel, "STNIAU") == 0) {
                strcpy(out, "しぇ");
            } else if (strcmp(conso_vowel, "TNYIAU") == 0) {
                strcpy(out, "いぇ");
            }
        }

        if (include_extra_sound) {
            const char *extra = get_second_sound(particle_str);
            strcat(out, extra);
        }
    } else {
        // 通常の処理（基本母音または二重母音）
        const char *c_roma = get_conso_roma(conso);
        if (c_roma == NULL) c_roma = "";

        int v_idx = get_vowel_index(vowel);
        if (v_idx < 0) v_idx = 0;

        const char *suffix = get_diphthong_suffix(vowel);
        const char *extra_sound = "";
        if (include_extra_sound) {
            extra_sound = get_second_sound(particle_str);
        }

        int c_idx = get_roma_index(c_roma);
        if (c_idx < 0) c_idx = 0;

        const char *base_kana = kana_table[c_idx][v_idx];

        strcpy(out, base_kana);
        strcat(out, suffix);
        strcat(out, extra_sound);
    }
}

// 子音・母音・助詞から、音節全体を生成する関数（英語音・マイナー音対応）
// 「です」処理など、助詞込みの完全な音節が必要な場合に使用
static void convert_to_syllable(const char *conso, const char *vowel, const char *particle_str, char *out) {
    out[0] = '\0';

    // particle-only / STN+母音なしは追加音のみ（Plover版と同等）
    if (is_extra_sound_only_stroke(conso, vowel)) {
        const char *extra = get_second_sound(particle_str);
        strcpy(out, extra);
        return;
    }

    // 例外かなを先に確認（特定の助詞では最優先）
    char conso_vowel[32];
    strcpy(conso_vowel, conso);
    strcat(conso_vowel, vowel);
    const char *exception_kana = check_exception_kana(conso_vowel);
    bool prefer_exception = (strcmp(particle_str, "n") == 0 ||
                             strcmp(particle_str, "tk") == 0 ||
                             strcmp(particle_str, "ntk") == 0);

    // 英語音をチェック（母音+助詞）- 最優先
    char vowel_particle[32];
    strcpy(vowel_particle, vowel);
    strcat(vowel_particle, particle_str);
    const char *first_vowel = NULL;
    const char *suffix = NULL;
    bool is_english = check_english_diphthong(vowel_particle, &first_vowel, &suffix);

    if (is_english) {
        // 英語音の場合
        const char *c_roma = get_conso_roma(conso);
        if (c_roma == NULL) c_roma = "";

        int v_idx = -1;
        for (uint8_t j = 0; j < sizeof(vowel_table)/sizeof(vowel_table[0]); j++) {
            if (strcmp(vowel_table[j].roma, first_vowel) == 0) {
                v_idx = vowel_table[j].index;
                break;
            }
        }
        if (v_idx < 0) v_idx = 0;

        int c_idx = get_roma_index(c_roma);
        if (c_idx < 0) c_idx = 0;

        const char *base_kana = kana_table[c_idx][v_idx];

        // 英語モードの置き換え
        if (strcmp(base_kana, "ち") == 0) {
            base_kana = "てぃ";
        } else if (strcmp(base_kana, "ぢ") == 0) {
            base_kana = "でぃ";
        } else if (strcmp(base_kana, "づ") == 0) {
            base_kana = "どぅ";
        } else if (strcmp(base_kana, "ぁ") == 0) {
            base_kana = "すた";
        } else if (strcmp(base_kana, "ぃ") == 0) {
            base_kana = "すち";
        } else if (strcmp(base_kana, "ぅ") == 0) {
            base_kana = "すてぃ";
        } else if (strcmp(base_kana, "ぇ") == 0) {
            base_kana = "すて";
        } else if (strcmp(base_kana, "ぉ") == 0) {
            base_kana = "すと";
        }

        strcpy(out, base_kana);
        strcat(out, suffix);

        // 連結後の組み合わせ置き換え
        if (strcmp(out, "るしょん") == 0) {
            strcpy(out, "りゅーしょん");
        } else if (strcmp(out, "ふしょん") == 0) {
            strcpy(out, "ふゅーじょん");
        }
        return;
    }

    // マイナー二重母音をチェック（母音+助詞）
    bool is_minor = check_minor_diphthong(vowel_particle, &first_vowel, &suffix);

    if (is_minor) {
        const char *c_roma = get_conso_roma(conso);
        if (c_roma == NULL) c_roma = "";

        int v_idx = -1;
        for (uint8_t j = 0; j < sizeof(vowel_table)/sizeof(vowel_table[0]); j++) {
            if (strcmp(vowel_table[j].roma, first_vowel) == 0) {
                v_idx = vowel_table[j].index;
                break;
            }
        }
        if (v_idx < 0) v_idx = 0;

        int c_idx = get_roma_index(c_roma);
        if (c_idx < 0) c_idx = 0;

        const char *base_kana = kana_table[c_idx][v_idx];

        strcpy(out, base_kana);
        strcat(out, suffix);
        return;
    }

    // 例外的なかなのマッピング（英語音・マイナー音でない場合）
    if (prefer_exception && exception_kana != NULL) {
        strcpy(out, exception_kana);

        // 入力ストロークで判定
        if (strcmp(particle_str, "tk") == 0) {
            if (strcmp(conso_vowel, "SKIAU") == 0) {
                strcpy(out, "ちぇ");
            } else if (strcmp(conso_vowel, "STKNIAU") == 0) {
                strcpy(out, "じぇ");
            } else if (strcmp(conso_vowel, "STNIAU") == 0) {
                strcpy(out, "しぇ");
            } else if (strcmp(conso_vowel, "TNYIAU") == 0) {
                strcpy(out, "いぇ");
            }
        }

        const char *extra = get_second_sound(particle_str);
        strcat(out, extra);
        return;
    }

    // 通常の処理（convert_to_kanaと同じ）に追加音を加える
    convert_to_kana(conso, vowel, particle_str, true, out);
}

// 助詞変換
static void transform_joshi(const char *left_stroke, const char *right_stroke, char *out) {
    // right_strokeの先頭側nを取り除いた助詞キー（Python版 split("n", 1)[-1] と同等）
    char right_tk[16] = {0};
    const char *n_pos = strchr(right_stroke, 'n');
    if (n_pos != NULL) {
        strncpy(right_tk, n_pos + 1, sizeof(right_tk) - 1);
    } else {
        strncpy(right_tk, right_stroke, sizeof(right_tk) - 1);
    }
    bool has_comma = (strstr(right_stroke, "n") != NULL);

    // インデックス取得
    int l_idx = -1, r_idx = -1, r_raw_idx = -1;
    for (uint8_t i = 0; i < sizeof(particle_key_list)/sizeof(particle_key_list[0]); i++) {
        if (strcmp(left_stroke, particle_key_list[i]) == 0) l_idx = i;
        if (strcmp(right_tk, particle_key_list[i]) == 0) r_idx = i;
        if (strcmp(right_stroke, particle_key_list[i]) == 0) r_raw_idx = i;
    }

    if (l_idx < 0 || r_idx < 0 || r_raw_idx < 0) {
        out[0] = '\0';
        return;
    }

    const char *left_joshi = l_particle[l_idx];
    const char *right_joshi = r_particle[r_idx];
    const char *right_raw_joshi = r_particle[r_raw_idx];

    // 助詞組み立て
    if ((strcmp(left_stroke, "n") == 0 || strcmp(left_stroke, "") == 0) &&
        strcmp(right_stroke, "ntk") == 0) {
        strcpy(out, right_raw_joshi);
        if (strcmp(left_stroke, "n") == 0) {
            strcat(out, "、");
        }
    } else if (strcmp(left_stroke, "n") == 0 && right_stroke[0] != '\0') {
        strcpy(out, right_joshi);
        strcat(out, "、");
    } else if (left_stroke[0] != '\0' &&
               (strcmp(right_stroke, "k") == 0 || strcmp(right_stroke, "nk") == 0)) {
        if (strcmp(left_stroke, "nt") == 0 || strcmp(left_stroke, "ntk") == 0) {
            strcpy(out, left_joshi);
            strcat(out, "の");
        } else {
            strcpy(out, "の");
            strcat(out, left_joshi);
        }
        if (strcmp(out, "のの") == 0) {
            if (strcmp(prev_joshi, "な") == 0) {
                strcpy(out, "のが");
            } else {
                strcpy(out, "な");
            }
        }
        if (has_comma) strcat(out, "、");
    } else {
        strcpy(out, left_joshi);
        strcat(out, right_joshi);
        if (has_comma) strcat(out, "、");
    }

    strncpy(prev_joshi, out, sizeof(prev_joshi) - 1);
    prev_joshi[sizeof(prev_joshi) - 1] = '\0';
}

mejiro_result_t mejiro_transform(const char *mejiro_id) {
    mejiro_result_t result = {{0}, 0, false};

    char left[32] = {0};
    char right[32] = {0};
    parse_mejiro_id(mejiro_id, left, right);

    // 削除操作（-U、-AU）の場合は持ち越しの「っ」をクリア
    if (strcmp(mejiro_id, "-U") == 0 || strcmp(mejiro_id, "-AU") == 0) {
        pending_tsu = false;
    }

    // 全押し（STKNYIAUntk#-STKNYIAUntk*）は「何も出力しない」ことで誤入力をキャンセル
    if (strcmp(left, "STKNYIAUntk#") == 0 && strcmp(right, "STKNYIAUntk*") == 0) {
        return result;  // output is empty, success remains false
    }

    // 左側のパース: 子音(STKN) + 母音(YIAU) + 助詞(ntk)
    char l_conso[16] = {0};
    char l_vowel[16] = {0};
    char l_particle_str[16] = {0};

    const char *p = left;
    while (*p && (*p == 'S' || *p == 'T' || *p == 'K' || *p == 'N')) {
        size_t len = strlen(l_conso);
        l_conso[len] = *p;
        l_conso[len + 1] = '\0';
        p++;
    }
    while (*p && (*p == 'Y' || *p == 'I' || *p == 'A' || *p == 'U')) {
        size_t len = strlen(l_vowel);
        l_vowel[len] = *p;
        l_vowel[len + 1] = '\0';
        p++;
    }
    while (*p && (*p == 'n' || *p == 't' || *p == 'k' || *p == '#')) {
        size_t len = strlen(l_particle_str);
        l_particle_str[len] = *p;
        l_particle_str[len + 1] = '\0';
        p++;
    }

    // 右側のパース: 子音(STKN) + 母音(YIAU) + 助詞(ntk) + 末尾のアスタリスク(*)
    char r_conso[16] = {0};
    char r_vowel[16] = {0};
    char r_particle_str[16] = {0};
    bool has_asterisk = false;

    p = right;
    while (*p && (*p == 'S' || *p == 'T' || *p == 'K' || *p == 'N')) {
        size_t len = strlen(r_conso);
        r_conso[len] = *p;
        r_conso[len + 1] = '\0';
        p++;
    }
    while (*p && (*p == 'Y' || *p == 'I' || *p == 'A' || *p == 'U')) {
        size_t len = strlen(r_vowel);
        r_vowel[len] = *p;
        r_vowel[len + 1] = '\0';
        p++;
    }
    while (*p && (*p == 'n' || *p == 't' || *p == 'k')) {
        size_t len = strlen(r_particle_str);
        r_particle_str[len] = *p;
        r_particle_str[len + 1] = '\0';
        p++;
    }
    if (*p == '*') {
        has_asterisk = true;
        p++;
    }

    // 完全なストロークを構築（アスタリスクを除く）
    char full_stroke[128];
    snprintf(full_stroke, sizeof(full_stroke), "%s%s%s-%s%s%s",
             l_conso, l_vowel, l_particle_str,
             r_conso, r_vowel, r_particle_str);

    // 一般略語用のストローク（助詞なし）
    char abbr_stroke[128];
    snprintf(abbr_stroke, sizeof(abbr_stroke), "%s%s-%s%s",
             l_conso, l_vowel, r_conso, r_vowel);

    // ユーザー略語チェック（最優先）
    // 先に入力そのもの（# を含む可能性あり）で照合し、未一致時のみ分解後ストロークで照合する。
    char raw_stroke[128] = {0};
    strncpy(raw_stroke, mejiro_id, sizeof(raw_stroke) - 1);
    char *asterisk_pos = strchr(raw_stroke, '*');
    if (asterisk_pos != NULL) {
        *asterisk_pos = '\0';
    }
    bool raw_has_asterisk = strchr(mejiro_id, '*') != NULL;

    abbreviation_result_t user_abbr = mejiro_user_abbreviation(raw_stroke, raw_has_asterisk);
    if (!user_abbr.success) {
        user_abbr = mejiro_user_abbreviation(full_stroke, has_asterisk);
    }
    if (user_abbr.success) {
        result.kana_length = utf8_char_count(user_abbr.output);
        kana_to_roma(user_abbr.output, result.output, sizeof(result.output));
        result.success = true;
        return result;
    }

    // 一般略語チェック
    abbreviation_result_t abstract_abbr = mejiro_abstract_abbreviation(abbr_stroke, has_asterisk);
    if (abstract_abbr.success) {
        // 一般略語の出力に助詞を追加
        char kana_output[256] = {0};
        strcpy(kana_output, abstract_abbr.output);
        result.kana_length = utf8_char_count(kana_output);

        // 助詞がある場合は追加
        if (strlen(l_particle_str) > 0 || strlen(r_particle_str) > 0) {
            // 助詞パターンを構築
            char particle_pattern[32];
            snprintf(particle_pattern, sizeof(particle_pattern), "%s-%s", l_particle_str, r_particle_str);

            // 特定の助詞パターンに対して語尾に変換
            if (strcmp(particle_pattern, "n-") == 0) {
                strcat(kana_output, "である");
            } else if (strcmp(particle_pattern, "-n") == 0) {
                strcat(kana_output, "だ");
            } else if (strcmp(particle_pattern, "n-n") == 0) {
                strcat(kana_output, "だった");
            } else if (strcmp(particle_pattern, "-nt") == 0) {
                strcat(kana_output, "。");
            } else if (strcmp(particle_pattern, "-nk") == 0) {
                strcat(kana_output, "、");
            } else if (strcmp(particle_pattern, "n-nt") == 0) {
                strcat(kana_output, "?");
            } else if (strcmp(particle_pattern, "n-nk") == 0) {
                strcat(kana_output, "!");
            } else {
                // その他の助詞は通常通り処理
                char joshi_output[128] = {0};
                transform_joshi(l_particle_str, r_particle_str, joshi_output);
                replace_he_with_ka(joshi_output);
                strcat(kana_output, joshi_output);
            }
            result.kana_length = utf8_char_count(kana_output);
        }

        kana_to_roma(kana_output, result.output, sizeof(result.output));
        result.success = true;
        return result;
    }

    // 動詞略語チェック
    char left_kana_temp[64] = {0};
    char right_kana_temp[64] = {0};
    char left_syllable_temp[64] = {0};
    char right_syllable_temp[64] = {0};

    // 左側の仮名を生成（動詞語幹用）
    if (strlen(l_conso) > 0 || strlen(l_vowel) > 0) {
        convert_to_kana(l_conso, l_vowel, "", false, left_kana_temp);
    }
    // 音節は常に生成（particle-onlyを含む）
    convert_to_syllable(l_conso, l_vowel, l_particle_str, left_syllable_temp);
    // 右側の仮名を生成（動詞語幹用）
    if (strlen(r_conso) > 0 || strlen(r_vowel) > 0) {
        convert_to_kana(r_conso, r_vowel, "", false, right_kana_temp);
    }
    convert_to_syllable(r_conso, r_vowel, r_particle_str, right_syllable_temp);

    verb_result_t verb_result = mejiro_verb_conjugate(
        l_conso, l_vowel, l_particle_str,
        r_conso, r_vowel, r_particle_str,
        left_kana_temp, right_kana_temp,
        left_syllable_temp, right_syllable_temp,
        has_asterisk
    );

    if (verb_result.success) {
        // 動詞活用結果をローマ字に変換して返す
        char kana_output[128];
        strcpy(kana_output, verb_result.output);
        result.kana_length = utf8_char_count(kana_output);
        kana_to_roma(kana_output, result.output, sizeof(result.output));
        result.success = true;
        return result;
    }

    // 助詞のみの場合は助詞変換
    bool is_particle_only = (strlen(l_conso) == 0 && strlen(l_vowel) == 0 &&
                             strlen(r_conso) == 0 && strlen(r_vowel) == 0 &&
                             (strlen(l_particle_str) > 0 || strlen(r_particle_str) > 0));

    // 左+助詞かどうかを先に判定（左側の追加音を除外するため）
    // ただし、ntk-nの場合は除外（これは左側の追加音「ーん」として処理すべき）
    bool is_left_plus_particle = ((strlen(l_conso) > 0 || strlen(l_vowel) > 0) &&
                                  strlen(r_conso) == 0 && strlen(r_vowel) == 0 &&
                                  strlen(r_particle_str) > 0 &&
                                  !(strcmp(l_particle_str, "ntk") == 0 && strcmp(r_particle_str, "n") == 0));

    // ntk-nの特殊ケース: 左の追加音と右の助詞として処理
    bool is_ntk_n = (strlen(l_conso) > 0 || strlen(l_vowel) > 0) &&
                   strlen(r_conso) == 0 && strlen(r_vowel) == 0 &&
                   strcmp(l_particle_str, "ntk") == 0 && strcmp(r_particle_str, "n") == 0;

    // 母音の補完処理
    // 左側の母音が空で、左側に子音がある場合、前回の母音を使用
    if (strlen(l_vowel) == 0 && strlen(l_conso) > 0 && strcmp(l_conso, "STN") != 0) {
        strcpy(l_vowel, last_vowel_stroke);
    }
    // 右側の母音が空で、右側に子音がある場合、左の母音を使用（左も空なら前回の母音）
    if (strlen(r_vowel) == 0 && strlen(r_conso) > 0 && strcmp(r_conso, "STN") != 0) {
        if (strlen(l_vowel) > 0) {
            strcpy(r_vowel, l_vowel);
        } else {
            strcpy(r_vowel, last_vowel_stroke);
        }
    }

    // 使用した母音を保存（右が優先、なければ左、どちらもなければ保存しない）
    if (strlen(r_vowel) > 0) {
        strcpy(last_vowel_stroke, r_vowel);
    } else if (strlen(l_vowel) > 0) {
        strcpy(last_vowel_stroke, l_vowel);
    }

    // 「っ」の持ち越し判定（左側と右側を別々に判定）
    // ただし、英語音・マイナー二重母音（母音+助詞）に該当する場合は「っ」を出力しないため、持ち越し対象から除外する
    char left_vowel_particle[32] = {0};
    strcpy(left_vowel_particle, l_vowel);
    strcat(left_vowel_particle, l_particle_str);
    const char *dummy_first_vowel_left = NULL;
    const char *dummy_suffix_left = NULL;
    bool is_english_left = check_english_diphthong(left_vowel_particle, &dummy_first_vowel_left, &dummy_suffix_left);
    bool is_minor_left = check_minor_diphthong(left_vowel_particle, &dummy_first_vowel_left, &dummy_suffix_left);

    char right_vowel_particle[32] = {0};
    strcpy(right_vowel_particle, r_vowel);
    strcat(right_vowel_particle, r_particle_str);
    const char *dummy_first_vowel_right = NULL;
    const char *dummy_suffix_right = NULL;
    bool is_english_right = check_english_diphthong(right_vowel_particle, &dummy_first_vowel_right, &dummy_suffix_right);
    bool is_minor_right = check_minor_diphthong(right_vowel_particle, &dummy_first_vowel_right, &dummy_suffix_right);

    bool has_final_tsu_left = ((strlen(l_conso) > 0 || strlen(l_vowel) > 0) &&
                               strcmp(l_particle_str, "tk") == 0 &&
                               strlen(r_conso) == 0 && strlen(r_vowel) == 0 && strlen(r_particle_str) == 0 &&
                               !is_english_left && !is_minor_left);
    bool has_final_tsu_right = ((strlen(r_conso) > 0 || strlen(r_vowel) > 0) &&
                                strcmp(r_particle_str, "tk") == 0 &&
                                !is_left_plus_particle &&
                                !is_english_right && !is_minor_right);
    bool has_final_tsu = has_final_tsu_left || has_final_tsu_right;

    if (is_particle_only) {
        transform_joshi(l_particle_str, r_particle_str, result.output);
    } else {
        // 前回持ち越しの「っ」を先頭に追加
        if (pending_tsu) {
            strcpy(result.output, "っ");
            pending_tsu = false;
        } else {
            result.output[0] = '\0';
        }

        // 「っ」の持ち越し判定
        // 左側のみでtk助詞があり、右側がない場合（英語音・マイナー二重母音は除外）
        if ((strlen(l_conso) > 0 || strlen(l_vowel) > 0) &&
            strcmp(l_particle_str, "tk") == 0 &&
            strlen(r_conso) == 0 && strlen(r_vowel) == 0 && strlen(r_particle_str) == 0 &&
            !is_english_left && !is_minor_left) {
            has_final_tsu = true;
        }
        // 右側にtk助詞がある場合（左+助詞パターンでない、かつ英語音・マイナー二重母音は除外）
        if ((strlen(r_conso) > 0 || strlen(r_vowel) > 0) &&
            strcmp(r_particle_str, "tk") == 0 &&
            !is_left_plus_particle &&
            !is_english_right && !is_minor_right) {
            has_final_tsu = true;
        }

        // 左側変換
        if (strlen(l_conso) > 0 || strlen(l_vowel) > 0) {
            char left_kana[64] = {0};
            // 左側自体が「っ」で終わる場合のみ追加音を除外
            bool include_extra = !is_left_plus_particle && !has_final_tsu_left;
            convert_to_kana(l_conso, l_vowel, l_particle_str, include_extra, left_kana);
            strcat(result.output, left_kana);
        }

        // 左+助詞: 左側にかながあり、右側には助詞のみがある場合
        // ただし、左の追加音が「ntk」で右の助詞が「n」の場合（追加音が「ー」+「ん」）は例外
        if (is_left_plus_particle) {

            // コマンドテーブルから助詞文字列を検索
            char particle_pattern[32];
            strcpy(particle_pattern, l_particle_str);
            strcat(particle_pattern, "-");
            strcat(particle_pattern, r_particle_str);

            bool found_in_commands = false;
            for (uint8_t i = 0; i < mejiro_command_count; i++) {
                if (strcmp(particle_pattern, mejiro_commands[i].pattern) == 0) {
                    if (mejiro_commands[i].type == CMD_STRING) {
                        strcat(result.output, mejiro_commands[i].action.string);
                        found_in_commands = true;
                        break;
                    }
                }
            }

            // コマンドテーブルになければ、transform_joshiで助詞を生成
            if (!found_in_commands) {
                char joshi_output[64] = {0};
                transform_joshi(l_particle_str, r_particle_str, joshi_output);
                replace_he_with_ka(joshi_output);
                strcat(result.output, joshi_output);
            }
            replace_nana_with_nanoga(result.output, sizeof(result.output));
        }
        // 左のかながなくても、左の追加音キーがあってかつ右のかながある場合
        // 左の追加音を先に出力
        if (strlen(l_conso) == 0 && strlen(l_vowel) == 0 &&
            strlen(l_particle_str) > 0 &&
            (strlen(r_conso) > 0 || strlen(r_vowel) > 0)) {
            const char *left_extra_sound = get_second_sound(l_particle_str);
            strcat(result.output, left_extra_sound);
        }

        // 右側変換
        if (strlen(r_conso) > 0 || strlen(r_vowel) > 0) {
            char right_kana[64] = {0};
            // 右側自体が「っ」で終わる場合のみ追加音を除外
            bool include_extra = !has_final_tsu_right;
            convert_to_kana(r_conso, r_vowel, r_particle_str, include_extra, right_kana);
            strcat(result.output, right_kana);
        }
    }

    // 「っ」の持ち越し設定または単体出力
    if (has_final_tsu) {
        // STNtkのような母音なしで「っ」単体の場合は「っ」を出力
        if (strlen(l_conso) > 0 && strlen(l_vowel) == 0 &&
            strcmp(l_conso, "STN") == 0 && strcmp(l_particle_str, "tk") == 0 &&
            strlen(r_conso) == 0 && strlen(r_vowel) == 0) {
            strcpy(result.output, "っ");
        } else {
            // それ以外は次回に持ち越し（今回の出力から「っ」を除去）
            pending_tsu = true;
            // 出力の末尾の「っ」を削除
            char *tsu_pos = strstr(result.output, "っ");
            if (tsu_pos != NULL && tsu_pos[3] == '\0') {  // 末尾の「っ」のみ
                *tsu_pos = '\0';
            }
        }
    }

    // ntk-nの特殊ケース: 右の助詞「n」を「ん」として追加
    if (is_ntk_n) {
        strcat(result.output, "ん");
    }


    // ひらがな出力をローマ字に変換しつつ、ひらがな文字数を保持
    // 右だけの入力の場合は変換失敗として扱う（ただし右側の助詞単体は除く）
    bool is_right_only = (strlen(l_conso) == 0 && strlen(l_vowel) == 0 &&
                          strlen(l_particle_str) == 0 &&
                          (strlen(r_conso) > 0 || strlen(r_vowel) > 0));

    // 持ち越し状態で出力が空の場合は成功として扱わない
    if (strlen(result.output) > 0 && !is_right_only) {
        char kana_output[128];
        strcpy(kana_output, result.output);
        result.kana_length = utf8_char_count(kana_output);
        kana_to_roma(kana_output, result.output, sizeof(result.output));
        result.success = true;
    } else if (pending_tsu) {
        // 持ち越し中は空出力だが成功扱い
        result.kana_length = 0;
        result.success = true;
    } else {
        result.kana_length = 0;
        result.success = false;
    }

    return result;
}
