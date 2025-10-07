#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Error: Both parameters (directory and search string) must be specified. OKAYYY."
    exit 1
fi

filesdir="$1"
searchstr="$2"

if [ ! -d "$filesdir" ]; then
    echo "Error: '$filesdir' is not a valid directory."
    exit 1
fi

#file_count=$(find "$filesdir" -type f | wc -l)
file_count=$(ls -R -p $filesdir | grep -v / | wc -l)
#matching_lines=$(grep -r -c "$searchstr" "$filesdir" | awk -F: '{sum += $2} END {print sum}')
matching_lines=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

if [ $? -ne 0 ]; then
    echo "Error: Failed to search for '$searchstr' in '$filesdir'."
    exit 1
fi

echo "The number of files are $file_count and the number of matching lines are $matching_lines"
