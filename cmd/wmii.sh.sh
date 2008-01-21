
wmiiscript=$1
if [ -z "$scriptname" ]; then
	scriptname="$wmiiscript"; fi
echo Start $wmiiscript | wmiir write /event 2>/dev/null ||
	exit 1

Keys=""
Actions=""
Events=""

wi_nl='
'

_wi_script() {
	cat <<'!'
	BEGIN {
		arg[1] = "Nop"
		narg = 1;
		body = "";
	}
	function addevent() {
		var = arg[1] "s"
		for(i=2; i <= narg; i++) {
			printf "%s=\"$%s\n%s\"\n", var, var, arg[i]
			gsub("[^a-zA-Z_0-9]", "_", arg[i]);
			if(body != "") {
				printf "%s_%s() { %s\n }\n", arg[1], arg[i], body
				body = sprintf("%s_%s \"$@\"", arg[1], arg[2])
			}
		}
	}
	/^(Event|Key|Action)[ \t]/ {
		addevent()
		split($0, arg)
		narg = NF
		body = ""
	}
	/^[ \t]/ {
		body = body"\n"$0
	}

	END {
		addevent()
	}
!
}

_wi_text() {
	eval "cat <<!
$(sed "$_sed" | sed '/^[ 	]/s/\([$`]\)/\\\1/g')
"
}

wi_events() {
	_sed=""
	if [ "$1" = -s ]; then
		_sed="s/^$2//"
		shift 2
	fi
	eval "$(_wi_text | awk "$(_wi_script)")"
}

wi_fatal() {
	echo $scriptname: Fatal: $*
	exit 1
}

wi_notice() {
	xmessage $scriptname: Notice: $*
}

wi_readctl() {
	wmiir read /ctl | sed -n 's/^'$1' //p'
}

wmiifont="$(wi_readctl font)"
wmiinormcol="$(wi_readctl normcolors)"
wmiifocuscol="$(wi_readctl focuscolors)"

wi_menu() {
	eval "wi_menu() { $WMII_MENU"' "$@"; }'
	wi_menu "$@"
}
wi_9menu() {
	eval "wi_9menu() { $WMII_9MENU"' "$@"; }'
	wi_9menu "$@"
}

wi_proglist() {
        ls -lL $(echo $* | sed 'y/:/ /') 2>/dev/null \
		| awk '$1 ~ /^[^d].*x/ { print $NF }' \
		| sort | uniq
}

wi_actions() {
	{	wi_proglist $WMII_CONFPATH
	 	wi_getfuns Action
	} | sort | uniq
}

conf_which() {
	which=$(which which)
	prog=$(PATH="$WMII_CONFPATH" $which $1); shift
	[ -n "$prog" ] && $prog "$@"
}

wi_script() {
	_noprog=true
	if [ "$1" = -f ]; then
		shift
		_noprog=/dev/null
	fi
	which=$(which which)
	_prog=$(PATH="$WMII_CONFPATH" $which $1 || echo $_noprog); shift
	shift; echo "$_prog $*"
}

wi_runcmd() {
	if [ "$1" = -t ]; then
		shift
		set -- wihack -tags $(wmiir read /tag/sel/ctl | sed 1q) "$*"
	fi
	eval exec $* &
}

wi_tags() {
	wmiir ls /tag | sed 's,/,,; /^sel$/d'
}

wi_eventloop() {
	echo "$Keys" | wmiir write /keys

	wmiir read /event | while read wi_event
	do
		OIFS="$IFS"; IFS="$wi_nl"
		wi_arg=$(echo "$wi_event" | sed 's/^[^ ]* //')
		IFS="$OIFS"
		set -- $wi_event
		event=$1; shift
		Event_$event $@
	done 2>/dev/null
}

wi_events <<'!'
Event Start
	if [ "$1" = "$wmiiscript" ]; then
		exit
	fi
Event Key
	fn=$(echo "$@" | sed 's/[^a-zA-Z_0-9]/_/g')
	Key_$fn "$@"
!

Action() {
	action=$1; shift
	if [ -n "$action" ]; then
		Action_$action "$@" \
		|| conf_which $action "$@"
	fi
}

