
skiller-ctl
===========

The common and the multimedia keys of 
[Sharkoon Skiller keyboards](https://www.sharkoon.com/product/1693/SkillerProPlus)
work out of the box on Linux systems. With this tool, you can control the additional
features like the changeable background LED color and brightness from your PC.

Please note, as this tool is based on [libusb](http://www.libusb.org), the keyboard
is disconnected for a short time from the input handler of the kernel. Hence,
you might loose key presses while settings are changed. To avoid this problem,
this code has to be moved into the kernel.

To create this tool, the communication between the Windows driver and the device
has been eavesdropped using `usbmon` and `wireshark` (as described in
[this guide](http://kicherer.org/joomla/index.php/de/blog/38-reverse-engineering-a-usb-sound-card-with-midi-interface-for-linux)).

Currently supported
-------------------
* Skiller Pro Plus [04d9:a096]

Features
--------
* change profile
* set background LED color
* set brightness or pulsing brightness mode
* en-/disable "disco mode" - pulsing brightness with changing color
* set polling rate
* en-/disable windows key

ToDo
------
* add support for other Skiller (or Sharkoon) models
* configurable (macro) keys
* event daemon to handle special gaming keys
* move this code into kernel to avoid short disconnects of the keyboard

Build
-----
* Install [libusb-1.0](http://www.libusb.org)
* Execute "make"


Usage
-----
```
Usage: ./skiller-ctl [options]

Options:
--------

  -a                always power on the device (root privileges required)
  -b <number>       brightness between 0 and 10 (default: 10)
  -B                pulsing brightness
  -c <string>       set color (query possible values with -C)
  -C                list supported colors
  -d <number>       select device (default: 0)
  -i                disco mode - pulsing brightness with changing color
  -l                list available devices
  -p <number>       change profile (1-3)
  -r <number>       change polling rate to: 125, 250, 500 or 1000 Hz
  -w <number>       windows key on (1) or off (0)
```
