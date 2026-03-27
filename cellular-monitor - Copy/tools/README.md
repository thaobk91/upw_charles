# Requirements

nrfjpro

- Download here: [https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools/Download](https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools/Download)

Python3

Python libraries:

- pyserial
- argparse (included in Python standard library)

**Files**

- provision.py
- files/commissioning_app.hex for nrf9160 board
  - files/commissioning_app_nrf9151.hex for nrf9151 board
- files/merged.hex
  - Application
- files/mfw_nrf9160_1.3.7.zip
  - Modem Firmware (Version 1.3.7 may differ)
- files/mfw_nrf91x1_2.0.3.zip
  - Modem Firmware for nRF9151 boards

# Running

Download the needed requirements.

Place the needed files In the files/ folder of where you're running the provision.py script from.

Attach the device to a SWD programmer and a UART communication device.

You'll need to find the location of the UART device, for example /dev/tty.usbserial-B001BOIR

## Command Line Options

The provision script now supports command-line arguments for easier automation:

```
python provision.py [device_id] [--board=BOARD_TYPE] [--pulse=Y/N] [--device=DEVICE_PATH]
```

Options:

- `device_id`: The device ID or `i` to auto-increment (default: `i`)
- `--board`: Board type, either `nrf9151` or `nrf9160`
- `--pulse`: Whether the device is a pulse tracker (`y`) or magnetometer (`n`)
- `--device`: Path to the UART device

Examples:

1. Basic usage in increment mode:

   ```
   python provision.py i
   ```

2. Full command-line specification (no prompts):

   ```
   python provision.py 202408010000001 --board=nrf9151 --pulse=y --device=/dev/tty.usbserial-B001BOIR
   ```

3. Increment device ID with specific board type:

   ```
   python provision.py i --board=nrf9160 --pulse=n --device=COM24
   ```

4. Legacy format (still supported):

   ```
   python provision.py i /dev/tty.usbserial-B001BOIR
   ```

The script should end with the STDOUT of SUCCESS. Otherwise It list It's failure reason.

It should also be verified that the device blinks all three of It's LEDs (RED,GREEN, and BLUE) then blinks BLUE once.

If It flashes RED for 5 seconds this likely Indicates a SIM card Insertion Issue.

If It flashes RED for 15 seconds this Indicates that the sensor Is not corrected properly.

## Running on Windows

Install Python3

Use Command Prompt

Install pyserial:
pip install pyserial

The program will automatically figure out what programmer is attached to the computer. You have to figure out which COM port is for the UART USB device. You can find it in Device Manager under Ports or if you run the provisioning program like below it'll tell you what ports are available:

### Example finding USB port

Run command:
python provision.py i

Output:
last_device.txt file not found
device_id: 2024060000000000
Could not find UART device at location: /dev/tty.usbserial-B001BOIR make sure it is not in use already in another program.
Ports:
COM24 - USB Serial Port (COM24)
COM3 - JLink CDC UART Port (COM3)
COM5 - JLink CDC UART Port (COM5)
COM8 - nRF Connect USB CDC ACM (COM8)
COM4 - JLink CDC UART Port (COM4)

I found COM24 to be the USB UART device I needed.

Next Run command with COM port:
python provision.py i --device=COM24

Or using the legacy format:
python provision.py i COM24
