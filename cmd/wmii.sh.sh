
if [ -z "$scriptname" ]; then
	scriptname="$wmiiscript"; fi
echo Start $wmiiscript | wmiir write /event 2>/dev/null ||
	exit 1

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
			if(body == "")
				delete a[arg[1],arg[i]]
			else
				a[arg[1],arg[i]] = body
			if(i == 2) {
				# There's a bug here. Can you spot it?
				gsub("[^a-zA-Z_0-9]", "_", arg[2]);
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
		for(k in a) {
			split(k, b, SUBSEP)
			c[b[1]] = c[b[1]] b[2] "\n"
			gsub("[^a-zA-Z_0-9]", "_", b[2]);
			if(body != "")
				printf "%s_%s() { %s\n }\n", b[1], b[2], a[k]
		}
		for(k in c) {
			gsub("'", "'\"'\"'", c[k])
			printf "%ss='%s'\n", k, c[k]
		}
	}
!
}

_wi_text() {
	cat <<'!'
Event Start
	if [ "$1" = "$wmiiscript" ]; then
		exit
	fi
Event Key
	fn=$(echo "$@" | sed 's/[^a-zA-Z_0-9]/_/g')
	Key_$fn "$@"
!
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
	#cho "$(_wi_text | awk "$(_wi_script)")"
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
	 	echo -n "$Actions"
	} | sort | uniq
}

wi_runconf() {
	sflag=""; if [ "$1" = -s ]; then sflag=1; shift; fi
	which="$(which which)"
	prog=$(PATH="$WMII_CONFPATH" "$which" -- $1 2>/dev/null); shift
	if [ -n "$prog" ]; then
		if [ -z "$sflag" ]
		then "$prog" "$@"
		else . "$prog"
		fi
	else return 1
	fi
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
	eval exec "$*" &
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

Action() {
	action=$1; shift
	if [ -n "$action" ]; then
		Action_$action "$@" \
		|| wi_runconf $action "$@"
	fi
}

