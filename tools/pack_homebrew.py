#!/usr/bin/env python3
"""
DarkFox HomeBrew Packer

Echte Quell-Syntax in .py / .cpp / .cs
→ wird nach HomeBrew-Bytecode (Klartext-IR) übersetzt
→ verschlüsselt als .hb

  python tools/pack_homebrew.py --build
"""

from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = ROOT / "homebrew" / "src"
OUT_DIR = ROOT / "homebrew" / "out"

ALLOWED_EXT = {".py", ".cpp", ".cs"}

HB_KEY = b"DarkFoxHB-2026"
MAGIC = b"HB01"
VERSION = 1


def key_byte(pos: int) -> int:
    b = HB_KEY[pos % len(HB_KEY)]
    b ^= (pos * 31 + 17) & 0xFF
    b ^= (pos >> 8) & 0xFF
    return b & 0xFF


def crypt(data: bytes, offset: int = 0) -> bytes:
    out = bytearray(len(data))
    for i, c in enumerate(data):
        out[i] = c ^ key_byte(offset + i)
    return bytes(out)


def pack(title: str, payload: bytes, flags: int = 0) -> bytes:
    title_b = title.encode("utf-8")[:64]
    title_enc = crypt(title_b, 0)
    payload_enc = crypt(payload, len(title_b))
    header = MAGIC + bytes([VERSION, flags & 0xFF]) + struct.pack("<H", len(title_b))
    return header + title_enc + struct.pack("<I", len(payload)) + payload_enc


def _strip_comments_and_strings_keep_calls(src: str) -> list[str]:
    """Return non-empty code lines (rough)."""
    lines = []
    for line in src.splitlines():
        # remove // and # comments (not inside strings – simple)
        if "//" in line:
            line = line[: line.index("//")]
        if "#" in line and not line.strip().startswith("#include"):
            # python comments
            in_str = False
            out = []
            i = 0
            while i < len(line):
                c = line[i]
                if c in "'\"" and (i == 0 or line[i - 1] != "\\"):
                    in_str = not in_str
                    out.append(c)
                elif c == "#" and not in_str:
                    break
                else:
                    out.append(c)
                i += 1
            line = "".join(out)
        line = line.strip()
        if line:
            lines.append(line)
    return lines


def transpile(source: str, ext: str) -> str:
    """
    Translate real-looking API calls to HB IR lines.

    Supported API (same in py / cpp / cs):
      clear()
      color("CYAN") / color(CYAN)
      text(x, y, "msg")
      center(y, "msg")
      wait(ms)
      wait_touch()
      wifi()
      http("https://...")
    """
    ir: list[str] = []
    text = source

    # normalize: remove common wrappers
    text = re.sub(r"using\s+[^;]+;", "", text)
    text = re.sub(r"#include\s*<[^>]+>", "", text)
    text = re.sub(r"namespace\s+\w+\s*\{", "", text)
    text = re.sub(r"public\s+class\s+\w+\s*\{", "", text)
    text = re.sub(r"static\s+void\s+Main\s*\([^)]*\)\s*\{", "", text)
    text = re.sub(r"void\s+main\s*\([^)]*\)\s*\{", "", text)
    text = re.sub(r"def\s+main\s*\([^)]*\)\s*:", "", text)
    text = re.sub(r"if\s+__name__\s*==\s*['\"]__main__['\"]\s*:", "", text)
    text = re.sub(r"main\s*\(\s*\)", "", text)

    # Find API calls (allow optional HomeBrew. / hb. prefix)
    patterns = [
        (r"(?:HomeBrew\.|hb\.)?clear\s*\(\s*\)", lambda m: "CLEAR"),
        (r"(?:HomeBrew\.|hb\.)?wait_touch\s*\(\s*\)", lambda m: "WAIT_TOUCH"),
        (r"(?:HomeBrew\.|hb\.)?wifi\s*\(\s*\)", lambda m: "WIFI"),
        (
            r"(?:HomeBrew\.|hb\.)?color\s*\(\s*[\"']?([A-Za-z0-9]+)[\"']?\s*\)",
            lambda m: f"COLOR {m.group(1).upper()}",
        ),
        (
            r"(?:HomeBrew\.|hb\.)?wait\s*\(\s*(\d+)\s*\)",
            lambda m: f"WAIT {m.group(1)}",
        ),
        (
            r"(?:HomeBrew\.|hb\.)?center\s*\(\s*(\d+)\s*,\s*[\"']([^\"']*)[\"']\s*\)",
            lambda m: f"CENTER {m.group(1)} {m.group(2)}",
        ),
        (
            r"(?:HomeBrew\.|hb\.)?text\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*[\"']([^\"']*)[\"']\s*\)",
            lambda m: f"TEXT {m.group(1)} {m.group(2)} {m.group(3)}",
        ),
        (
            r"(?:HomeBrew\.|hb\.)?http\s*\(\s*[\"']([^\"']+)[\"']\s*\)",
            lambda m: f"HTTP {m.group(1)}",
        ),
    ]

    # Scan source in order for calls
    # Split by semicolons and newlines to preserve order
    chunks = re.split(r"[;\n]", text)
    for chunk in chunks:
        chunk = chunk.strip()
        if not chunk:
            continue
        for pat, fn in patterns:
            m = re.search(pat, chunk, re.IGNORECASE)
            if m:
                ir.append(fn(m))
                break

    if not ir:
        raise ValueError(
            "Keine API-Aufrufe gefunden. Erwartet z.B. clear(), color(\"CYAN\"), wifi(), http(\"https://...\")"
        )
    return "\n".join(ir) + "\n"


def load_info(folder: Path) -> dict:
    info_path = folder / "info.json"
    if not info_path.is_file():
        return {
            "title": folder.name,
            "author": "Unknown",
            "version": "1.0",
            "description": "",
            "payload": None,
        }
    with open(info_path, "r", encoding="utf-8") as f:
        return json.load(f)


def find_payload(folder: Path, info: dict) -> Path:
    named = info.get("payload")
    if named:
        p = folder / str(named)
        if not p.is_file():
            raise FileNotFoundError(f"Payload fehlt: {p}")
        if p.suffix.lower() not in ALLOWED_EXT:
            raise ValueError(f"Nur {sorted(ALLOWED_EXT)} erlaubt: {p.name}")
        return p
    found = []
    for ext in ALLOWED_EXT:
        found.extend(folder.glob(f"*{ext}"))
    uniq = []
    seen = set()
    for f in found:
        k = f.name.lower()
        if k not in seen:
            seen.add(k)
            uniq.append(f)
    if not uniq:
        raise FileNotFoundError(f"Keine .py/.cpp/.cs in {folder}")
    if len(uniq) > 1:
        raise ValueError(f"Mehrere Quellen: {[f.name for f in uniq]}")
    return uniq[0]


def build_one(folder: Path) -> Path:
    info = load_info(folder)
    title = str(info.get("title", folder.name))
    author = str(info.get("author", ""))
    version = str(info.get("version", "1.0"))
    description = str(info.get("description", ""))

    payload_path = find_payload(folder, info)
    src = payload_path.read_text(encoding="utf-8")
    lang = payload_path.suffix.lower().lstrip(".")

    ir = transpile(src, payload_path.suffix.lower())
    print(f"    IR:\n" + "\n".join(f"      {l}" for l in ir.strip().splitlines()))

    meta = (
        f"# HomeBrew\n"
        f"title={title}\n"
        f"author={author}\n"
        f"version={version}\n"
        f"description={description}\n"
        f"lang={lang}\n"
        f"---\n"
    )
    payload = (meta + ir).encode("utf-8")

    out_name = str(info.get("output", folder.name + ".hb"))
    if not out_name.lower().endswith(".hb"):
        out_name += ".hb"

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out_path = OUT_DIR / out_name
    out_path.write_bytes(pack(title, payload))

    print(f"OK  {folder.name}  [{lang}] -> {out_path.name} ({out_path.stat().st_size} bytes)")
    return out_path


def build_all() -> None:
    folders = sorted([p for p in SRC_DIR.iterdir() if p.is_dir()]) if SRC_DIR.is_dir() else []
    if not folders:
        print(f"Keine Projekte in {SRC_DIR}")
        sys.exit(1)
    print(f"Baue {len(folders)} Projekt(e)...\n")
    for folder in folders:
        try:
            build_one(folder)
        except Exception as e:
            print(f"FEHLER {folder.name}: {e}")
            sys.exit(1)
    print(f"\nFertig -> homebrew/out/")


def main() -> None:
    if len(sys.argv) >= 2 and sys.argv[1] == "--build":
        build_all()
        return
    print(__doc__)
    sys.exit(1)


if __name__ == "__main__":
    main()
