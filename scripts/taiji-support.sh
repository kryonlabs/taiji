#!/bin/sh

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
timeout=${TAIJI_SUPPORT_TIMEOUT:-180}

usage()
{
	cat >&2 <<EOF
usage: scripts/taiji-support.sh ape-smoke
       scripts/taiji-support.sh build-pcvirt
       scripts/taiji-support.sh debian-smoke
       scripts/taiji-support.sh install-linuxrun
       scripts/taiji-support.sh linuxrun-smoke
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

linuxrun_smoke()
{
	taiji_guest_expect linuxrun-smoke taiji-linuxrun-smoke-ok "$(cat <<'EOF'
echo taiji-linuxrun-smoke-start
cd /sys/src/cmd/linuxrun
if(! mk install){
	echo taiji-linuxrun-install-failed
	fshalt
}
if(! /386/bin/linuxrun -s){
	echo taiji-linuxrun-smoke-failed
	fshalt
}
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

debian_smoke()
{
	taiji_guest_expect debian-smoke taiji-debian-session-ok "$(cat <<'EOF'
echo taiji-debian-session-start
pkg=/debian/rootfs/tmp/taiji-deb-smoke.deb
tmp=/tmp/taiji-deb-smoke.$pid
rm -rf $tmp $pkg /debian/rootfs/tmp/taiji-deb-root >/dev/null >[2=1]
mkdir /debian/rootfs/tmp >/dev/null >[2=1]
mkdir $tmp
mkdir $tmp/control
mkdir $tmp/data
mkdir $tmp/data/usr
mkdir $tmp/data/usr/bin
echo '2.0' >$tmp/debian-binary
{
	echo 'Package: taiji-deb-smoke'
	echo 'Version: 1.0'
	echo 'Architecture: all'
	echo 'Maintainer: TaijiOS'
	echo 'Description: TaijiOS Debian namespace smoke package'
} >$tmp/control/control
{
	echo '#!/taiji/bin/rc'
	echo 'echo taiji-deb-payload-ok'
} >$tmp/data/usr/bin/taiji-deb-smoke
chmod +x $tmp/data/usr/bin/taiji-deb-smoke
@{builtin cd $tmp/control && tar cf $tmp/control.tar control}
@{builtin cd $tmp/data && tar cf $tmp/data.tar .}
@{builtin cd $tmp && ar rc $pkg debian-binary control.tar data.tar}
debian-session -c '
if(! ~ `{pwd} /root){
	echo taiji-debian-pwd-failed
	exit failed
}
if(! ~ $user root){
	echo taiji-debian-user-failed
	exit failed
}
if(! ~ $home /root){
	echo taiji-debian-home-failed
	exit failed
}
if(! ~ $namespace debian){
	echo taiji-debian-namespace-failed
	exit failed
}
if(! ~ $session debian){
	echo taiji-debian-session-env-failed
	exit failed
}
if(! test -r /etc/os-release){
	echo taiji-debian-os-release-failed
	exit failed
}
if(! test -x /taiji/bin/rc){
	echo taiji-debian-taiji-bin-failed
	exit failed
}
if(! test -d /var/lib/dpkg){
	echo taiji-debian-dpkg-dir-failed
	exit failed
}
if(! dpkg --root /tmp/taiji-deb-root -i /tmp/taiji-deb-smoke.deb){
	echo taiji-debian-dpkg-install-failed
	exit failed
}
if(! test -x /tmp/taiji-deb-root/usr/bin/taiji-deb-smoke){
	echo taiji-debian-payload-missing
	exit failed
}
/tmp/taiji-deb-root/usr/bin/taiji-deb-smoke
if(! grep -s taiji-deb-smoke /tmp/taiji-deb-root/var/lib/dpkg/status){
	echo taiji-debian-status-missing
	exit failed
}
rm -rf /tmp/taiji-deb-root /tmp/taiji-deb-smoke.deb >/dev/null >[2=1]
echo taiji-debian-session-ok
exit
'
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
if(! /386/bin/linuxrun -s){
	echo taiji-linuxrun-smoke-failed
	fshalt
}
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
debian-smoke)
	debian_smoke
	;;
install-linuxrun)
	install_linuxrun
	;;
linuxrun-smoke)
	linuxrun_smoke
	;;
linux-support-suite)
	linux_support_suite
	;;
*)
	usage
	;;
esac
