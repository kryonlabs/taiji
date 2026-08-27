#!/bin/sh

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
timeout=${TAIJI_SUPPORT_TIMEOUT:-180}

usage()
{
	cat >&2 <<EOF
usage: scripts/taiji-support.sh ape-smoke
       scripts/taiji-support.sh build-pcvirt
       scripts/taiji-support.sh install-linuxrun
       scripts/taiji-support.sh linux-support-suite
EOF
	exit 1
}

taiji_guest()
{
	(cd "$root" && ./q9 --raw --text tty-run "$1")
}

taiji_guest_expect()
{
	name=$1
	success=$2
	script=$3
	log=${TAIJI_SUPPORT_LOG:-/tmp/taiji-support-$name.log}
	pidfile=${TAIJI_SUPPORT_PIDFILE:-/tmp/taiji-support-$name.pid}

	: >"$log"
	rm -f "$pidfile"
	(cd "$root" && setsid sh -c 'echo $$ >"$1"; shift; exec "$@"' sh "$pidfile" \
		env Q9_COPY_IMAGE=1 Q9_TMPDIR="${Q9_TMPDIR:-/tmp}" TMPDIR="${Q9_TMPDIR:-/tmp}" \
		./q9 --raw --text tty-run "$script") >"$log" 2>&1 &
	q9_pid=$!

	stop_vm()
	{
		if [ -s "$pidfile" ]; then
			pgid=$(cat "$pidfile")
			/bin/kill -TERM -- -"$pgid" 2>/dev/null || true
		fi
		kill -TERM "$q9_pid" 2>/dev/null || true
		wait "$q9_pid" 2>/dev/null || true
		rm -f "$pidfile"
	}

	i=0
	while [ "$i" -lt "$timeout" ]; do
		if grep -q "$success" "$log"; then
			cat "$log"
			stop_vm
			return 0
		fi
		if grep -q 'taiji-.*-failed\|syntax error\|mk: .*error' "$log"; then
			tail -120 "$log" >&2
			stop_vm
			return 1
		fi
		if ! kill -0 "$q9_pid" 2>/dev/null; then
			if grep -q "$success" "$log"; then
				cat "$log"
				return 0
			fi
			tail -120 "$log" >&2
			return 1
		fi
		i=$((i + 1))
		sleep 1
	done

	echo "taiji-support: timed out waiting for $success; tail follows" >&2
	tail -120 "$log" >&2
	stop_vm
	return 1
}

ape_smoke()
{
	taiji_guest_expect ape-smoke taiji-compat-smoke-ok "$(cat <<'EOF'
echo taiji-ape-build-start
cd /sys/src/ape/lib/ap
if(! mk install){
	echo taiji-ape-build-failed
	fshalt
}
cd /sys/src/ape
if(! /rc/bin/ape/linuxcc compat-smoke.c -o /tmp/compat-smoke){
	echo taiji-compat-compile-failed
	fshalt
}
if(! /tmp/compat-smoke){
	echo taiji-compat-smoke-failed
	fshalt
}
echo taiji-compat-smoke-ok
fshalt
EOF
)"
}

install_linuxrun()
{
	taiji_guest_expect install-linuxrun taiji-linuxrun-install-ok "$(cat <<'EOF'
echo taiji-linuxrun-install-start
cd /sys/src/cmd/linuxrun
if(! mk install){
	echo taiji-linuxrun-install-failed
	fshalt
}
if(test -x /386/bin/linuxrun)
	echo taiji-linuxrun-install-ok
if not
	echo taiji-linuxrun-install-failed
fshalt
EOF
)"
}

build_pcvirt()
{
	taiji_guest_expect build-pcvirt taiji-pcvirt-build-ok "$(cat <<'EOF'
echo taiji-pcvirt-build-start
cd /sys/src/9/pc
if(! mk 'CONF=pcvirt' install){
	echo taiji-pcvirt-build-failed
	fshalt
}
if(test -x /386/9pcvirt)
	echo taiji-pcvirt-build-ok
if not
	echo taiji-pcvirt-build-failed
fshalt
EOF
)"
}

linux_support_suite()
{
	taiji_guest_expect linux-support-suite taiji-linux-support-suite-ok "$(cat <<'EOF'
echo taiji-linux-support-suite-start
cd /sys/src/ape/lib/ap
if(! mk install){
	echo taiji-ape-build-failed
	fshalt
}
cd /sys/src/ape
if(! /rc/bin/ape/linuxcc compat-smoke.c -o /tmp/compat-smoke){
	echo taiji-compat-compile-failed
	fshalt
}
if(! /tmp/compat-smoke){
	echo taiji-compat-smoke-failed
	fshalt
}
echo taiji-compat-smoke-ok
cd /sys/src/cmd/linuxrun
if(! mk install){
	echo taiji-linuxrun-install-failed
	fshalt
}
echo taiji-linuxrun-install-ok
cd /sys/src/9/pc
if(! mk 'CONF=pcvirt' install){
	echo taiji-pcvirt-build-failed
	fshalt
}
if(test -x /386/9pcvirt)
	echo taiji-pcvirt-build-ok
if not
	echo taiji-pcvirt-build-failed
echo taiji-linux-support-suite-ok
fshalt
EOF
)"
}

case "${1:-}" in
ape-smoke)
	ape_smoke
	;;
build-pcvirt)
	build_pcvirt
	;;
install-linuxrun)
	install_linuxrun
	;;
linux-support-suite)
	linux_support_suite
	;;
*)
	usage
	;;
esac
