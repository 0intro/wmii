#!/bin/sh
# Heavy access test of wmiiwm's fs

dump_fs() {
	echo $1
	wmiir read $1|wmiir write $1;
	for i in `wmiir read $1|awk '{print $10}'`
	do
		if test $i != "event"
		then
			dump_fs $1/$i
		fi
	done
}

while true
do
	dump_fs /
done
