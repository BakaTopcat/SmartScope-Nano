# SmartScope-Nano
DIY Arduino BlackMagic SmartScope scopes switcher

I've made a tiny Blackmagic SmartScope switcher which allows switching scopes without using PC.
The total cost of components is around $14.

Features:
- EEPROM remembers all the settings;
- Setup menu (no PC needed to configure);
- Dynamic/static IP settings;
- Custom monitor names;
- Up to 12 monitors could be controlled (actually up to 62, just change some constants).

You need:
- Arduino Nano;
- ENC28J60 shield;
- 128x64 I2C OLED;
- Rotary encoder with push button.

No passive electonics components needed. Could be assembled on breadboard.
On first setup, it's advised to set monitor 1 to A or B (it should be always active). This will avoid exhausting EEPROM in continuous search for at least one active monitor.

At the current moment:
- The device only sends the TCP packets and does not receive anything from monitors.
- The code hasn't been tested on actual SmartScope. Would be thankful for feedback.
