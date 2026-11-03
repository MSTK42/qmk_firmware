#!/usr/bin/env python3
"""Build site-compatible Mejiro31 UF2s from this checkout using pinned Vial core."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

KB = Path(__file__).resolve().parent
ROOT = KB.parents[2]
VIAL_URL = 'https://github.com/vial-kb/vial-qmk.git'
VIAL_REF = 'dd43959ae5c08d8a28d38a1acf7b04e86b14a344'
ENGINE_URL = 'https://github.com/JEEBIS27/Mejiro_Engine.git'
# Pin the runtime dictionary ABI and latest shared engine behavior.
ENGINE_REF = 'b7a52cfe88be66691d2bb35d6f672084e02a50c5'


def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)


def checkout(path, url, ref):
    if path.exists() and not (path / '.git').exists():
        raise ValueError(f'Refusing to use a non-Git cache directory: {path}')
    if not path.exists():
        run('git', 'init', str(path))
        run('git', 'remote', 'add', 'origin', url, cwd=path)
    current = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=path, capture_output=True, text=True)
    if ref == 'HEAD' or current.returncode or current.stdout.strip() != ref:
        run('git', 'fetch', '--depth', '1', url, ref, cwd=path)
        run('git', 'checkout', '--detach', 'FETCH_HEAD', cwd=path)
    return subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=path, text=True).strip()


def write_build_metadata(uf2, sources, variant, digest):
    metadata = {'format': 'mejiro31-build-v1', 'filename': uf2.name,
                'sha256': digest, 'sources': sources, **variant}
    uf2.with_suffix('.uf2.json').write_text(json.dumps(metadata, indent=2) + '\n')


def keyboard_source_fingerprint(keyboard):
    """Hash exactly the source tree copied into Vial, including local edits."""
    digest = hashlib.sha256()
    for path in sorted(Path(keyboard).rglob('*')):
        relative = path.relative_to(keyboard)
        if (not path.is_file() or any(part in ('target', '__pycache__') for part in relative.parts)
                or path.name.endswith('.uf2')):
            continue
        digest.update(relative.as_posix().encode() + b'\0' + path.read_bytes() + b'\0')
    return digest.hexdigest()


def expand_default_storage(raw):
    """Decode the linked 16x48 storage table into the editor's 12x58 maps."""
    if len(raw) != 16 * 48 * 2:
        raise ValueError('Expected sixteen 4x12 uint16_t default storage maps')
    values = struct.unpack('<768H', raw)
    storage = [list(values[i:i + 48]) for i in range(0, len(values), 48)]
    result = []
    for layer in range(12):
        hold = int(layer >= 6)
        logical_layer = layer % 6
        extra = storage[12 + hold][logical_layer * 6:logical_layer * 6 + 6]
        thumbs = storage[14 + hold][logical_layer * 4:logical_layer * 4 + 4]
        result.append(storage[layer] + extra + thumbs)
    return result


def export_mato_defaults(elf, output, sources):
    """Export defaults from the same linked ELF as the delivered firmware."""
    module_spec = importlib.util.spec_from_file_location('mejiro_usb_elf', KB / 'tests/test_plover_usb.py')
    module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(module)
    _, read_symbol = module.elf_symbols(Path(elf).read_bytes())
    defaults = expand_default_storage(read_symbol('keymaps'))
    data = {'format': 'mejiro31-mato-defaults-v1', 'profile': 'mato', 'profileId': 2,
            'directKeymap': True, 'keyCount': 58, 'storageLayers': 16,
            'rows': 4, 'cols': 12,
            'layers': ['mato', 'mato MO1', 'Numbers / symbols', 'Navigation', 'Function', 'Steno'],
            'defaults': defaults, 'sources': sources}
    raw = (json.dumps(data, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
    Path(output).write_bytes(raw)
    return hashlib.sha256(raw).hexdigest()


def verify_uf2(path):
    raw = path.read_bytes()
    if not raw or len(raw) % 512:
        raise ValueError('Invalid UF2 length')
    addresses = set()
    payload = bytearray()
    for index in range(len(raw) // 512):
        block = raw[index * 512:(index + 1) * 512]
        magic1, magic2, flags, address, size, number, total, family = struct.unpack_from('<8I', block)
        if (magic1, magic2, struct.unpack_from('<I', block, 508)[0]) != (0x0a324655, 0x9e5d5157, 0x0ab16f30):
            raise ValueError('Invalid UF2 magic')
        if flags != 0x2000 or family != 0xe48bff56 or size != 256 or number != index or total != len(raw) // 512:
            raise ValueError('Invalid RP2040 UF2 metadata')
        if address % 256 or address < 0x10000000 or address + size > 0x10200000 - 73728 or address in addresses:
            raise ValueError('Invalid UF2 flash address')
        addresses.add(address)
        payload.extend(block[32:32 + size])
    if b'M31WEB01' not in payload:
        raise ValueError('The site protocol is missing')
    return hashlib.sha256(raw).hexdigest()


def prepare(work, submodules, engine_source=None):
    vial, engine = work / 'vial', work / 'Mejiro_Engine'
    if engine_source is None:
        engine_revision = checkout(engine, ENGINE_URL, ENGINE_REF)
        engine_metadata = {'engine': engine_revision, 'engine_url': ENGINE_URL,
                           'engine_ref': ENGINE_REF}
    else:
        engine = Path(engine_source).resolve()
        for relative in ['Cargo.toml', 'Cargo.lock', 'native/mejiro.h', 'src/lib.rs']:
            if not (engine / relative).is_file():
                raise ValueError(f'Missing Engine source: {engine / relative}')
        engine_revision = subprocess.check_output(
            ['git', '-C', str(engine), 'rev-parse', 'HEAD'], text=True).strip()
        dirty = bool(subprocess.check_output(
            ['git', '-C', str(engine), 'status', '--porcelain'], text=True).strip())
        digest = hashlib.sha256()
        paths = [engine / name for name in ['Cargo.toml', 'Cargo.lock', 'native/Cargo.toml', 'native/mejiro.h']]
        paths += sorted((engine / 'src').rglob('*.rs'))
        paths += sorted((engine / 'native/src').rglob('*.rs'))
        for path in paths:
            digest.update(str(path.relative_to(engine)).encode() + b'\0' + path.read_bytes() + b'\0')
        engine_metadata = {'engine': engine_revision, 'engine_source': str(engine),
                           'engine_ref': 'local', 'engine_dirty': dirty,
                           'engine_source_sha256': digest.hexdigest()}
    checkout(vial, VIAL_URL, VIAL_REF)
    print(f'Mejiro_Engine: {engine_metadata}', flush=True)
    dest = vial / 'keyboards/jeebis/mejiro31'
    marker = vial / '.mejiro31-vial-build.json'
    if dest.exists():
        if not marker.exists():
            raise ValueError('Refusing to replace a keyboard in an unmanaged Vial tree')
        shutil.rmtree(dest)
    shutil.copytree(KB, dest, ignore=shutil.ignore_patterns('target', '__pycache__', '*.uf2'))
    for name in ['plover-hid.patch', 'usb-queue-alignment.patch']:
        # Windows checkouts may use CRLF even when Vial's working tree uses LF.
        patch = (KB / 'keymaps/vial' / name).read_bytes().replace(b'\r\n', b'\n')
        reverse = subprocess.run(['git', 'apply', '--reverse', '--check', '-'], cwd=vial, input=patch, capture_output=True)
        if reverse.returncode:
            subprocess.run(['git', 'apply', '--check', '-'], cwd=vial, input=patch, check=True)
            subprocess.run(['git', 'apply', '-'], cwd=vial, input=patch, check=True)
    # Allow cancellation during unlock and clear the polling barrier on lock.
    core = vial / 'quantum/via.c'
    text = core.read_text().replace('cmd != vial_unlock_start && cmd != vial_unlock_poll)', 'cmd != vial_unlock_start && cmd != vial_unlock_poll && cmd != vial_lock)')
    core.write_text(text)
    core = vial / 'quantum/vial.c'
    text = core.read_text()
    if 'case vial_lock: {\n            vial_unlock_in_progress = 0;' not in text:
        text = text.replace('case vial_lock: {', 'case vial_lock: {\n            vial_unlock_in_progress = 0;')
    core.write_text(text)
    # Named keymaps share the keyboard's Vial definition.
    build_vial = vial / 'builddefs/build_vial.mk'
    text = build_vial.read_text().replace('$(KEYMAP_PATH)/vial.json', '$(VIAL_DEFINITION)')
    build_vial.write_text(text)
    cargo = dest / 'steno/rust/Cargo.toml' 
    cargo.write_text(cargo.read_text().replace('../../../../../../Mejiro_Engine', str(engine)))
    for path in [dest / 'steno/engine.c', dest / 'steno/engine.h']:
        path.write_text(path.read_text().replace('../../../../../Mejiro_Engine/native/mejiro.h', str(engine / 'native/mejiro.h')))
    rules = dest / 'rules.mk'
    rules.write_text(rules.read_text().replace('$(abspath $(KEYBOARD_PATH_1)/../../../../Mejiro_Engine)', str(engine)))
    if submodules:
        run('git', 'submodule', 'update', '--init', '--depth', '1', 'lib/chibios', 'lib/chibios-contrib', 'lib/pico-sdk', 'lib/printf', cwd=vial)
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    keyboard_dirty = bool(subprocess.check_output(
        ['git', 'status', '--porcelain', '--', str(KB.relative_to(ROOT))], cwd=ROOT, text=True).strip())
    marker.write_text(json.dumps({'vial': VIAL_REF, **engine_metadata,
                                 'keyboard_head': revision, 'keyboard_dirty': keyboard_dirty,
                                 'keyboard_source_sha256': keyboard_source_fingerprint(KB)}, indent=2) + '\n')
    return vial


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--work-dir', type=Path, default=Path(os.environ.get('MJI_VIAL_WORK_DIR', str(ROOT / '.build/mejiro31-vial'))))
    p.add_argument('--engine-source', type=Path, help='Use a local Engine working tree, including uncommitted changes; default: remote HEAD')
    p.add_argument('--keymap', help='Existing Mejiro31 keymap name; uses its original defaults')
    p.add_argument('--action', choices=['build','flash','clean'], default='build')
    p.add_argument('--profile', choices=['jp', 'en', 'all'], default='all')
    p.add_argument('--layout', choices=['qwerty', 'graphite'], default='qwerty')
    p.add_argument('--jobs', type=int, default=4)
    p.add_argument('--prepare-only', action='store_true')
    p.add_argument('--skip-submodules', action='store_true', help='Only for host tests or an already initialized cache')
    args = p.parse_args()
    if args.keymap and (not args.keymap.replace('_','').replace('-','').isalnum() or not (KB / 'keymaps' / args.keymap / 'keymap.c').is_file()):
        p.error('Unknown Mejiro31 keymap')
    work = args.work_dir.resolve()
    if work == ROOT or work.is_relative_to(KB):
        p.error('The build cache must be outside the keyboard sources')
    work.mkdir(parents=True, exist_ok=True)
    vial = (prepare(work, not args.skip_submodules, args.engine_source)
            if args.engine_source is not None else prepare(work, not args.skip_submodules))
    print(f'Prepared Vial source: {vial}', flush=True)
    if args.prepare_only:
        return
    output = ROOT / '.build'
    output.mkdir(exist_ok=True)
    sources = json.loads((vial / '.mejiro31-vial-build.json').read_text())
    variants = [(None, args.keymap)] if args.keymap else [(profile, 'vial') for profile in (['jp', 'en'] if args.profile == 'all' else [args.profile])]
    for profile, keymap in variants:
        shutil.rmtree(vial / f'.build/obj_jeebis_mejiro31_{keymap}', ignore_errors=True)
        target = f'jeebis/mejiro31:{keymap}' + (f':{args.action}' if args.action != 'build' else '')
        options = [] if args.keymap else [f'MEJIRO_PROFILE={int(profile == "en")}', f'MEJIRO_LAYOUT={int(args.layout == "graphite")}']
        run('make', target, f'-j{args.jobs}', *options, cwd=vial)
        if args.action == 'clean':
            continue
        # Validate the linked Vial core before publishing any firmware copies.
        run(sys.executable, str(KB / 'tests/test_plover_usb.py'),
            str(vial / f'.build/jeebis_mejiro31_{keymap}.elf'))
        language = profile or {'jp_default': 'jp', 'en_default': 'en'}.get(keymap)
        filename = f'mejiro31_{language}.uf2' if language else f'mejiro31_{keymap}.uf2'
        uf2 = output / filename
        variant = {'keymap': keymap}
        if keymap == 'jp_mato':
            defaults_path = output / 'mejiro31_jp_mato.defaults.json'
            defaults_sha256 = export_mato_defaults(
                vial / f'.build/jeebis_mejiro31_{keymap}.elf', defaults_path, sources)
            variant.update(profile='mato', profile_id=2, direct_keymap=True,
                           defaults_file=defaults_path.name, defaults_sha256=defaults_sha256)
        elif profile is not None:
            variant.update(profile=profile, layout=args.layout)
        shutil.copyfile(vial / f'.build/jeebis_mejiro31_{keymap}.uf2', uf2)
        digest = verify_uf2(uf2)
        uf2.with_suffix('.uf2.sha256').write_text(f'{digest}  {uf2.name}\n')
        write_build_metadata(uf2, sources, variant, digest)
        print(f'Verified site-compatible UF2: {uf2}', flush=True)
        if args.keymap:
            named_uf2 = ROOT / filename
            shutil.copyfile(uf2, named_uf2)
            write_build_metadata(named_uf2, sources, variant, digest)
        # Only the verified, compact names are public firmware outputs.
        for intermediate in [vial / f'.build/jeebis_mejiro31_{keymap}.uf2',
                             vial / f'jeebis_mejiro31_{keymap}.uf2']:
            intermediate.unlink(missing_ok=True)


def run_cli():
    # Keep an explicitly installed toolchain. Nix users can build without
    # changing their login-shell PATH or relying on a machine-specific store path.
    arguments = sys.argv[1:]
    if '--prepare-only' in arguments or '--help' in arguments or '-h' in arguments:
        return main()
    if shutil.which('arm-none-eabi-gcc'):
        return main()
    nix = shutil.which('nix')
    if not nix:
        raise SystemExit('arm-none-eabi-gcc was not found. Install the Arm GNU embedded toolchain, or install Nix to supply it automatically.')
    if os.environ.get('MEJIRO_NIX_TOOLCHAIN') == '1':
        raise SystemExit('The Nix build environment does not provide arm-none-eabi-gcc.')
    print('arm-none-eabi-gcc is missing; entering the project QMK Nix development environment.', flush=True)
    environment = dict(os.environ, MEJIRO_NIX_TOOLCHAIN='1')
    result = subprocess.run([
        nix, '--extra-experimental-features', 'nix-command flakes',
        'develop', f'path:{ROOT / "nix"}', '--command',
        'python3', str(Path(__file__).resolve()), *arguments,
    ], env=environment)
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    run_cli()
