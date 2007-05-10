/^[ 	]*WMII_TERM[ 	]*=/ {
	print "E " FILENAME
	print FNR "c"
	print
	print "."
	print "w"
	nextfile
}

