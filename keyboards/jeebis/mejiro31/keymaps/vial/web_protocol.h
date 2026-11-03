// SPDX-License-Identifier: GPL-3.0-or-later
// Mejiro31 site extension, carried by VIA's custom Raw HID handler.
// Request: FA 01 op (0=get, 1=layouts, 2=first-up), bytes 4..11="M31WEB01".
// Response: same header, byte 3=status (0=ok,1=locked,2=invalid,3=busy).
// Settings: byte12 flags (JIS=1, alternative=2, Mejiro=4), 13 EN, 14 JP.
// Byte18: first-up (0=wait for all keys up, 1=send on first key up).
#pragma once
#include "web_layouts.h"
#include "via.h"
#include "rule_protocol.h"
// A different firmware build resets both its keymap and initial layout choices.
void via_init_kb(void) {
    // Start writable; an explicit Vial lock remains effective until unlocked or rebooted.
    vial_unlocked = 1;
    if (!via_eeprom_is_valid()) eeconfig_init_user();
}
// Three timings share the keyboard's separate EEPROM word, leaving maps and
// dictionary banks untouched. Bit 31 marks this packed millisecond format.
static bool web_timing_valid(mji_timing_t timing) {
    return timing.combo_ms >= 1 && timing.combo_ms <= 1000 &&
           timing.continuation_ms >= 1 && timing.continuation_ms <= 1000 &&
           timing.long_hold_ms > timing.continuation_ms && timing.long_hold_ms <= 2000;
}
static mji_timing_t web_load_timing(void) {
    uint32_t word = eeconfig_read_kb();
    mji_timing_t timing = {word & 1023, (word >> 10) & 1023, (word >> 20) & 2047};
    if (!(word & UINT32_C(0x80000000)) || !web_timing_valid(timing))
        timing = (mji_timing_t){MJI_COMBO_MS, MJI_CONTINUATION_MS, MJI_LONG_HOLD_MS};
    return timing;
}
// The unified engine consumes raw positions before Vial's normal keycode
// gate. Mirror its unlock barrier, including keys released during the dialog.
static bool web_unlock_paused;
static bool web_pause_for_unlock(void) {
    if (!vial_unlock_in_progress) {
        web_unlock_paused = false;
        return false;
    }
    if (!web_unlock_paused) switch_context();
    web_unlock_paused = true;
    return true;
}
static void web_reply(uint8_t *data, uint8_t status) {
    data[3] = status;
    data[12] = input.layout.jis_host | (input.layout.alt_enabled << 1) | (mejiro_mode << 2);
    data[13] = web_en_layout;
    data[14] = web_jp_layout;
    data[15] = MJI_MATO_DIRECT ? 2 : MJI_EN_PROFILE;
    data[16] = DYNAMIC_KEYMAP_LAYER_COUNT;
    data[17] = WEB_LAYOUT_COUNT;
    data[18] = mejiro_first_up_chord_send;
    data[19] = 1; // Timing protocol capability.
    const uint16_t values[] = {input.timing.combo_ms, input.timing.continuation_ms, input.timing.long_hold_ms};
    for (uint8_t i = 0; i < 3; ++i) {
        data[20 + i * 2] = values[i] & 255;
        data[21 + i * 2] = values[i] >> 8;
    }
}
void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
    if (length == 32 && data[0] == 0xfa && data[1] == 2) {
        rule_protocol(data);
        return;
    }
    if (length != 32 || data[0] != 0xfa || data[1] != 1 || memcmp(data + 4, "M31WEB01", 8)) {
        data[0] = 0xff;
        return;
    }
    if (!ready) { web_reply(data, 3); return; }
    if (data[2] == 0) { web_reply(data, 0); return; }
    if (data[2] == 3) {
        mji_timing_t timing = {data[20] | (uint16_t)data[21] << 8,
                               data[22] | (uint16_t)data[23] << 8,
                               data[24] | (uint16_t)data[25] << 8};
        if (!web_timing_valid(timing)) { web_reply(data, 2); return; }
        if (!vial_unlocked) { web_reply(data, 1); return; }
        switch_context();
        input.timing = timing;
        uint32_t word = UINT32_C(0x80000000) | timing.combo_ms |
                        (uint32_t)timing.continuation_ms << 10 | (uint32_t)timing.long_hold_ms << 20;
        if (eeconfig_read_kb() != word) eeconfig_update_kb(word);
        web_reply(data, 0);
        return;
    }
    if (data[2] == 2) {
        if (data[18] > 1) { web_reply(data, 2); return; }
        if (!vial_unlocked) { web_reply(data, 1); return; }
        bool enabled = data[18] != 0;
        if (mejiro_first_up_chord_send != enabled) {
            switch_context();
            mejiro_first_up_chord_send = enabled;
            save_settings();
        }
        web_reply(data, 0);
        return;
    }
    if (data[2] != 1 || data[12] > 7 || data[13] >= WEB_LAYOUT_COUNT || data[14] >= WEB_LAYOUT_COUNT) {
        web_reply(data, 2); return;
    }
    if (MJI_MATO_DIRECT && ((data[12] & 2) || data[13] || data[14])) {
        web_reply(data, 2); return;
    }
    if (!vial_unlocked) { web_reply(data, 1); return; }
    // Configuration changes release existing output and suppress held switches.
    mji_layout_config_t config = input.layout;
    config.jis_host = (data[12] & 1) != 0;
    config.alt_enabled = (data[12] & 2) != 0;
    config.maps[0] = web_layouts[data[13]];
    config.maps[1] = web_layouts[data[14]];
    if (!select_layout(&config)) { web_reply(data, 2); return; }
    web_en_layout = data[13];
    web_jp_layout = data[14];
    mejiro_mode = (data[12] & 4) != 0;
    switch_context();
    save_settings();
    web_reply(data, 0);
}
