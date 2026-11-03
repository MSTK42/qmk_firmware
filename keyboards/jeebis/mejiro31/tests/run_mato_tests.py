#!/usr/bin/env python3
"""Exercise mato through the actual QMK adapter and WebHID settings handler."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

KB = Path(__file__).resolve().parents[1]
ROOT = KB.parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--source-dir', type=Path, default=ROOT)
args = parser.parse_args()
source = args.source_dir.resolve()
kb = source / 'keyboards/jeebis/mejiro31'
with tempfile.TemporaryDirectory(prefix='mejiro-mato-tests-') as temporary:
    work = Path(temporary)
    (work / 'dynamic_keymap.h').write_text('#define dynamic_keymap_get_keycode keycode_at_keymap_location\n')
    (work / 'vial.h').write_text('extern int vial_unlocked;\nextern int vial_unlock_in_progress;\n')
    (work / 'via.h').write_text('bool via_eeprom_is_valid(void);\n')
    fixture = (kb / 'tests/test_qmk_adapter.c').read_text().replace('int main(void)', 'int adapter_regression_main(void)')
    fixture = fixture.replace('"../keymaps/jp_default/keymap.c"', f'"{kb.as_posix()}/keymaps/jp_mato/keymap.c"')
    (work / 'adapter_fixture.c').write_text(fixture)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-g',
             '-DMJI_WEB_VIAL', '-DDYNAMIC_KEYMAP_LAYER_COUNT=16', '-DQMK_KEYBOARD_H="qmk_stub.h"']
    for include in [work, kb / 'tests/qmk_stubs', kb, kb / 'keymaps/vial', source / 'quantum',
                    source / 'platforms', source / 'quantum/sequencer', source / 'quantum/keymap_extras']:
        flags += ['-I', str(include)]
    binary = work / ('adapter.exe' if os.name == 'nt' else 'adapter')
    subprocess.run([os.environ.get('CC', 'cc'), *flags, str(kb / 'mejiro_input.c'),
                    str(kb / 'mejiro_layout.c'), str(kb / 'steno/plover.c'),
                    str(source / 'quantum/plover_hid.c'), str(KB / 'tests/test_mato_adapter.c'),
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

