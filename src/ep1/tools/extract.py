# 全 landb の英文を text/en.json（英文 → [ファイル, 話し手, ID] の一覧）に書き出す
import sys, os, glob, json
sys.path.insert(0, os.path.dirname(__file__))
from landb import Landb
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
out = {}
for f in sorted(glob.glob(os.path.join(ROOT, 'orig', '0_dlog', '*_english.landb'))):
    L = Landb(open(f, 'rb').read()); n = os.path.basename(f)
    for e in L.entries:
        t = e['text'].decode('cp1252', errors='replace')
        if not t.strip(): continue
        out.setdefault(t, []).append([n, e['prefix'].decode('cp1252', errors='replace'), e['id']])
json.dump(out, open(os.path.join(ROOT, 'text', 'en.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
print(len(out), 'texts')
