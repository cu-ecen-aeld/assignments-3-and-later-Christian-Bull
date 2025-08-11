#!/bin/sh

set -e
set -u

if [ $# -lt 2 ]; then
    echo "Parameters not specified"
    exit 1
fi

file=$1

mkdir -p "${file%/*}" && touch "$file"

echo $2 > $file
