#!/bin/sh -f
# start wmiirc

export WMII_CONFPATH="$HOME/.wmii-CONFVERSION:CONFPREFIX/wmii-CONFVERSION"

if wmii9rc; then
	WMIIRC=`PATH="$WMII_CONFPATH:$PATH" which rc.wmii`
else
	WMIIRC=`PATH="$WMII_CONFPATH:$PATH" which wmiirc`
fi

mkdir $HOME/.wmii-CONFVERSION 2>/dev/null && CONFPREFIX/wmii-CONFVERSION/welcome &
exec "$WMIIRC" $@
