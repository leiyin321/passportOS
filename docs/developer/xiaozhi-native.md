[简体中文](xiaozhi-native.zh_CN.md)

# XiaoZhi AI Native

Source: [`examples/xiaozhi_native`](../../examples/xiaozhi_native/main.c). App ID `org.passport.xiaozhi-native`, version 1.0.4, runtime `native-elf`, ABI 2. **Upgrade to Passport OS 1.0.45 before downloading. OS 1.0.34 only implements Native ABI 1.** The Lua app retains its separate ID and private data.

The application implements service configuration/activation, WebSocket authentication and hello negotiation, manual push-to-talk, Opus packet forwarding, STT/transcript and TTS reply display. It does not use a Lua interpreter or an OS XiaoZhi provider. System Wi-Fi, HTTPS/WSS transport/policy/TLS, consent, Opus codec workers, managed display and teardown belong to OS [generic ABI 2 services](native-services.md). Native app source owns the service URL, request headers, JSON messages and conversation state.

The protocol follows the [official WebSocket specification](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md); the [FoloToy Passport adaptation](https://github.com/FoloToy/folo-ai-passport-xiaozhi) provides board/application context. Neither the server's firmware URL nor OTA command is installed by the MiniApp: system updates remain in Passport Settings.

## Use

1. Update OS through Settings → System Update, then download **XiaoZhi AI Native** from the local App Store.
2. Open it and press OK once to grant microphone consent. UP/DOWN denies consent. Reopen the app to request consent again.
3. It waits for actual OS Wi-Fi readiness, requests the official service configuration, opens authenticated WSS and exchanges hello. An activation code, when provided, is displayed for linking at xiaozhi.me; activation belongs to the application/service, not OS settings.
4. On the voice page, hold OK to speak and release to finish. Replies play through the system audio worker. UP/DOWN cycles voice, conversation and service pages. Double OK returns Home and reclaims resources.
5. On errors, OK retries from the voice page or the service page. Service timeouts do not spin through endless reconnects. HTTP/WSS errors retain numeric completion status. Voice buffers are bounded; the display shows a short transcript/reply excerpt.

UI labels follow system English/Chinese; app-owned palettes follow system light/dark mode with high-contrast foregrounds. The OS font subset adds the missing voice/activation/page glyphs, with coverage checked alongside existing fonts. Remote arbitrary transcript characters outside the OS subset may still lack glyphs.

Capture sends mono 16 kHz, 60 ms Opus. Server hello must select WebSocket/Opus, mono, a supported Opus rate (8/12/16/24/48 kHz), and 20/40/60 ms frames when supplied. Opus playback decodes to the system's mono 16 kHz output. Unsupported formats, overlong credentials, unavailable devices and denied consent fail visibly; credentials and activation codes are never put in published logs.

## Build and publish

Activate ESP-IDF 5.5.3 first:

```sh
PYTHONPATH=sdk python -m passport native build examples/xiaozhi_native
PYTHONPATH=sdk python -m passport pack examples/xiaozhi_native \
  --key /path/to/os-publisher-private.pem --public sdk/publisher-public.pem \
  -o dist/xiaozhi-native.pap
PYTHONPATH=sdk python -m passport validate dist/xiaozhi-native.pap --public sdk/publisher-public.pem
PYTHONPATH=sdk python -m passport publish-index dist --public sdk/publisher-public.pem \
  --base-url http://YOUR_MAC_IP:8040 -o dist/index.json
PYTHONPATH=sdk python -m passport serve dist --host 0.0.0.0 --port 8040
```

The HTTP origin is a local development service requiring the existing device LAN grant; packages still require RSA signature and SHA-256 verification. Do not expose private signing keys. Native installation accepts only the built-in OS publisher. The catalog's `native-elf/2` feature is informational; an older firmware will reject its unsupported manifest ABI.

Validation and current device acceptance are tracked in the [test report](../test-reports/xiaozhi-native.md). Host mocks verify protocol and cleanup requests; they do not prove remote activation or audible speech.

A single OK on the conversation page sends the official `listen/detect` greeting, allowing service/playback checks without holding the microphone. Transient configuration errors retry at most three times, then wait for explicit retry. For immutable store publication, generate the index from a staging directory containing one current package per app, while retaining older version files in the served directory.
