#!/usr/bin/env python3
"""Pixel checks on QEMU screendumps (P6 PPM).

usage: ppmcheck.py <file.ppm> <check> [args]
checks:
  mostly_black      - >=90% of pixels near black
  has_white         - >=0.1% near-white pixels (text)
  row_uniform y     - given row is a single color (no panel edge there)
  region_white x1 y1 x2 y2 - region contains near-white pixels
  seg_face x1 y1 x2 y2     - region contains Taiji theme surface pixels
  ocean_background_at x y  - pixel is Ocean light desktop background
  q9themes_no_plan9        - Plan9 row is absent from the themes list
  cursor_win2000 x y       - Windows 2000 style pointer is near x,y
  window_title_at x1 y1 x2 y2 - region contains a blue window titlebar
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
    def near(c, target, tol):
        return all(abs(c[i]-target[i]) < tol[i] for i in range(3))
    def anynear(c, targets, tol):
        return any(near(c, target, tol) for target in targets)
    light_backgrounds = [
        (226, 238, 252), (208, 232, 248), (224, 240, 224),
        (248, 232, 216), (240, 232, 248), (248, 216, 224),
        (245, 233, 223), (232, 236, 223), (243, 228, 200),
        (192, 192, 192), (228, 244, 238), (231, 234, 244),
        (255, 255, 234),
    ]
    dark_backgrounds = [
        (24, 40, 56), (16, 37, 64), (21, 48, 32),
        (48, 24, 16), (32, 21, 48), (48, 21, 32),
        (39, 34, 44), (31, 42, 34), (37, 26, 18),
        (32, 32, 32), (18, 45, 40), (18, 24, 51),
        (27, 27, 22),
    ]
    surfaces = [
        (212, 228, 245), (192, 221, 238), (209, 229, 209),
        (236, 216, 198), (226, 215, 238), (236, 201, 210),
        (231, 217, 208), (217, 224, 208), (228, 207, 170),
        (212, 208, 200), (210, 231, 223), (215, 221, 236),
        (239, 239, 210), (34, 54, 72), (28, 52, 80),
        (32, 60, 42), (62, 36, 24), (46, 32, 66),
        (64, 32, 42), (53, 45, 56), (45, 58, 48),
        (51, 36, 24), (48, 48, 48), (27, 59, 52),
        (27, 36, 69), (43, 43, 35),
    ]
    def surface(c):
        return anynear(c, surfaces, (25, 25, 25))
    def backgroundish(c):
        return anynear(c, light_backgrounds + dark_backgrounds, (35, 35, 35))
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
        # backdrop: Taiji Default light desktop, dialog face = theme surface
        bg = pix(100, 60)
        dlg = pix(512, 350)
        sys.exit(0 if backgroundish(bg) and surface(dlg) else 1)
    if check == "grey_at" or check == "not_grey_at":
        # theme surface (Taiji Default light, with legacy Plan9 accepted)
        x, y = int(sys.argv[3]), int(sys.argv[4])
        grey = surface(pix(x, y))
        sys.exit(0 if (grey == (check == "grey_at")) else 1)
    if check == "deskicon_at":
        # desktop icons top-left: many pixels differ from the wallpaper
        n = 0
        for y in range(6, 110, 2):
            for x in range(10, 100, 2):
                if not backgroundish(pix(x, y)):
                    n += 1
        sys.exit(0 if n > 120 else 1)
    if check == "submenu_at":
        # start menu programs submenu: black item text on the theme surface
        x1, y1, x2, y2 = map(int, sys.argv[3:7])
        nw = ng = 0
        for y in range(y1, y2, 2):
            for x in range(x1, x2, 2):
                r, g, b = pix(x, y)
                if r < 60 and g < 60 and b < 60:
                    nw += 1
                if surface((r, g, b)):
                    ng += 1
        sys.exit(0 if nw > 15 and ng > 200 else 1)
    if check == "rundlg_at":
        # Run dialog: kryon accent title strip, theme surface face,
        # near-background entry field
        r1, g1, b1 = pix(500, 310)
        r3, g3, b3 = pix(400, 400)
        navy = r1 < 40 and g1 < 60 and 90 <= b1 <= 200
        grey = surface(pix(450, 345))
        entry = r3 > 175 and g3 > 175 and b3 > 175
        sys.exit(0 if navy and grey and entry else 1)
    if check == "white_at":
        x, y = int(sys.argv[3]), int(sys.argv[4])
        r, g, b = pix(x, y)
        sys.exit(0 if r > 220 and g > 220 and b > 220 else 1)
    if check == "navy_at":
        x, y = int(sys.argv[3]), int(sys.argv[4])
        r, g, b = pix(x, y)
        sys.exit(0 if r < 40 and g < 40 and 100 <= b <= 180 else 1)
    if check == "ocean_background_at":
        x, y = int(sys.argv[3]), int(sys.argv[4])
        sys.exit(0 if near(pix(x, y), (208, 232, 248), (8, 8, 8)) else 1)
    if check == "q9themes_no_plan9":
        # The hidden Plan9 row would occupy this empty strip beneath Cobalt.
        # Count non-face pixels there; swatches/text from a real row exceed it.
        n = 0
        for y in range(415, 435, 2):
            for x in range(82, 266, 2):
                if not surface(pix(x, y)):
                    n += 1
        sys.exit(0 if n < 60 else 1)
    if check == "cursor_win2000":
        x, y = int(sys.argv[3]), int(sys.argv[4])
        rows = [
            "B...............",
            "BW..............",
            "BWW.............",
            "BWWW............",
            "BWWWW...........",
            "BWWWWW..........",
            "BWWWWWW.........",
            "BWWWWWWW........",
            "BWWWWWWWW.......",
            "BWWWWWBBBB......",
            "BWWWBB..........",
            "BWWBWB..........",
            "BWB.WB..........",
            "BB..WB..........",
            "B....WB.........",
            ".....BB.........",
        ]
        def score_at(x0, y0):
            black = white = 0
            for yy, row in enumerate(rows):
                for xx, want in enumerate(row):
                    if x0 + xx < 0 or x0 + xx >= w or y0 + yy < 0 or y0 + yy >= h:
                        continue
                    r, g, b = pix(x0 + xx, y0 + yy)
                    if want == "B" and r < 45 and g < 45 and b < 45:
                        black += 1
                    elif want == "W" and r > 220 and g > 220 and b > 220:
                        white += 1
            return black, white
        for oy in range(-2, 3):
            for ox in range(-2, 3):
                black, white = score_at(x + ox, y + oy)
                if black >= 22 and white >= 42:
                    sys.exit(0)
        sys.exit(1)
    if check == "window_title_at":
        x1, y1, x2, y2 = map(int, sys.argv[3:7])
        n = 0
        for y in range(y1, y2, 2):
            for x in range(x1, x2, 2):
                r, g, b = pix(x, y)
                if r < 80 and g < 180 and b > 90:
                    n += 1
        sys.exit(0 if n > 40 else 1)
    if check == "region_white":
        x1, y1, x2, y2 = map(int, sys.argv[3:7])
        for y in range(y1, y2, 2):
            for x in range(x1, x2, 2):
                r, g, b = pix(x, y)
                if r > 200 and g > 200 and b > 200:
                    sys.exit(0)
        sys.exit(1)
    if check == "seg_face":
        # theme surface pixels - the Kryon taskbar/panel face
        x1, y1, x2, y2 = map(int, sys.argv[3:7])
        for y in range(y1, y2):
            for x in range(x1, x2):
                if surface(pix(x, y)):
                    sys.exit(0)
        sys.exit(1)
    if check == "seg_blue":
        # legacy alias: the Kryon taskbar/panel surface
        x1, y1, x2, y2 = map(int, sys.argv[3:7])
        for y in range(y1, y2):
            for x in range(x1, x2):
                if surface(pix(x, y)):
                    sys.exit(0)
        sys.exit(1)
    sys.exit(2)

if __name__ == "__main__":
    main()
