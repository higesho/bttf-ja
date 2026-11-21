# 訳のチェック：python tools/check_chunk.py c03 [c04 ...]   （work/chunks/cNN_ja.json）
import json, os, re, sys
sys.path.insert(0, os.path.dirname(__file__))
from jptext import encode, REPL
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); ROOT = __import__('ep').EPD   # エピソードの作業フォルダ
TAGS = re.compile(r'\[[^\]]*\]|\{[^}]*\}|<<[^>]*>>')
def check(name):
    d = os.path.join(ROOT, 'work', 'chunks')
    src = json.load(open(os.path.join(d, name + '.json'), encoding='utf-8'))
    p = os.path.join(d, name + '_ja.json')
    if not os.path.exists(p): return ['%s: no output file' % name]
    try: ja = json.load(open(p, encoding='utf-8'))
    except Exception as e: return ['%s: JSON error %s' % (name, e)]
    errs = []
    for it in src:
        n = str(it['n']); en = it['en']
        if n not in ja or not ja[n]: errs.append('%s: missing' % n); continue
        t = ja[n]
        if TAGS.findall(t) != TAGS.findall(en): errs.append('%s: tags differ: %s / %s' % (n, TAGS.findall(en), TAGS.findall(t)))
        if t.count('\n') != en.count('\n'): errs.append('%s: newline count %d != %d' % (n, t.count('\n'), en.count('\n')))
        body = TAGS.sub('', t)
        s = body
        for a, b in REPL.items(): s = s.replace(a, b)
        try: s.encode('euc_jp')
        except UnicodeEncodeError as e: errs.append('%s: not EUC-JP: %r' % (n, s[e.start:e.end]))
        if re.search(r'[A-Za-z]{4,}', body) and not re.search(r'[぀-ヿ一-鿿]', body): errs.append('%s: not translated? %r' % (n, body[:40]))
        if len(TAGS.sub('', en)) > 20 and len(body) > len(TAGS.sub('', en)) * 1.1: errs.append('%s: too long (%d > en %d)' % (n, len(body), len(TAGS.sub('', en))))
    return errs
if __name__ == '__main__':
    bad = 0
    for a in sys.argv[1:]:
        e = check(a); bad += len(e)
        for x in e: print(a, x)
        if not e: print(a, 'OK')
    sys.exit(1 if bad else 0)
