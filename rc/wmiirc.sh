#!BINSH -f
# Configure wmii
wmiiscript=wmiirc # For wmii.sh
. wmii.sh

# Configuration Variables
MODKEY=Mod1
UP=k
DOWN=j
LEFT=h
RIGHT=l

# Bars
noticetimeout=5
noticebar=/rbar/!notice

# Colors tuples: "<text> <background> <border>"
WMII_NORMCOLORS='#000000 #c1c48b #81654f'
WMII_FOCUSCOLORS='#000000 #81654f #000000'

WMII_BACKGROUND='#333333'
WMII_FONT='-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*'

set -- $(echo $WMII_NORMCOLORS $WMII_FOCUSCOLORS)
WMII_TERM="xterm"

# Menu history
hist="$(wmiir namespace)/history"
histnum=5000

# Column Rules
wmiir write /colrules <<!
/gimp/ -> 17+83+41
/.*/ -> 62+38 # Golden Ratio
!

# Tagging Rules
wmiir write /tagrules <<!
/MPlayer|VLC/ -> ~
!

# Status Bar Info
status() {
	echo -n $(uptime | sed 's/.*://; s/,//g') '|' $(date)
}

local_events() { true;}
wi_runconf -s wmiirc_local

echo $WMII_NORMCOLORS | wmiir create $noticebar

# Event processing
events() {
	sed 's/^	//' <<'!'
	# Events
	Event CreateTag
		echo "$WMII_NORMCOLORS" "$@" | wmiir create "/lbar/$@"
	Event DestroyTag
		wmiir remove "/lbar/$@"
	Event FocusTag
		wmiir xwrite "/lbar/$@" "$WMII_FOCUSCOLORS" "$@"
	Event UnfocusTag
		wmiir xwrite "/lbar/$@" "$WMII_NORMCOLORS" "$@"
	Event UrgentTag
		shift
		wmiir xwrite "/lbar/$@" "*$@"
	Event NotUrgentTag
		shift
		wmiir xwrite "/lbar/$@" "$@"
	Event LeftBarClick LeftBarDND
		shift
		wmiir xwrite /ctl view "$@"
	Event Unresponsive
		{
			client=$1; shift
			msg="The following client is not responding. What would you like to do?$wi_nl"
			resp=$(wihack -transient $client \
				      xmessage -nearmouse -buttons Kill,Wait -print \
				               "$msg $(wmiir read /client/sel/label)")
			if [ "$resp" = Kill ]; then
				wmiir xwrite /client/$client/ctl slay &
			fi
		}&
	Event Notice
		wmiir xwrite $noticebar $wi_arg

		kill $xpid 2>/dev/null # Let's hope this isn't reused...
		{ sleep $noticetimeout; wmiir xwrite $noticebar ' '; }&
		xpid = $!
	Menu Client-3-Delete
		wmiir xwrite /client/$1/ctl kill
	Menu Client-3-Kill
		wmiir xwrite /client/$1/ctl slay
	Menu Client-3-Fullscreen
		wmiir xwrite /client/$1/ctl Fullscreen on
	Event ClientMouseDown
		wi_fnmenu Client $2 $1 &
	Menu LBar-3-Delete
		tag=$1; clients=$(wmiir read "/tag/$tag/index" | awk '/[^#]/{print $2}')
		for c in $clients; do
			if [ "$tag" = "$(wmiir read /client/$c/tags)" ]; then
				wmiir xwrite /client/$c/ctl kill
			else
				wmiir xwrite /client/$c/tags -$tag
			fi
			if [ "$tag" = "$(wi_seltag)" ]; then
				newtag=$(wi_tags | awk -v't='$tag '
					$1 == t { if(!l) getline l
						  print l
						  exit }
					{ l = $0 }')
				wmiir xwrite /ctl view $newtag
			fi
		done
	Event LeftBarMouseDown
		wi_fnmenu LBar "$@" &
	# Actions
	Action quit
		wmiir xwrite /ctl quit
	Action exec
		wmiir xwrite /ctl exec "$@"
	Action rehash
		proglist $PATH >$progsfile
	Action status
		set +xv
		if wmiir remove /rbar/status 2>/dev/null; then
			sleep 2
		fi
		echo "$WMII_NORMCOLORS" | wmiir create /rbar/status
		while status | wmiir write /rbar/status; do
			sleep 1
		done
	# Key Bindings
	Key $MODKEY-Control-t
		case $(wmiir read /keys | wc -l | tr -d ' \t\n') in
		0|1)
			echo -n "$Keys" | wmiir write /keys
			wmiir xwrite /ctl grabmod $MODKEY;;
		*)
			wmiir xwrite /keys $MODKEY-Control-t
			wmiir xwrite /ctl grabmod Mod3;;
		esac
	Key $MODKEY-space
		wmiir xwrite /tag/sel/ctl select toggle
	Key $MODKEY-d
		wmiir xwrite /tag/sel/ctl colmode sel default-max
	Key $MODKEY-s
		wmiir xwrite /tag/sel/ctl colmode sel stack-max
	Key $MODKEY-m
		wmiir xwrite /tag/sel/ctl colmode sel stack+max
	Key $MODKEY-a
		action $(wi_actions | wimenu -h "${hist}.actions" -n $histnum) &
	Key $MODKEY-p
		eval wmiir setsid "$(wimenu -h "${hist}.progs" -n $histnum <$progsfile)" &
	Key $MODKEY-t
		wmiir xwrite /ctl view $(wi_tags | wimenu -h "${hist}.tags" -n 50) &
	Key $MODKEY-Return
		eval wmiir setsid $WMII_TERM &
	Key $MODKEY-Shift-space
		wmiir xwrite /tag/sel/ctl send sel toggle
	Key $MODKEY-f
		wmiir xwrite /client/sel/ctl Fullscreen toggle
	Key $MODKEY-Shift-c
		wmiir xwrite /client/sel/ctl kill
	Key $MODKEY-Shift-t
		wmiir xwrite "/client/$(wmiir read /client/sel/ctl)/tags" $(wi_tags | wimenu -h "${hist}.tags" -n 50) &
	Key $MODKEY-$LEFT
		wmiir xwrite /tag/sel/ctl select left
	Key $MODKEY-$RIGHT
		wmiir xwrite /tag/sel/ctl select right
	Key $MODKEY-$DOWN
		wmiir xwrite /tag/sel/ctl select down
	Key $MODKEY-$UP
		wmiir xwrite /tag/sel/ctl select up
	Key $MODKEY-Control-$DOWN
		wmiir xwrite /tag/sel/ctl select down stack
	Key $MODKEY-Control-$UP
		wmiir xwrite /tag/sel/ctl select up stack
	Key $MODKEY-Shift-$LEFT
		wmiir xwrite /tag/sel/ctl send sel left
	Key $MODKEY-Shift-$RIGHT
		wmiir xwrite /tag/sel/ctl send sel right
	Key $MODKEY-Shift-$DOWN
		wmiir xwrite /tag/sel/ctl send sel down
	Key $MODKEY-Shift-$UP
		wmiir xwrite /tag/sel/ctl send sel up
!
	for i in 0 1 2 3 4 5 6 7 8 9; do
		sed 's/^	//' <<!
	Key $MODKEY-$i
		wmiir xwrite /ctl view "$i"
	Key $MODKEY-Shift-$i
		wmiir xwrite /client/sel/tags "$i"
!
	done
}
wi_events <<!
$(events)
$(local_events)
!
unset events local_events

# WM Configuration
wmiir write /ctl <<!
	font $WMII_FONT
	focuscolors $WMII_FOCUSCOLORS
	normcolors $WMII_NORMCOLORS
	grabmod $MODKEY
	border 1
!
xsetroot -solid "$WMII_BACKGROUND" &

export WMII_FONT WMII_TERM
export WMII_FOCUSCOLORS WMII_SELCOLORS WMII_NORMCOLORS

# Misc
progsfile="$(wmiir namespace)/.proglist"
action status &
wi_proglist $PATH >$progsfile &

# Setup Tag Bar
IFS="$wi_nl"
wmiir rm $(wmiir ls /lbar | sed 's,^,/lbar/,') >/dev/null
seltag=$(wmiir read /tag/sel/ctl | sed 1q)
unset IFS
wi_tags | while read tag
do
	if [ "$tag" = "$seltag" ]; then
		echo "$WMII_FOCUSCOLORS" "$tag"
	else
		echo "$WMII_NORMCOLORS" "$tag"
	fi | wmiir create "/lbar/$tag"
done

wi_eventloop

