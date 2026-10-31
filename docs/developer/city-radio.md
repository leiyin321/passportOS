[简体中文](city-radio.zh_CN.md)

# Native City Radio

`org.passport.city-radio` 0.1.2 is an independent Native ABI 2 app inspired by the public [City Voices feature description](https://ai-passport.folotoy.cn/plays/65/). No original firmware, source code, artwork or audio recordings are included. Source: [application](../../examples/city_radio/main.c), [catalog](../../examples/city_radio/catalog.c).

## Installation and controls

Upgrade Passport OS to **1.0.59** through Settings → System update **before** installing City Radio from the local App Store. Earlier hosts cannot load the optional media extension or accept the new permission. Wi-Fi provisioning, system time synchronization, screen timeout and alarm handling remain OS functions. Playback needs no microphone permission and no Mac streaming proxy.

| Screen | UP / DOWN | OK | Hold OK |
|---|---|---|---|
| Listening | Select previous/next station within the city; hold UP/DOWN for volume ±5 | Pause/resume; retry after failure | Menu |
| Menu/list | Move selection; lists paginate | Open/select | Listening; station list goes back to cities |
| City search | Choose A–Z, Search or Delete | Append letter / search / delete | Listening |
| Clock | Any short/long key wakes listening | Wake | Wake |

Double OK retains the OS Home action; UP/DOWN have no double-click mapping. The first key waking the clock does not tune or pause. Volume changes use system media volume for this boot, without changing alarm volume. Persistent default volume is configured in OS Settings.

Seven city shortcuts cover Beijing, Shanghai, Guangzhou, Shenzhen, Hangzhou, Changsha and Chengdu, with two catalog slots each. Search matches the prefix of their pinyin initials: BJ, SH, GZ, SZ, HZ, CS, CD. This first catalog is bundled; it is not a complete online city directory or automatic location service. Chengdu's second slot is explicitly a national backup. Radio providers can change or remove streams; a successful HTTP response cannot prove that the named program is currently on air.

Recent history keeps up to ten unique stations, newest first. Successful playback records the station; restart resumes the saved station. Style and history use the app's private storage, schema 1 with stable catalog indexes (append new entries rather than reordering). Two original styles provide a tuner/spectrum and a blue cassette view. Their animation indicates playback state, not a measured FFT. The clock reads OS time and displays China Standard Time (UTC+8). Listening switches to the clock after 30 seconds without input; playback continues.

Sleep timer cycles Off → 15 → 30 → 60 minutes → Off, stopping playback when due. It is session-local and does not create a system alarm. Exiting the app stops streaming; background playback is not supported. Existing system alarms close the foreground app before ringing.

## General OS media extension

Include [passport_native_media.h](../../sdk/native/include/passport_native_media.h). The ABI 2 core prefix remains 80 bytes, HID prefix 96 bytes, and appended media host 120 bytes on C3. Check the full size before casting. Previous Native ABI 1/2 and Lua contracts remain intact. Declare `audio.playback` and `network.fetch`; playback is currently trusted Native ABI 2 only. Do not combine this playback owner with microphone/Opus capture in one app session.

| Method | Contract |
|---|---|
| `media_play(url)` | Queue one copied MP3 URL (HTTPS, or public HTTP with `audio.http`); 1 accepted, 0 busy/rejected; no synchronous DNS/network/audio I/O |
| `media_stop()` | Request asynchronous stop; wait until state is no longer Buffering/Playing/Stopping before another play |
| `media_state()` | 0 idle, 1 buffering, 2 playing, 3 stopping, 4 failed, 5 ended |
| `media_volume(percent)` | -1 queries; 0…100 updates system media volume for this boot; other values return -1 |
| `wall_time()` | Unix seconds; 0 while OS time is unsynchronized |
| `battery_percent()` | Read-only 0…100; -1 if unavailable |

Only MP3 elementary streams are supported initially; AAC, HLS/M3U8, playlists and DRM are not. The app owns the station catalog, URL choice, retry policy, history and UI. The OS owns transport, decoder, PCM downmix, I2S, media volume and cleanup. App 0.1.2 declares the optional `audio.http` permission and uses public, unauthenticated HTTP broadcasts; ordinary fetch/WS keep their original secure policy. Network reception uses the existing worker; a separate playback task consumes a fixed block pool of 512-byte compressed chunks, with full prebuffering before start/recovery. Another HTTP/WS request cannot run simultaneously. State polling avoids retaining ELF callbacks in workers.

The existing exact-domain allowlist, public-address/DNS pinning and TLS certificate validation apply to the initial URL and every redirect (maximum three). An allowlisted redirect is accepted; other destinations are rejected. HTTPS/443 remains the default. OS 1.0.56 adds the explicit `audio.http` exception: trusted Native ABI 2 plus `audio.playback` and `network.fetch`, exact manifest domains, public IPv4 and HTTP/80 only. Private/self addresses, credentials in the authority and HTTP query strings are rejected. No custom authentication headers or request body are supported by media playback. HTTP audio is unencrypted; use it only for public broadcasts without secrets. The app allows `lhttp.qingting.fm` and `lhttp.qtfm.cn`. Metadata is requested off (`Icy-MetaData: 0`); servers that insist on interleaving ICY metadata are rejected. Limits are 24 hours, 512 MiB and a 3-second socket timeout per request. The app makes at most three attempts per selection/resume and offers OK retry after exhaustion.

The MP3 assembler is bounded to 1,536 bytes, skips bounded ID3 tags, checks Layer III headers and stops after 64 KiB without synchronization. Mono/stereo 8–48 kHz, 16-bit output is supported; stereo downmixes to the mono speaker. [Decoder provenance](../../components/passport_mp3/README.md) and [test report](../test-reports/city-radio.md) describe memory and validation limits.

## Build and continuation

```sh
PYTHONPATH=sdk python -m passport native build examples/city_radio
PYTHONPATH=sdk python -m passport pack examples/city_radio --key <private.pem> --public sdk/publisher-public.pem -o <city-radio.pap>
./tools/validate.sh
```

Keep signing keys and raw stream captures outside version control. To continue, use the test report and existing handoff checkpoint. Device acceptance must check actual station audio, physical button timing, Chinese glyphs, stereo downmix, repeated tune/pause/exit, sleep expiry, alarm interruption, Wi-Fi loss/reconnect, heap recovery and decoder timing on C3.

## Current buffering and quality profile (OS 1.0.59 / app 0.1.2)

The Quality menu selects Smooth (default, provider `/24k.mp3`) or Standard (`/64k.mp3`). Beijing Music's smooth stream measured 32 kbit/s; URL names do not guarantee bitrate. Quality uses a separate private `quality` key, preserving the schema-1 station/history/style data. Changing quality restarts active playback; paused playback remains paused.

Public HTTP starts with 32 separately allocated 512-byte blocks and grows up to 64 after codec/DAC initialization, preserving a 14 KiB heap reserve at growth time. Actual depth depends on runtime memory. HTTPS uses 16 blocks. Network and playback stacks are 6 KiB and 4 KiB respectively. Output-only BSP initialization avoids unused microphone DMA. PCM writes contain at most 240 mono samples.

Short network reads are packed into full blocks; only EOF publishes a partial tail. Thus queue depth represents actual compressed bytes rather than receive-call count. Start and recovery prefill the allocated pool. This trades start latency for resilience to bursty stations; it cannot guarantee continuity through arbitrary remote outages. The measured source delivered bursts separated by up to 5.64 seconds even on the Mac. Keep the previous mode restored after temporarily disabling Wi-Fi power saving. Decoder working memory is prewarmed/reset before transport allocation. All tasks, codec resources and blocks are reclaimed on exit. See the test report for actual resource measurements and remaining acceptance.
