---
title: YETTY
section: 1
header: User Commands
---

# NAME
yetty - Serial port terminal software for developers

# SYNOPSIS
**yetty** *PORTNAME* *BAUDRATE*

**yetty** **-**

# DESCRIPTION
**yeTTY** is an open source application for embedded developers to view logs from serial port.

If launched without any arguments, you will be presented with a GUI dialog to
choose the port and baud rate. You may also specify the port name and baud rate as
command line arguments.

# OPTIONS

### serial port
To connect to a serial device, provide the following:

* **PORTNAME**: The absolute path to the serial port.
* **BAUDRATE**: The required baud rate.

### stdin Mode
* **-**: A single dash indicates that yetty should read data from standard input
instead of a serial port.


# EXAMPLES
**Open a specific port:**
:   yetty /dev/ttyUSB0 115200

**Read from stdin:**
:   yetty -

# AUTHOR
Arjun AK mail@aa55.dev

# REPORTING BUGS
Report bugs to https://github.com/aa55-dev/yeTTY/issues
