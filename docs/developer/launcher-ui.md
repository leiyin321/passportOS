[简体中文](launcher-ui.zh_CN.md)

# Launcher interface

The 240x320 Home screen uses three icon/text cards: Apps, Store and Settings. They open the
existing Library, App Store and settings destinations.
UP/DOWN cycle in reading order; single OK enters/confirms; double OK returns Home.
Each card combines a shared semantic icon, a short name and a secondary description.
The focused card has an accent border, filled icon badge and checkmark; focus remains visible
without depending only on color. Wi-Fi and battery stay at the top; the control hint stays below.

Layout: 216x200 content at (12,80); the three cards use the available area. Existing
Montserrat14/20 and the System Icon Registry provide all imagery. A single persistent LVGL
draw object paints the cards. No raster assets, custom fonts, animation timer or full-screen
framebuffer are added.
The LVGL lock protects the displayed focus snapshot and visibility updates. Permission dialogs
hide the card grid; apps and other system pages retain their own layout.

The earlier text-only launcher was physically confirmed by the user. The revised card interface
requires a fresh visual check after the firmware update; its automated device checks are recorded
in the current execution checkpoint.

Historical automated device checks PASS: all four destinations in the earlier launcher, 200 focus changes, and app exit back Home.
Across focus changes, display frames advanced 28 to 462; free heap 82,344 to 82,260 bytes
(-84), largest block stayed 73,728 bytes. After grid exit, app asset bytes are zero.
Firmware 1,768,784 bytes, +944 bytes; static BSS +8 bytes. Build/host/device checks PASS;
physical visual approval of this revised layout remains pending.
