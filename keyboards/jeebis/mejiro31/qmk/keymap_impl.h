// Shared implementation included by each declarative keymap.
// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "mejiro_input.h"
#include "steno/chord.h"
#include "steno/engine.h"
#include "layouts/catalog.h"
#ifdef MJI_WEB_VIAL
#    include "web_layouts.h"
#    ifndef MJI_WEB_EN_DEFAULT
#        define MJI_WEB_EN_DEFAULT MJI_WEB_LAYOUT_ID(MJI_ENGLISH_LAYOUT)
#    endif
#    ifndef MJI_WEB_JP_DEFAULT
#        define MJI_WEB_JP_DEFAULT MJI_WEB_LAYOUT_ID(MJI_JAPANESE_LAYOUT)
#    endif
#endif
#include "keymap_introspection.h"
#include "steno/plover.h"
#include "os_detection.h"
#include "eeconfig.h"
#ifdef MJI_WEB_VIAL
#    include "dynamic_keymap.h"
#    include "vial.h"
#endif
#include <string.h>

#ifndef MJI_MATO_DIRECT
#    define MJI_MATO_DIRECT 0
#endif
#if MJI_MATO_DIRECT
// Keymap-specific timings also apply to the saved-setting fallback.
#    undef MJI_COMBO_MS
#    undef MJI_CONTINUATION_MS
#    undef MJI_LONG_HOLD_MS
#    define MJI_COMBO_MS 50
#    define MJI_CONTINUATION_MS 50
#    define MJI_LONG_HOLD_MS 175
#endif

// Tap layers 0..5; matching Hold maps are stored at 6..11.
enum { MJI_QWERTY, MJI_SHIFTED, MJI_NUMBER, MJI_NUMBER_SHIFTED, MJI_FUNCTION, MJI_STENO, MJI_HOLD_OFFSET };
#ifdef MJI_WEB_VIAL
#    define MJI_CUSTOM_BASE QK_KB_0
#    define mji_keycode_at dynamic_keymap_get_keycode
#else
#    define MJI_CUSTOM_BASE SAFE_RANGE
#    define mji_keycode_at keycode_at_keymap_location
#endif
enum { MJI_00 = MJI_CUSTOM_BASE, MJI_000, MJI_MODE, MJI_JIS, MJI_ALT, MJI_MEJIRO, MJI_STICKY, MJI_IME };

#if MJI_MATO_DIRECT
#    include "defaults_mato.h"
#else
#    include "defaults.h"
#endif

// Physical coordinates, independent of key assignments or language/layout.
// Logical 0..47 use the original matrix cells; 48..53 use extension storage.
// Logical 54..57 use the new thumb-combo storage layers 14/15.
// No additional physical switches or scan rows are introduced.
static const mji_pair_t physical_pairs[] = {
    {1, 13, 24, 0}, {2, 14, 25, 0}, {3, 15, 26, 0}, {4, 16, 27, 0}, {5, 17, 31, 0}, {6, 18, 32, 0}, {7, 19, 33, 0}, {8, 20, 34, 0}, {9, 21, 35, 0}, {10, 22, 36, 0}, {11, 23, 37, 0}, {4, 5, 38, 1}, {16, 17, 39, 1}, {6, 7, 44, 1}, {18, 19, 45, 1}, {42, 43, 46, 1},
    {0, 12, 48, 0}, {0, 1, 49, 1}, {12, 13, 50, 1}, {40, 41, 51, 1}, {22, 23, 52, 1}, {10, 11, 53, 1},
    {28, 40, 54, 0}, {28, 41, 55, 0}, {30, 42, 56, 0}, {30, 43, 57, 0},
};

static mji_engine_t input;
static bool         mejiro_mode = MJI_DEFAULT_MEJIRO, steno_mode, ready, mac;
static uint8_t      host_mods;
static uint8_t      keyboard_language;
static uint16_t     held_codes[MJI_TOKEN_COUNT];
static uint8_t      refs[256];
static uint16_t     steno_keys[MJI_PHYSICAL_COUNT];
static uint64_t     steno_held;
static bool         shortcut_active;
static bool         replay_down[256];
static uint8_t      replay_saved_mods;

static void capture_key(uint8_t code, bool pressed) {
    if (!code) return;
    mejiro_hid_event event = {.usage = code, .page = 7, .mods = get_mods(), .kind = pressed ? 1 : 2};
    if (IS_CONSUMER_KEYCODE(code)) {
        event.page = 12;
        event.usage = KEYCODE2CONSUMER(code);
    } else if (IS_SYSTEM_KEYCODE(code)) {
        event.page = 1;
        event.usage = KEYCODE2SYSTEM(code);
    }
    mejiro_engine_record(event);
}
static void register_output(uint8_t code) {
    capture_key(code, true);
    register_code(code);
}
static void unregister_output(uint8_t code) {
    capture_key(code, false);
    unregister_code(code);
}
static void tap_output(uint8_t code) {
    capture_key(code, true);
    tap_code16(code);
    capture_key(code, false);
}
void mejiro_keyboard_replay_begin(void) {
    replay_saved_mods = get_mods();
    memset(replay_down, 0, sizeof(replay_down));
    clear_keyboard();
}
void mejiro_keyboard_replay_event(const mejiro_hid_event *event) {
    set_mods(event->mods);
    set_weak_mods(0);
    clear_oneshot_mods();
    send_keyboard_report();
    if (!event->kind) return;
    if (event->page == 7 && event->usage < 256) {
        if (event->kind == 1) register_code(event->usage);
        else unregister_code(event->usage);
        replay_down[event->usage] = event->kind == 1;
    } else if (event->page == 12) host_consumer_send(event->kind == 1 ? event->usage : 0);
    else if (event->page == 1) host_system_send(event->kind == 1 ? event->usage : 0);
}
void mejiro_keyboard_replay_end(void) {
    for (unsigned code = 1; code < 256; ++code)
        if (replay_down[code]) unregister_code(code);
    host_consumer_send(0);
    host_system_send(0);
    clear_keyboard();
    set_mods(replay_saved_mods);
    for (unsigned code = 1; code < 256; ++code)
        if (refs[code]) register_code(code);
    send_keyboard_report();
}

static bool shortcut_held(void) {
    if (!steno_mode) return false;
    for (uint8_t i = 0; i < MJI_TOKEN_COUNT; ++i) {
        mji_token_t *t = &input.tokens[i];
        if ((t->state == MJI_ACTIVE || t->state == MJI_PREPARED) && (t->binding.hold_mods & ~MOD_MASK_SHIFT)) return true;
    }
    return false;
}
static void reset_steno_input(void) {
    // Plover owns chord assembly: releasing the HID state completes any
    // already-transmitted stroke. Held physical keys stay ignored until up.
    mejiro_plover_reset();
    input.ignored |= steno_held;
    steno_held = 0;
    memset(steno_keys, 0, sizeof(steno_keys));
    mejiro_reset_state();
}
static void sync_shortcut(void) {
    bool active = shortcut_held();
    if (active && !shortcut_active) reset_steno_input();
    if (!active && shortcut_active) mejiro_engine_flush_keyboard();
    shortcut_active = active;
}

// Versioned user word, independent of VIA's editable keymap storage.
#ifdef MJI_WEB_VIAL
#    if MJI_MATO_DIRECT
#        define MJI_SETTINGS_MAGIC UINT32_C(0x4d330000)
#    else
#        define MJI_SETTINGS_MAGIC UINT32_C(0x4d320000)
#    endif
#    define MJI_SETTINGS_MASK UINT32_C(0x3fff)
#    define MJI_LAYOUT_SETTINGS (((uint32_t)web_en_layout << 3) | ((uint32_t)web_jp_layout << 8))
#    define MJI_CHORD_SETTINGS ((uint32_t)!mejiro_first_up_chord_send << 13)
#    define MJI_DEFAULT_LAYOUT_SETTINGS (((uint32_t)MJI_WEB_EN_DEFAULT << 3) | ((uint32_t)MJI_WEB_JP_DEFAULT << 8))
static uint8_t web_en_layout = MJI_WEB_EN_DEFAULT, web_jp_layout = MJI_WEB_JP_DEFAULT;
#else
#    define MJI_SETTINGS_MAGIC UINT32_C(0x4d310100)
#    define MJI_SETTINGS_MASK UINT32_C(7)
#    define MJI_LAYOUT_SETTINGS UINT32_C(0)
#    define MJI_CHORD_SETTINGS UINT32_C(0)
#    define MJI_DEFAULT_LAYOUT_SETTINGS UINT32_C(0)
#endif
static void save_settings(void) {
    uint32_t word = MJI_SETTINGS_MAGIC | input.layout.jis_host | ((uint32_t)input.layout.alt_enabled << 1) | ((uint32_t)mejiro_mode << 2) | MJI_LAYOUT_SETTINGS | MJI_CHORD_SETTINGS;
    if (eeconfig_read_user() != word) eeconfig_update_user(word);
}
void eeconfig_init_user(void) {
#ifdef MJI_WEB_VIAL
    eeconfig_update_kb(0); // Reset timing defaults with the other initial settings.
#endif
    eeconfig_update_user(MJI_SETTINGS_MAGIC | MJI_DEFAULT_JIS | (MJI_MATO_DIRECT ? UINT32_C(0) : UINT32_C(2)) | ((uint32_t)MJI_DEFAULT_MEJIRO << 2) | MJI_DEFAULT_LAYOUT_SETTINGS);
}

static uint8_t expand_mods(uint8_t mods) {
    return mods & 0x10 ? (mods & 0x0f) << 4 : mods;
}
static uint8_t platform_mods(uint8_t mods) {
    if (!mac) return mods;
    return (mods & ~(MOD_MASK_CTRL | MOD_MASK_GUI)) | ((mods & MOD_MASK_CTRL) << 3) | ((mods & MOD_MASK_GUI) >> 3);
}
static void report_mods(uint8_t mods) {
    uint8_t native = platform_mods(mods);
    if (native != get_mods()) mejiro_engine_record((mejiro_hid_event){.page = 7, .mods = native});
    set_mods(native);
    set_weak_mods(0);
    clear_oneshot_mods();
    send_keyboard_report();
}
static void mods_callback(void *user, uint8_t mods) {
    (void)user;
    host_mods = mods;
    report_mods(mods);
}
static void clear_callback(void *user) {
    (void)user;
    // These releases really go to the host during a mode boundary. Complete
    // their recorded stream even after steno_mode has already been toggled.
    for (unsigned code = 1; code < 256; ++code)
        if (refs[code]) capture_key(code, false);
    if (get_mods()) mejiro_engine_record((mejiro_hid_event){.page = 7});
    reset_steno_input();
    shortcut_active = false;
    memset(held_codes, 0, sizeof(held_codes));
    memset(refs, 0, sizeof(refs));
    memset(steno_keys, 0, sizeof(steno_keys));
    steno_held = 0;
    host_mods  = 0;
    clear_keyboard();
    layer_clear();
}
static void switch_context(void) {
    mji_reset(&input, steno_mode ? MJI_STENO : MJI_QWERTY);
    default_layer_set((layer_state_t)1 << input.base_layer);
}

// Platform codec only. Layout policy and symbol translation live in the engine.
static bool decode_callback(void *user, uint16_t code, mji_key_t *key) {
    (void)user;
    if (IS_QK_MODS(code)) {
        *key = (mji_key_t){QK_MODS_GET_BASIC_KEYCODE(code), expand_mods(QK_MODS_GET_MODS(code))};
        return true;
    }
    if (code <= QK_BASIC_MAX) {
        *key = (mji_key_t){code, 0};
        return true;
    }
    return false;
}
static bool select_layout(const mji_layout_config_t *config) {
    return mji_set_layout(&input, config);
}
static mji_stroke_t encode_callback(void *user, mji_stroke_t stroke, uint8_t mods) {
    (void)user;
    if (mac && (stroke.code == KC_HOME || stroke.code == KC_END)) {
        uint8_t effective = (mods & ~stroke.clear_mods) | stroke.mods;
        bool document = platform_mods(effective) & MOD_MASK_GUI;
        stroke.code = stroke.code == KC_HOME ? (document ? KC_UP : KC_LEFT) : (document ? KC_DOWN : KC_RIGHT);
        // Logical Ctrl becomes Command at the platform boundary. Preserve
        // Shift/Alt; suppress host Ctrl for the document-boundary variant.
        stroke.mods |= MOD_LCTL;
        if (document) {
            stroke.mods &= ~MOD_MASK_GUI;
            stroke.clear_mods |= MOD_MASK_GUI;
        }
    }
    return stroke;
}

// Steno is a separate input mode; its explicit taps share host-layout encoding
// without passing through keyboard compression or alternative-letter mappings.
bool mejiro_steno_jis_host(void) {
    return input.layout.jis_host;
}
void mejiro_steno_tap(uint16_t code) {
    mji_key_t key;
    if (!decode_callback(NULL, code, &key)) {
        tap_code16(code);
        return;
    }
    mji_layout_config_t config = input.layout;
    config.mato_shift_pairs = false; // Steno emissions already name their intended symbols.
    config.alt_enabled         = false;
    mji_stroke_t stroke        = mji_layout_resolve(&config, key, MJI_STENO, 0);
    uint8_t      saved         = get_mods();
    // Convert the saved platform mods back to logical mods before encoding.
    stroke.mods = platform_mods(stroke.mods);
    stroke.clear_mods = platform_mods(stroke.clear_mods);
    stroke = encode_callback(NULL, stroke, platform_mods(saved));
    set_mods(platform_mods((platform_mods(saved) & ~stroke.clear_mods) | stroke.mods));
    send_keyboard_report();
    tap_code16(stroke.code);
    set_mods(saved);
    send_keyboard_report();
}
static mji_binding_t binding(uint16_t tap, uint16_t hold) {
    mji_binding_t b = {.tap = tap, .hold_layer = MJI_NO_LAYER};
    // Native modifier/layer actions use the same engine Hold representation.
    if (!hold && (IS_MODIFIER_KEYCODE(tap) || IS_QK_MOMENTARY(tap))) {
        hold  = tap;
        b.tap = KC_NO;
    }
    if (IS_MODIFIER_KEYCODE(hold))
        b.hold_mods = MOD_BIT(hold);
    else if (IS_QK_MODS(hold) && QK_MODS_GET_BASIC_KEYCODE(hold) == KC_NO)
        b.hold_mods = expand_mods(QK_MODS_GET_MODS(hold));
    else if (IS_QK_MOMENTARY(hold))
        b.hold_layer = QK_MOMENTARY_GET_LAYER(hold);
    else if (hold != KC_TRNS)
        b.hold_code = hold;
    if (b.tap == MJI_STICKY || (IS_QK_ONE_SHOT_MOD(b.tap) && (expand_mods(QK_ONE_SHOT_MOD_GET_MODS(b.tap)) & MOD_MASK_SHIFT))) b.tap = MJI_STICKY_SHIFT;
    if (b.tap == KC_TRNS) b.tap = KC_NO;
    return b;
}
static uint16_t read_binding_code(uint8_t layer, uint16_t logical) {
    if (logical < 48) return mji_keycode_at(layer, logical / MATRIX_COLS, logical % MATRIX_COLS);
#ifdef MJI_WEB_VIAL
    if (logical >= 54 && logical < 58 && layer < 12) {
        uint8_t index = (layer % 6) * 4 + logical - 54;
        return dynamic_keymap_get_keycode(layer < 6 ? 14 : 15, index / MATRIX_COLS, index % MATRIX_COLS);
    }
    if (logical < 54 && layer < 12) {
        uint8_t index = (layer % 6) * 6 + logical - 48;
        return dynamic_keymap_get_keycode(layer < 6 ? 12 : 13, index / MATRIX_COLS, index % MATRIX_COLS);
    }
#endif
    return KC_NO;
}
static mji_binding_t lookup_callback(void *user, uint16_t logical, const mji_context_t *c) {
    (void)user;
    for (uint8_t i = 0; i < c->layer_count; i++) {
        uint8_t layer = c->layers[i];
        if (layer >= MJI_HOLD_OFFSET) continue;
        if (steno_mode && (c->mods & ~MOD_MASK_SHIFT) && layer == MJI_STENO) layer = MJI_QWERTY;
        if (!MJI_MATO_DIRECT && (c->mods & MOD_MASK_SHIFT)) {
            if (layer == MJI_QWERTY)
                layer = MJI_SHIFTED;
            else if (layer == MJI_NUMBER)
                layer = MJI_NUMBER_SHIFTED;
        }
        if (layer == MJI_STENO) {
            for (uint8_t pair = 0; pair < sizeof(physical_pairs) / sizeof(*physical_pairs); ++pair)
                if (physical_pairs[pair].logical == logical) return (mji_binding_t){.hold_layer = MJI_NO_LAYER};
        }
        uint16_t tap = read_binding_code(layer, logical);
        uint16_t hold = read_binding_code(layer + MJI_HOLD_OFFSET, logical);
        if (tap == KC_TRNS && hold == KC_TRNS) continue;
        mji_binding_t b = binding(tap, hold);
        b.layer         = layer;
        // Seven physical thumb keys plus the right-thumb combo (42 + 43).
        // Main-finger virtual positions on rows 2/3 remain remappable.
        b.skip_alt = logical == 28 || logical == 29 || logical == 30 ||
                     logical == 40 || logical == 41 || logical == 42 ||
                     logical == 43 || logical == 46;
        return b;
    }
    return (mji_binding_t){.hold_layer = MJI_NO_LAYER};
}
static bool custom_action(uint16_t code, bool pressed) {
    switch (code) {
        case MJI_MODE:
            if (pressed) {
                mji_layout_config_t config = input.layout;
                if (!steno_mode) keyboard_language = config.language;
                steno_mode = !steno_mode;
                bool ime_on_entry = !MJI_EN_PROFILE && steno_mode && mejiro_mode;
                if (ime_on_entry) config.language = MJI_JAPANESE;
                if (!steno_mode) config.language = keyboard_language;
                switch_context();
                if (config.language != input.layout.language) select_layout(&config);
                // Send after releasing the previous context's keys and mods.
                if (ime_on_entry || !steno_mode) {
                    uint8_t language = config.language == MJI_JAPANESE ? KC_LNG1 : KC_LNG2;
                    capture_key(language, true);
                    tap_code16(language);
                    capture_key(language, false);
                }
                if (steno_mode) mejiro_engine_flush_keyboard();
            }
            return true;
        case MJI_JIS:
            if (pressed) {
                mji_layout_config_t config = input.layout;
                config.jis_host            = !config.jis_host;
                select_layout(&config);
                save_settings();
            }
            return true;
        case MJI_ALT:
            if (MJI_MATO_DIRECT) return true; // Direct maps cannot be translated a second time.
            if (pressed) {
                mji_layout_config_t config = input.layout;
                config.alt_enabled         = !config.alt_enabled;
                select_layout(&config);
                save_settings();
            }
            return true;
        case MJI_MEJIRO:
            if (pressed) {
                mejiro_mode = !mejiro_mode;
                switch_context();
                save_settings();
            }
            return true;
        case MJI_IME:
            if (pressed) {
                mji_layout_config_t config = input.layout;
                config.language = config.language == MJI_JAPANESE ? MJI_ENGLISH : MJI_JAPANESE;
                select_layout(&config);
                tap_output(config.language == MJI_JAPANESE ? KC_LNG1 : KC_LNG2);
            }
            return true;
        case KC_LNG1:
        case KC_LNG2:
            if (pressed) {
                mji_layout_config_t config = input.layout;
                config.language            = code == KC_LNG1 ? MJI_JAPANESE : MJI_ENGLISH;
                select_layout(&config);
                tap_output(code);
            }
            return true;
        case QK_BOOTLOADER:
            if (pressed) {
                switch_context();
                reset_keyboard();
            }
            return true;
        default:
            return false;
    }
}
static void key_callback(void *user, uint8_t source, uint16_t code, bool pressed, uint8_t mods) {
    (void)user;
    if (custom_action(code, pressed)) return;
    // Unsupported QMK feature encodings must not truncate into unrelated HID
    // keys. The portable engine can represent them; add an explicit adapter
    // action before exposing such a feature in the editor.
    if (code > QK_BASIC_MAX && code != MJI_00 && code != MJI_000) return;
    if (pressed) {
        bool zero_action = code == MJI_00 || code == MJI_000;
        // Emit the extra zero(s), then retain an ordinary 0 for OS repeat.
        if (zero_action) {
            // Pulse without invalidating another logical owner's held 0.
            if (refs[KC_0]) unregister_output(KC_0);
            report_mods(mods);
            tap_output(KC_0);
            if (code == MJI_000) tap_output(KC_0);
            code = KC_0;
        }
        held_codes[source] = code;
        report_mods(mods);
        uint8_t basic = code;
        if (!refs[basic]++ || zero_action) register_output(basic);
    } else {
        code = held_codes[source];
        if (!code) return;
        uint8_t basic = code;
        if (refs[basic] && !--refs[basic]) unregister_output(basic);
        held_codes[source] = 0;
    }
    report_mods(mods);
}

#ifdef MJI_WEB_VIAL
#    include "web_protocol.h"
#endif

void keyboard_post_init_user(void) {
    mejiro_engine_init();
    mac = detected_host_os() == OS_MACOS || detected_host_os() == OS_IOS;
#ifdef NKRO_ENABLE
    keymap_config.nkro = true;
#endif
    uint32_t settings = eeconfig_read_user();
    if ((settings & ~MJI_SETTINGS_MASK) != MJI_SETTINGS_MAGIC) {
        eeconfig_init_user();
        settings = eeconfig_read_user();
    }
    mejiro_mode = (settings & 4) != 0;
#ifdef MJI_WEB_VIAL
    mejiro_first_up_chord_send = (settings & UINT32_C(0x2000)) == 0;
    mejiro_rules_load();
    web_en_layout = (settings >> 3) & 31;
    web_jp_layout = (settings >> 8) & 31;
    if (web_en_layout >= WEB_LAYOUT_COUNT || web_jp_layout >= WEB_LAYOUT_COUNT) {
        web_en_layout = MJI_WEB_EN_DEFAULT;
        web_jp_layout = MJI_WEB_JP_DEFAULT;
    }
#endif
    mji_callbacks_t cb = {.lookup = lookup_callback, .key = key_callback, .mods = mods_callback, .clear = clear_callback, .decode = decode_callback, .encode = encode_callback};
    mji_timing_t timing = {MJI_COMBO_MS, MJI_CONTINUATION_MS, MJI_LONG_HOLD_MS};
#ifdef MJI_WEB_VIAL
    timing = web_load_timing();
#endif
    ready              = mji_init(&input, physical_pairs, sizeof(physical_pairs) / sizeof(*physical_pairs), timing, MJI_QWERTY, cb, NULL);
    input.mato_mode = MJI_MATO_DIRECT;
    if (ready) {
        mji_layout_config_t config = {.maps = {MJI_LAYOUT_MAP(MJI_ENGLISH_LAYOUT), MJI_LAYOUT_MAP(MJI_JAPANESE_LAYOUT)}, .alt_layers = (UINT32_C(1) << MJI_QWERTY) | (UINT32_C(1) << MJI_SHIFTED), .literal_symbol_layers = (UINT32_C(1) << MJI_SHIFTED) | (UINT32_C(1) << MJI_NUMBER_SHIFTED), .language = MJI_ENGLISH, .alt_enabled = true, .jis_host = MJI_DEFAULT_JIS};
#ifdef MJI_WEB_VIAL
        config.maps[0] = web_layouts[web_en_layout];
        config.maps[1] = web_layouts[web_jp_layout];
#endif
        config.jis_host = (settings & 1) != 0;
        config.alt_enabled = (settings & 2) != 0;
#if MJI_MATO_DIRECT
        config.alt_enabled = false;
        config.alt_layers = 0;
        config.literal_symbol_layers = 0;
        config.mato_shift_pairs = true;
#endif
        ready                      = select_layout(&config);
    }
    default_layer_set((layer_state_t)1 << MJI_QWERTY);
}

bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    if (!ready || record->event.key.row >= MATRIX_ROWS || record->event.key.col >= MATRIX_COLS) return false;
    uint8_t  p       = record->event.key.row * MATRIX_COLS + record->event.key.col;
    uint64_t mask    = UINT64_C(1) << p;
    bool     pressed = record->event.pressed;
#ifdef MJI_WEB_VIAL
    if (web_pause_for_unlock()) {
        if (pressed) { input.physical |= mask; input.ignored |= mask; }
        else { input.physical &= ~mask; input.ignored &= ~mask; }
        return false;
    }
#endif
    sync_shortcut();
    // Steno keys bypass keyboard compression, but share the mode-reset barrier.
    bool steno_layer = steno_mode && !shortcut_active;
    for (uint8_t i = 0; i < MJI_TOKEN_COUNT; i++) {
        if ((input.tokens[i].state == MJI_ACTIVE || input.tokens[i].state == MJI_PREPARED) && input.tokens[i].binding.hold_layer != MJI_NO_LAYER) steno_layer = false;
    }
    // A key routed through the keyboard engine keeps that release path even
    // if its modifier was released before it.
    if (steno_mode && !(input.ignored & mask) && (pressed || !input.owner[p])) {
        uint16_t kc = pressed ? (steno_layer ? mji_keycode_at(MJI_STENO, p / MATRIX_COLS, p % MATRIX_COLS) : KC_NO) : steno_keys[p];
        if (is_stn_key(kc)) {
            if (pressed) {
                steno_keys[p] = kc;
                steno_held |= mask;
                input.physical |= mask;
            } else {
                steno_held &= ~mask;
                input.physical &= ~mask;
                steno_keys[p] = 0;
            }
            if (mejiro_mode) {
                if (pressed)
                    mejiro_on_press(kc);
                else {
                    mejiro_on_release(kc);
                }
            } else {
                mejiro_plover_key(kc, pressed);
            }
            return false;
        }
    }
    if (steno_mode && mejiro_mode && pressed) mejiro_engine_reset();
    mji_event(&input, p, pressed, timer_read32());
    sync_shortcut();
    return false; // Raw keyboard events are fully consumed by this engine.
}
void matrix_scan_user(void) {
    if (!ready) return;
#ifdef MJI_WEB_VIAL
    if (web_pause_for_unlock()) return;
#endif
    mji_tick(&input, timer_read32());
    sync_shortcut();
    // OS detection may finish after startup; don't change mapping mid-hold.
    if (!input.physical && !input.event_count) mac = detected_host_os() == OS_MACOS || detected_host_os() == OS_IOS;
}
