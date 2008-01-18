#!/bin/sh -f

usage() {
	echo 1>&2 Usage: \
	"$0 [-transient <window>] [-type <window_type>[,...]] [-tags <tags>] <command> [<arg> ...]"
	exit 1
}

checkarg='[ ${#@} -gt 0 ] || usage'

while [ ${#@} -gt 0 ]
do
	case $1 in
	-transient)
		shift; eval $checkarg
		export WMII_HACK_TRANSIENT=$1
		shift;;
	-type)
		shift; eval $checkarg
		export WMII_HACK_TYPE=$1
		shift;;
	-tags)
		shift; eval $checkarg
		export WMII_HACK_TAGS=$1
		shift;;
	-*)
		usage;;
	*)
		break;;
	esac
done

eval $checkarg

if [ ! -u "`which $1`" -a ! -g "`which $1`" ]
then
	export LD_PRELOAD=libwmii_hack.so
	export LD_LIBRARY_PATH="LIBDIR${LD_LIBRARY_PATH:+:}${LD_LIBRARY_PATH}"
else
	unset WMII_HACK_TRANSIENT WMII_HACK_TYPE WMII_HACK_TAGS
fi
exec "$@"

