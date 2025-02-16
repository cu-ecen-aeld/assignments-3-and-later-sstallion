#!/bin/sh
# finder.sh filesdir searchstr

if [[ $# != 2 ]]; then
	echo >&2 "Usage: $0 filesdir searchstr"
	exit 1
fi

filesdir=$1
searchstr=$2
if [[ ! -d $filesdir ]]; then
	echo >&2 "$0: $filesdir is not a directory"
	exit 1
fi

files=0
lines=0
for f in $filesdir/**; do
	if [[ ! -f $f ]]; then
		continue
	fi
	files=$(( $files + 1 ))
	lines=$(( $lines + $(grep "${searchstr}" $f | wc -l) ))
done
echo "The number of files are $files and the number of matching lines are $lines"
exit 0
