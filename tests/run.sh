#!/bin/sh
# Requires a C compiler, pkg-config, the X11/Xft/Xinerama/XRender development libraries,
# Fontconfig, and Xvfb. Run with: sh tests/run.sh
set -eu

project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/dwm-regression.XXXXXX")
xpid=
cleanup() {
	if [ -n "$xpid" ]; then
		kill "$xpid" 2>/dev/null || :
		wait "$xpid" 2>/dev/null || :
	fi
	rm -rf "$tmp"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp -noreset \
	3>"$tmp/display" >"$tmp/xvfb.log" 2>&1 &
xpid=$!
i=0
while [ ! -s "$tmp/display" ]; do
	i=$((i + 1))
	if [ "$i" -ge 100 ] || ! kill -0 "$xpid" 2>/dev/null; then
		cat "$tmp/xvfb.log" >&2
		exit 1
	fi
	sleep 0.05
done
read -r number <"$tmp/display"
export DISPLAY=":$number"
export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"

for mode in xinerama single; do
	define=
	if [ "$mode" = xinerama ]; then define=-DXINERAMA; fi
	# Intentional word splitting for compiler flags supplied by pkg-config.
	${CC:-cc} -std=c99 -pedantic -Wall -Wno-deprecated-declarations \
		-D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L '-DVERSION="6.2"' $define \
		-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
		$(pkg-config --cflags x11 xft xinerama xrender fontconfig) \
		"$project/tests/regression.c" "$project/util.c" -o "$tmp/regression-$mode" \
		-Wl,--wrap=XftFontMatch -Wl,--wrap=XftDrawStringUtf8 \
		$(pkg-config --libs x11 xft xinerama xrender fontconfig)
	"$tmp/regression-$mode"
done
