#!/usr/bin/env python3
"""
DarkFox HomeBrew Packer / Builder

Baut verschluesselte .hb Dateien aus dem Ordner homebrew/src/

Einzeldatei:
  python tools/pack_homebrew.py "Titel" --text "Hallo" out.hb
  python tools/pack_homebrew.py "Titel" input.bin out.hb

Alle Sources bauen:
  python tools/pack_homebrew.py --build

Format HB01 + Stream-XOR Schluessel DarkFoxHB-2026
"""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = ROOT / "homebrew" / "src"
OUT_DIR = ROOT / "homebrew" / "out"

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
            "payload": "payload.txt",
        }
    with open(info_path, "r", encoding="utf-8") as f:
        return json.load(f)


def build_one(folder: Path) -> Path:
    info = load_info(folder)
    title = str(info.get("title", folder.name))
    author = str(info.get("author", ""))
    version = str(info.get("version", "1.0"))
    description = str(info.get("description", ""))
    payload_name = str(info.get("payload", "payload.txt"))

    payload_path = folder / payload_name
    if not payload_path.is_file():
        raise FileNotFoundError(f"Payload fehlt: {payload_path}")

    raw = payload_path.read_bytes()

    # Meta-Header im Klartext-Payload (vor Verschluesselung der ganzen Datei)
    meta = (
        f"# HomeBrew\n"
        f"title={title}\n"
        f"author={author}\n"
        f"version={version}\n"
        f"description={description}\n"
        f"---\n"
    ).encode("utf-8")
    payload = meta + raw

    out_name = str(info.get("output", folder.name + ".hb"))
    if not out_name.lower().endswith(".hb"):
        out_name += ".hb"

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out_path = OUT_DIR / out_name
    data = pack(title, payload)
    out_path.write_bytes(data)

    print(f"OK  {folder.name}")
    print(f"    Titel : {title}")
    print(f"    Author: {author}")
    print(f"    Ver   : {version}")
    print(f"    Out   : {out_path.relative_to(ROOT)} ({len(data)} bytes)")
    return out_path


def build_all() -> None:
    if not SRC_DIR.is_dir():
        print(f"Kein Source-Ordner: {SRC_DIR}")
        sys.exit(1)

    folders = sorted([p for p in SRC_DIR.iterdir() if p.is_dir()])
    if not folders:
        print(f"Keine Projekte in {SRC_DIR}")
        sys.exit(1)

    print(f"Baue {len(folders)} HomeBrew Projekt(e)...\n")
    built = []
    for folder in folders:
        try:
            built.append(build_one(folder))
        except Exception as e:
            print(f"FEHLER {folder.name}: {e}")

    print(f"\nFertig: {len(built)} Datei(en) in homebrew/out/")
    print("Kopiere die .hb Dateien auf die SD (/, /homebrew/ oder /hb/)")


def pack_cli(title: str, payload: bytes, out_path: Path) -> None:
    data = pack(title, payload)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(data)
    print(f"OK: {out_path} ({len(data)} bytes)")


def main() -> None:
    if len(sys.argv) >= 2 and sys.argv[1] == "--build":
        build_all()
        return

    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)

    title = sys.argv[1]
    if sys.argv[2] == "--text":
        if len(sys.argv) < 5:
            print("Usage: pack_homebrew.py TITLE --text \"text\" out.hb")
            sys.exit(1)
        payload = sys.argv[3].encode("utf-8")
        out_path = Path(sys.argv[4])
    else:
        payload = Path(sys.argv[2]).read_bytes()
        out_path = Path(sys.argv[3])

    pack_cli(title, payload, out_path)


if __name__ == "__main__":
    main()
