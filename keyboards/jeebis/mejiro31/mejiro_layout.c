// SPDX-License-Identifier: GPL-2.0-or-later
#include "mejiro_layout.h"
#define SHIFT UINT8_C(0x22)
#define LSFT UINT8_C(0x02)

static bool symbol(uint16_t code) {
    return (code >= 0x1e && code <= 0x27) || (code >= 0x2d && code <= 0x38);
}
// US semantic symbol positions -> JIS HID usages. Shift is an output property,
// not a change to the logical Hold state. Both shifted and plain forms are
// explicit, including symbols that need Shift on just one of the host layouts.
static mji_key_t jis_symbol(uint16_t code, bool shifted) {
    static const mji_key_t digits[10][2] = {
        {{0x1e, 0}, {0x1e, LSFT}}, {{0x1f, 0}, {0x2f, 0}}, {{0x20, 0}, {0x20, LSFT}}, {{0x21, 0}, {0x21, LSFT}}, {{0x22, 0}, {0x22, LSFT}}, {{0x23, 0}, {0x2e, 0}}, {{0x24, 0}, {0x23, LSFT}}, {{0x25, 0}, {0x34, LSFT}}, {{0x26, 0}, {0x25, LSFT}}, {{0x27, 0}, {0x26, LSFT}},
    };
    static const mji_key_t punctuation[12][2] = {
        {{0x2d, 0}, {0x87, LSFT}},    // - _
        {{0x2d, LSFT}, {0x33, LSFT}}, // = +
        {{0x30, 0}, {0x30, LSFT}},    // [ {
        {{0x32, 0}, {0x32, LSFT}},    // ] }
        {{0x87, 0}, {0x89, LSFT}},    // backslash |
        {{0x32, 0}, {0x32, LSFT}},    // non-US #/~ position (not a US symbol)
        {{0x33, 0}, {0x34, 0}},       // ; :
        {{0x24, LSFT}, {0x1f, LSFT}}, // ' "
        {{0x2f, LSFT}, {0x2e, LSFT}}, // ` ~
        {{0x36, 0}, {0x36, LSFT}},    // , <
        {{0x37, 0}, {0x37, LSFT}},    // . >
        {{0x38, 0}, {0x38, LSFT}},    // / ?
    };
    if (code >= 0x1e && code <= 0x27) return digits[code - 0x1e][shifted];
    return punctuation[code - 0x2d][shifted];
}

// A direct mato key names its unheld-Shift character, even when that character
// needs Shift in US QMK notation (e.g. ':' or '@'). A held Shift selects the
// author's JIS physical pair: ':' -> '*', '@' -> '`', '-' -> '='.
static mji_stroke_t mato_symbol(const mji_layout_config_t *c, mji_key_t key, uint8_t mods) {
    mji_key_t host = key.code == 0x89 ? (mji_key_t){0x89, key.mods & SHIFT} : jis_symbol(key.code, (key.mods & SHIFT) != 0);
    if (mods & SHIFT) host.mods |= LSFT;
    if (!c->jis_host) {
        // Invert the semantic-to-JIS table after selecting the JIS pair, so
        // choosing a US host does not silently change the author's symbols.
        mji_key_t semantic = {0, 0}; // JIS Shift+0 has no printable counterpart.
        if (host.code == 0x89 && !host.mods) semantic.code = 0x31; // Yen on a US host is backslash.
        for (uint16_t code = 0x1e; code <= 0x38; code++) {
            if (!symbol(code) || code == 0x32) continue;
            for (uint8_t shift = 0; shift < 2; shift++) {
                mji_key_t candidate = jis_symbol(code, shift != 0);
                if (candidate.code == host.code && candidate.mods == host.mods)
                    semantic = (mji_key_t){code, shift ? LSFT : 0};
            }
        }
        host = semantic;
    }
    return (mji_stroke_t){host.code, (key.mods & ~SHIFT) | host.mods, SHIFT};
}

mji_stroke_t mji_layout_resolve(const mji_layout_config_t *c, mji_key_t key, uint8_t layer, uint8_t mods) {
    // JIS Alt+Zenkaku/Hankaku is a shortcut, not a literal US grave symbol.
    if (c->jis_host && key.code == 0x35 && ((mods | key.mods) & 0x44)) return (mji_stroke_t){key.code, key.mods, 0};
    if (c->mato_shift_pairs && ((symbol(key.code) && key.code != 0x32) || key.code == 0x89)) return mato_symbol(c, key, mods);
    bool literal = layer < 32 && (c->literal_symbol_layers & (UINT32_C(1) << layer)) && symbol(key.code);
    bool    remapped       = false;
    uint8_t explicit_shift = key.mods & SHIFT;
    if (c->alt_enabled && layer < 32 && (c->alt_layers & (UINT32_C(1) << layer)) && !((mods | key.mods) & ~SHIFT)) {
        mji_layout_map_t map = c->maps[c->language == MJI_JAPANESE ? MJI_JAPANESE : MJI_ENGLISH];
        for (uint16_t i = 0; i < map.count; i++)
            if (map.entries[i].base == key.code) {
                key      = ((mods | key.mods) & SHIFT) ? map.entries[i].shifted : map.entries[i].plain;
                remapped = true;
                break;
            }
    }
    mji_stroke_t out = {key.code, key.mods, 0};
    if (symbol(key.code) && key.code != 0x32) {
        bool shifted = (((remapped || literal) ? 0 : mods) | key.mods) & SHIFT;
        if (c->jis_host) {
            mji_key_t host = jis_symbol(key.code, shifted);
            out            = (mji_stroke_t){host.code, (key.mods & ~SHIFT) | host.mods, SHIFT};
        } else if (remapped || literal) {
            // A map's symbol is literal: shifted '-' can intentionally map to
            // plain '='. Do not reapply the source key's Shift afterwards.
            out.clear_mods = SHIFT;
        }
    } else if (remapped) {
        // Explicit shifted letters must still be shifted without a held Shift.
        out.mods |= explicit_shift;
    }
    return out;
}
