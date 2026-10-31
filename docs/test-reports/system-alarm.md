[简体中文](system-alarm.zh_CN.md)

# System Alarm Implementation Report

## UX follow-up — 1.0.16 / build 2026102519

The alarm detail screen now identifies and brackets the selected setting. A
disabled alarm opens with Enabled selected, so UP/DOWN toggles it directly;
an enabled alarm opens at Hour. On-screen bilingual guidance explains short OK
field cycling and the 1.5-second save delay. The shared home and Settings icon
browser was removed as a user feature; the underlying registry remains in use.

| Check | Result |
| --- | --- |
| Full host test suite and repository checks | PASS (`./tools/validate.sh`) |
| ESP-IDF 5.5.3 build and image layout | PASS |
| App image / OTA slot | 2,067,840 / 2,621,440 bytes; 553,600 bytes free (21.12%) |
| Merged image | 2,198,912 bytes |
| Signed update served over HTTP | PASS; fetched image digest and RSA signature verified |
| Device interaction and RAM readings | NOT RUN; no device connection in this change |

Signed 1.0.16 / build 2026102519 is available from the Mac update service at
`http://192.168.1.10:8765/latest.json`. Image SHA-256:
`82bcd67fbeee4858dd69f92d31afed6870d7cca8478704a0bb2c764b0ea81fd6`.

Date: 2026-10-25 · Firmware: 1.0.14 / build 2026102517

## Implemented behavior

- System-owned daily alarm configured at Settings → Alarm; no MiniApp API.
- UP/DOWN edits the selected hour, minute, or enabled field; short OK advances the field.
- Configuration persists in NVS (`osconfig`) after 1.5 seconds without another edit.
- Scheduling waits for valid SNTP time; trigger is deduplicated to once per local calendar day.
- At trigger, the foreground app is stopped, display is enabled, and an audio worker plays a repeating tone. Any button requests stop and returns Home.
- Wake-from-deep-sleep and snooze are not implemented.

## Validation results

| Check | Result |
| --- | --- |
| Alarm scheduling/adjustment host test | PASS (`tests/test_alarm.c`) |
| Repository static checks and host suite | PASS (`./tools/validate.sh`) |
| ESP-IDF 5.5.3 firmware build and layout verification | PASS |
| Archive integrity | PASS (`tools/archive_firmware.py verify`) |
| Release manifest RSA signature and image digest | PASS |
| Fetch image through local update HTTP service | PASS; byte count and SHA-256 match |
| Physical Settings flow, alarm firing, speaker, dismissal | NOT RUN; no hardware connection requested |
| Runtime RAM peak and codec cleanup on device | NOT MEASURED |

## Resource budget

- Application image: 2,067,328 bytes of 2,621,440 bytes; 554,112 bytes free
  (21.13%). Merged initial-flash image: 2,198,400 bytes.
- The notification worker reserves a 3,072-byte FreeRTOS stack while an alarm is
  sounding. Its 320-sample signed 16-bit PCM buffer occupies 640 bytes of that
  stack. This is a static budget, not a measured peak stack watermark.
- Scheduler/configuration state is statically allocated; no persistent heap
  allocation is added by the clock matcher. Actual device heap/stack watermark
  and post-dismiss codec resource readings remain unverified.

## Published update

Signed release `1.0.14` / build `2026102517` is staged at
`build/system-updates/local-update/` and served by the existing Mac service at
`http://192.168.1.10:8765/latest.json`. The app image is 2,067,328 bytes with
SHA-256 `214c4082b77dcd0aa2e426f3e109f94dfa7a8c2ac30792435c9a1ff8c912d7a1`.
The downloaded image was checked against the published manifest. The device
was not connected or flashed; install it from Settings → System Update when
ready.
