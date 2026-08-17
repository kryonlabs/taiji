#!/usr/bin/env python3
"""Click at (x,y) on the q9 VM via VNC using gvnc (real client stack).
usage: gvncclick.py x y"""
import sys, time
import gi
gi.require_version('GtkVnc', '2.0')
gi.require_version('GLib', '2.0')
from gi.repository import GtkVnc, GLib

x, y = int(sys.argv[1]), int(sys.argv[2])
conn = GtkVnc.Connection()
loop = GLib.MainLoop()
state = {"clicks": 0}

def on_connected(src, cname):
    conn.pointer_event(0, x, y)
    time.sleep(0.15)
    conn.pointer_event(1, x, y)
    time.sleep(0.15)
    conn.pointer_event(0, x, y)
    time.sleep(0.3)
    loop.quit()

conn.connect("vnc-connected", on_connected)
conn.open_host("127.0.0.1", "5900")
GLib.timeout_add_seconds(8, loop.quit)
loop.run()
conn.close()
print("clicked", x, y)
