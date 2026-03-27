#!/bin/bash
protoc --include_imports -o filedescriptor.desc msg.proto
if [ $? -ne 0 ]; then
    echo "Error: protoc command failed."
    exit 1
fi
