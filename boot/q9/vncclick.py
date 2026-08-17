#!/usr/bin/env python3
import socket, sys, time

def recvn(s, n):
    b = b""
    while len(b) < n:
        c = s.recv(n - len(b))
        if not c:
            raise EOFError
        b += c
    return b

x, y = int(sys.argv[1]), int(sys.argv[2])
s = socket.create_connection(("127.0.0.1", int(__import__("os").environ.get("Q9_VNC_PORT", "5901"))), timeout=5)
s.settimeout(5)
recvn(s, 12)                       # server version
s.sendall(b"RFB 003.008\n")
n = recvn(s, 1)[0]                 # number of security types
recvn(s, n)                        # types
s.sendall(bytes([1]))              # None
recvn(s, 4)                        # security result
s.sendall(bytes([1]))              # ClientInit (shared)
hdr = recvn(s, 24)                 # ServerInit: w,h pixelformat, namelen
namelen = int.from_bytes(hdr[20:24], "big")
recvn(s, namelen)

def ptr(mask, x, y):
    s.sendall(bytes([5, mask, x >> 8, x & 0xFF, y >> 8, y & 0xFF]))

time.sleep(0.3)
ptr(0, x, y); time.sleep(0.4)
ptr(1, x+1, y); time.sleep(0.8)
ptr(0, x, y); time.sleep(0.5)
s.close()
