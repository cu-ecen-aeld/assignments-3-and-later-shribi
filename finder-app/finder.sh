#!/bin/sh
#Author: shribi

set -e
set -u

FINDDIR=/home
FINDSTR=""
IS_FILE=0

if [ $# -ne 2 ];
then 
	echo 2 Args Required
	exit 1
fi

if [ -f $1 ]; then
	IS_FILE=1
fi

FINDDIR=$1

if [ -z $2 ];
then
	echo Enter a valid string
	exit 1
else
	FINDSTR=$2
fi

echo Finding $2 in $1 ...

file_count=0
match_lines=0
if [ $IS_FILE -eq 1 ]; then
	file_count=1
	match_lines=$(grep -F "$FINDSTR" "$FINDDIR" 2>/dev/null | wc -l)
else
	file_count=$(find "$FINDDIR" -type f | wc -l)
	match_lines=$(grep -r -h -F "$FINDSTR" "$FINDDIR" 2>/dev/null | wc -l)
fi

echo "The number of files are $file_count and the number of matching lines are $match_lines"
