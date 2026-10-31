[简体中文](system-volume.zh_CN.md)

# System volume acceptance — 2026-10-29

Passport OS 1.0.46 adds independent media and alarm volume in Settings → Sounds. UP/DOWN adjusts by 5 percentage points, OK switches controls, double-OK returns Home. Values clamp to 0–100%; 0% is silent. Defaults remain 45%/35%. Both values persist in system NVS and do not change microphone gain or app permission.

## Validation

Build: PASS — complete `./tools/validate.sh`, ESP-IDF 5.5.3, verified archive, signed Settings OTA, matching on-device ELF and successful OTA self-test.

Host tests: PASS — complete gate plus unsigned-bound/clamping/independent-channel tests, muted and configured-volume stream worker reinitialization, existing teardown/failure checks and glyph coverage for the new Chinese labels.

Device tests: PASS for the following bounded checks:

- Both settings reach 0% and 100%, stay clamped after extra input and do not change the other channel.
- Media 50% / alarm 40% survive a real device restart. English and Chinese settings snapshots retain both values across language changes.
- Native XiaoZhi reaches the official service and decodes 51 reply packets. The actual audio worker applies `volume=50`, replacing the old hardcoded 45%.
- Playback exits with executable-pool usage zero. Sanitized exit counters: `METRIC exit free=104272 minimum=10448 largest=38912 internal_free=104272 internal_largest=38912 retention_free=96512 retention_largest=38912 stack=2212 lua=0 lua_peak=0`.
- Test changes are restored to 45% media / 35% alarm and Chinese UI. Alarm time/enabled settings are preserved.

Unverified: human confirmation of speaker loudness/mute/Chinese rendering, actual scheduled alarm playback at different levels, long-duration audio and measured acoustic output. Software/driver counters do not establish perceived volume.

## Size and behavior

| Artifact | Bytes |
|---|---:|
| OTA application | 2,332,320 |
| Merged firmware | 2,463,392 |

The setting state uses two atomic percentages; no new permanent task or heap-owned buffer is introduced. Workers own codec-volume calls. Invalid saved percentages retain defaults; persistence failure reports an error and leaves the previous active percentage. This does not change the package ABI, partitions or Lua support.

## Artifact and resume

`build/firmware/74ba34369c09345eb2243b93b1bf3be2791af4e730f63ae2cbfe16b159b0693f`

ELF SHA-256: `612fdc286b09af19736d318efef56b9821a4d9d6075fe2e011bb17f88508f20d`.

Signed release build: `2026102913`. The local system-update service serves 1.0.46. Raw logs remain in ignored `local/volume/`. See [settings guide](../developer/system-settings.md) and [`volume.h`](../../os/services/audio/volume.h). No Git commit/push was performed.
