#!/bin/sh
# Headless GUI smoke test for the rio panel + boot splash.
# Boots the q9 VM, drives it with VNC clicks, pixel-checks screendumps.

set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
cd "$root"
T=boot/q9
. $root/boot/q9/monclick.sh
pass=0; fail=0
ok()   { echo "PASS: $1"; pass=$((pass+1)); }
bad()  { echo "FAIL: $1"; fail=$((fail+1)); }
check() { if python3 $T/ppmcheck.py "$@"; then return 0; else return 1; fi; }

fastshot() {
	printf 'screendump /home/wao/Projects/plan9/boot/q9/%s.ppm\n' "$1" \
		| socat - UNIX-CONNECT:$T/monitor.sock >>$T/screendump.err 2>&1
}

boot_vm() {
	pkill -f '^qemu-system-x86_64' 2>/dev/null || true
	sleep 1
	rm -f $T/monitor.sock $T/screendump.err
	mon_curx=0
	mon_cury=0
	Q9_TEST=1 setsid ./q9 --raw --headless gui >$T/qemu.log 2>&1 &
	for i in $(seq 1 120); do
		grep -q "q9: gui ready" $T/qemu.log 2>/dev/null && break
		sleep 1
	done
	for i in $(seq 1 10); do
		[ -S $T/monitor.sock ] && return 0
		sleep 1
	done
	echo "FAIL: monitor socket missing" >&2
	return 1
}

# poll fast (0.4s) until the boot menu is on screen; leaves it in menu.ppm
wait_menu() {
	i=0
	while [ $i -lt 150 ]; do
		fastshot menu
		if [ -s $T/menu.ppm ] && check $T/menu.ppm menu_at; then
			return 0
		fi
		sleep 0.4
		i=$((i+1))
	done
	return 1
}

# poll fast until the progress bar's blue segments are visible
wait_bar() {
	i=0
	while [ $i -lt 100 ]; do
		fastshot bar
		if [ -s $T/bar.ppm ] && check $T/bar.ppm seg_blue 380 370 640 400; then
			return 0
		fi
		sleep 0.1
		i=$((i+1))
	done
	return 1
}

# poll fast until the logon dialog is up (blue backdrop, grey dialog)
wait_login() {
	i=0
	while [ $i -lt 60 ]; do
		fastshot login
		if [ -s $T/login.ppm ] && check $T/login.ppm logon_at; then
			return 0
		fi
		sleep 0.4
		i=$((i+1))
	done
	return 1
}

echo "== boot 1: splash menu + progress bar =="
boot_vm
if wait_menu; then
	ok "boot manager menu renders (black, white text)"
else
	bad "boot manager menu renders"
fi
if wait_bar; then
	ok "progress bar animation renders"
else
	bad "progress bar animation renders"
fi

echo "== boot 1: logon screen =="
if wait_login; then
	ok "logon dialog renders (blue backdrop, dialog)"
else
	bad "logon dialog renders"
fi
# click OK (dialog ~380x210 centered; OK button at ~512,443)
monclick 512 443
sleep 2

echo "== boot 1: desktop + start menu =="
sleep 8
fastshot desk
sleep 1
if check $T/desk.ppm grey_at 500 754; then
	ok "taskbar panel present at bottom"
else
	bad "taskbar panel present at bottom"
fi
monclick 40 754
sleep 1
fastshot start
sleep 1
if check $T/start.ppm region_white 100 480 300 640; then
	ok "start menu opens with items"
else
	bad "start menu opens with items"
fi

echo "== boot 2: safe mode disables panel =="
boot_vm
if wait_menu; then
	monclick 200 126 || true
	sleep 14
	fastshot safe
	sleep 1
	if check $T/safe.ppm not_grey_at 500 754; then
		ok "safe mode: no panel at bottom"
	else
		bad "safe mode: no panel at bottom"
	fi
else
	bad "safe mode: boot menu never appeared"
fi

pkill -f '^qemu-system-x86_64' 2>/dev/null || true
rm -f $T/menu.ppm $T/bar.ppm $T/desk.ppm $T/start.ppm $T/safe.ppm $T/login.ppm $T/bar.ok
echo "== results: $pass passed, $fail failed =="
[ "$fail" -eq 0 ]
