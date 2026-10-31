<p align="right">
  <a href="system-settings.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Passport OS system settings

Settings uses an iOS-inspired grouped list sized for the 240×320 display. From
Home, select **Settings** with UP/DOWN and press OK. The first-level menu is a
scrollable viewport: UP/DOWN browses every entry, a scrollbar shows the current
position, and OK opens a detail page. More settings can be added without
cramming everything onto one screen. Double-OK returns Home. A single OK
confirms the selected action; long OK is available to MiniApps.

## Current entries

| Group | Entry | Behavior |
| --- | --- | --- |
| Connectivity | Wi-Fi | Shows station state and starts the temporary phone setup AP |
| Connectivity | Bluetooth | Shows on-demand BLE HID state; pairing starts in the Native Bluetooth app |
| Personalization | Display | Changes backlight in 10% steps (20–100%) |
| Personalization | Appearance | Switches the OS interface between Dark and Light; saves to NVS |
| Personalization | Screen Timeout | Selects 15 s, 30 s, 1 min, 2 min, 5 min, or Never |
| Personalization | Sounds | Independent media/alarm volume, 0–100% in 5% steps; OK switches the selected control |
| Privacy | Permissions | Explains per-app microphone consent, audio streaming to allowlisted endpoints, and manifest-controlled network access |
| System | Date & Time | Automatically syncs through `pool.ntp.org` on Wi-Fi; shows date, time, sync wait state, and Asia/Shanghai (UTC+08:00) |
| System | Alarm | Daily system alarm with configurable time and enabled state; waits for valid system time and stops on any key |
| System | System Update | Checks a signed HTTPS manifest, downloads to the inactive OTA slot, and asks before installing a verified image |
| System | Language | Chooses English or Simplified Chinese; persists in system NVS and applies to all system pages and prompts |
| System | Storage | Shows package-store availability and installed app count |
| System | About | Shows the Passport OS firmware semantic version, target, ESP-IDF version, and App API version |

Brightness and timeout are stored in NVS (`osconfig`) and restored at boot. A
timeout turns off the display backlight; the first button press turns it back on
without activating the selected control. Microphone prompts and phone Wi-Fi
setup keep the screen awake. `Never` disables the inactivity timer.

Apps requesting `audio.stream` receive a distinct consent message that their
Opus audio may be sent to the app's allowlisted network service while capture
is active. Capture starts only when the app explicitly requests the generic
stream API; OS lifecycle cleanup stops it on app exit or runtime fault.

System time uses the public SNTP server `pool.ntp.org` and starts syncing after
Wi-Fi receives an address. The clock appears in the top status bar and shows
`--:--` until the first valid sync. The configured time zone is
`Asia/Shanghai` (UTC+08:00). The system clock continues while Wi-Fi is
disconnected during the same boot; after a power-loss reboot it waits for
network synchronization again. The system alarm schedules only after time is
valid and persists in `osconfig` NVS. See the
[system alarm specification](system-alarm.md).

System Update first requires Passport OS to be installed and a trusted HTTPS
`latest.json` source to be configured through the USB developer CLI. The user
separately confirms checking, downloading, and installing. The updater switches
to the inactive OTA slot only after signature and image validation. Initial
installation/migration from vendor firmware remains a USB flash; see the
[system update guide](system-updates.md) for the manifest and release command.

UP/DOWN switches between English and Simplified Chinese; the selection is saved
automatically. MiniApps can read `passport.system.language()` (`en` or `zh-CN`)
and choose their own translated content. The OS does not translate app supplied
strings automatically.

Appearance is a read-only hint for MiniApps through
`passport.system.display_mode()` (`dark` or `light`). MiniApps choose whether to
follow the system mode or use independent colors. They can set foreground color
with `passport.text(value, rgb24)` and draw background surfaces with
`passport.rect`; the OS theme does not override app canvas colors.

The selection covers OS navigation, setting names/values/details, status and
button hints, setup and permission dialogs, App Store/install errors, OTA
progress/errors, and runtime error notices. App names and content remain
app-owned. Chinese system text uses a generated Source Han Sans subset followed
by LVGL's built-in CJK fallback; source, license, and regeneration details are
in [`assets/README.md`](../../assets/README.md).

Bluetooth remains informational until its pairing service is implemented. The
system alarm uses the on-board audio codec and speaker for its alert tone. Add
future settings by extending the bounded settings
registry, giving each entry an accurate status/detail page, and persisting only
real configuration under the dedicated `osconfig` namespace. Keep blocking work
out of button handling.

## Sound volume

Available since Passport OS 1.0.46.

In **Settings → Sounds**, UP increases and DOWN decreases the selected volume by 5 percentage points. OK switches between Media and Alarm; double-OK returns. Both controls support 0% (silent) through 100%, clamp at their limits, and save automatically to `osconfig` NVS (`media_volume`, `alarm_volume`). Defaults are 45% media and 35% alarm. Invalid saved values fall back to these defaults. A failed save reports an error and keeps the last active value.

The OS owns both controls. Lua and Native Opus playback use Media; the system alarm worker uses Alarm. Starting playback no longer overwrites the system setting. Audio workers apply changes in their own task context, including after codec wake/reinitialization. Changing playback volume does not change microphone capture gain or grant microphone permission. Acoustic level, speaker distortion and perceptual loudness still require listening on the device.

Validation: [system volume acceptance](../test-reports/system-volume.md).

Bluetooth details and the iPhone guide: [Native Bluetooth peripherals](bluetooth-peripheral.md). Classic headphone audio/calls are unavailable on ESP32-C3.
