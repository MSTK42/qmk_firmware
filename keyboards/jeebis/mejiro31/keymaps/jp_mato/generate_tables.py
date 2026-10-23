"""Translate the visually checked author profile into firmware tables."""
import json
import sys
from pathlib import Path
BASE = Path(__file__).resolve().parent
profile = json.loads((BASE / 'mato-source.json').read_text(encoding='utf-8'))
aliases = {'BLANK':'KC_NO','TRANSPARENT':'KC_TRNS','MO1':'MATO_MO1',
           'HOLD_MO2_TAP_ZKHK':'MATO_DUAL2','HOLD_MO3_TAP_SPACE':'MATO_DUAL3',
           'HOLD_MO4_TAP_ENTER':'MATO_DUAL4','SHIFT':'KC_LSFT',
           'SHIFT_SPACE':'MATO_SHIFT_SPACE','SHIFT_ENTER':'MATO_SHIFT_ENTER',
           'CTRL_Z':'LCTL(KC_Z)','CTRL_X':'LCTL(KC_X)','CTRL_C':'LCTL(KC_C)','CTRL_V':'LCTL(KC_V)'}
positions=profile['positions']
assert len(positions)==24 and len(profile['layers'])==5
out=['// SPDX-License-Identifier: GPL-2.0-or-later',
     '// Generated from mato-source.json. Source: '+profile['source_article'],
     '#include "mato_engine.h"','#include "quantum_keycodes.h"','#include "keymap_japanese.h"',
     'const uint16_t mato_maps[MATO_LAYERS][MATO_KEYS] = {']
for layer in profile['layers']:
    out.append('    {'+', '.join(aliases.get(layer['keys'][p],layer['keys'][p]) for p in positions)+'},')
out+=['};','const uint8_t mato_combo_keys[MATO_COMBOS][2] = {']
for combo in profile['modifier_combos']:
    out.append('    {'+', '.join(str(positions.index(p)) for p in combo['keys'])+'},')
out+=['};','const uint16_t mato_combo_mods[MATO_COMBOS] = {']
mods={'SHIFT':'KC_LSFT','GUI':'KC_LGUI','ALT':'KC_LALT','CTRL':'KC_LCTL'}
out.append('    '+', '.join(mods[c['modifier']] for c in profile['modifier_combos']))
out+=['};','']
result = '\n'.join(out)
if '--check' in sys.argv:
    assert (BASE/'mato_tables.c').read_text(encoding='utf-8') == result, 'Run generate_tables.py to refresh mato_tables.c'
else:
    (BASE/'mato_tables.c').write_text(result,encoding='utf-8')
