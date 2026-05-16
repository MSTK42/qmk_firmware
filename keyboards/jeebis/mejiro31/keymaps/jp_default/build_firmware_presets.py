#!/usr/bin/env python3

from __future__ import annotations

import sys
from pathlib import Path
import re

SCRIPT_DIR = Path(__file__).resolve().parent
SHARED_DIR = SCRIPT_DIR.parent
if str(SHARED_DIR) not in sys.path:
    sys.path.insert(0, str(SHARED_DIR))

from firmware_preset_builder import PresetBuilderConfig, RewriteRule, run_builder

ALT_EN_PATTERN = re.compile(
    r"^static alt_layout_def_t alt_en_layout = ALT_LAYOUT\([a-z0-9_]+\);.*$",
    re.MULTILINE,
)
ALT_JP_PATTERN = re.compile(
    r"^static alt_layout_def_t alt_jp_layout = ALT_LAYOUT\([a-z0-9_]+\);.*$",
    re.MULTILINE,
)

CONFIG = PresetBuilderConfig(
    script_dir=SCRIPT_DIR,
    keyboard="jeebis/mejiro31",
    keymap="jp_default",
    build_stem="jeebis_mejiro31_jp_default",
    preset_fields=("firmware_name", "english_layout", "japanese_layout"),
    layout_fields=("english_layout", "japanese_layout"),
    description=(
        "Build firmware presets for jeebis/mejiro31:jp_default by temporarily "
        "rewriting alt_en_layout / alt_jp_layout in keymap.c."
    ),
    list_format=lambda preset: (
        f"{preset['firmware_name']}: "
        f"en={preset['english_layout']}, jp={preset['japanese_layout']}"
    ),
    rewrite_rules=(
        RewriteRule(
            pattern=ALT_EN_PATTERN,
            replacement_factory=lambda preset: (
                "static alt_layout_def_t alt_en_layout = "
                f"ALT_LAYOUT({preset['english_layout']});  // 英語モード時のレイアウト"
            ),
            error_message="Could not rewrite alt_en_layout in keymap.c.",
        ),
        RewriteRule(
            pattern=ALT_JP_PATTERN,
            replacement_factory=lambda preset: (
                "static alt_layout_def_t alt_jp_layout = "
                f"ALT_LAYOUT({preset['japanese_layout']});  // 日本語モード時のレイアウト"
            ),
            error_message="Could not rewrite alt_jp_layout in keymap.c.",
        ),
    ),
    output_subdir=".build/export",
)


if __name__ == "__main__":
    try:
        raise SystemExit(run_builder(CONFIG))
    except KeyboardInterrupt:
        print("Build cancelled.", file=sys.stderr)
        raise SystemExit(130)
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        raise SystemExit(1)
