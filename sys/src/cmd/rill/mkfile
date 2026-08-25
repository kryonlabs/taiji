< /$objtype/mkfile

TARG=rill
KRYON=/sys/src/kryon
KAPSULE=/sys/src/kapsule
SHELF=/sys/src/shelf
BIN=/$objtype/bin
OUT=$O.out

CPPFLAGS=-I../include -I$KRYON/src/platform/plan9/include -I$KRYON/include \
	-I$KAPSULE/src -I$SHELF/src \
	-DKRYON_BACKEND_LIBDRAW -DKRYON_PLATFORM_PLAN9 -DKRYON_NATIVE_PLAN9
KTERMFLAGS=-DKAPSULE_PLAN9_EMBEDDED_HOST
CFLAGS=-FTVw

OFILES=\
	src/main.$O\
	src/rill_shell.$O\
	src/platform_plan9.$O\
	$KAPSULE/src/app_clipboard.$O\
	$KAPSULE/src/app_commands.$O\
	$KAPSULE/src/app_context_menu.$O\
	$KAPSULE/src/app_input.$O\
	$KAPSULE/src/app_menu.$O\
	$KAPSULE/src/app_profile.$O\
	$KAPSULE/src/app_search.$O\
	$KAPSULE/src/app_sessions.$O\
	$KAPSULE/src/app_terminal_view.$O\
	$KAPSULE/src/config.$O\
	$KAPSULE/src/input.$O\
	$KAPSULE/src/kapsule_host.$O\
	$KAPSULE/src/launch_options.$O\
	$KAPSULE/src/palette.$O\
	$KAPSULE/src/profile.$O\
	$KAPSULE/src/selection.$O\
	$KAPSULE/src/session.$O\
	$KAPSULE/src/session_store.$O\
	$KAPSULE/src/terminal.$O\
	$KAPSULE/src/terminal_csi.$O\
	$KAPSULE/src/terminal_dcs.$O\
	$KAPSULE/src/terminal_keys.$O\
	$KAPSULE/src/terminal_modes.$O\
	$KAPSULE/src/terminal_mouse.$O\
	$KAPSULE/src/terminal_osc.$O\
	$KAPSULE/src/terminal_parser.$O\
	$KAPSULE/src/terminal_paste.$O\
	$KAPSULE/src/terminal_pty_plan9.$O\
	$KAPSULE/src/terminal_screen.$O\
	$KAPSULE/src/terminal_search.$O\
	$KAPSULE/src/terminal_sgr.$O\
	$KAPSULE/src/terminal_sixel.$O\
	$KAPSULE/src/terminal_text.$O\
	$KAPSULE/src/terminal_view.$O\
	$SHELF/src/shelf.$O\
	$SHELF/src/shelf_host.$O\

LIB=/$objtype/lib/libkryon.a /$objtype/lib/libstdio.a

all:V: $OUT

install:V: $BIN/$TARG

$BIN/$TARG: $OUT
	cp $OUT $BIN/$TARG

$OUT: $OFILES $LIB
	$LD -o $target $prereq -ldraw -lmemdraw -lthread

src/%.$O: src/%.c
	cd src && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i

clean:V:
	rm -f src/*.[$OS] src/*.i [$OS].out $TARG $KAPSULE/src/*.[$OS] \
		$KAPSULE/src/*.i $SHELF/src/*.[$OS] $SHELF/src/*.i

$KAPSULE/src/kapsule_host.$O: $KAPSULE/src/kapsule_host.c
	cd $KAPSULE/src && cpp -+ $CPPFLAGS $KTERMFLAGS '-DCreateAppHost=KtermCreateAppHost' '-DDestroyAppHost=KtermDestroyAppHost' kapsule_host.c > kapsule_host.i && $CC $CFLAGS -c kapsule_host.i && mv kapsule_host.i.$O kapsule_host.$O && rm -f kapsule_host.i

$KAPSULE/src/%.$O: $KAPSULE/src/%.c
	cd $KAPSULE/src && cpp -+ $CPPFLAGS $KTERMFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i

$SHELF/src/shelf_host.$O: $SHELF/src/shelf_host.c
	cd $SHELF/src && cpp -+ $CPPFLAGS '-DCreateAppHost=RillShelfCreateAppHost' '-DDestroyAppHost=RillShelfDestroyAppHost' shelf_host.c > shelf_host.i && $CC $CFLAGS -c shelf_host.i && mv shelf_host.i.$O shelf_host.$O && rm -f shelf_host.i

$SHELF/src/%.$O: $SHELF/src/%.c
	cd $SHELF/src && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i
