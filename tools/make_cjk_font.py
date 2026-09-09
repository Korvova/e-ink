"""Build data/notosc.ttf: Noto Sans SC instanced at wght=500 and subset to
Latin + Cyrillic + CJK punctuation + fullwidth forms + GB2312 level-1 hanzi (3755) + extra chars.

Usage: python tools/make_cjk_font.py [extra text whose characters must be included]
Source: _fonts/NotoSansSC.ttf (variable font from google/fonts, OFL).
"""
import os, sys
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools import subset

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "_fonts", "NotoSansSC.ttf")
DST = os.path.join(ROOT, "data", "notosc.ttf")

extra = " ".join(sys.argv[1:])

# GB2312 level-1 hanzi: rows 0xB0..0xD7
chars = set()
for hi in range(0xB0, 0xD8):
    for lo in range(0xA1, 0xFF):
        try:
            chars.add(bytes([hi, lo]).decode("gb2312"))
        except UnicodeDecodeError:
            pass
for cp in list(range(0x20, 0x7F)) + list(range(0xA0, 0x180)) + list(range(0x400, 0x460)) + \
          list(range(0x2010, 0x2028)) + list(range(0x3000, 0x3040)) + list(range(0xFF00, 0xFF66)) + \
          [0x2116, 0x20AC, 0x2022, 0x00B0, 0x2013, 0x2014, 0x2026]:
    chars.add(chr(cp))
chars.update(extra)
codepoints = sorted(ord(c) for c in chars)
print("characters:", len(codepoints))

f = TTFont(SRC)
if "fvar" in f:
    f = instancer.instantiateVariableFont(f, {"wght": 500})
opts = subset.Options()
opts.layout_features = ["kern"]
opts.name_IDs = ["*"]
opts.notdef_outline = True
opts.hinting = False
sub = subset.Subsetter(opts)
sub.populate(unicodes=codepoints)
sub.subset(f)
os.makedirs(os.path.dirname(DST), exist_ok=True)
f.save(DST)
print(DST, os.path.getsize(DST), "bytes")
