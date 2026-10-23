#!/usr/bin/env python3
"""Execute the production mato engine and mode wrapper with host hardware stubs.

Requires a C99 compiler (cc/gcc/clang or --cc PATH). The existing QMK headers
provide the real HID constants. Hardware/legacy text input are not emulated;
the unchanged legacy edge-toggle functions are extracted and executed verbatim.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
KEYMAP = ROOT / 'keyboards/jeebis/mejiro31/keymaps/jp_mato'


def function(source, name):
    start = source.index('static ', source.rfind('\n', 0, source.index(name + '(')) + 1)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('MATO_CC'))
    args = parser.parse_args()
    compiler = args.cc or next((shutil.which(c) for c in ('cc', 'gcc', 'clang') if shutil.which(c)), None)
    if not compiler:
        parser.error('C99 compiler required; set MATO_CC or pass --cc PATH')
    subprocess.run([sys.executable, str(KEYMAP / 'generate_tables.py'), '--check'], check=True)
    legacy = (KEYMAP.parent / 'jp_default/keymap.c').read_text(encoding='utf-8')
    wrapper = (KEYMAP / 'keymap.c').read_text(encoding='utf-8')
    assert '#include "../jp_default/keymap.c"' in wrapper
    build_parent = ROOT / '.build'
    build_parent.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mejiro-mato-test-', dir=build_parent) as temporary:
        build = Path(temporary)
        # TinyCC on Windows sometimes treats the same #pragma-once header as
        # distinct through different include paths. Guards wrap unchanged text.
        for path in (ROOT/'quantum/keycodes.h', ROOT/'quantum/quantum_keycodes.h',
                     ROOT/'quantum/keymap_extras/keymap_us.h', ROOT/'quantum/keymap_extras/keymap_japanese.h',
                     ROOT/'quantum/sequencer/sequencer.h', ROOT/'quantum/quantum_keycodes_legacy.h'):
            guard = 'MATO_HOST_' + path.name.upper().replace('.', '_')
            (build/path.name).write_text('#ifndef '+guard+'\n#define '+guard+'\n'+path.read_text(encoding='utf-8')+'\n#endif\n', encoding='utf-8')
        names = ('reset_edge_toggle_state', 'edge_toggle_has_edge_down', 'toggle_keyboard_mejiro_layer',
                 'is_o24_edge_toggle_key', 'update_o24_edge_toggle_other', 'process_o24_edge_toggle_key',
                 'process_steno_edge_toggle_key')
        (build/'legacy_edges.inc').write_text('\n'.join(function(legacy, name) for name in names), encoding='utf-8')
        (build/'mato_wrapper.inc').write_text(wrapper[wrapper.index('static mato_engine_t mato;'):], encoding='utf-8')
        tests = sorted((KEYMAP/'tests').glob('*.c'))
        assert len(tests) >= 2, 'Both independent engine review and wrapper tests are required'
        for test in tests:
            binary = build/(test.stem + ('.exe' if os.name == 'nt' else ''))
            command = [compiler, '-std=c99', '-Wall', '-Wextra', '-I'+str(build), '-I'+str(KEYMAP),
                       '-I'+str(ROOT/'quantum'), '-I'+str(ROOT/'quantum/keymap_extras'),
                       str(KEYMAP/'mato_engine.c'), str(KEYMAP/'mato_tables.c'), str(test), '-o', str(binary)]
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True)
    print('PASS: source-table parity, production engine, legacy edge toggles and mode wrapper')


if __name__ == '__main__':
    main()
