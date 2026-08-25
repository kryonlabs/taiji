#!/bin/sh
set -u
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
cd "$root"
T=boot/q9
. $root/boot/q9/monclick.sh
pkill -f '^qemu-system-x86_64' 2>/dev/null || true
sleep 0.5
rm -f $T/monitor.sock $T/m1.ppm $T/m2.ppm $T/m3.ppm $T/dbg.err
setsid ./q9 --raw --headless gui >$T/qemu.log 2>&1 &
shot() {
	printf 'screendump /home/wao/Projects/taiji/boot/q9/%s.ppm\n' "$1" \
		| socat - UNIX-CONNECT:$T/monitor.sock >>$T/dbg.err 2>&1
}
# wait for boot menu
i=0
while [ $i -lt 90 ]; do
	shot m1
	if [ -s $T/m1.ppm ] && python3 $T/ppmcheck.py $T/m1.ppm mostly_black \
		&& python3 $T/ppmcheck.py $T/m1.ppm has_white; then
		break
	fi
	sleep 1
	i=$((i+1))
done
python3 - <<EOF
import socket,time
for _ in range(30):
    try:
        socket.create_connection(("127.0.0.1",5900),timeout=1).close(); break
    except OSError: time.sleep(1)
EOF
echo "menu waited $i s"
monclick 200 126
sleep 0.5
shot m2
sleep 0.5
ffmpeg -y -loglevel error -i $T/m2.ppm $T/m2.png
sleep 5
shot m3
sleep 0.5
ffmpeg -y -loglevel error -i $T/m3.ppm $T/m3.png
echo done
