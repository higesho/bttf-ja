# Noto Sans JP（SIL OFL）を太さ 500 の静的フォントにし、訳で使う文字だけに絞って build/font.ttf（資源 BTTFJP/2）を作る
import os, json
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools import subset
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
ja = json.load(open(os.path.join(ROOT, 'text', 'ja.json'), encoding='utf-8'))
chars = set(''.join(ja.values()))
chars |= set('、。！？「」『』（）…─〜・ー：；')
chars |= set(chr(c) for c in range(0x3041, 0x3097)) | set(chr(c) for c in range(0x30a1, 0x30fb))
chars |= set(chr(c) for c in range(0xff01, 0xff5f))
f = TTFont(r'C:\Windows\Fonts\NotoSansJP-VF.ttf')
f = instancer.instantiateVariableFont(f, {'wght': 500})
opt = subset.Options()
opt.name_IDs = ['*']; opt.name_languages = ['*']; opt.notdef_outline = True; opt.hinting = False
opt.layout_features = []; opt.drop_tables += ['DSIG']
s = subset.Subsetter(opt)
s.populate(unicodes=[ord(c) for c in chars if ord(c) >= 0x20])
s.subset(f)
for rec in f['name'].names:
    if rec.nameID in (1, 4, 16): rec.string = 'BTTFJP Sans'
    elif rec.nameID == 6: rec.string = 'BTTFJPSans'
out = os.path.join(ROOT, 'build', 'font.ttf')
f.save(out)
print('font.ttf', len(chars), 'chars', os.path.getsize(out), 'bytes')
