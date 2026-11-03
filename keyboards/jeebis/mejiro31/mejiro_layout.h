// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>

// Platform-independent HID keyboard usages and modifier bits. Custom actions
// remain opaque and must not be decoded as keyboard usages by the backend.
typedef struct {
    uint16_t code;
    uint8_t  mods;
} mji_key_t;
typedef struct {
    uint16_t code;
    uint8_t  mods, clear_mods;
} mji_stroke_t;
typedef struct {
    uint16_t  base;
    mji_key_t plain, shifted;
} mji_layout_entry_t;
typedef struct {
    const mji_layout_entry_t *entries;
    uint16_t                  count;
} mji_layout_map_t;
enum { MJI_ENGLISH, MJI_JAPANESE };
typedef struct {
    mji_layout_map_t maps[2];
    uint32_t         alt_layers;
    uint32_t         literal_symbol_layers; // Symbols on explicit Shift faces are already selected.
    uint8_t          language;
    bool             alt_enabled, jis_host;
    bool             mato_shift_pairs; // Direct symbols retain the author's JIS Shift pairs.
} mji_layout_config_t;

// Pure resolution: no queue, OS modifiers, QMK encodings or global variables.
mji_stroke_t mji_layout_resolve(const mji_layout_config_t *config, mji_key_t key, uint8_t layer, uint8_t context_mods);
