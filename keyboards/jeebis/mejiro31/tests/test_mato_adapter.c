// SPDX-License-Identifier: GPL-2.0-or-later
#include "adapter_fixture.c"
int vial_unlocked;
int vial_unlock_in_progress;
bool via_eeprom_is_valid(void) { return true; }

static void mato_setup(void) {
    timing_word = 0;
    setup();
    vial_unlocked = 1;
    assert(input.mato_mode && input.layout.mato_shift_pairs);
    assert(!input.layout.alt_enabled && input.layout.jis_host);
    assert(input.timing.combo_ms == 50 && input.timing.continuation_ms == 50 && input.timing.long_hold_ms == 175);
}
static void tap_at(uint32_t time, uint8_t position) {
    event(time, position, true);
    event(time + 20, position, false);
    tick(time + 200);
}
static void request(uint8_t *packet, uint8_t op, uint8_t flags, uint8_t en, uint8_t jp) {
    memset(packet, 0, 32);
    packet[0] = 0xfa; packet[1] = 1; packet[2] = op;
    memcpy(packet + 4, "M31WEB01", 8);
    packet[12] = flags; packet[13] = en; packet[14] = jp;
    raw_hid_receive_kb(packet, 32);
}
static void test_direct_keys(void) {
    mato_setup();
    tap_at(0, 1);
    assert(presses[KC_L] == 1 && !presses[KC_Q]);
    dynamic_maps[0][0][1] = KC_Q; // Same edit the GUI writes into dynamic storage.
    tap_at(300, 1);
    assert(presses[KC_Q] == 1 && presses[KC_L] == 1);
    all_up();
    mato_setup();
    event(0, 43, true); // Consonant layer.
    tap_at(10, 3);
    event(220, 43, false);
    assert(presses[KC_G] == 1 && !presses[KC_K]);
    all_up();
}
static void test_shift_keeps_layer(void) {
    mato_setup();
    event(0, 1, true); event(20, 13, true); // L+H => Shift.
    tap_at(30, 3);
    assert(presses[KC_K] == 1 && !presses[KC_G]);
    assert(pressed_mods[KC_K] & MOD_BIT(KC_LSFT));
    event(240, 1, false);
    assert(!native_mods); // First combo release ends the modifier.
    tap_at(260, 3);
    assert(presses[KC_K] == 2 && !(pressed_mods[KC_K] & MOD_BIT(KC_LSFT)));
    event(500, 13, false);
    assert(!presses[KC_L] && !presses[KC_H]);
    all_up();
}
static void test_symbols(void) {
    mato_setup();
    event(0, 40, true); // Numbers/symbols.
    tap_at(10, 3); // ':' is a JIS colon, not a converted US semicolon.
    assert(presses[0x34] == 1 && !(pressed_mods[0x34] & MOD_BIT(KC_LSFT)));
    event(230, 42, true); // Shift on the symbol face.
    tap_at(240, 3); // ':' + held Shift => '*'.
    assert(presses[0x34] == 2 && (pressed_mods[0x34] & MOD_BIT(KC_LSFT)));
    event(450, 42, false); event(460, 40, false);
    assert(!presses[KC_ENT]);
    all_up();
}
static void test_mode_and_ime(void) {
    mato_setup();
    tap_at(0, 40); // Short press toggles IME; no symbol layer remains held.
    assert(input.layout.language == MJI_JAPANESE && presses[KC_LNG1] == 1);
    tap_at(300, 29);
    assert(steno_mode && mejiro_mode);
    tap_at(600, 1);
    assert(steno_events >= 2);
    tap_at(900, 29);
    assert(!steno_mode && input.layout.language == MJI_JAPANESE);
    tap_at(1200, 40);
    assert(input.layout.language == MJI_ENGLISH && presses[KC_LNG2] >= 1);
    all_up();
}
static void test_protocol(void) {
    mato_setup();
    uint8_t packet[32];
    request(packet, 0, 0, 0, 0);
    assert(packet[3] == 0 && packet[12] == 5 && packet[13] == 0 && packet[14] == 0);
    assert(packet[15] == 2 && packet[16] == 16 && packet[19] == 1);
    assert(packet[20] == 50 && packet[22] == 50 && packet[24] == 175);
    uint32_t saved = settings_word;
    request(packet, 1, 7, 0, 0); assert(packet[3] == 2 && settings_word == saved);
    request(packet, 1, 5, 1, 0); assert(packet[3] == 2 && settings_word == saved);
    request(packet, 1, 5, 0, 16); assert(packet[3] == 2 && settings_word == saved);
    vial_unlocked = 0;
    request(packet, 1, 4, 0, 0); assert(packet[3] == 1 && settings_word == saved);
    vial_unlocked = 1;
    request(packet, 1, 4, 0, 0);
    assert(packet[3] == 0 && !input.layout.jis_host && !input.layout.alt_enabled);
    keyboard_post_init_user();
    assert(!input.layout.jis_host && input.mato_mode && input.layout.mato_shift_pairs);
    // A previous official-profile setting must not silently change this profile.
    settings_word = UINT32_C(0x4d320007);
    keyboard_post_init_user();
    assert(input.layout.jis_host && !input.layout.alt_enabled && web_en_layout == 0 && web_jp_layout == 0);
    all_up();
}
int main(void) {
    test_direct_keys();
    test_shift_keeps_layer();
    test_symbols();
    test_mode_and_ime();
    test_protocol();
    puts("mato QMK adapter: direct edits, Shift pairs, mode/IME and profile-safe WebHID OK");
    return 0;
}
