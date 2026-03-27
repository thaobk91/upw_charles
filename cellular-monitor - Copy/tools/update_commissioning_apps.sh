#!/bin/bash

# Script to update commissioning app hex files from build directories
# This script copies merged.hex files from build directories and renames them
# to the appropriate commissioning app files in tools/files/
# It also creates a build package for manufacturer distribution

set -e  # Exit on any error

echo "Updating commissioning app hex files..."

# Create files directory if it doesn't exist
mkdir -p files

bm_path="../build_nowi_nrf9151_commission/zephyr/merged.hex"
pm_path="../build_pipe_monitor_commission_nrf9151/zephyr/merged.hex"

# Copy and rename building monitor commissioning app
if [ -f "$bm_path" ]; then
    echo "Copying building monitor commissioning app..."
    cp "$bm_path" "files/commissioning_app_nrf9151.hex"
    echo "✓ Updated files/commissioning_app_nrf9151.hex"
else
    echo "✗ Error: $bm_path not found"
    exit 1
fi


# Copy and rename pipe monitor commissioning app
if [ -f "$pm_path" ]; then
    echo "Copying pipe monitor commissioning app..."
    cp "$pm_path" "files/commissioning_app_pm_nrf9151.hex"
    echo "✓ Updated files/commissioning_app_pm_nrf9151.hex"
else
    echo "✗ Error: $pm_path not found"
    exit 1
fi

echo "All commissioning app files updated successfully!"

# Create build package for manufacturer
echo "Creating build package for manufacturer..."

# Create build directory
mkdir -p files/build
mkdir -p files/build/files

# Copy provision script to build directory
cp provision.py files/build/
echo "✓ Copied provision.py to build package"

# Copy commissioning app files to build directory
cp files/commissioning_app_nrf9151.hex files/build/files/
cp files/commissioning_app_pm_nrf9151.hex files/build/files/
echo "✓ Copied commissioning app files to build package"

# Copy firmware files to build directory (if they exist)
if [ -f "files/mfw_nrf91x1_2.0.2.zip" ]; then
    cp files/mfw_nrf91x1_2.0.2.zip files/build/files/
    echo "✓ Copied mfw_nrf91x1_2.0.2.zip to build package"
fi

# Deprecated these were for nrf9160
# if [ -f "files/mfw_nrf9160_1.3.7.zip" ]; then
#     cp files/mfw_nrf9160_1.3.7.zip files/build/files/
#     echo "✓ Copied mfw_nrf9160_1.3.7.zip to build package"
# fi

# if [ -f "files/commissioning_app.hex" ]; then
#     cp files/commissioning_app.hex files/build/files/
#     echo "✓ Copied commissioning_app.hex to build package"
# fi

echo "Build package created successfully in files/build/"
echo "Ready for manufacturer distribution!" 