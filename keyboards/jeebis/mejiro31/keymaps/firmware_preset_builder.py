from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable


VALID_LAYOUTS = {
    "qwerty",
    "graphite",
    "colemak",
    "colemak_dh",
    "dvorak",
    "workman",
    "handsdown_neu",
    "sturdy",
    "engram",
    "gallium",
    "canary",
    "astarte",
    "boo",
    "eucalyn",
    "eucalyn_biacco",
    "merlin",
    "o24",
    "tomisuke",
}


@dataclass(frozen=True)
class RewriteRule:
    pattern: re.Pattern[str]
    replacement_factory: Callable[[dict[str, str]], str]
    error_message: str


@dataclass(frozen=True)
class PresetBuilderConfig:
    script_dir: Path
    keyboard: str
    keymap: str
    build_stem: str
    preset_fields: tuple[str, ...]
    layout_fields: tuple[str, ...]
    description: str
    list_format: Callable[[dict[str, str]], str]
    rewrite_rules: tuple[RewriteRule, ...]
    output_subdir: str = "."

    @property
    def keymap_path(self) -> Path:
        return self.script_dir / "keymap.c"

    @property
    def presets_path(self) -> Path:
        return self.script_dir / "firmware_presets.json"

    @property
    def backup_path(self) -> Path:
        return self.script_dir / "keymap.c.preset-build.bak"


def find_repo_root(script_dir: Path) -> Path:
    for candidate in script_dir.parents:
        if (candidate / "Makefile").exists() and (candidate / "builddefs").is_dir():
            return candidate
    raise RuntimeError("QMK repository root could not be found from script location.")


def load_presets(config: PresetBuilderConfig) -> list[dict[str, str]]:
    raw = json.loads(config.presets_path.read_text(encoding="utf-8"))
    if not isinstance(raw, list):
        raise ValueError(f"{config.presets_path} must contain a JSON array.")

    presets: list[dict[str, str]] = []
    seen_names: set[str] = set()

    for index, item in enumerate(raw, start=1):
        if not isinstance(item, dict):
            raise ValueError(f"Preset #{index} must be a JSON object.")

        missing = set(config.preset_fields) - item.keys()
        if missing:
            missing_fields = ", ".join(sorted(missing))
            raise ValueError(f"Preset #{index} is missing fields: {missing_fields}")

        preset = {field: str(item[field]).strip() for field in config.preset_fields}
        firmware_name = preset["firmware_name"]
        if not firmware_name:
            raise ValueError(f"Preset #{index} has an empty firmware_name.")
        if firmware_name in seen_names:
            raise ValueError(f"Duplicate firmware_name: {firmware_name}")

        for field in config.layout_fields:
            layout = preset[field]
            if layout not in VALID_LAYOUTS:
                valid_list = ", ".join(sorted(VALID_LAYOUTS))
                raise ValueError(
                    f"Preset {firmware_name} uses unknown {field}: {layout}. "
                    f"Available layouts: {valid_list}"
                )

        seen_names.add(firmware_name)
        presets.append(preset)

    return presets


def render_keymap(
    source: str,
    preset: dict[str, str],
    rewrite_rules: tuple[RewriteRule, ...],
) -> str:
    updated = source
    for rule in rewrite_rules:
        updated, count = rule.pattern.subn(rule.replacement_factory(preset), updated, count=1)
        if count != 1:
            raise RuntimeError(rule.error_message)
    return updated


def find_built_artifact(repo_root: Path, build_stem: str, started_at: float) -> Path:
    root_candidates = [
        path
        for path in repo_root.glob(f"{build_stem}.*")
        if path.is_file() and path.stat().st_mtime >= started_at - 1
    ]
    if root_candidates:
        return max(root_candidates, key=lambda path: path.stat().st_mtime)

    build_candidates = [
        path
        for path in (repo_root / ".build").glob(f"{build_stem}.*")
        if path.is_file()
        and path.suffix not in {".elf", ".tmp"}
        and path.stat().st_mtime >= started_at - 1
    ]
    if build_candidates:
        return max(build_candidates, key=lambda path: path.stat().st_mtime)

    raise FileNotFoundError(f"Built artifact for {build_stem} was not found after compile.")


def restore_keymap(config: PresetBuilderConfig, original_text: str) -> None:
    config.keymap_path.write_text(original_text, encoding="utf-8")
    if config.backup_path.exists():
        config.backup_path.unlink()


def build_preset(
    config: PresetBuilderConfig,
    preset: dict[str, str],
    original_text: str,
    repo_root: Path,
) -> Path:
    rewritten = render_keymap(original_text, preset, config.rewrite_rules)

    shutil.copy2(config.keymap_path, config.backup_path)
    config.keymap_path.write_text(rewritten, encoding="utf-8")

    try:
        started_at = time.time()
        subprocess.run(
            ["qmk", "compile", "-kb", config.keyboard, "-km", config.keymap],
            cwd=repo_root,
            check=True,
        )
        artifact = find_built_artifact(repo_root, config.build_stem, started_at)
        destination_dir = repo_root / config.output_subdir
        destination_dir.mkdir(parents=True, exist_ok=True)
        renamed_artifact = destination_dir / f"{preset['firmware_name']}{artifact.suffix}"
        artifact.replace(renamed_artifact)
        return renamed_artifact
    finally:
        restore_keymap(config, original_text)


def parse_args(description: str) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "preset",
        nargs="?",
        help="firmware_name defined in firmware_presets.json",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Build every preset defined in firmware_presets.json",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List available presets without building",
    )
    return parser.parse_args()


def select_presets(args: argparse.Namespace, presets: list[dict[str, str]]) -> list[dict[str, str]]:
    if args.all and args.preset:
        raise ValueError("Use either a preset name or --all, not both.")
    if not args.all and not args.preset:
        raise ValueError("Specify a preset name or use --all.")

    if args.all:
        return presets

    preset_by_name = {preset["firmware_name"]: preset for preset in presets}
    if args.preset not in preset_by_name:
        available = ", ".join(sorted(preset_by_name))
        raise ValueError(f"Unknown preset: {args.preset}. Available presets: {available}")
    return [preset_by_name[args.preset]]


def run_builder(config: PresetBuilderConfig) -> int:
    args = parse_args(config.description)
    presets = load_presets(config)

    if args.list:
        for preset in presets:
            print(config.list_format(preset))
        return 0

    repo_root = find_repo_root(config.script_dir)
    original_text = config.keymap_path.read_text(encoding="utf-8")

    for preset in select_presets(args, presets):
        artifact = build_preset(config, preset, original_text, repo_root)
        print(f"Built {preset['firmware_name']} -> {artifact.relative_to(repo_root)}")

    return 0
