
[ -z "$scriptname" ] && scriptname="$wmiiscript"
echo Start $wmiiscript | wmiir write /event 2>/dev/null ||
	exit 1

wi_newline='
'

_wi_script() {
	# Awk script to mangle key/event/action definition spec
	# into switch-case functions and lists of the cases. Also
	# generates a simple key binding help text based on KeyGroup
	# clauses and comments that appear on key lines.
	#
	# Each clause (Key, Event, Action) generates a function of the
	# same name which executes the indented text after the matching
	# clause. Clauses are selected based on the first argument passed
	# to the mangled function. Additionally, a variable is created named
	# for the plouralized version of the clause name (Keys, Events,
	# Actions) which lists each case value. These are used for actions
	# menus and to write wmii's /keys file.
	cat <<'!'
	BEGIN {
		arg[1] = "Nop"
		narg = 1;
		body = "";
		keyhelp = ""
	}
	function quote(s) {
		gsub(/'/, "'\\''", s)
		return "'" s "'"
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
				body = sprintf("%s %s \"$@\"", arg[1], arg[2])
			}
		}
	}
	/^(Key)Group[ \t]/ {
		sub(/^[^ \t]+[ \t]+/, "")
		keyhelp = keyhelp "\n  " $0 "\n"
	}
	/^(Event|Key|Action|Menu)[ \t]/ {
		addevent()
		split($0, tmp, /[ \t]+#[ \t]*/)
		narg = split(tmp[1], arg)
		if(arg[1] == "Key" && tmp[2])
			for (i=2; i <= narg; i++)
				keyhelp = keyhelp sprintf("    %-20s %s\n",
					        arg[i], tmp[2])
		body = ""
	}
	/^[ \t]/ {
		sub(/^(        |\t)/, "")
		body = body"\n"$0
	}

	END {
		addevent()
		for(k in a) {
			split(k, b, SUBSEP)
			c[b[1]] = c[b[1]] b[2] "\n"
			if(body != "")
				d[b[1]] = d[b[1]] quote(b[2]) ")" a[k] "\n;;\n"
		}
		for(k in c)
			printf "%ss=%s\n", k, quote(c[k])
		for(k in d) {
			printf "%s() {\n", k
			printf " %s=$1; shift\n", tolower(k)
			printf "case $%s in\n%s\n*) return 1\nesac\n", tolower(k), d[k]
			printf "}\n"
		}
		print "KeysHelp=" quote(keyhelp)
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
	Key "$@"
!
	eval "cat <<!
$( (test ! -t 0 && cat; for a; do eval "$a"; done) | sed '/^[ 	]/s/\([$`\\]\)/\\\1/g')
!
"
}

wi_events() {
	#cho "$(_wi_text "$@" | awk "$(_wi_script)")" | cat -n
	eval "$(_wi_text "$@" | awk "$(_wi_script)")"
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

wi_fnmenu() {
	group="$1-$2"; shift 2
	_last="$(echo $group|tr - _)_last"
	eval "last=\"\$$_last\""
	res=$(set -- $(echo "$Menus" | awk -v "s=$group" 'BEGIN{n=length(s)}
		         substr($1,1,n) == s{print substr($1,n+2)}')
	      [ $# != 0 ] && wmii9menu -i "$last" "$@")
	if [ -n "$res" ]; then
		eval "$_last="'"$res"'
		Menu $group-$res "$@"
	fi
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

wi_seltag() {
	wmiir read /tag/sel/ctl | sed 1q | tr -d '\012'
}

wi_selclient() {
	wmiir read /client/sel/ctl | sed 1q | tr -d '\012'
}

wi_readevent() {
	wmiir read /event
}

wi_eventloop() {
	echo "$Keys" | wmiir write /keys

	wi_readevent | while read wi_event
	do
		IFS="$wi_newline"
		wi_arg=$(echo "$wi_event" | sed 's/^[^ ]* //')
		unset IFS
		set -- $wi_event
		event=$1; shift
		Event $event "$@"
	done
	true
}

action() {
	action=$1; shift
	if [ -n "$action" ]; then
		set +x
		Action $action "$@" \
		|| wi_runconf $action "$@"
	fi
}

