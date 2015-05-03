#!/bin/bash
../mount-poi ../drive ../drive.poi
echo "// READ FILE TEST //"
file="../drive/GoogleFile.txt"
while read line
do
	echo "$line"
done <"$file"
sleep 0.1
fusermount -u ../drive
