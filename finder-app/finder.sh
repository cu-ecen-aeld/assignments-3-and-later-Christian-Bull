#!/bin/sh

set -e
set -u

if [ $# -lt 2 ]; then
    echo "Parameters not specified"
    exit 1
fi

dir=$1

if [ ! -z "$dir" ]; then
    echo "$1 exists"
    
    grep -crnw $dir -e $2

    numfiles=$(eval grep -crnw $dir -e $2 | wc -l)
    nummatches=$(eval grep -rnw $dir -e $2 | wc -l)

    echo "The number of files are $numfiles and the number of matching lines are $nummatches"
else
	echo "$1 not found"
	exit 1
fi
