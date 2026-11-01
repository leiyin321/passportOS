import re
import json
import unittest
from pathlib import Path


class SystemFontCoverage(unittest.TestCase):
    def test_appearance_setting_is_saved_and_read_only_to_apps(self):
        source = Path('main/passport_main.c').read_text()
        self.assertIn('{"Appearance", "外观"}', source)
        self.assertIn('nvs_set_u8(h, "display_mode"', source)
        runtime = Path('os/runtime/runtime.c').read_text()
        self.assertIn('lua_setfield(s, -2, "display_mode")', runtime)

    def test_system_hint_alarm_and_setting_glyphs_are_embedded(self):
        source = Path('assets/fonts/passport_cjk_extra_16.c').read_text()
        match = re.search(
            r'static const uint16_t unicode_list_0\[\] = \{(.*?)\};',
            source, re.S)
        self.assertIsNotNone(match)
        offsets = [int(value, 16) for value in
                   re.findall(r'0x([0-9a-fA-F]+)', match.group(1))]
        cmap = re.search(r'\.range_start\s*=\s*(\d+),\s*\.range_length',
                         source)
        self.assertIsNotNone(cmap)
        codepoints = {int(cmap.group(1)) + offset for offset in offsets}
        glyphs = re.search(
            r'static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc\[\] = '
            r'\{(.*?)\n\};', source, re.S)
        self.assertIsNotNone(glyphs)
        descriptors = re.findall(
            r'\{\.bitmap_index\s*=\s*\d+,\s*\.adv_w\s*=\s*\d+,\s*'
            r'\.box_w\s*=\s*(\d+),\s*\.box_h\s*=\s*(\d+)',
            glyphs.group(1))
        for cp in (0x53CC, 0x51FB, 0x6EDA, 0x7F16, 0x8F91, 0x95F9,
                   0x89C2, 0x8BFB, 0x9EA6, 0x98CE, 0x6682,
                   0x8BD5):
            self.assertIn(cp, codepoints)
            glyph_index = offsets.index(cp - int(cmap.group(1))) + 1
            width, height = map(int, descriptors[glyph_index])
            self.assertGreater(width, 0)
            self.assertGreater(height, 0)
        self.assertEqual(len(codepoints), 145)

    def test_native_xiaozhi_chinese_labels_have_glyphs(self):
        extra = Path('assets/fonts/passport_cjk_extra_16.c').read_text()
        start = int(re.search(r'\.range_start\s*=\s*(\d+)', extra)[1])
        offsets = re.search(r'unicode_list_0\[\] = \{(.*?)\};', extra, re.S)[1]
        supported = {start + int(value,16) for value in re.findall(r'0x([0-9a-fA-F]+)',offsets)}
        fallback = Path('assets/fonts/passport_cjk_chat_16.c').read_text()
        supported.update(int(value,16) for value in re.findall(r'U\+([0-9A-Fa-f]+)',fallback))
        required = {ord(c) for c in Path('examples/xiaozhi_native/main.c').read_text() if '\u4e00' <= c <= '\u9fff'}
        self.assertFalse(required - supported, ''.join(chr(cp) for cp in sorted(required-supported)))

    def test_chat_font_and_widget_are_independent_of_system_language(self):
        supported=set()
        for name in ('passport_cjk_extra_16','passport_cjk_chat_16'):
            text=Path('assets/fonts/'+name+'.c').read_text()
            supported.update(int(v,16) for v in re.findall(r'U\+([0-9a-fA-F]+)',text))
        previous=set(json.loads(Path('tests/fixtures/lvgl-sourcehan-9.5.0-codepoints.json').read_text())['codepoints'])
        self.assertFalse(previous-supported, 'Previous built-in glyph inventory lost')
        app=''.join(Path(p).read_text() for p in ('examples/bluetooth_peripheral/main.c','examples/city_radio/main.c','examples/city_radio/catalog.c'))
        for char in app:
            if ord(char)>127:self.assertIn(ord(char),supported)
        for hi in range(0xb0,0xd8):
            for lo in range(0xa1,0xff):
                try:char=bytes((hi,lo)).decode('gb2312')
                except UnicodeDecodeError:continue
                self.assertIn(ord(char),supported)
        for char in '嘿，您好！媒体音量闹钟静音切换项目设置自动保存失败上下调节每次扫码连接热点打开配网页手动网络密码显示二维码下一步取消在手机完成配网未弹出页面可扫码请打开手机热点配置已保存个人允许他人加入开启最大兼容性正在自动连接尚未重新修改绑定小智登录后台输入激活码绑定后过期按重试扫码或输入上方网址系统大小字节正在读取存储预留可用数据项查看详情当前应用包历史版本缓存及开销持久已每包含页另计刷新约浏览不可重试':self.assertIn(ord(char),supported)
        source=Path('main/passport_main.c').read_text()
        self.assertGreaterEqual(source.count('lv_obj_set_style_text_font(app_body, multilingual_font(), 0)'),2)

    def test_settings_scroll_footer_glyphs_exist_in_lvgl_fallback(self):
        source = Path(
            'assets/fonts/passport_cjk_chat_16.c').read_text()
        for codepoint in ('U+52A8', 'U+514B', 'U+4E0D', 'U+53EF', 'U+7528', 'U+91CD'):
            self.assertIn(codepoint, source)


if __name__ == '__main__':
    unittest.main()
