#!/bin/sh

eval "text() {
	cat <<!
$(sed '/^[[:blank:]]/s/\([$`]\)/\\\1/g')
!
}"

script() {
	cat <<'!'
	BEGIN {
		arg[1] = "Nop"
		body = "";
	}
	function addevent() {
		if(arg[1] == "Key")
			keys[arg[2]] = 1;

		var = arg[1] "s"
		printf "%s=\"$%s %s\"\n", var, var, arg[2]

		gsub("[^a-zA-Z_0-9]", "_", arg[2]);
		if(body != "")
			printf "%s_%s() { %s\n }\n", arg[1], arg[2], body
	}

	/^(Event|Key|Action)[ \t]/ {
		addevent()
		split($0, arg)
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

text | awk "`script`"

