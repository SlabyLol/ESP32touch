#!/usr/bin/env python3
"""
DarkFox HomeBrew Packer

Quellen NUR: .py  .cpp  .cs
Ausgabe:     .hb  (verschluesselt)

  python tools/pack_homebrew.py --build
  python tools/pack_homebrew.py "Titel" main.py out.hb
"""

from __future__ import annotations

import json
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
            raise ValueError(
                f"Nur {sorted(ALLOWED_EXT)} erlaubt, nicht {p.suffix}: {p.name}"
            )
        return p

    found = []
    for ext in ALLOWED_EXT:
        found.extend(folder.glob(f"*{ext}"))
        found.extend(folder.glob(f"*{ext.upper()}"))
    # dedupe
    uniq = []
    seen = set()
    for f in found:
        key = f.name.lower()
        if key not in seen:
            seen.add(key)
            uniq.append(f)
    if not uniq:
        raise FileNotFoundError(
            f"Keine Quelldatei in {folder} (.py / .cpp / .cs)"
        )
    if len(uniq) > 1:
        raise ValueError(
            f"Mehrere Quellen in {folder}: {[f.name for f in uniq]} – eine in info.json payload setzen"
        )
    return uniq[0]


def build_one(folder: Path) -> Path:
    info = load_info(folder)
    title = str(info.get("title", folder.name))
    author = str(info.get("author", ""))
    version = str(info.get("version", "1.0"))
    description = str(info.get("description", ""))

    payload_path = find_payload(folder, info)
    raw = payload_path.read_bytes()
    lang = payload_path.suffix.lower().lstrip(".")

    meta = (
        f"# HomeBrew\n"
        f"title={title}\n"
        f"author={author}\n"
        f"version={version}\n"
        f"description={description}\n"
        f"lang={lang}\n"
        f"---\n"
    ).encode("utf-8")
    payload = meta + raw

    out_name = str(info.get("output", folder.name + ".hb"))
    if not out_name.lower().endswith(".hb"):
        out_name += ".hb"

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out_path = OUT_DIR / out_name
    out_path.write_bytes(pack(title, payload))

    print(f"OK  {folder.name}")
    print(f"    Quelle: {payload_path.name} ({lang})")
    print(f"    Titel : {title}")
    print(f"    Out   : {out_path.relative_to(ROOT)} ({out_path.stat().st_size} bytes)")
    return out_path


def build_all() -> None:
    if not SRC_DIR.is_dir():
        print(f"Kein Source-Ordner: {SRC_DIR}")
        sys.exit(1)
    folders = sorted([p for p in SRC_DIR.iterdir() if p.is_dir()])
    if not folders:
        print(f"Keine Projekte in {SRC_DIR}")
        sys.exit(1)
    print(f"Baue {len(folders)} HomeBrew Projekt(e) [.py|.cpp|.cs -> .hb]...\n")
    built = []
    for folder in folders:
        try:
            built.append(build_one(folder))
        except Exception as e:
            print(f"FEHLER {folder.name}: {e}")
            sys.exit(1)
    print(f"\nFertig: {len(built)} .hb in homebrew/out/")


def main() -> None:
    if len(sys.argv) >= 2 and sys.argv[1] == "--build":
        build_all()
        return

    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)

    title = sys.argv[1]
    src = Path(sys.argv[2])
    out_path = Path(sys.argv[3])

    if not src.is_file():
        print(f"Datei fehlt: {src}")
        sys.exit(1)
    if src.suffix.lower() not in ALLOWED_EXT:
        print(f"Nur {sorted(ALLOWED_EXT)} erlaubt, nicht {src.suffix}")
        sys.exit(1)

    data = pack(title, src.read_bytes())
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(data)
    print(f"OK: {src.name} -> {out_path} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
