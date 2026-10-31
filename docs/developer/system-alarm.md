[简体中文](system-alarm.zh_CN.md)

# Passport OS System Alarm

The alarm belongs to Passport OS. MiniApps cannot create, change, or dismiss
system alarms through an SDK API.

## User controls

Open **Settings → Alarm**. The alarm is disabled by default and defaults to
07:00. The detail page identifies the item being edited. The selected hour or
minute is bracketed in the time; the enabled item is shown as `[Off]` or
`[On]`. When the alarm is off, entering the page selects Enabled so UP/DOWN
can turn it on or off immediately. When it is already on, the page starts at
Hour. UP/DOWN adjusts the selected item; short OK cycles through hour, minute,
and enabled state. Changes are saved to the `osconfig` NVS namespace after
1.5 seconds without another adjustment. The Settings list displays the
configured time and enabled state. The system uses 24-hour local time and
repeats every day.

At the configured minute, Passport OS closes the foreground MiniApp, turns on
the display, and plays a repeating speaker tone. Press any key to stop the tone
and return Home. The alert is not snoozable in this implementation.

## Scheduling and lifecycle

The scheduler uses the existing SNTP-backed system clock in `Asia/Shanghai`
(UTC+08:00). It does not schedule while the clock is invalid. It fires at most
once per local calendar day, so the main loop cannot retrigger the same alarm
minute. After reboot, the persisted alarm is restored and becomes eligible
after time synchronization. Timezone remains fixed; daylight-saving changes
are not applicable to the configured timezone.

Alarm playback runs in a dedicated FreeRTOS worker and uses the existing BSP
audio codec/speaker API. Dismissal requests the worker to stop and suspend the
codec. App audio workers are stopped before the alarm owns the audio device.

## Limits and verification

The alert requires the device to be powered and Passport OS to be running. The
current system deep-sleep mode is a timer/GPIO wake demo and is not integrated
with alarm deadlines; wake-from-deep-sleep at the configured time is therefore
not promised. Verify the configured time, persistence across restart, audible
alert, any-key dismissal, app interruption, and audio cleanup on hardware.

The pure scheduling and adjustment logic has host tests in `tests/test_alarm.c`.
