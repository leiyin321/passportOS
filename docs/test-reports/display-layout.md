[简体中文](display-layout.zh_CN.md)

# Display manager and app layout validation

## Build and host checks

- D0 board source: ST7789P3 portrait 240×320, three ADC buttons UP/DOWN/OK; ESP-IDF5.5.3, LVGL9.5.0.
- Manifest validation accepts standard/fullscreen and portrait, rejects unsupported modes/orientation.
- Host layout tests cover exact rectangles and viewport clipping.
- Lua tests cover `get_viewport`, fullscreen and standard sizes, declarative bindings, invalid modes, clipping, graphics/game/audio events.
- Complete validation gate passed, including the Wi-Fi setup form parser tests.

## Device checks

Firmware archive: `build/firmware/bd6adf80646a9dc16fe8fdc94e01de7e6410e15117c4531c0de72350c8c9657b/`.
Merged image SHA-256: `bd6adf80646a9dc16fe8fdc94e01de7e6410e15117c4531c0de72350c8c9657b`; application SHA-256:
`232d2d8765f0e377d1cd4b5795d736df26e72596904af7d263d4d16a2bfc8872`. Application size: 1,798,816 /
2,621,440 bytes (31% free); merged image: 1,929,888 bytes. App-only image was flashed to `0x20000`, preserving data partitions.

The final app matrix ran on the Passport: Weather and Chat in standard mode (240×272), Tetris and VoiceBird in
fullscreen (240×320). Weather fetched fresh Open-Meteo data (23°C, clear code 0); Chat completed a real text echo
through the approved Mac WS service. Tetris rendered and received button events. VoiceBird opened the microphone;
a separate repeat-session run captured 209 PCM frames over 4.22s, with 56ms first-frame latency and RMS peak 37.
The seven-app catalog loaded, and Tetris was downloaded and installed through App Store, then launched fullscreen.

| Measurement | Device result |
| --- | ---: |
| Lowest free heap since boot (includes package/install activity) | 16,968 bytes |
| Lowest free heap during a running app | 43,632 bytes |
| Lowest largest-free-block reading | 32,768 bytes |
| Lowest observed main stack high-water mark | 1,212 bytes |
| Highest Lua peak | 21,669 bytes (under the 32 KiB per-app limit) |
| LVGL pool free / max used | about 13,820 / 10,004 bytes |
| VoiceBird asset cache peak / after exit | 216 / 0 bytes |
| 200 mixed standard/fullscreen render-and-exit cycles | PASS, 53.73 s |
| Free heap before / after those cycles | 73,092 / 73,088 bytes (−4 bytes) |
| Images and active display mode after each exit | 0 images; standard 240×272 |

Session averages were 11.1 fps Weather, 17.0 fps Chat, 16.3 fps Tetris and 3.2 fps VoiceBird. They include network,
permission and worker startup time, so they are not pure rendering rates. The largest measured Lua callback was
10.124 ms; all callbacks stayed below the 12ms runtime deadline. The global LVGL refresh counter reached 799.

USB Wi-Fi provisioning restored the station connection; fresh weather and Store/WS network tests then passed.
Phone provisioning was started from Settings on the flashed firmware. The screen exposed the generated AP name,
12-character key and local URL; the key is redacted from this report. The pure form parser host test passed. Actual
phone association and form POST were not run in this session.

## Unverified

The user previously confirmed physical Home and UP/DOWN/OK operation. Physical long-OK timing, human visual review
of the new Settings page and app cards, phone association/form submission, and uninterrupted display during
prolonged full load remain unverified.
