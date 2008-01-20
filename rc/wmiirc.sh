#!/bin/sh -f
# Configure wmii
. wmii.sh wmiirc

# Configuration Variables
MODKEY=Mod1
UP=k
DOWN=j
LEFT=h
RIGHT=l

# Colors tuples: "<text> <background> <border>"
WMII_NORMCOLORS='#000000 #c1c48b #81654f'
WMII_FOCUSCOLORS='#000000 #81654f #000000'

WMII_BACKGROUND='#333333'
WMII_FONT='-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*'

set -- $(echo $WMII_NORMCOLORS $WMII_FOCUSCOLORS)
WMII_MENU='dmenu -b -fn "$WMII_FONT" -nf '"'$1' -nb '$2' -sf '$4' -sb '$5'"
WMII_9MENU='wmii9menu -font "$WMII_FONT" -nf '"'$1' -nb '$2' -sf '$4' -sb '$5' -br '$6'"
WMII_TERM="xterm"

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

# Event processing
wi_events -s '	' <<'!'
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
	Event LeftBarClick
		shift
		wmiir xwrite /ctl view "$@"
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
	Event ClientMouseDown
		client=$1; button=$2
		case "$button" in
		3)
			do=$(wi_9menu -initial "${menulast:-SomeRandomName}" Nop Delete Fullscreen)
			case "$do" in
			Delete)
				wmiir xwrite /client/$client/ctl kill;;
			Fullscreen)
				wmiir xwrite /client/$client/ctl Fullscreen on;;
			esac
			menulast=${do:-"$menulast"}
		esac
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
		wmiir xwrite /tag/sel/ctl colmode sel default
	Key $MODKEY-s
		wmiir xwrite /tag/sel/ctl colmode sel stack
	Key $MODKEY-m
		wmiir xwrite /tag/sel/ctl colmode sel max
	Key $MODKEY-a
		Action $(wi_actions | wi_menu) &
	Key $MODKEY-p
		sh -c "$(wi_menu <$progsfile)" &
	Key $MODKEY-t
		wmiir xwrite /ctl "view $(wi_tags | wi_menu)" &
	Key $MODKEY-Return
		eval $WMII_TERM &
	Key $MODKEY-Shift-space
		wmiir xwrite /tag/sel/ctl send sel toggle
	Key $MODKEY-f
		wmiir xwrite /client/sel/ctl Fullscreen toggle
	Key $MODKEY-Shift-c
		wmiir xwrite /client/sel/ctl kill
	Key $MODKEY-Shift-t
		wmiir xwrite "/client/$(wmiir read /client/sel/ctl)/tags" "$(wi_tags | wi_menu)" &
	Key $MODKEY-$LEFT
		wmiir xwrite /tag/sel/ctl select left
	Key $MODKEY-$RIGHT
		wmiir xwrite /tag/sel/ctl select right
	Key $MODKEY-$DOWN
		wmiir xwrite /tag/sel/ctl select down
	Key $MODKEY-$UP
		wmiir xwrite /tag/sel/ctl select up
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
		wi_events -s '	' <<!
	Key $MODKEY-$i
		wmiir xwrite /ctl view "$i"
	Key $MODKEY-Shift-$i
		wmiir xwrite /client/sel/tags "$i"
!
	done

# WM Configuration
wmiir write /ctl <<!
	view 1
	font $WMII_FONT
	focuscolors $WMII_FOCUSCOLORS
	normcolors $WMII_NORMCOLORS
	grabmod $MODKEY
	border 1
!
xsetroot -solid "$WMII_BACKGROUND" &

export WMII_MENU WMII_9MENU WMII_FONT WMII_TERM
export WMII_FOCUSCOLORS WMII_SELCOLORS WMII_NORMCOLORS

# Misc
progsfile="$WMII_NS_DIR/.proglist"
Action status &
wi_proglist $PATH >$progsfile &

# Setup Tag Bar
(IFS="$(echo)"; wmiir rm $(wmiir ls /lbar | sed 's,^,/lbar/,'))
seltag="$(wmiir read /tag/sel/ctl | sed 1q)"
wi_tags | while read tag
do
	if [ "$tag" = "$seltag" ]; then
		echo "$WMII_FOCUSCOLORS" "$tag"
	else
		echo "$WMII_NORMCOLORS" "$tag"
	fi | wmiir create "/lbar/$tag"
done

wi_eventloop

