#!/bin/bash

# Check if AWS CLI is installed
if ! command -v aws &> /dev/null; then
    echo "Error: AWS CLI is not installed. Please install it first."
    exit 1
fi

# Set variables
S3_BUCKET="s3://nowi-protobuf-filedesc/msg"
FILE_NAME="filedescriptor.desc"

# Check if file exists
if [ ! -f "$FILE_NAME" ]; then
    echo "Error: $FILE_NAME not found in current directory"
    exit 1
fi

# Push file to S3
echo "Uploading $FILE_NAME to $S3_BUCKET..."
aws s3 cp "$FILE_NAME" "$S3_BUCKET/"

# Check if upload was successful
if [ $? -eq 0 ]; then
    echo "Successfully uploaded $FILE_NAME to $S3_BUCKET"
else
    echo "Error: Failed to upload $FILE_NAME to $S3_BUCKET"
    exit 1
fi
