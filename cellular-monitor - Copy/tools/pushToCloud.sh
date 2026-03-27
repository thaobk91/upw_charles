#!/bin/bash

# Change to the project root directory (parent of tools directory)
cd "$(dirname "$0")/.." || exit 1

CONFIG_FILE="tools/build_configs.txt"

# Function to upload build to S3
# Args: build_dir s3_path [config_name]
upload_to_s3() {
    local build_dir="$1"
    local s3_path="$2"
    local config_name="$3"
    
    # Check if build directory exists
    if [ ! -d "$build_dir" ]; then
        echo "Error: Build directory '$build_dir' not found"
        exit 1
    fi

    # Check if app_update.bin exists
    local bin_path="${build_dir}/zephyr/app_update.bin"
    if [ ! -f "$bin_path" ]; then
        echo "Error: app_update.bin not found in ${bin_path}"
        exit 1
    fi

    # Confirmation step
    echo -e "\nPlease confirm:"
    if [ -n "$config_name" ]; then
        echo "Configuration: ${config_name}"
    fi
    echo "Build: ${build_dir}"
    echo "Destination: ${s3_path}"
    read -p "Proceed with upload? (y/N): " confirm

    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        echo "Upload cancelled"
        exit 0
    fi

    # Upload to S3
    echo "Uploading ${bin_path} to ${s3_path}..."
    aws s3 cp "$bin_path" "$s3_path"

    if [ $? -eq 0 ]; then
        echo "Upload successful!"
    else
        echo "Upload failed!"
        exit 1
    fi
}

echo "=== Push to Cloud ==="

# Try to load predefined builds from config
if [ -f "$CONFIG_FILE" ]; then
    # Read config file into arrays (skip comment lines)
    declare -a config_names
    declare -a config_build_dirs
    declare -a config_s3_buckets
    
    while IFS='|' read -r name build_dir s3_bucket; do
        # Skip comments and empty lines
        [[ "$name" =~ ^#.*$ ]] && continue
        [[ -z "$name" ]] && continue
        
        config_names+=("$name")
        config_build_dirs+=("$build_dir")
        config_s3_buckets+=("$s3_bucket")
    done < "$CONFIG_FILE"
    
    # If we have predefined builds, show them
    if [ ${#config_names[@]} -gt 0 ]; then
        echo -e "\nPredefined builds:"
        for i in "${!config_names[@]}"; do
            echo "$((i+1)). ${config_names[$i]}"
        done
        echo "0. Custom build and destination"
        
        # Get user selection
        while true; do
            read -p $'\nSelect option (0-'"${#config_names[@]}"'): ' choice
            if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 0 ] && [ "$choice" -le "${#config_names[@]}" ]; then
                if [ "$choice" -eq 0 ]; then
                    # Custom mode - break and continue to custom logic
                    break
                else
                    # Predefined mode
                    idx=$((choice-1))
                    selected_build="${config_build_dirs[$idx]}"
                    selected_s3="${config_s3_buckets[$idx]}"
                    selected_name="${config_names[$idx]}"
                    
                    upload_to_s3 "$selected_build" "$selected_s3/app_update.bin" "$selected_name"
                    exit 0
                fi
            else
                echo "Invalid selection. Please enter 0-${#config_names[@]}."
            fi
        done
    fi
fi

# Custom mode (original script logic)
# Find all build directories
build_dirs=($(ls -d build_* 2>/dev/null))

if [ ${#build_dirs[@]} -eq 0 ]; then
    echo "No build directories found!"
    exit 1
fi

# Print available builds
echo -e "\nAvailable builds:"
for i in "${!build_dirs[@]}"; do
    echo "$((i+1)). ${build_dirs[$i]}"
done

# Get user selection
while true; do
    read -p "Select build number (1-${#build_dirs[@]}): " selection
    if [[ "$selection" =~ ^[0-9]+$ ]] && [ "$selection" -ge 1 ] && [ "$selection" -le "${#build_dirs[@]}" ]; then
        selected_build="${build_dirs[$((selection-1))]}"
        break
    else
        echo "Invalid selection. Please try again."
    fi
done

# Get available S3 folders
echo -e "\nFetching available S3 destinations..."
s3_base="s3://nrf91-fotas"

# Get folders and store in array
IFS=$'\n' s3_folders=($(aws s3 ls "$s3_base/" | grep PRE | awk '{print $2}' | sed 's#/$##'))

if [ ${#s3_folders[@]} -eq 0 ]; then
    echo "No folders found in $s3_base"
    exit 1
fi

# Print available destinations
echo -e "\nAvailable S3 destinations:"
for i in "${!s3_folders[@]}"; do
    echo "$((i+1)). ${s3_folders[$i]}"
done

# Get destination selection
while true; do
    read -p "Select destination number (1-${#s3_folders[@]}): " dest_selection
    if [[ "$dest_selection" =~ ^[0-9]+$ ]] && [ "$dest_selection" -ge 1 ] && [ "$dest_selection" -le "${#s3_folders[@]}" ]; then
        selected_folder="${s3_folders[$((dest_selection-1))]}"
        s3_path="$s3_base/$selected_folder/app_update.bin"
        break
    else
        echo "Invalid selection. Please try again."
    fi
done

# Upload using shared function
upload_to_s3 "$selected_build" "$s3_path"
