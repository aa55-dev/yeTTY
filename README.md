# yeTTY
![screenshot](img/screenshot1.png "screenshot")
yeTTY is an open source application for embedded developers to view logs from serial port.

## Download

Source code is available on [https://github.com/aa55-dev/yeTTY/](https://github.com/aa55-dev/yeTTY/)

## Features
1. Text highlighting

    yeTTY uses KDE's KTextEditor for displaying the logs. KTextEditor provides yeTTY with syntax highlighting and text search.

2. Auto reconnection

    In case your board gets disconnected, yeTTY will keep on attempting to reconnect to the same port.

3. Audio alert on string match

    yeTTY can monitor the output for a specified keyword and alert you with a sound upon match.

4. Long term run mode

    This is useful if you need to keep the board running overnight. yeTTY will capture the logs from serial port and compress it and save them to storage.

5. Suspend during flashing

    If your board uses the same serial port for both logs and flashing, prepend the flashing command with `yetty_suspend` to suspend yeTTY while the board is being flashed.

    ESP32 Example:

    ```
    yetty_suspend ttyUSB0 idf.py -p /dev/ttyUSB0 flash
    ```

6. Auto baud rate detection

    yeTTY can attempt to automatically detect the baud rate by trying out different baud rates till it finds one with readable ASCII text.

## Building

**1. Install dependencies**

Debian bookworm:
```
sudo apt install cmake g++ qtbase5-dev libqt5serialport5-dev qtmultimedia5-dev libkf5texteditor-dev libzstd-dev libsystemd-dev libboost-stacktrace-dev
```
Debian trixie:
```
sudo apt install cmake g++ qt6-base-dev qt6-serialport-dev qt6-multimedia-dev libkf6texteditor-dev libzstd-dev libsystemd-dev libboost-stacktrace-dev libbacktrace-dev
```
**2. Build**

```
cmake -B build .
cmake --build build
```

**3. Install**
```
cmake --install build
```
