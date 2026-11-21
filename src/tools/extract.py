# 全 landb の英文を text/en.json（英文 → [ファイル, 話し手, ID] の一覧）に書き出す（BTTF_EP でエピソードを選ぶ）
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from landb import Landb
import ep
out = {}
for n, data in sorted(ep.landbs().items()):
    L = Landb(data)
    assert L.build() == data, n
    for e in L.entries:
        t = e['text'].decode('cp1252', errors='replace')
        if not t.strip(): continue
        out.setdefault(t, []).append([n, e['prefix'].decode('cp1252', errors='replace'), e['id']])
json.dump(out, open(os.path.join(ep.EPD, 'text', 'en.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
print('ep%d' % ep.EP, len(out), 'texts')
