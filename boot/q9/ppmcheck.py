#!/usr/bin/env python3
"""Pixel checks on QEMU screendumps (P6 PPM).

usage: ppmcheck.py <file.ppm> <check> [args]
checks:
  mostly_black      - >=90% of pixels near black
  has_white         - >=0.1% near-white pixels (text)
  row_uniform y     - given row is a single color (no panel edge there)
  region_white x1 y1 x2 y2 - region contains near-white pixels
  seg_blue x1 y1 x2 y2     - region contains win2k blue (#3A6EA5-ish) pixels
"""
import sys

def load(path):
    with open(path, "rb") as f:
        data = f.read()
    toks, off = [], 0
    while len(toks) < 4:
        while data[off:1+off].isspace():
            off += 1
        if data[off:1+off] == b"#":
            while data[off:1+off] != b"\n":
                off += 1
            continue
        start = off
        while not data[off:1+off].isspace():
            off += 1
        toks.append(data[start:off])
    off += 1
    w, h = int(toks[1]), int(toks[2])
    return w, h, data[off:off + w*h*3]

def main():
    path, check = sys.argv[1], sys.argv[2]
    w, h, px = load(path)
    def pix(x, y):
        i = (y*w + x)*3
        return px[i], px[i+1], px[i+2]
    if check == "mostly_black":
        n = dark = 0
        for y in range(0, h, 4):
            for x in range(0, w, 4):
                r, g, b = pix(x, y)
                n += 1
                if r < 40 and g < 40 and b < 40:
                    dark += 1
        sys.exit(0 if dark*10 >= n*9 else 1)
    if check == "has_white":
        n = 0
        for y in range(0, h, 2):
            for x in range(0, w, 2):
                r, g, b = pix(x, y)
                if r > 200 and g > 200 and b > 200:
                    n += 1
        sys.exit(0 if n > w*h//4//1000 else 1)
    if check == "row_uniform":
        y = int(sys.argv[3])
        c = pix(0, y)
        for x in range(1, w, 3):
            if pix(x, y) != c:
                sys.exit(1)
        sys.exit(0)
    if check == "menu_at":
        # boot manager menu: solid white highlight bar on the first entry
        # (kernel boot text is black+white too, but has no solid runs)
        n = sum(1 for x in range(45, 360) if pix(x, 100) == (255, 255, 255))
        sys.exit(0 if n > 200 else 1)
    if check == "logon_at":
        # backdrop: win2k logon blue band near top; dialog grey at center
        r, g, b = pix(100, 60)
        dr, dg, db = pix(512, 350)
        sys.exit(0 if (30 <= r <= 90 and 90 <= g <= 140 and 140 <= b <= 200
                       and abs(dr-dg) < 12 and abs(dg-db) < 12 and dr > 140) else 1)
    if check == "grey_at" or check == "not_grey_at":
        x, y = int(sys.argv[3]), int(sys.argv[4])
        r, g, b = pix(x, y)
        grey = abs(r-g) < 12 and abs(g-b) < 12 and r > 140
        sys.exit(0 if (grey == (check == "grey_at")) else 1)
    if check == "region_white":
        x1, y1, x2, y2 = map(int, sys.argv[3:7])
        for y in range(y1, y2, 2):
            for x in range(x1, x2, 2):
                r, g, b = pix(x, y)
                if r > 200 and g > 200 and b > 200:
                    sys.exit(0)
        sys.exit(1)
    if check == "seg_blue":
        x1, y1, x2, y2 = map(int, sys.argv[3:7])
        for y in range(y1, y2):
            for x in range(x1, x2):
                r, g, b = pix(x, y)
                if 30 <= r <= 90 and 90 <= g <= 130 and 150 <= b <= 190:
                    sys.exit(0)
        sys.exit(1)
    sys.exit(2)

if __name__ == "__main__":
    main()
