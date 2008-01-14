#!/bin/sh -f
# start wmiirc

export home="$HOME"
lconf="$home/.wmii-CONFVERSION" 
gconf="CONFPREFIX/.wmii-CONFVERSION" 

export WMII_CONFPATH="$conf:$gconf"
export POSIXLY_CORRECT=gnu_hippies

if wmii9rc; then
	WMIIRC=`PATH="$WMII_CONFPATH:$PATH" which rc.wmii`
else
	WMIIRC=`PATH="$WMII_CONFPATH:$PATH" which wmiirc`
fi

mkdir $conf 2>/dev/null && $gconf/welcome &
exec "$WMIIRC" "$@"

