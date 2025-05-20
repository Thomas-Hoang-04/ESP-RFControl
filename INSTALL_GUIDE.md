# ESP-IDF Project Installation Guide

_Note: This installation guide will target VS Code user. Eclipse user can refer
to the
[**Espressif official plugin**](https://github.com/espressif/idf-eclipse-plugin/blob/master/README.md)
along with this video [**here**](https://www.youtube.com/watch?v=w8FkCJEAE90).
Users who wish to use other Code Editor/IDE can follow the guide on
[**Section 2: Install ESP-IDF**](#2-install-esp-idf) to manually install the
framework on their operating system of choice_

---

## Installation Steps

### Prerequisites

#### Hardware

- An ESP32 development board _(i.e. ESP32, ESP32-C3, ESP32-S3, etc)_
- USB cable (USB-A to Micro-USB, USB-A/USB-C to USB-C depending on your board)
- Computer running Windows/Linux

#### Software

_For Windows:_

- Windows 10 or later
- Git for Windows
- VS Code

_For Linux:_

- Linux _(Ubuntu 20.04 or later recommended)_
- Git
- Python 3.8 or newer with pip
- VS Code

### 1. Install VS Code

- Download and install [Visual Studio Code](https://code.visualstudio.com/)
  - For Ubuntu/Debian: `sudo apt install code` or download .deb package from the
    website
  - For other Linux distributions: Download appropriate package from the website

### 2. Install ESP-IDF

> **Note:** This section is meant for those who wish to manually install the
> framework on their operating system and integrate it into your editor of
> choice. Those who use VS Code could also choose to download and set up ESP-IDF
> from the extension for a more simplified procedure _(refer to
> [**Section 4: Configure VS Code Extension**](#4-configure-vs-code-extension)
> for more details)_

#### For Windows Users:

1. Download the
   [ESP-IDF Windows Installer](https://dl.espressif.com/dl/esp-idf/?idf=4.4)
2. Run the installer and follow the on-screen instructions
3. Select "Express Installation" for simplicity
4. Use recommended installation directory (`%userprofile%\Desktop\esp-idf`)
5. At the end of installation, check "Run ESP-IDF PowerShell Environment"

#### For Linux Users:

1. Install prerequisites:

   ```bash
   sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
   ```

2. Create a directory for ESP-IDF:

   ```bash
   mkdir -p ~/esp
   cd ~/esp
   ```

3. Clone the ESP-IDF repository:

   ```bash
   git clone --recursive https://github.com/espressif/esp-idf.git
   ```

4. Run the installation script:

   ```bash
   cd ~/esp/esp-idf
   ./install.sh esp32,esp32c3,esp32s3
   ```

5. Set up environment variables:

   ```bash
   . ~/esp/esp-idf/export.sh
   ```

   Add this line to your ~/.bashrc or ~/.zshrc to make it permanent:

   ```bash
   echo ". $HOME/esp/esp-idf/export.sh" >> ~/.bashrc
   ```

### 3. Install VS Code Extension

1. Open VS Code
2. Go to **Extensions** (Ctrl+Shift+X)
3. Search for "Espressif IDF"
4. Install the "Espressif IDF" extension

### 4. Configure VS Code Extension

1. Open VS Code
2. Press `F1` or `Ctrl+Shift+P` to open the command palette
3. Type `ESP-IDF: Configure ESP-IDF extension` and select it
4. Choose `Express` mode
5. Select the ESP-IDF directory:
   - Windows: typically `%userprofile%\Desktop\esp-idf`
   - Linux: typically `$HOME/esp/esp-idf`
6. Select your target chip (ESP32, ESP32-C3, ESP32-S3, etc.)
7. Complete the configuration process

## Running Your Project

### 1. Open Your Project in VS Code

1. Launch VS Code
2. Go to `File > Open Folder`
3. Navigate to your project directory
4. Click `Select Folder`

### 2. Build the Project

1. In VS Code, press `F1` and select "ESP-IDF: Add VS Code configuration
   folder". This will contain the JSON files with attributes for VS Code to
   recognize ESP-IDF compiler & SDK options for this project
2. After the folder is added, press `F1` and select
   `ESP-IDF: Select Espressif Device Target`
3. Choose your target chip type _(ESP32, ESP32-C3, ESP32-S3, etc.)_
4. After the chip selection is finished, press `Ctrl+E G` or press `F1` again
   and select `ESP-IDF: SDK Configuration Editor (menuconfig)` to generate an
   SDK config file for the project
   > The properties in this menuconfig file can be edited to configure the SDK
   > and the targeted device for the project. _**Edit with caution** as any
   > change can be breaking (Read
   > [**the official documentation**](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/index.html)
   > of ESP-IDF **for the targeted device** before editing this file)_
5. After the `menuconfig` file is created, close the Configuration Editor
6. Inside the project folder, press `Ctrl+E B` or press `F1` again and select
   `ESP-IDF: Build project`
7. Wait for the build to complete

### 3. Flash to Device

1. Connect your ESP32 board to your computer via USB
2. Identify the serial port:
   - Windows: Check `Device Manager > Ports (COM & LPT)`
   - Linux: Run `ls /dev/tty*` (usually appears as `/dev/ttyUSB0` or
     `/dev/ttyACM0`)
3. Press `F1` and select `ESP-IDF: Select port to use (COM, tty, usbserial)`
4. Choose the correct port
5. Press `F1` again and select `ESP-IDF: Flash device`
6. Wait for the flash sequence to complete

### 4. Monitor Serial Output

1. Press `F1` and select `ESP-IDF: Monitor device`
2. View the serial output from your device
3. Press `Ctrl+]` to exit the monitor

## Troubleshooting

### Windows Issues

- If you encounter "port not found" errors, check your USB connection and
  drivers
- Ensure you have the correct COM port selected
- For permission issues, try running VS Code as administrator

### Linux Issues

- If you get permission errors when accessing serial ports, add your user to the
  dialout group:
  ```bash
  sudo usermod -a -G dialout $USER
  ```
  Then log out and log back in for changes to take effect.
- If the device is not recognized, try installing/reinstalling additional USB
  drivers:
  ```bash
  sudo apt-get remove brltty
  sudo apt-get install brltty
  ```
- For installation errors, make sure all dependencies are installed correctly

---

_For more detailed information, refer to
[**Espressif's official documentation**](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)._

_**Created by Minh Hai Hoang. ©2025**_
