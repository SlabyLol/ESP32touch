#!/usr/bin/env python3
"""
DarkFox HomeBrew Packer
Erzeugt verschluesselte .hb Dateien fuer ESP32touch.

Format:
  Magic "HB01" (4)
  Version 1 (1)
  Flags 0 (1)
  TitleLen uint16 LE (2)
  Title encrypted (TitleLen)
  PayloadLen uint32 LE (4)
  Payload encrypted (PayloadLen)

Usage:
  python pack_homebrew.py "Mein Titel" input.txt output.hb
  python pack_homebrew.py "Demo" --text "Hallo von HomeBrew" demo.hb
"""

import sys
import struct

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


def pack(title: str, payload: bytes) -> bytes:
    title_b = title.encode("utf-8")[:64]
    title_enc = crypt(title_b, 0)
    payload_enc = crypt(payload, len(title_b))

    header = MAGIC + bytes([VERSION, 0]) + struct.pack("<H", len(title_b))
    body = title_enc + struct.pack("<I", len(payload)) + payload_enc
    return header + body


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)

    title = sys.argv[1]

    if sys.argv[2] == "--text":
        if len(sys.argv) < 5:
            print("Usage: pack_homebrew.py TITLE --text \"text\" out.hb")
            sys.exit(1)
        payload = sys.argv[3].encode("utf-8")
        out_path = sys.argv[4]
    else:
        in_path = sys.argv[2]
        out_path = sys.argv[3]
        with open(in_path, "rb") as f:
            payload = f.read()

    data = pack(title, payload)
    with open(out_path, "wb") as f:
        f.write(data)

    print(f"OK: {out_path}")
    print(f"  Titel : {title}")
    print(f"  Payload: {len(payload)} bytes")
    print(f"  Datei  : {len(data)} bytes")
    print(f"  Lege die Datei auf die SD: / oder /homebrew/")


if __name__ == "__main__":
    main()
