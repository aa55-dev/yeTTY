---
title: YETTY_SUSPEND
section: 1
header: User Commands
---

# NAME
yetty_suspend - Temporarily release a serial port from yeTTY to run another command

# SYNOPSIS
**yetty_suspend** *PORT_NAME* *PROGRAM* [*PROGRAM_ARGS*...]

## DESCRIPTION
**yetty_suspend** is designed to coordinate access to a serial port
between **yeTTY** and another tool (like a flash programmer).

When executed, it will use DBus to locate and temporarily suspend the yetty instance
that is using the mentioned port. It will then execute the program specified in the
command line argument and once completed, resume yetty.

## OPTIONS
* **PORT_NAME**: Path to the serial port.
* **PROGRAM**: The external executable to run.
* **PROGRAM_ARGS**: Any arguments to be passed directly to the *PROGRAM*.

## EXAMPLES
`yetty_suspend /dev/ttyACM1 esptool.py -p /dev/ttyACM1 chip_id`

# AUTHOR
Arjun AK mail@aa55.dev

# REPORTING BUGS
Report bugs to https://github.com/aa55-dev/yeTTY/issues
