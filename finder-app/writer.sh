#!/bin/sh
# writer.sh writefile writestr

if [[ $# != 2 ]]; then
	echo >&2 "Usage: $0 writefile writestr"
	exit 1
fi

writefile=$1
writestr=$2

mkdir -p $(dirname $writefile)
echo $writestr >$writefile || exit 1
exit 0
