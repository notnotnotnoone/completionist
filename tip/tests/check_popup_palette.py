"""Quiet source parity check for the native Evergreen palette."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CSS = ROOT / "docs/roadmap/theme.css"
HEADER = ROOT / "tip/renderer/popup_palette.h"

FIELDS = [
    "paper", "paper-2", "surface", "ink", "muted", "ghost", "line", "line-strong",
    "sign", "sign-deep", "sign-ink", "sign-dim", "hw-engine", "hw-tsf", "hw-tooling",
    "hw-browser", "accent", "accent-ink", "accent-soft", "on-accent", "ok", "ok-soft",
    "warn", "warn-soft", "bad", "bad-soft", "todo", "todo-soft", "vms", "vms-ink",
]


def css_values() -> tuple[dict[str, str], dict[str, str]]:
    source = CSS.read_text(encoding="utf-8")
    values = re.findall(r"--([a-z0-9-]+):\s*(#[0-9a-fA-F]{6})", source)
    light: dict[str, str] = {}
    dark: dict[str, str] = {}
    for role, color in values:
        color = color.upper()
        if role not in light:
            light[role] = color
        dark[role] = color
    return light, dark


def cpp_theme(source: str, name: str) -> list[str]:
    match = re.search(rf"k{name}\s*\{{(.*?)\}};", source, re.S)
    if not match:
        raise ValueError(f"missing k{name} theme")
    return ["#" + "".join(f"{int(component, 16):02X}" for component in rgb)
            for rgb in re.findall(r"\{(0x[0-9A-Fa-f]{2}),(0x[0-9A-Fa-f]{2}),(0x[0-9A-Fa-f]{2})\}", match.group(1))]


def main() -> int:
    light_css, dark_css = css_values()
    source = HEADER.read_text(encoding="utf-8")
    actual = {"light": cpp_theme(source, "Light"), "dark": cpp_theme(source, "Dark")}
    expected = {"light": light_css, "dark": dark_css}
    errors = []
    for mode, values in expected.items():
        if len(actual[mode]) != len(FIELDS):
            errors.append(f"{mode}: expected {len(FIELDS)} native roles, found {len(actual[mode])}")
            continue
        for role, native in zip(FIELDS, actual[mode], strict=True):
            canonical = values.get(role)
            if native != canonical:
                errors.append(f"{mode} {role}: native={native}, theme.css={canonical}")
    if errors:
        print("\n".join(errors))
        return 1
    print("Evergreen popup palette matches theme.css (30 roles, light/dark).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
