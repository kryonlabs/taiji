#!/bin/sh
# Click at absolute screen position using QEMU monitor mouse injection.
# Homes the guest cursor to the top-left corner first (deltas clamp at
# 0,0), then moves in steps of at most 127 (larger PS/2 deltas get
# mangled). Source, don't run.
# usage: monclick x y   (after: . boot/q9/monclick.sh)
T=${T:-boot/q9}
monclick() {
	_x=$1 _y=$2
	{
		for _i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
			printf 'mouse_move -127 -127\n'
			sleep 0.05
		done
		while [ $_x -gt 127 ]; do
			printf 'mouse_move 127 0\n'; _x=$((_x-127)); sleep 0.05
		done
		while [ $_y -gt 127 ]; do
			printf 'mouse_move 0 127\n'; _y=$((_y-127)); sleep 0.05
		done
		printf 'mouse_move %d %d\n' "$_x" "$_y"
		sleep 0.3
		printf 'mouse_button 1\n'
		sleep 0.6
		printf 'mouse_button 0\n'
	} | socat - UNIX-CONNECT:$T/monitor.sock >/dev/null 2>&1
	sleep 0.4
}

# move without clicking (hover), same homing strategy
monmove() {
	_x=$1 _y=$2
	{
		for _i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
			printf 'mouse_move -127 -127\n'
			sleep 0.05
		done
		while [ $_x -gt 127 ]; do
			printf 'mouse_move 127 0\n'; _x=$((_x-127)); sleep 0.05
		done
		while [ $_y -gt 127 ]; do
			printf 'mouse_move 0 127\n'; _y=$((_y-127)); sleep 0.05
		done
		printf 'mouse_move %d %d\n' "$_x" "$_y"
	} | socat - UNIX-CONNECT:$T/monitor.sock >/dev/null 2>&1
	sleep 0.4
}

# right click at position
monrclick() {
	_x=$1 _y=$2
	{
		for _i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
			printf 'mouse_move -127 -127\n'
			sleep 0.05
		done
		while [ $_x -gt 127 ]; do
			printf 'mouse_move 127 0\n'; _x=$((_x-127)); sleep 0.05
		done
		while [ $_y -gt 127 ]; do
			printf 'mouse_move 0 127\n'; _y=$((_y-127)); sleep 0.05
		done
		printf 'mouse_move %d %d\n' "$_x" "$_y"
		sleep 0.3
		printf 'mouse_button 4\n'
		sleep 0.6
		printf 'mouse_button 0\n'
	} | socat - UNIX-CONNECT:$T/monitor.sock >/dev/null 2>&1
	sleep 0.4
}
