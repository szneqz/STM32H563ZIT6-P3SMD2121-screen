# STM32H563ZIT6-P3SMD2121-screen

## How to fix stlink

1. Go to `/etc/udev/rules.d/`
2. Edit `49-stlinkv3.rules`
3. Change all `GROUP=` to `uucp`
4. Reboot PC