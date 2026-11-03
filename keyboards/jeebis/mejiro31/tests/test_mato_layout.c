// SPDX-License-Identifier: GPL-2.0-or-later
#include "../mejiro_layout.h"
#include <assert.h>
#include <stdio.h>

enum { LSFT = 2, RSFT = 0x20, SHIFT = 0x22, CTRL = 1 };
typedef struct {
    const char *name;
    mji_key_t semantic;
    mji_key_t jis[2], us[2];
} symbol_case_t;

// Independent expected physical strokes for the author's displayed JIS pairs.
// The input uses semantic US notation; the output follows the selected host.
static const symbol_case_t symbols[] = {
    {"- =", {0x2d, 0}, {{0x2d, 0}, {0x2d, LSFT}}, {{0x2d, 0}, {0x2e, 0}}},
    {", <", {0x36, 0}, {{0x36, 0}, {0x36, LSFT}}, {{0x36, 0}, {0x36, LSFT}}},
    {". >", {0x37, 0}, {{0x37, 0}, {0x37, LSFT}}, {{0x37, 0}, {0x37, LSFT}}},
    {"/ ?", {0x38, 0}, {{0x38, 0}, {0x38, LSFT}}, {{0x38, 0}, {0x38, LSFT}}},
    {"backslash _", {0x31, 0}, {{0x87, 0}, {0x87, LSFT}}, {{0x31, 0}, {0x2d, LSFT}}},
    {"yen |", {0x89, 0}, {{0x89, 0}, {0x89, LSFT}}, {{0x31, 0}, {0x31, LSFT}}},
    {": *", {0x33, LSFT}, {{0x34, 0}, {0x34, LSFT}}, {{0x33, LSFT}, {0x25, LSFT}}},
    {"; +", {0x33, 0}, {{0x33, 0}, {0x33, LSFT}}, {{0x33, 0}, {0x2e, LSFT}}},
    {"@ grave", {0x1f, LSFT}, {{0x2f, 0}, {0x2f, LSFT}}, {{0x1f, LSFT}, {0x35, 0}}},
    {"[ {", {0x2f, 0}, {{0x30, 0}, {0x30, LSFT}}, {{0x2f, 0}, {0x2f, LSFT}}},
    {"] }", {0x30, 0}, {{0x32, 0}, {0x32, LSFT}}, {{0x30, 0}, {0x30, LSFT}}},
    {"^ ~", {0x23, LSFT}, {{0x2e, 0}, {0x2e, LSFT}}, {{0x23, LSFT}, {0x35, LSFT}}},
    {"1 !", {0x1e, 0}, {{0x1e, 0}, {0x1e, LSFT}}, {{0x1e, 0}, {0x1e, LSFT}}},
    {"2 quote", {0x1f, 0}, {{0x1f, 0}, {0x1f, LSFT}}, {{0x1f, 0}, {0x34, LSFT}}},
    {"3 #", {0x20, 0}, {{0x20, 0}, {0x20, LSFT}}, {{0x20, 0}, {0x20, LSFT}}},
    {"4 $", {0x21, 0}, {{0x21, 0}, {0x21, LSFT}}, {{0x21, 0}, {0x21, LSFT}}},
    {"5 %", {0x22, 0}, {{0x22, 0}, {0x22, LSFT}}, {{0x22, 0}, {0x22, LSFT}}},
    {"6 &", {0x23, 0}, {{0x23, 0}, {0x23, LSFT}}, {{0x23, 0}, {0x24, LSFT}}},
    {"7 apostrophe", {0x24, 0}, {{0x24, 0}, {0x24, LSFT}}, {{0x24, 0}, {0x34, 0}}},
    {"8 (", {0x25, 0}, {{0x25, 0}, {0x25, LSFT}}, {{0x25, 0}, {0x26, LSFT}}},
    {"9 )", {0x26, 0}, {{0x26, 0}, {0x26, LSFT}}, {{0x26, 0}, {0x27, LSFT}}},
    {"0 no shifted character", {0x27, 0}, {{0x27, 0}, {0x27, LSFT}}, {{0x27, 0}, {0, 0}}},
};
static void expect(const char *name, mji_stroke_t actual, mji_key_t expected, uint8_t clear) {
    if (actual.code != expected.code || actual.mods != expected.mods || actual.clear_mods != clear) {
        fprintf(stderr, "%s: expected %x/%x/clear%x, got %x/%x/clear%x\n", name,
                expected.code, expected.mods, clear, actual.code, actual.mods, actual.clear_mods);
        assert(false);
    }
}
static void test_author_pairs_on_both_hosts(void) {
    static const uint8_t holds[] = {0, LSFT, RSFT};
    for (unsigned host = 0; host < 2; ++host) {
        mji_layout_config_t config = {.jis_host = host != 0, .mato_shift_pairs = true};
        for (unsigned i = 0; i < sizeof(symbols) / sizeof(*symbols); ++i) {
            const symbol_case_t *test = &symbols[i];
            for (unsigned shift = 0; shift < sizeof(holds); ++shift) {
                mji_key_t expected = host ? test->jis[shift != 0] : test->us[shift != 0];
                for (uint8_t layer = 0; layer < 5; ++layer) {
                    expect(test->name, mji_layout_resolve(&config, test->semantic, layer, holds[shift]), expected, SHIFT);
                    // The chosen symbol clears only Shift, preserving a held Ctrl.
                    mji_stroke_t with_ctrl = mji_layout_resolve(&config, test->semantic, layer, holds[shift] | CTRL);
                    expect(test->name, with_ctrl, expected, SHIFT);
                    assert((((holds[shift] | CTRL) & ~with_ctrl.clear_mods) | with_ctrl.mods) == (CTRL | expected.mods));
                }
            }
        }
    }
}
static void test_explicit_modifiers_and_letters(void) {
    for (unsigned host = 0; host < 2; ++host) {
        mji_layout_config_t config = {.jis_host = host != 0, .mato_shift_pairs = true};
        expect("Ctrl+colon", mji_layout_resolve(&config, (mji_key_t){0x33, CTRL | LSFT}, 2, LSFT),
               host ? (mji_key_t){0x34, CTRL | LSFT} : (mji_key_t){0x25, CTRL | LSFT}, SHIFT);
        expect("A with held Shift", mji_layout_resolve(&config, (mji_key_t){4, 0}, 0, LSFT), (mji_key_t){4, 0}, 0);
        expect("explicit Ctrl+C", mji_layout_resolve(&config, (mji_key_t){6, CTRL}, 3, LSFT), (mji_key_t){6, CTRL}, 0);
        expect("right Shift semantic colon", mji_layout_resolve(&config, (mji_key_t){0x33, RSFT}, 2, 0),
               host ? (mji_key_t){0x34, 0} : (mji_key_t){0x33, LSFT}, SHIFT);
    }
    mji_layout_config_t jis = {.jis_host = true, .mato_shift_pairs = true};
    expect("Alt+Zenkaku context", mji_layout_resolve(&jis, (mji_key_t){0x35, 0}, 0, 4), (mji_key_t){0x35, 0}, 0);
    expect("Alt+Zenkaku encoded", mji_layout_resolve(&jis, (mji_key_t){0x35, 0x40}, 0, 0), (mji_key_t){0x35, 0x40}, 0);
}
static void test_disabled_keeps_standard_semantics(void) {
    mji_layout_config_t us = {0};
    mji_layout_config_t jis = {.jis_host = true};
    // Standard Shift+- remains underscore; mato Shift+- would be equals.
    expect("standard US minus", mji_layout_resolve(&us, (mji_key_t){0x2d, 0}, 0, LSFT), (mji_key_t){0x2d, 0}, 0);
    expect("standard JIS minus", mji_layout_resolve(&jis, (mji_key_t){0x2d, 0}, 0, LSFT), (mji_key_t){0x87, LSFT}, SHIFT);
    // Explicit ':' and '@' retain their standard meanings under held Shift.
    expect("standard JIS colon", mji_layout_resolve(&jis, (mji_key_t){0x33, LSFT}, 2, LSFT), (mji_key_t){0x34, 0}, SHIFT);
    expect("standard US colon", mji_layout_resolve(&us, (mji_key_t){0x33, LSFT}, 2, LSFT), (mji_key_t){0x33, LSFT}, 0);
    expect("standard JIS at", mji_layout_resolve(&jis, (mji_key_t){0x1f, LSFT}, 0, LSFT), (mji_key_t){0x2f, 0}, SHIFT);
    expect("standard US at", mji_layout_resolve(&us, (mji_key_t){0x1f, LSFT}, 0, LSFT), (mji_key_t){0x1f, LSFT}, 0);
}
int main(void) {
    test_author_pairs_on_both_hosts();
    test_explicit_modifiers_and_letters();
    test_disabled_keeps_standard_semantics();
    puts("mato layout tests passed (JIS/US, left/right Shift, Ctrl, standard regression)");
    return 0;
}
