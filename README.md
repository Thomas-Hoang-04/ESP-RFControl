# Vibration Motor control with RF Signal (433Mhz)

---

This is the source code for my RF Vibration Motor project. The code enables an
ESP32 module to receive RF signals from an RF Receiver and decode those signals,
while also control the included vibration motor module when a certain signal is
received.

The RF signal decoding function is ported from this Arduino library:
[_**rc-switch**_](https://github.com/sui77/rc-switch)

_**Schematics: [OSHWLAB](https://oshwlab.com/thomasdrive04/project-2-mcu-v2)**_

---

### Tools & Techstack

<p>
  <img alt="esp-idf" src="https://img.shields.io/badge/-Espressif-gray?style=for-the-badge&logo=espressif"/>
  <img alt="vs-code" src="https://custom-icon-badges.demolab.com/badge/Visual%20Studio%20Code-0078d7.svg?style=for-the-badge&logo=vsc&logoColor=white"/>
</p>

### Prerequisites

- ESP-IDF v5.3+
- Code Editor/IDE (preferably VS Code/Eclipse or Espressif-IDE for optimal
  support)

### Installation Guide

_**Refer to this installation guide [here](./INSTALL_GUIDE.md)**_

### Project Structures

```
├── main/ - Main project code
    ├── CMakeLists.txt - Directory level CMake file for build target & ESP native component management
    ├── main.c - Main app terminal
    ├── rf_common.h - Macros, types, constant and template interface for the main app functionalities
    ├── rf_receiver.c - Implement the RF reception & signal decoding functions
    ├── rf_timer.c - Implement the ESP32 GPTimer API for LED & motor status control
    ├── rf_utility.c - Implement utility functions for reset and LED trigger
├── .gitignore - List of files & directories ignored by Git versioning
├── CMakeELists.txt - Project level CMake file for general management
├── LICENSE - GNU General Public License v3.0 license for this project
├── INSTALL_GUIDE.md - Project installation guide
├── README.md - This documents
├── VERSION.txt - Current
```

## Notes:

- This project is configured for ESP32-C3, with the pinout configuration defined
  in the main/main.c. If you wish to reconfigure for other targets _(i.e. ESP32,
  ESP32-S3, etc)_, please rewrite the pinout scheme if necessary.
- For ESP32-C3, **avoid GPIO20 and GPIO21** as they can interfere with the
  primary UART channel. **Caution on using GPIO2, GPIO8 and GPIO9** as they are
  **strapping pins** and can interfere with the boot sequence if not set up
  properly _**(recommended as I/O with pull-up resistors enabled)**_
- A full version of the ported rc-switch library _(which handles RF signal
  encoding/decoding for both transmission & reception) can be found on the
  [main](https://github.com/Thomas-Hoang-04/ESP-RFControl/blob/main/README.md)
  branch of this repository_
- If you wish to **activate other interrupts** beside the one for RF signal
  reception, please be advised that the **RF functionality may be disrupted** as
  a result since the interrupts can collide with one another, causing some to be
  masked
- API guides for the ESP-IDF component used _(for ESP32-C3)_:
  [GPIO](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32c3/api-reference/peripherals/gpio.html),
  [General Purpose Timer (GPTimer)](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32c3/api-reference/peripherals/gptimer.html)

### Special Thanks to

- [_**Mr Nguyễn Đức Tiến, MSc**_](https://github.com/neittien0110). I would like
  to express my sincere gratitude to you for suppling the necessary components
  for this project, providing detailed and supportive feedbacks, and sharing
  usefuls tip on real-life deployment of a product. All of which assist me
  substantially on completing this project as well as on my path to become a
  better embedded developer.

_**Created by Minh Hai Hoang. ©2025**_
