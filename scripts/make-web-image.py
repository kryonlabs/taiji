#!/usr/bin/env python3
import sys
import gzip
import zlib
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
    if not patch_kernel_payload(image):
        return 1
    if not patch_root_payload(image):
        return 1
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(image)
    return 0


def patch_root_payload(image: bytearray) -> bool:
    marker = b"gzfilesystem "
    start = image.find(marker)
    if start < 0:
        print("embedded gzfilesystem marker not found", file=sys.stderr)
        return False

    line_end = image.find(b"\n", start)
    if line_end < 0:
        print("embedded gzfilesystem header is unterminated", file=sys.stderr)
        return False

    try:
        old_size = int(image[start + len(marker) : line_end])
    except ValueError:
        print("embedded gzfilesystem size is invalid", file=sys.stderr)
        return False

    payload_start = line_end + 1
    payload_end = payload_start + old_size
    compressed = bytes(image[payload_start:payload_end])
    try:
        payload = bytearray(gzip.decompress(compressed))
    except gzip.BadGzipFile as exc:
        print(f"could not decompress embedded root payload: {exc}", file=sys.stderr)
        return False

    replacements = {
        b"exec rio": b"exec rc ",
        b"acme -i riostart # rio already running": b"exec rc -i                            ",
        b"\tip/ipconfig": b"\t#ipconfig  ",
    }
    for old, new in replacements.items():
        if len(old) != len(new):
            print(f"internal replacement length mismatch for {old!r}", file=sys.stderr)
            return False
        count = payload.count(old)
        if count == 0:
            print(f"embedded root payload did not contain {old!r}", file=sys.stderr)
            return False
        payload[:] = payload.replace(old, new)

    recompressed = gzip.compress(bytes(payload), compresslevel=9, mtime=0)
    new_size = len(recompressed)
    if new_size > old_size:
        print(
            f"patched root payload grew from {old_size} to {new_size} bytes",
            file=sys.stderr,
        )
        return False
    if len(str(new_size)) != len(str(old_size)):
        print(
            f"patched root payload size changed digit width from {old_size} to {new_size}",
            file=sys.stderr,
        )
        return False

    image[start + len(marker) : line_end] = str(new_size).encode("ascii")
    image[payload_start:payload_end] = recompressed + b"\0" * (old_size - len(recompressed))
    return True


def patch_kernel_payload(image: bytearray) -> bool:
    gzip_magic = b"\x1f\x8b\x08"
    root_marker = b"gzfilesystem "
    kernel_start = image.find(gzip_magic)
    root_start = image.find(root_marker)
    if kernel_start < 0 or root_start < 0 or kernel_start >= root_start:
        print("could not locate embedded kernel gzip stream", file=sys.stderr)
        return False

    kernel_region = bytes(image[kernel_start:root_start])
    decompressor = zlib.decompressobj(16 + zlib.MAX_WBITS)
    try:
        payload = bytearray(decompressor.decompress(kernel_region))
    except zlib.error as exc:
        print(f"could not decompress embedded kernel payload: {exc}", file=sys.stderr)
        return False
    if not decompressor.eof:
        print("embedded kernel gzip stream did not terminate", file=sys.stderr)
        return False
    old_size = len(kernel_region) - len(decompressor.unused_data)
    compressed = kernel_region[:old_size]

    old = b"can't open /srv/usb"
    new = b"web boot has no usb"
    if len(old) != len(new):
        print("internal kernel replacement length mismatch", file=sys.stderr)
        return False
    if payload.count(old) == 0:
        print("embedded kernel payload did not contain USB warning string", file=sys.stderr)
        return False
    payload[:] = payload.replace(old, new)

    recompressed = gzip.compress(bytes(payload), compresslevel=9, mtime=0)
    if len(recompressed) > old_size:
        print(
            f"patched kernel payload grew from {old_size} to {len(recompressed)} bytes",
            file=sys.stderr,
        )
        return False

    image[kernel_start : kernel_start + old_size] = recompressed + b"\0" * (old_size - len(recompressed))
    return True


if __name__ == "__main__":
    raise SystemExit(main())
