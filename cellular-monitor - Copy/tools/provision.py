#!/usr/bin/env python3

import serial
import serial.tools.list_ports
import sys
import time
import subprocess
import argparse

"""
Provision Script for Cellular Building Monitor

This script provisions a device by programming it with firmware and setting configuration.

Examples:
    Incremental mode:
    $ python provision.py i
    
    CLI mode with all parameters:
    $ python provision.py 202408010000001 --board=nrf9151 --pulse=y --device=/dev/ttyUSB0 --devicetype=pm
    
    Increment device ID with specific board type:
    $ python provision.py i --board=nrf9160 --pulse=n --devicetype=bm
    
    Legacy format (still supported):
    $ python provision.py i /dev/tty.usbserial-B001BOIR
"""

timeout=1

# Set up argument parser
parser = argparse.ArgumentParser(description='Provision a cellular building monitor device')
parser.add_argument('device_id', nargs='?', default='i', help='Device ID or "i" to increment from last device ID')
parser.add_argument('--board', choices=['nrf9151', 'nrf9160'], help='Board type (nrf9151 default or nrf9160)')
parser.add_argument('--pulse', choices=['y', 'n'], help='Is this a pulse tracker (y) or magnetometer (n)')
parser.add_argument('--device', help='Serial device location')
parser.add_argument('--devicetype', choices=['bm', 'pm'], default='bm', help='Device type: bm (building monitor) or pm (pipe monitor)')
parser.add_argument('--program', help='Whether to skip programming or not')

# Parse known args first to handle the case of positional device_location for backward compatibility
args, unknown = parser.parse_known_args()

# default location
device_location='/dev/tty.usbserial-B001BOIR'

program = True
if (args.program == 'n'):
    print("Skipping programming")
    program = False

# Process device location argument - with backward compatibility
if args.device:
    device_location = args.device
elif len(unknown) > 0:
    # Old-style: python provision.py <device_id> <device_location>
    device_location = unknown[0]
    print(f"Using device location from positional argument: {device_location}")

# Process board type argument
if args.board:
    nrf9151 = 'y' if args.board == 'nrf9151' else 'n'
else:
    # Default to nrf9151 board
    nrf9151 = 'y'

# Process pulse tracker argument
if args.pulse:
    is_pulse_tracker = args.pulse
else:
    # Default to magnetometer (not pulse tracker)
    is_pulse_tracker = 'n'

# Process device type argument
device_type = args.devicetype

# get device ID from input command line args
device_id = args.device_id

if (device_id == 'i'):
    # load last device number from files last_device.txt
    # check if last file exists
    try:
        with open('files/last_device.txt', 'r') as f:
            last_device = f.read()
            print('last_device:', last_device)
            # device_id is in hex so could have A-F characters
            device_id = last_device[:-8] + str(int(last_device[-8:]) + 1).zfill(8)
    except FileNotFoundError:
        print("last_device.txt file not found, using default device ID numbering scheme.")
        # device ID format is YYYYMMMBBXXXXXXXX where YYYY is year, MMM is month, BB is batch 00, XXXXXXXX is device ID starting from 00000000
        # get current year and month
        year = str(time.localtime().tm_year)
        month = str(time.localtime().tm_mon).zfill(2)
        device_id = year + month + '00' + '00000000'

print('device_id:', device_id)
print('board type:', 'nrf9151' if nrf9151 == 'y' else 'nrf9160')
print('device type:', 'Pulse Tracker' if is_pulse_tracker == 'y' else 'Magnetometer')
print('device category:', 'Pipe Monitor' if device_type == 'pm' else 'Building Monitor')
print('device location:', device_location)

try:
    port = serial.Serial(device_location, baudrate=115200, timeout=timeout)
except:
    print("Could not find UART device at location:", device_location, " make sure it is not in use already in another program.")
    print("Ports: ")
    for port in serial.tools.list_ports.comports():
        print(port)
    sys.exit(1)

print("port open:", port.isOpen())

# Check device ID
if len(device_id) != 16:
    print("Device ID must be 16 characters long")
    sys.exit(1)

print('start')

# Function to run a command and print its output
def run_command(command):
    try:
        print(f"Running command: {' '.join(command)}")
        # Run the command and pipe output to std as it's running
        
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        while True:
            output = process.stdout.readline()
            if output == '' and process.poll() is not None:
                break
            if output:
                print(output.strip())

        # Get the return code
        rc = process.poll()
        print("Return code:", rc)
        if (rc != 0):
            print("FAILED - Error running command")
            sys.exit(1)
        return rc
    except subprocess.CalledProcessError as e:
        # Print the error if the command fails
        print(f"Error:\n{e.stderr}")
        sys.exit(1)

if (program):
    # Run each command
    print("recover")
    run_command(["nrfjprog", "--recover"])
    print("erase")
    run_command(["nrfjprog", "--eraseall"])

    print("load mfw")
    if (nrf9151 == 'y'):
        run_command(["nrfjprog", "--program", "files/mfw_nrf91x1_2.0.3.zip", "--verify"])
    else:
        # run nrfjprog --program files/mfw_nrf9160_1.3.7.zip --verify
        run_command(["nrfjprog", "--program", "files/mfw_nrf9160_1.3.7.zip", "--verify"])

    print("load provisioning app")
    if (nrf9151 == 'y'):
        if (device_type == 'pm'):
            run_command(["nrfjprog", "--program", "files/commissioning_app_pm_nrf9151.hex", "--verify"])
        else:
            run_command(["nrfjprog", "--program", "files/commissioning_app_nrf9151.hex", "--verify"])
    else:
        run_command(["nrfjprog", "--program", "files/commissioning_app.hex", "--verify"])

    print("reset after programming")
    # run nrfjprog --reset
    run_command(["nrfjprog", "--reset"])

print("clear out logs")
response = port.readall()
print(response.decode('ascii'))

# function to write then read response
def write_read(string):
    print("writing: " + string)
    port.write(bytes(string, 'ascii'))
    print('response:\n')
    # response = port.read_until(b"OK")
    # print(response.decode('ascii'))
    start = time.time()
    success = False
    while time.time() < start + timeout:
        line = port.readline()
        print(line.decode('ascii'))
        if b'OK' in line:
            success = True
            break

    if (success == False):
        print('Write failed for command:', string)
        exit()
 
# set device type first before ID
if (is_pulse_tracker == 'y'):
    # Set the device type based on user input
    print("setting device type to pulse tracker")
    pulse_cmd = "AT+PULSE=y\r"
    write_read(pulse_cmd)

print("writing device ID " + device_id)
string = "AT+DEVICEID="+device_id+"\r"
write_read(string)

print("disabling logging")
write_read("AT+LOGGING=0\r")

# APP PROTECT
print("protecting app")
run_command(["nrfjprog", "--rbp", "ALL"])

print("reset to finish")
# run nrfjprog --reset
run_command(["nrfjprog", "--pinreset"]) # have to use pinreset now with app protection on

# save last device number to file
with open('files/last_device.txt', 'w') as f:
    f.write(device_id)

print('SUCCESS ' + device_id)