#!/bin/sh -f
RC=""
IFS=:
for i in "$PLAN9" `echo P9PATHS`; do
	if [ -d "$i" -a -x "$i/bin/rc" ]; then
		export PLAN9="$i"
		RC="$i/bin/rc"
		break;
	fi
done

if [ ! -n "$RC" ]; then
	exit 1
fi

if [ -n "$1" ]; then
	exec "$RC" "$@"
else
	true
fi
