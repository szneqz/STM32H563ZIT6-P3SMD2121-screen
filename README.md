# STM32H563ZIT6-P3SMD2121-screen

![Effect](photos/Screenshot_20260803_000445.png "Visual effect")

## How to connect hardware
![Connections](photos/ekranik.png?raw=true "Hardware connections")

## How to fix stlink

1. Go to `/etc/udev/rules.d/`
2. Edit `49-stlinkv3.rules`
3. Change all `GROUP=` to `uucp`
4. Reboot PC
