#!AWKPATH -f
BEGIN {
	arg[1] = "Nop"
	body = "";
	writekeys = "wmiir write /keys"
	print "IFS=''"
}

function addevent() {
	if(arg[1] == "Key")
		keys[arg[2]] = 1;

	var = arg[1] "s"
	print var "=\"$" var " " arg[2] "\""

	gsub("[^a-zA-Z_0-9]", "_", arg[2]);
	if(body != "")
		print arg[1] "_" arg[2] "() {" body "\n}"
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
	for(key in keys)
		print key | writekeys;
	close(writekeys);
}
