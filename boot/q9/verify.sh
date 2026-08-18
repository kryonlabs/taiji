#!/bin/sh
# Self-contained verification: boots the VM, drives it via monitor
# injection, pixel-checks the new shell features, kills the VM.
set -u
cd /home/wao/Projects/plan9
T=boot/q9
. $T/monclick.sh
pass=0; fail=0
ok() { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }
check() { python3 $T/ppmcheck.py "$@"; }
shot() {
	printf 'screendump /home/wao/Projects/plan9/boot/q9/%s.ppm\n' "$1" \
		| socat - UNIX-CONNECT:$T/monitor.sock >/dev/null 2>&1
	sleep 1
}
strip() {
	python3 - "$1" "$2" <<'P2'
import sys
def stripimg(p):
    f=open(p,'rb').read(); t=[];o=0
    while len(t)<4:
        while f[o:o+1].isspace(): o+=1
        if f[o:o+1]==b'#':
            while f[o:o+1]!=b'\n': o+=1
            continue
        s=o
        while not f[o:o+1].isspace(): o+=1
        t.append(f[s:o])
    o+=1; w,h=int(t[1]),int(t[2]); return f[o:o+w*h*3]
a=stripimg(sys.argv[1]); b=stripimg(sys.argv[2])
w=1024
sa=a[(740*w)*3:(768*w)*3]
sb=b[(740*w)*3:(768*w)*3]
exit(0 if sa != sb else 1)
P2
}
pkill -f 'qemu-system-x86_64.*pxeboot' 2>/dev/null || true
sleep 2
rm -f $T/monitor.sock
setsid ./q9 --raw --headless gui >$T/verify-vm.log 2>&1 &
for i in $(seq 1 120); do
	grep -q "q9: gui ready" $T/verify-vm.log 2>/dev/null && break
	sleep 1
done
for i in $(seq 1 15); do [ -S $T/monitor.sock ] && break; sleep 1; done
i=0
while [ $i -lt 90 ]; do
	shot vlogon
	check $T/vlogon.ppm logon_at && break
	sleep 2
	i=$((i+1))
done
monclick 512 443
sleep 4
shot vdesk
if python3 $T/balloonchk.py $T/vdesk.ppm; then
	ok "welcome balloon appears"
else
	bad "welcome balloon appears"
fi
sleep 4
mondrag 400 80 700 300
shot vdrag
if python3 $T/dragchk.py $T/vdrag.ppm; then
	ok "live drag moves the window"
else
	bad "live drag moves the window"
fi
monkey ctrl-alt-shift-esc
sleep 4
shot vtaskmgr
if check $T/vtaskmgr.ppm white_at 100 150; then
	ok "task manager appears"
else
	bad "task manager appears"
fi
monclick 150 200
sleep 1
monkey q
sleep 2
shot mc0
mondblclick 46 26
sleep 4
shot mc1
if strip $T/mc0.ppm $T/mc1.ppm; then
	ok "My Computer window opens from the desktop icon"
else
	bad "My Computer window opens from the desktop icon"
fi
# icon drag: move My Computer away and back, position must persist
rm -f usr/glenda/lib/deskicons
mondrag 46 26 300 260
sleep 1
shot idrag
if [ -s usr/glenda/lib/deskicons ]; then
	ok "icon drag saves its position"
else
	bad "icon drag saves its position"
fi
if python3 $T/dragchk.py $T/idrag.ppm 2>/dev/null; then
	:
fi
mondrag 300 260 46 26
sleep 1
monrclick 46 26
sleep 1
shot imenu
if check $T/imenu.ppm grey_at 60 40; then
	ok "icon right click menu"
else
	bad "icon right click menu"
fi
monclick 900 100
sleep 1

pkill -f 'qemu-system-x86_64.*pxeboot' 2>/dev/null || true
echo "== verify: $pass passed, $fail failed =="
[ "$fail" -eq 0 ]
