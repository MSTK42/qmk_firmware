#!/usr/bin/env python3
"""Check mato build provenance and the editor defaults shipped with its UF2."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('build_vial', Path(__file__).resolve().parents[1] / 'build_vial.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class MatoBuildTests(unittest.TestCase):
    def test_linked_storage_exports_every_tap_hold_and_extra_position(self):
        # Distinct sentinels expose wrong row order, Tap/Hold banks and thumb offsets.
        storage = [[layer * 100 + position for position in range(48)] for layer in range(16)]
        raw = struct.pack('<768H', *(value for row in storage for value in row))
        exported = builder.expand_default_storage(raw)
        self.assertEqual(len(exported), 12)
        for layer, row in enumerate(exported):
            self.assertEqual(len(row), 58)
            self.assertEqual(row[:48], storage[layer])
            for logical in range(48, 58):
                bank = (12 if logical < 54 else 14) + int(layer >= 6)
                stride = 6 if logical < 54 else 4
                offset = logical - (48 if logical < 54 else 54)
                self.assertEqual(row[logical], storage[bank][(layer % 6) * stride + offset])
        for size in (0, 12 * 48 * 2, 16 * 48 * 2 - 1, 16 * 48 * 2 + 2):
            with self.assertRaises(ValueError):
                builder.expand_default_storage(bytes(size))

    def test_source_fingerprint_tracks_contents_and_paths_but_not_build_caches(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'keymap.c'
            source.write_text('first', encoding='utf-8')
            first = builder.keyboard_source_fingerprint(root)
            for directory in ('target/release', '__pycache__'):
                cache = root / directory
                cache.mkdir(parents=True)
                (cache / 'cache.bin').write_bytes(b'build output')
            (root / 'previous.uf2').write_bytes(b'firmware')
            self.assertEqual(first, builder.keyboard_source_fingerprint(root))
            source.write_text('uncommitted edit', encoding='utf-8')
            second = builder.keyboard_source_fingerprint(root)
            self.assertNotEqual(first, second)
            source.rename(root / 'other.c')
            self.assertNotEqual(second, builder.keyboard_source_fingerprint(root))

    def test_mato_uf2_metadata_identifies_profile_and_exact_default_export(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            kb = root / 'keyboard'
            (kb / 'keymaps/jp_mato').mkdir(parents=True)
            (kb / 'keymaps/jp_mato/keymap.c').touch()
            vial = root / 'work/vial'
            (vial / '.build').mkdir(parents=True)
            sources = {'engine': 'a' * 40, 'vial': 'b' * 40, 'keyboard_head': 'c' * 40,
                       'keyboard_dirty': False, 'keyboard_source_sha256': 'd' * 64}
            (vial / '.mejiro31-vial-build.json').write_text(json.dumps(sources))
            block = bytearray(512)
            struct.pack_into('<8I', block, 0, 0x0a324655, 0x9e5d5157, 0x2000,
                             0x10000000, 256, 0, 1, 0xe48bff56)
            block[32:40] = b'M31WEB01'
            struct.pack_into('<I', block, 508, 0x0ab16f30)
            input_uf2 = vial / '.build/jeebis_mejiro31_jp_mato.uf2'
            input_uf2.write_bytes(block)
            argv = ['build_vial.py', '--work-dir', str(root / 'work'), '--keymap', 'jp_mato']

            # Never publish a UF2 if its linked defaults cannot be checked.
            with patch.object(builder, 'ROOT', root), patch.object(builder, 'KB', kb), \
                 patch.object(builder, 'prepare', return_value=vial), patch.object(builder, 'run'), \
                 patch.object(builder, 'export_mato_defaults', side_effect=ValueError('bad ELF defaults')), \
                 patch('sys.argv', argv):
                with self.assertRaisesRegex(ValueError, 'bad ELF defaults'):
                    builder.main()
            self.assertFalse((root / '.build/mejiro31_jp_mato.uf2').exists())
            self.assertFalse((root / 'mejiro31_jp_mato.uf2').exists())

            payload = b'{"profile":"mato","defaults":[]}\n'
            def export(elf, output, actual_sources):
                self.assertEqual(elf, vial / '.build/jeebis_mejiro31_jp_mato.elf')
                self.assertEqual(actual_sources, sources)
                output.write_bytes(payload)
                return hashlib.sha256(payload).hexdigest()

            with patch.object(builder, 'ROOT', root), patch.object(builder, 'KB', kb), \
                 patch.object(builder, 'prepare', return_value=vial), patch.object(builder, 'run'), \
                 patch.object(builder, 'export_mato_defaults', side_effect=export), patch('sys.argv', argv):
                builder.main()
            for uf2 in (root / '.build/mejiro31_jp_mato.uf2', root / 'mejiro31_jp_mato.uf2'):
                metadata = json.loads(uf2.with_suffix('.uf2.json').read_text())
                self.assertEqual(metadata['profile'], 'mato')
                self.assertEqual(metadata['profile_id'], 2)
                self.assertTrue(metadata['direct_keymap'])
                self.assertEqual(metadata['keymap'], 'jp_mato')
                self.assertEqual(metadata['sources'], sources)
                self.assertEqual(metadata['defaults_file'], 'mejiro31_jp_mato.defaults.json')
                self.assertEqual(metadata['defaults_sha256'], hashlib.sha256(payload).hexdigest())
                self.assertEqual(metadata['sha256'], builder.verify_uf2(uf2))
            self.assertEqual((root / '.build/mejiro31_jp_mato.defaults.json').read_bytes(), payload)


if __name__ == '__main__':
    unittest.main()
