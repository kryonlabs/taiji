# Debian Namespace

This directory stores the backing filesystem for TaijiOS' Debian startup
namespace.

`/debian/rootfs` is implementation storage from the normal TaijiOS namespace.
When `debian-session` starts, that directory is bound over `/`, so users in the
Debian session see it as the filesystem root.

TaijiOS support commands are grafted into the Debian namespace at `/taiji/bin`
and `/taiji/ape/bin`. Debian package payloads should install into `/bin`,
`/usr/bin`, `/etc`, `/var`, and the other normal root paths.
