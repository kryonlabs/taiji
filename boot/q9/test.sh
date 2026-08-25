#!/bin/sh
# Headless GUI smoke test for the Rill desktop + boot splash.
# Boots the TaijiOS VM, drives it with VNC clicks, pixel-checks screendumps.

set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
cd "$root"
T=boot/q9
. $root/boot/q9/monclick.sh
pass=0; fail=0
ok()   { echo "PASS: $1"; pass=$((pass+1)); }
bad()  { echo "FAIL: $1"; fail=$((fail+1)); }
check() { if python3 $T/ppmcheck.py "$@"; then return 0; else return 1; fi; }

reset_theme() {
	mkdir -p usr/glenda/lib/kryon lib/kryon
	printf 'name=Default\nmode=light\nstyle=material\n' >usr/glenda/lib/kryon/theme
	printf 'name=Default\nmode=light\nstyle=material\n' >lib/kryon/system-theme
	printf 'tiles\n' >usr/glenda/lib/wallpaper
}

log_clean() {
	! grep -E 'panic|suicide|sys: trap|double-free|write error|allocb' $T/qemu.log >/dev/null 2>&1
}

fastshot() {
	printf 'screendump %s/boot/q9/%s.ppm\n' "$root" "$1" \
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
		if [ -s $T/bar.ppm ] && check $T/bar.ppm seg_face 380 370 640 400; then
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
	while [ $i -lt 100 ]; do
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
reset_theme
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
# Press Enter instead of clicking a resolution-dependent OK button.
monkey ret
sleep 2

echo "== boot 1: Rill desktop + start menu =="
sleep 8
fastshot desk
sleep 1
if check $T/desk.ppm grey_at 500 754; then
	ok "desktop panel present"
else
	bad "desktop panel present"
fi
monmove 400 300
sleep 1
fastshot cursor
sleep 1
if check $T/cursor.ppm cursor_win2000 400 300; then
	ok "Windows 2000 style cursor renders"
else
	bad "Windows 2000 style cursor renders"
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

echo "== boot 1: desktop icons =="
fastshot dicons
sleep 1
if check $T/dicons.ppm deskicon_at; then
	ok "desktop icons render"
else
	bad "desktop icons render"
fi
if [ -x 386/bin/inbe ] && sed -n '1p' lib/q9/desktop | grep -q '^Inner Breeze	inbe	inbe$'; then
	ok "Inner Breeze is preinstalled on desktop"
else
	bad "Inner Breeze is preinstalled on desktop"
fi

echo "== boot 1: programs submenu populated =="
monmove 100 585
sleep 1
fastshot subm
sleep 1
if check $T/subm.ppm submenu_at 210 520 385 760; then
	ok "programs submenu lists programs"
else
	bad "programs submenu lists programs"
fi

echo "== boot 1: run dialog =="
monclick 100 666
sleep 1
fastshot rund
sleep 1
if check $T/rund.ppm rundlg_at; then
	ok "run dialog opens with entry field"
else
	bad "run dialog opens with entry field"
fi
monclick 585 427
sleep 1

echo "== boot 1: start menu task manager =="
monclick 40 754
sleep 1
monmove 100 640
sleep 1
monrelclick 170 25
sleep 4
fastshot tmg
sleep 1
if check $T/tmg.ppm white_at 100 150; then
	ok "task manager window appears"
else
	bad "task manager window appears"
fi
monclick 622 42
sleep 2

echo "== boot 1: desktop context menu =="
monrclick 700 300
sleep 1
fastshot dmenu
sleep 1
if check $T/dmenu.ppm grey_at 720 330; then
	ok "desktop context menu opens"
else
	bad "desktop context menu opens"
fi
monclick 900 200
sleep 1

echo "== boot 1: Inner Breeze desktop launch =="
mondblclick 52 28
sleep 8
fastshot inbe
sleep 1
if check $T/inbe.ppm window_title_at 40 60 760 120; then
	ok "Inner Breeze launches from desktop icon"
else
	bad "Inner Breeze launches from desktop icon"
fi
if log_clean; then
	ok "boot 1 log has no panic or trap"
else
	bad "boot 1 log has no panic or trap"
fi

echo "== boot 2: themes app live apply =="
reset_theme
boot_vm
if wait_login; then
	ok "themes boot logon dialog renders"
else
	bad "themes boot logon dialog renders"
fi
monkey ret
sleep 10
monclick 40 754
sleep 1
monmove 100 585
sleep 1
monrelclick 170 168
sleep 3
fastshot themes
sleep 1
if check $T/themes.ppm q9themes_no_plan9 &&
	! grep -q 'Plan9' sys/src/cmd/q9themes/q9themes.c; then
	ok "themes app hides Plan9 palette"
else
	bad "themes app hides Plan9 palette"
fi
monclick 148 168
sleep 1
monclick 515 446
sleep 3
fastshot themeo
sleep 1
if check $T/themeo.ppm ocean_background_at 900 600; then
	ok "themes app Apply repaints running desktop"
else
	bad "themes app Apply repaints running desktop"
fi
if grep -q '^name=Ocean$' usr/glenda/lib/kryon/theme &&
	grep -q '^name=Ocean$' lib/kryon/system-theme; then
	ok "themes app persists Ocean as system theme"
else
	bad "themes app persists Ocean as system theme"
fi
if log_clean; then
	ok "boot 2 log has no panic or trap"
else
	bad "boot 2 log has no panic or trap"
fi

echo "== boot 3: safe mode disables panel =="
reset_theme
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
	if log_clean; then
		ok "boot 3 log has no panic or trap"
	else
		bad "boot 3 log has no panic or trap"
	fi
else
	bad "safe mode: boot menu never appeared"
fi

pkill -f '^qemu-system-x86_64' 2>/dev/null || true
reset_theme
rm -f $T/menu.ppm $T/bar.ppm $T/desk.ppm $T/start.ppm $T/safe.ppm $T/login.ppm $T/bar.ok $T/dicons.ppm $T/inbe.ppm $T/subm.ppm $T/rund.ppm $T/tmg.ppm $T/dmenu.ppm $T/themes.ppm $T/themeo.ppm
echo "== results: $pass passed, $fail failed =="
[ "$fail" -eq 0 ]
