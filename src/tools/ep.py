# エピソードの選択（環境変数 BTTF_EP、既定 1）と共通のパス
#   EPD: そのエピソードの作業フォルダ（Ep1 は _jp 直下、Ep2 以降は _jp/epN）。text/ work/chunks/ build/ を持つ
import os, glob, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
EP = int(os.environ.get('BTTF_EP', '1'))
DIRS = {1: 'Back to the Future Ep 1', 2: 'Back to the Future Ep 2', 3: 'Back to the Future 103',
        4: 'Back to the Future 104', 5: 'Back to the Future 105'}
GAME = os.path.abspath(os.path.join(ROOT, '..', '..', DIRS[EP]))
EPD = ROOT if EP == 1 else os.path.join(ROOT, 'ep%d' % EP)
for d in ('text', 'build', os.path.join('work', 'chunks')): os.makedirs(os.path.join(EPD, d), exist_ok=True)

def landbs():
    """Pack 直下の .ttarch から *_english.landb を集める（名前順で先に見つかったものを使う。DLL と同じ規則）"""
    sys.path.insert(0, os.path.dirname(__file__))
    from ttarch import TTArch
    bf = None
    if EP == 5:                       # エピソード5はアーカイブとファイルが暗号化されている（tools/bfemu.py）
        from bfemu import BF
        bf = BF()
    out = {}
    for p in sorted(glob.glob(os.path.join(GAME, 'Pack', '*.ttarch')), key=lambda x: os.path.basename(x).lower()):
        a = TTArch(p, bf.decrypt if bf else None)
        if not a.info: continue
        for name, off, sz, z in a.files:
            if name.lower().endswith('_english.landb') and name not in out:
                d = a.read(off, sz)
                out[name] = bf.meta(d) if bf else d
    return out

def prev_ja():
    """前のエピソードまでの訳（英文→訳）。後のエピソードの訳を優先する。fixes で "" にしたもの（英語のまま）も含む"""
    import json
    out = {}
    for e in range(1, EP):
        d = ROOT if e == 1 else os.path.join(ROOT, 'ep%d' % e)
        for n in ('ja.json', 'fixes.json'):
            p = os.path.join(d, 'text', n)
            if os.path.exists(p): out.update(json.load(open(p, encoding='utf-8')))
    return out
