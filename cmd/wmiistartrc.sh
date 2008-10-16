#!BINSH -f
# start wmiirc

export home="$HOME"
lconf="$home/.wmii-CONFVERSION" 
gconf="CONFPREFIX/wmii-CONFVERSION" 

export WMII_CONFPATH="$lconf:$gconf"
#export POSIXLY_CORRECT=gnu_hippies

which="$(which which)"
if wmii9rc; then
	WMIIRC="$(PATH="$WMII_CONFPATH:$PATH" $which rc.wmii)"
else
	WMIIRC="$(PATH="$WMII_CONFPATH:$PATH" $which wmiirc)"
fi

mkdir $lconf 2>/dev/null && $gconf/welcome &
"$WMIIRC" "$@" || exec "$gconf/wmiirc" "$@"

