<p align="right">
  <a href="passport-lab.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Passport Lab MiniApp

`org.passport.device-lab` is a Store-installable MiniApp inspired by the official
AI Passport factory hardware demonstrations. It uses only the existing Passport
OS public MiniApp API and does not reproduce the factory test menu.

The app offers a display color-field test, button press/release/UP-DOWN long
event test, consent-gated local microphone RMS meter, app-private persistent
storage check, and an explanation of hardware tests reserved to the system. The
standard OS bar remains visible, including battery percentage. Brightness and
screen timeout are managed in Settings.

The MiniApp cannot scan Wi-Fi, start BLE advertising, inspect raw button ADC,
play raw audio, or enter light/deep sleep. Those functions remain system-owned
under the capability policy. The Audio page reports RMS features only; no raw
audio is exposed, stored, or sent. Long-OK remains reserved by the OS to return
to Home.

The signed PAP is in `examples/packages/passport-lab.pap` and the static Store
catalog is generated under `dist/`. To rebuild it with the project publisher:

```sh
PYTHONPATH=sdk python3 - <<'PY'
from pathlib import Path
from passport import pack
Path('examples/packages/passport-lab.pap').write_bytes(
    pack('examples/passport_lab', 'local/publisher-private.pem',
         'sdk/publisher-public.pem'))
PY
cp examples/packages/passport-lab.pap dist/passport-lab.pap
passport publish-index dist --public sdk/publisher-public.pem \
  --base-url http://192.168.1.10:8040 -o dist/index.json
```

The publisher private key stays in ignored local storage and must never be
copied into source control or the MiniApp package.
