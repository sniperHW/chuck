#!/bin/sh

lua gen_proto.lua
cd proto
for file in $(ls *)
do
	echo "$file" |grep -q ".proto"
	if [ $? -eq 0 ]
	then
		temp_file=`basename $file .proto`
	    protoc -o "$temp_file.pb" "$file"
	fi
done
cd ..
lua gen_pbc.lua
