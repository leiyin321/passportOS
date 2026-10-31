[简体中文](THIRD_PARTY.zh_CN.md)

# Attribution and redistribution

Preserve the root MIT license and FoloToy copyright. Passport OS is a derivative
project; community publication must identify the OS and its author separately
from the hardware manufacturer. Do not imply manufacturer endorsement.

- ESP-IDF 5.5.3: Espressif, Apache-2.0; fetched from its official source.
- LVGL and ESP-managed components: retain the licenses supplied through the
  dependency lock and component registry. Generated managed files are not source exports.
- Vendored ELF Loader: Espressif Apache-2.0, retained in
  [`license.txt`](../components/elf_loader/license.txt); C3 modifications are in-tree.
- Lua: its [upstream license](../components/passport_lua/LICENSE.html).
- MP3 decoder: [component license](../components/passport_mp3/LICENSE).
- Fonts: [asset provenance](../assets/README.md), including Source Han OFL and
  FontAwesome licenses; generated font files retain provenance.
- cJSON simulator copy retains its [license](../simulator/vendor/cJSON/LICENSE).

App developers retain responsibility for app assets, stream permissions, third-party
AI services and backend terms. Do not copy firmware secrets, private service
credentials or proprietary assets into a contribution. Public distribution of
an app does not grant rights to its external content providers.
