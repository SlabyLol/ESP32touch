#!/usr/bin/env python3
"""DarkFox HomeBrew Packer - echte .py/.cpp/.cs API -> .hb"""

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


def transpile(source: str, ext: str) -> str:
    text = source
    # strip language noise (no fragile nested quotes in regex)
    text = re.sub(r"using\s+[^;]+;", "", text)
    text = re.sub(r"#include\s+\S+", "", text)
    text = re.sub(r"namespace\s+\w+\s*\{", "", text)
    text = re.sub(r"public\s+class\s+\w+\s*\{", "", text)
    text = re.sub(r"static\s+void\s+Main\s*\([^)]*\)\s*\{", "", text)
    text = re.sub(r"void\s+main\s*\([^)]*\)\s*\{", "", text)
    text = re.sub(r"def\s+main\s*\([^)]*\)\s*:", "", text)
    text = re.sub(r'if\s+__name__\s*==\s*["\']__main__["\']\s*:', "", text)
    text = re.sub(r"main\s*\(\s*\)", "", text)

    patterns = [
        (r"(?:HomeBrew\.|hb\.)?clear\s*\(\s*\)", lambda m: "CLEAR"),
        (r"(?:HomeBrew\.|hb\.)?wait_touch\s*\(\s*\)", lambda m: "WAIT_TOUCH"),
        (r"(?:HomeBrew\.|hb\.)?show\s*\(\s*\)", lambda m: "GUI_RUN"),
        (r"(?:HomeBrew\.|hb\.)?gui_run\s*\(\s*\)", lambda m: "GUI_RUN"),
        (r"(?:HomeBrew\.|hb\.)?wifi\s*\(\s*\)", lambda m: "WIFI"),
        (r"(?:HomeBrew\.|hb\.)?color\s*\(\s*[\"']?([A-Za-z0-9]+)[\"']?\s*\)",
         lambda m: "COLOR " + m.group(1).upper()),
        (r"(?:HomeBrew\.|hb\.)?bg\s*\(\s*[\"']?([A-Za-z0-9]+)[\"']?\s*\)",
         lambda m: "BG " + m.group(1).upper()),
        (r"(?:HomeBrew\.|hb\.)?wait\s*\(\s*(\d+)\s*\)",
         lambda m: "WAIT " + m.group(1)),
        (r"(?:HomeBrew\.|hb\.)?center\s*\(\s*(\d+)\s*,\s*[\"']([^\"']*)[\"']\s*\)",
         lambda m: "CENTER " + m.group(1) + " " + m.group(2)),
        (r"(?:HomeBrew\.|hb\.)?label\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*[\"']([^\"']*)[\"']\s*\)",
         lambda m: "LABEL " + m.group(1) + " " + m.group(2) + " " + m.group(3)),
        (r"(?:HomeBrew\.|hb\.)?text\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*[\"']([^\"']*)[\"']\s*\)",
         lambda m: "TEXT " + m.group(1) + " " + m.group(2) + " " + m.group(3)),
        (r"(?:HomeBrew\.|hb\.)?http\s*\(\s*[\"']([^\"']+)[\"']\s*\)",
         lambda m: "HTTP " + m.group(1)),
        (r"(?:HomeBrew\.|hb\.)?box\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)",
         lambda m: "BOX " + m.group(1) + " " + m.group(2) + " " + m.group(3) + " " + m.group(4)),
        (r"(?:HomeBrew\.|hb\.)?fillbox\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)",
         lambda m: "FILLBOX " + m.group(1) + " " + m.group(2) + " " + m.group(3) + " " + m.group(4)),
        (r"(?:HomeBrew\.|hb\.)?button\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*[\"']([^\"']*)[\"']\s*\)",
         lambda m: "BUTTON " + m.group(1) + " " + m.group(2) + " " + m.group(3) + " " + m.group(4) + " " + m.group(5) + " " + m.group(6)),
    ]

    ir = []
    for chunk in re.split(r"[;\n]", text):
        chunk = chunk.strip()
        if not chunk:
            continue
        for pat, fn in patterns:
            m = re.search(pat, chunk, re.IGNORECASE)
            if m:
                ir.append(fn(m))
                break

    if not ir:
        raise ValueError("Keine API-Aufrufe gefunden")
    return "\n".join(ir) + "\n"


def load_info(folder: Path) -> dict:
    p = folder / "info.json"
    if not p.is_file():
        return {"title": folder.name, "author": "", "version": "1.0", "description": "", "payload": None}
    return json.loads(p.read_text(encoding="utf-8"))


def find_payload(folder: Path, info: dict) -> Path:
    named = info.get("payload")
    if named:
        p = folder / str(named)
        if not p.is_file():
            raise FileNotFoundError(p)
        if p.suffix.lower() not in ALLOWED_EXT:
            raise ValueError("Nur .py .cpp .cs erlaubt: " + p.name)
        return p
    found = []
    for ext in ALLOWED_EXT:
        found.extend(folder.glob("*" + ext))
    uniq, seen = [], set()
    for f in found:
        if f.name.lower() not in seen:
            seen.add(f.name.lower())
            uniq.append(f)
    if len(uniq) != 1:
        raise FileNotFoundError("Quelle in " + str(folder) + ": " + str(uniq))
    return uniq[0]


def build_one(folder: Path) -> Path:
    info = load_info(folder)
    title = str(info.get("title", folder.name))
    path = find_payload(folder, info)
    ir = transpile(path.read_text(encoding="utf-8"), path.suffix.lower())
    print("    IR:")
    for line in ir.strip().splitlines():
        print("      " + line)
    meta = (
        "# HomeBrew\ntitle=" + title
        + "\nauthor=" + str(info.get("author", ""))
        + "\nversion=" + str(info.get("version", "1.0"))
        + "\ndescription=" + str(info.get("description", ""))
        + "\nlang=" + path.suffix.lower().lstrip(".")
        + "\n---\n"
    )
    out_name = str(info.get("output", folder.name + ".hb"))
    if not out_name.endswith(".hb"):
        out_name += ".hb"
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out = OUT_DIR / out_name
    out.write_bytes(pack(title, (meta + ir).encode("utf-8")))
    print("OK  " + folder.name + " -> " + out.name + " (" + str(out.stat().st_size) + " bytes)")
    return out


def build_all() -> None:
    folders = sorted(p for p in SRC_DIR.iterdir() if p.is_dir()) if SRC_DIR.is_dir() else []
    if not folders:
        print("Keine Projekte")
        sys.exit(1)
    print("Baue " + str(len(folders)) + " Projekt(e)...\n")
    for f in folders:
        try:
            build_one(f)
        except Exception as e:
            print("FEHLER " + f.name + ": " + str(e))
            sys.exit(1)
    print("\nFertig -> homebrew/out/")


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "--build":
        build_all()
    else:
        print(__doc__)
        sys.exit(1)
