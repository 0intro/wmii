#!@BINSH@ -f
# Configure wmii
wmiiscript=wmiirc # For wmii.sh
. wmii.sh


# Configuration Variables
MODKEY=Mod4
UP=k
DOWN=j
LEFT=h
RIGHT=l

# Bars
noticetimeout=5
noticebar=/rbar/!notice

# Colors tuples: "<text> <background> <border>"
export WMII_NORMCOLORS='#000000 #c1c48b #81654f'
export WMII_FOCUSCOLORS='#000000 #81654f #000000'

export WMII_BACKGROUND='#333333'
export WMII_FONT='-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*'

set -- $(echo $WMII_NORMCOLORS $WMII_FOCUSCOLORS)
export WMII_TERM="@TERMINAL@"

# Ask about MODKEY on first run
    if ! test -d "${WMII_CONFPATH%%:*}"; then
        mkdir "${WMII_CONFPATH%%:*}"
        res=$(wihack -type DIALOG xmessage -nearmouse -buttons Windows,Alt -print -fn $WMII_FONT \
              "Welcome to wmii,$wi_newline$wi_newline" \
              "Most of wmii's default key bindings make use of the$wi_newline" \
              "Windows key, or equivalent. For keyboards lacking such$wi_newline" \
              "a key, many users change this to the Alt key.$wi_newline$wi_newline" \
              "Which would you prefer?")
        [ "$res" = "Alt" ] && MODKEY=Mod1
        echo "MODKEY=$MODKEY" >"${WMII_CONFPATH%%:*}/wmiirc_local"
        chmod +x "${WMII_CONFPATH%%:*}/wmiirc_local"
    fi

# Menu history
hist="${WMII_CONFPATH%%:*}/history"
histnum=5000

# Column Rules
wmiir write /colrules <<!
    /gimp/ -> 17+83+41
    /.*/ -> 62+38 # Golden Ratio
!

# Tagging Rules
wmiir write /rules <<!
    # Apps with system tray icons like to their main windows
    # Give them permission.
    /^Pidgin:/ allow=+activate

    # MPlayer and VLC don't float by default, but should.
    /MPlayer|VLC/ floating=on

    # ROX puts all of its windows in the same group, so they open
    # with the same tags.  Disable grouping for ROX Filer.
    /^ROX-Filer:/ group=0
!

# Status Bar Info
status() {
	echo -n label $(uptime | sed 's/.*://; s/, / /g') '|' $(date)
}

# Generic overridable startup details
startup() { witray & }

wi_runconf -s wmiirc_local
startup

echo colors $WMII_NORMCOLORS | wmiir create $noticebar

# Event processing
wi_events <<'!'
# Events
Event CreateTag
	echo colors "$WMII_NORMCOLORS$wi_newline" label "$@" | wmiir create "/lbar/$@"
Event DestroyTag
	wmiir remove "/lbar/$@"
Event FocusTag
	wmiir xwrite "/lbar/$@" colors "$WMII_FOCUSCOLORS"
Event UnfocusTag
	wmiir xwrite "/lbar/$@" colors "$WMII_NORMCOLORS"
Event UrgentTag
	shift
	wmiir xwrite "/lbar/$@" label "*$@"
Event NotUrgentTag
	shift
	wmiir xwrite "/lbar/$@" label "$@"
Event LeftBarClick LeftBarDND
	shift
	wmiir xwrite /ctl view "$@"
Event Unresponsive
	{
		client=$1; shift
		msg="The following client is not responding. What would you like to do?$wi_newline"
		resp=$(wihack -transient $client \
			      xmessage -nearmouse -buttons Kill,Wait -print \
			      -fn "${WMII_FONT%%,*}" "$msg $(wmiir read /client/sel/label)")
		if [ "$resp" = Kill ]; then
			wmiir xwrite /client/$client/ctl slay &
		fi
	}&
Event Notice
	wmiir xwrite $noticebar $wi_arg

	kill $xpid 2>/dev/null # Let's hope this isn't reused...
	{ sleep $noticetimeout; wmiir xwrite $noticebar ' '; }&
	xpid = $!

# Menus
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
		if [ "$tag" = "$(wmiir read /client/$c/tags)" ]
		then wmiir xwrite /client/$c/ctl kill
		else wmiir xwrite /client/$c/ctl tags -$tag
		fi
		[ "$tag" = "$(wi_seltag)" ] &&
			wmiir xwrite /ctl view $(wi_tags | wi_nexttag)
	done
Event LeftBarMouseDown
	wi_fnmenu LBar "$@" &

# Actions
Action showkeys
	echo "$KeysHelp" | xmessage -file - -fn ${WMII_FONT%%,*}
Action quit
	wmiir xwrite /ctl quit
Action exec
	wmiir xwrite /ctl exec "$@"
Action rehash
	wi_proglist $PATH >$progsfile
Action status
	set +xv
	if wmiir remove /rbar/status 2>/dev/null; then
		sleep 2
	fi
	echo colors "$WMII_NORMCOLORS" | wmiir create /rbar/status
	while status | wmiir write /rbar/status; do
		sleep 1
	done

# Key Bindings
KeyGroup Moving around
Key $MODKEY-$LEFT   # Select the client to the left
	wmiir xwrite /tag/sel/ctl select left
Key $MODKEY-$RIGHT  # Select the client to the right
	wmiir xwrite /tag/sel/ctl select right
Key $MODKEY-$UP     # Select the client above
	wmiir xwrite /tag/sel/ctl select up
Key $MODKEY-$DOWN   # Select the client below
	wmiir xwrite /tag/sel/ctl select down

Key $MODKEY-space   # Toggle between floating and managed layers
	wmiir xwrite /tag/sel/ctl select toggle

KeyGroup Moving through stacks
Key $MODKEY-Control-$UP    # Select the stack above
	wmiir xwrite /tag/sel/ctl select up stack
Key $MODKEY-Control-$DOWN  # Select the stack below
	wmiir xwrite /tag/sel/ctl select down stack

KeyGroup Moving clients around
Key $MODKEY-Shift-$LEFT   # Move selected client to the left
	wmiir xwrite /tag/sel/ctl send sel left
Key $MODKEY-Shift-$RIGHT  # Move selected client to the right
	wmiir xwrite /tag/sel/ctl send sel right
Key $MODKEY-Shift-$UP     # Move selected client up
	wmiir xwrite /tag/sel/ctl send sel up
Key $MODKEY-Shift-$DOWN   # Move selected client down
	wmiir xwrite /tag/sel/ctl send sel down

Key $MODKEY-Shift-space   # Toggle selected client between floating and managed layers
	wmiir xwrite /tag/sel/ctl send sel toggle

KeyGroup Client actions
Key $MODKEY-f # Toggle selected client's fullsceen state
	wmiir xwrite /client/sel/ctl Fullscreen toggle
Key $MODKEY-Shift-c # Close client
	wmiir xwrite /client/sel/ctl kill

KeyGroup Changing column modes
Key $MODKEY-d # Set column to default mode
	wmiir xwrite /tag/sel/ctl colmode sel default-max
Key $MODKEY-s # Set column to stack mode
	wmiir xwrite /tag/sel/ctl colmode sel stack-max
Key $MODKEY-m # Set column to max mode
	wmiir xwrite /tag/sel/ctl colmode sel stack+max

KeyGroup Running programs
Key $MODKEY-a      # Open wmii actions menu
	action $(wi_actions | wimenu -h "${hist}.actions" -n $histnum) &
Key $MODKEY-p      # Open program menu
	eval wmiir setsid "$(wimenu -h "${hist}.progs" -n $histnum <$progsfile)" &

Key $MODKEY-Return # Launch a terminal
	eval wmiir setsid $WMII_TERM &

KeyGroup Other
Key $MODKEY-Control-t # Toggle all other key bindings
	case $(wmiir read /keys | wc -l | tr -d ' \t\n') in
	0|1)
		echo -n "$Keys" | wmiir write /keys
		wmiir xwrite /ctl grabmod $MODKEY;;
	*)
		wmiir xwrite /keys $MODKEY-Control-t
		wmiir xwrite /ctl grabmod Mod3;;
	esac

KeyGroup Tag actions
Key $MODKEY-t       # Change to another tag
	wmiir xwrite /ctl view $(wi_tags | wimenu -h "${hist}.tags" -n 50) &
Key $MODKEY-Shift-t # Retag the selected client
	# Assumes left-to-right order of evaluation
	wmiir xwrite /client/$(wi_selclient)/ctl tags $(wi_tags | wimenu -h "${hist}.tags" -n 50) &
Key $MODKEY-n	    # Move to the next tag
	wmiir xwrite /ctl view $(wi_tags | wi_nexttag)
Key $MODKEY-b	    # Move to the previous tag
	wmiir xwrite /ctl view $(wi_tags | sort -r | wi_nexttag)
!
	for i in 0 1 2 3 4 5 6 7 8 9; do
		wi_events <<!
Key $MODKEY-$i		 # Move to the numbered view
	wmiir xwrite /ctl view "$i"
Key $MODKEY-Shift-$i     # Retag selected client with the numbered tag
	wmiir xwrite /client/sel/ctl tags "$i"
!
done
wi_events -e

# WM Configuration
wmiir write /ctl <<!
	font $WMII_FONT
	focuscolors $WMII_FOCUSCOLORS
	normcolors $WMII_NORMCOLORS
	grabmod $MODKEY
	border 1
!
xsetroot -solid "$WMII_BACKGROUND" &

# Misc
progsfile="$(wmiir namespace)/.proglist"
action status &
wi_proglist $PATH >$progsfile &

# Setup Tag Bar
IFS="$wi_newline"
wmiir rm $(wmiir ls -p /lbar) >/dev/null
seltag=$(wmiir read /tag/sel/ctl | sed 1q)
unset IFS
wi_tags | while read tag
do
	if [ "$tag" = "$seltag" ]; then
		echo colors "$WMII_FOCUSCOLORS"
		echo label $tag
	else
		echo colors "$WMII_NORMCOLORS"
		echo label $tag
	fi | wmiir create "/lbar/$tag"
done

wi_eventloop

