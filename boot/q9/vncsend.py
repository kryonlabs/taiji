#!/usr/bin/env python3
"""Send keys to the q9 VM via VNC. usage: vncsend.py KEY [KEY...]
KEY is a character (a, b, ...) or a name: ret, spc, bks, tab, esc."""
import socket, sys, time

def recvn(s, n):
    b = b""
    while len(b) < n:
        c = s.recv(n - len(b))
        if not c:
            raise EOFError
        b += c
    return b

names = {
    "ret": 0xff0d, "spc": 0x20, "bks": 0xff08, "tab": 0xff09,
    "esc": 0xff1b, "del": 0xffff, "enter": 0xff0d, "bs": 0xff08,
    "ctrl": 0xffe3, "shift": 0xffe1, "alt": 0xffe9, "win": 0xffeb,
}

s = socket.create_connection(("127.0.0.1", int(__import__("os").environ.get("Q9_VNC_PORT", "5901"))), timeout=5)
s.settimeout(5)
recvn(s, 12)
s.sendall(b"RFB 003.008\n")
n = recvn(s, 1)[0]
recvn(s, n)
s.sendall(bytes([1]))
recvn(s, 4)
s.sendall(bytes([1]))
hdr = recvn(s, 24)
recvn(s, int.from_bytes(hdr[20:24], "big"))
# QEMU ignores input until the client sets pixel format/encodings and
# sends at least one framebuffer update request
s.sendall(bytes([0, 0, 0, 0]) + bytes(16))                  # SetPixelFormat
s.sendall(bytes([2, 0, 1, 0, 0, 0, 0]))                     # SetEncodings: 1 = RAW
s.sendall(bytes([3, 0, 0, 0, 0, 0, 4, 0, 4]))               # FBUpdateRequest
time.sleep(0.3)

def key(kc, down):
    s.sendall(bytes([4, down, kc >> 24, kc >> 16 & 0xFF, kc >> 8 & 0xFF, kc & 0xFF]))

for arg in sys.argv[1:]:
    down, up = 1, 1
    if arg.startswith("+"):
        arg, down = arg[1:], 1
        up = 0
    elif arg.startswith("-"):
        arg, down = arg[1:], 0
        up = 1
    if arg in names:
        kc = names[arg]
    elif len(arg) == 1:
        kc = ord(arg)
    else:
        continue
    if down:
        key(kc, 1)
        time.sleep(0.15)
    if up:
        key(kc, 0)
        time.sleep(0.1)
    time.sleep(0.15)
s.close()
