#!/usr/bin/env python3
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: make-web-image.py input.raw plan9.ini output.raw", file=sys.stderr)
        return 2

    src = Path(sys.argv[1])
    ini = Path(sys.argv[2])
    dst = Path(sys.argv[3])
    image = bytearray(src.read_bytes())
    marker = b"[menu]\n"
    start = image.find(marker)
    if start < 0:
        print(f"{src}: embedded plan9.ini marker not found", file=sys.stderr)
        return 1

    end = image.find(b"\0", start)
    if end < 0:
        print(f"{src}: embedded plan9.ini terminator not found", file=sys.stderr)
        return 1

    old_len = end - start
    replacement = ini.read_bytes()
    if not replacement.endswith(b"\n"):
        replacement += b"\n"
    if len(replacement) > old_len:
        print(
            f"{ini}: {len(replacement)} bytes does not fit existing {old_len}-byte plan9.ini slot",
            file=sys.stderr,
        )
        return 1

    image[start : start + old_len] = replacement + b"\0" * (old_len - len(replacement))
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(image)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
