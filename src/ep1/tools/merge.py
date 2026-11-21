# work/chunks/cNN_ja.json をまとめて text/ja.json（英文 → 訳）を作る。text/fixes.json（英文 → 訳）で上書き
import json, os, glob
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
out = {}
n = 0
for p in sorted(glob.glob(os.path.join(ROOT, 'work', 'chunks', 'c[0-9][0-9].json'))):
    src = json.load(open(p, encoding='utf-8'))
    q = p[:-5] + '_ja.json'
    if not os.path.exists(q): print('missing', os.path.basename(q)); continue
    ja = json.load(open(q, encoding='utf-8'))
    for it in src:
        t = ja.get(str(it['n']))
        if t: out[it['en']] = t; n += 1
fx = os.path.join(ROOT, 'text', 'fixes.json')
if os.path.exists(fx):
    for k, v in json.load(open(fx, encoding='utf-8')).items(): out[k] = v
json.dump(out, open(os.path.join(ROOT, 'text', 'ja.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
print('ja.json', len(out), 'entries (chunks %d)' % n)
