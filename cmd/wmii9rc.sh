#!/bin/sh -f
RC=""
IFS=:
for i in "$PLAN9" `echo "P9PATHS"`; do
	if [ -d "$i" -a -x "$i/bin/rc" ]; then
		export PLAN9="$i"
		RC="$i/bin/rc"
		break;
	fi
done

if [ -z "$RC" ]; then
	exit 1
fi

if [ ! -x "$PLAN9/bin/read" ]; then
	echo 1>&2 $0: Found rc, but not read'(1)'. You probably have an out-of-date 9base installed.
fi

if [ -n "$1" ]; then
	exec "$RC" "$@"
else
	true
fi
