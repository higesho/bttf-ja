# text/en.json を会話の順（ファイル→ID）に並べ、work/chunks/cNN.json に分ける
import json, os, re, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); ROOT = __import__('ep').EPD   # エピソードの作業フォルダ
en = json.load(open(os.path.join(ROOT, 'text', 'en.json'), encoding='utf-8'))
SKIP_IDS = {226639961, 226639962, 226639963}      # 仮置きの歌詞（訳さない）
TAGS = re.compile(r'\[[^\]]*\]|\{[^}]*\}|<<[^>]*>>')
# Ep2 以降：Ep1 で訳した（または英語のまま残すと決めた）英文は訳さない（merge.py が引き継ぐ）
EP1 = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'text'))
done = set()
if ROOT != os.path.dirname(EP1):
    done |= set(json.load(open(os.path.join(EP1, 'ja.json'), encoding='utf-8')))
    done |= set(json.load(open(os.path.join(EP1, 'fixes.json'), encoding='utf-8')))
rows = []
for t, refs in en.items():
    if t in done: continue
    f, sp, i = refs[0]
    if any(r[2] in SKIP_IDS for r in refs): continue
    if not TAGS.sub('', t).strip(): continue        # タグだけの行は訳さない
    sps = sorted(set(r[1] for r in refs if r[1]))
    rows.append((f, i, '/'.join(sps), t))
rows.sort()
N = int(sys.argv[1]) if len(sys.argv) > 1 else 140
d = os.path.join(ROOT, 'work', 'chunks'); os.makedirs(d, exist_ok=True)
k = 0
for s in range(0, len(rows), N):
    items = [{'n': j, 'file': f.replace('_english.landb', ''), 'speaker': sp, 'en': t} for j, (f, i, sp, t) in enumerate(rows[s:s + N], s)]
    json.dump(items, open(os.path.join(d, 'c%02d.json' % k), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    k += 1
print(len(rows), 'rows', k, 'chunks')
