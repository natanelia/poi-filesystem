#!/bin/bash
echo "// READ FILE TEST //"
file="../coba/test"
while read line
do
	echo "$line"
done <"$file"
