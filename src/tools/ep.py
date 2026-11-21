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
    out = {}
    for p in sorted(glob.glob(os.path.join(GAME, 'Pack', '*.ttarch')), key=lambda x: os.path.basename(x).lower()):
        a = TTArch(p)
        if not a.info: continue
        for name, off, sz, z in a.files:
            if name.lower().endswith('_english.landb') and name not in out: out[name] = a.read(off, sz)
    return out
