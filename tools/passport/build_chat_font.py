#!/usr/bin/env python3
"""Build the shared compressed 2-bpp multilingual font (LVGL RLE)."""
import argparse,json,re,subprocess
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('--converter',default='lv_font_conv');args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
builtin=(root/'managed_components/lvgl__lvgl/src/font/lv_font_source_han_sans_sc_16_cjk.c').read_text()
builtin_chars={int(x,16) for x in re.findall(r'U\+([0-9a-fA-F]+)',builtin)}
covered=set()
extra=(root/'assets/fonts/passport_cjk_extra_16.c').read_text();base=int(re.search(r'\.range_start\s*=\s*(\d+)',extra)[1]);covered|={base+int(x,16) for x in re.findall(r'0x([0-9a-fA-F]+)',re.search(r'unicode_list_0\[\] = \{(.*?)\};',extra,re.S)[1])}
common={chr(cp) for cp in builtin_chars}
for hi in range(0xb0,0xd8):
 for lo in range(0xa1,0xff):
  try:common.add(bytes((hi,lo)).decode('gb2312'))
  except UnicodeDecodeError:pass
common.update('嘿嗯喽嗨哟哇啦哦咱嘛咋啥咯耶喔哎呀诶，。！？；：“”‘’（）【】《》…—')
# Include maintained Native UI labels (e.g. Shenzhen's level-2 character).
for manifest in sorted((root/'examples').glob('*/manifest.json')):
 if json.loads(manifest.read_text()).get('runtime')=='native-elf':
  for source in sorted(manifest.parent.glob('*.c')):
   common.update(c for c in source.read_text() if ord(c)>127)
symbols=''.join(sorted(c for c in common if ord(c) not in covered))
output=root/'assets/fonts/passport_cjk_chat_16.c'
font_dir=root/'managed_components/lvgl__lvgl/scripts/built_in_font'
regular=''.join(c for c in symbols if not (0xe000<=ord(c)<=0xf8ff))
icons=''.join(c for c in symbols if 0xe000<=ord(c)<=0xf8ff)
subprocess.run([args.converter,'--font',str(font_dir/'SourceHanSansSC-Normal.otf'),'--symbols',regular,
 '--font',str(font_dir/'FontAwesome5-Solid+Brands+Regular.woff'),'--symbols',icons,
 '--size','16','--bpp','2','--format','lvgl','--lv-font-name','passport_cjk_chat_16',
 '--lv-include','lvgl.h','--output',str(output)],check=True)
# Keep generated metadata portable rather than embedding the developer's home path.
s=output.read_text().replace(str(root)+'/','');output.write_text(s)
print('GB2312 level 1 + previous built-in inventory + conversation:',len(common),'requested;',len(symbols),'shared glyphs')
