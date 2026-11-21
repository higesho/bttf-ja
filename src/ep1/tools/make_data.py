# text/ja.json（英文→訳）と text/ids.json（"ファイル#ID"→訳）から build/tr.bin（資源 BTTFJP/1）を作る
import json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
from jptext import encode
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
def fnv64(b):
    h = 0xcbf29ce484222325
    for x in b: h = ((h ^ x) * 0x100000001b3) & 0xffffffffffffffff
    return h
recs = {}
def load(p):
    p = os.path.join(ROOT, 'text', p)
    return json.load(open(p, encoding='utf-8')) if os.path.exists(p) else {}
for en, ja in load('ja.json').items():
    if ja: recs[fnv64(en.encode('cp1252'))] = encode(ja)
for k, ja in load('ids.json').items():
    if ja: recs[fnv64(('#' + k).encode())] = encode(ja)
keys = sorted(recs)
pool = bytearray(); offs = []
base = 8 + 12 * len(keys)
for k in keys:
    offs.append(base + len(pool)); pool += recs[k] + b'\0'
out = b'BTJ1' + struct.pack('<I', len(keys)) + b''.join(struct.pack('<QI', k, o) for k, o in zip(keys, offs)) + bytes(pool)
os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
open(os.path.join(ROOT, 'build', 'tr.bin'), 'wb').write(out)
print('tr.bin', len(keys), 'entries', len(out), 'bytes')
