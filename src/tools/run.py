# テスト用ゲームを DLL の共有メモリ経由で操作する（前面の窓は使わない）
#   python tools/run.py watch ADDR [SLOT] [rw]   ハードウェアブレークポイント（rw: r=読み書き w=書込）
#   python tools/run.py dump                    監視結果をログへ
#   python tools/run.py cur X Y                 カーソル位置（クライアント座標）
#   python tools/win.py start            copy build/dinput8.dll into the game, launch with BTTFJP_TEST=1
#   python tools/win.py shot NAME [W]    back-buffer capture -> capture/NAME.png (optional width)
#   python tools/win.py key K [K..]      tap keys (DIK names: enter esc space up down left right tab f1.. a..z 0..9)
#   python tools/win.py hold K MS
#   python tools/win.py move DX DY       relative mouse move (mickeys)
#   python tools/win.py click [left|right]
#   python tools/win.py lclick X Y | rclick X Y   move cursor (client coords) and click
#   python tools/win.py wait MS
#   python tools/win.py stop             kill the game, remove the DLL from the game folder
import sys, os, time, mmap, struct, subprocess, shutil, ctypes
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
# エピソードは環境変数 BTTF_EP（1〜5、既定 1）で選ぶ
EPS = {1: ('Back to the Future Ep 1', 31290), 2: ('Back to the Future Ep 2', 94500), 3: ('Back to the Future 103', 94510),
       4: ('Back to the Future 104', 94520), 5: ('Back to the Future 105', 94530)}
EP = int(os.environ.get('BTTF_EP', '1'))
GAME = os.path.abspath(os.path.join(ROOT, '..', '..', EPS[EP][0]))
EXE = 'BackToTheFuture10%d.exe' % EP
PIDF = os.path.join(ROOT, 'work', 'pid.txt')
SHM_SIZE = 4384
O_WR, O_RD, O_CMD, O_REQ, O_DONE, O_PATH, O_FRAME, O_BUSY = 4, 8, 12, 4108, 4112, 4116, 4376, 4380

DIK = {'esc': 1, 'enter': 0x1c, 'space': 0x39, 'tab': 0x0f, 'back': 0x0e, 'up': 0xc8, 'down': 0xd0,
       'left': 0xcb, 'right': 0xcd, 'lshift': 0x2a, 'lctrl': 0x1d, 'lalt': 0x38, 'pgup': 0xc9, 'pgdn': 0xd1,
       'home': 0xc7, 'end': 0xcf, 'del': 0xd3, 'minus': 0x0c, 'equals': 0x0d}
for i, c in enumerate('1234567890'): DIK[c] = 2 + i
for row, start in (('qwertyuiop', 0x10), ('asdfghjkl', 0x1e), ('zxcvbnm', 0x2c)):
    for i, c in enumerate(row): DIK[c] = start + i
for i in range(10): DIK['f%d' % (i + 1)] = 0x3b + i
DIK['f11'] = 0x57; DIK['f12'] = 0x58

def shm():
    m = mmap.mmap(-1, SHM_SIZE, tagname='BTTFJP_TESTIO')
    if struct.unpack_from('<I', m, 0)[0] != 0x504a5442:
        raise SystemExit('game (test DLL) not running')
    return m

def push(m, *cmds):
    for c in cmds:
        while True:
            wr, rd = struct.unpack_from('<ii', m, O_WR)
            if wr - rd < 250: break
            time.sleep(0.02)
        c = list(c) + [0] * (4 - len(c))
        struct.pack_into('<4i', m, O_CMD + (wr & 255) * 16, *c)
        struct.pack_into('<i', m, O_WR, wr + 1)

def settle(m, timeout=30):
    t = time.time()
    while time.time() - t < timeout:
        wr, rd = struct.unpack_from('<ii', m, O_WR)
        busy = struct.unpack_from('<i', m, O_BUSY)[0]
        if wr == rd and not busy: return
        time.sleep(0.03)

def frame(m): return struct.unpack_from('<i', m, O_FRAME)[0]

def tap(m, k, hold=60, gap=80):
    code = DIK[k] if k in DIK else int(k, 0)
    push(m, (1, code), (6, hold), (2, code), (6, gap))

def shot(name, width=None):
    from PIL import Image
    m = shm()
    settle(m)
    os.makedirs(os.path.join(ROOT, 'capture'), exist_ok=True)
    bmp = os.path.join(ROOT, 'work', 'shot.bmp')
    if os.path.exists(bmp): os.remove(bmp)
    p = bmp.encode('mbcs') + b'\0'
    m[O_PATH:O_PATH + len(p)] = p
    req = struct.unpack_from('<i', m, O_REQ)[0] + 1
    struct.pack_into('<i', m, O_REQ, req)
    t = time.time()
    while struct.unpack_from('<i', m, O_DONE)[0] != req:
        if time.time() - t > 15: raise SystemExit('shot timeout (game paused/minimized?)')
        time.sleep(0.05)
    im = Image.open(bmp).convert('RGB')
    if width: im = im.resize((int(width), int(im.height * int(width) / im.width)), Image.LANCZOS)
    out = os.path.join(ROOT, 'capture', name + '.png')
    im.save(out)
    print(out, im.size)

def start():
    exe = os.path.join(GAME, EXE)
    open(os.path.join(GAME, 'steam_appid.txt'), 'w').write(str(EPS[EP][1]))  # テスト専用（直接起動用）
    shutil.copy(os.path.join(ROOT, 'build' if EP == 1 else 'ep%d/build' % EP, 'dinput8.dll'), os.path.join(GAME, 'dinput8.dll'))
    env = dict(os.environ)
    env['BTTFJP_TEST'] = '1'
    env['BTTFJP_LOG'] = os.path.join(ROOT, 'work', 'bttfjp.log')
    args = []
    for a in sys.argv[2:]:
        if '=' in a:
            k, v = a.split('=', 1); env[k] = v
        else: args.append(a)
    p = subprocess.Popen([exe] + args, cwd=GAME, env=env)
    open(PIDF, 'w').write(str(p.pid))
    print('pid', p.pid)

def stop():
    try:
        pid = int(open(PIDF).read())
        subprocess.run(['taskkill', '/PID', str(pid), '/F'], capture_output=True)
    except Exception as e:
        print(e)
    time.sleep(1)
    for f in ('dinput8.dll', 'steam_appid.txt'):
        try: os.remove(os.path.join(GAME, f))
        except OSError as e: print(e)

def game_hwnd():
    import ctypes.wintypes as W
    u = ctypes.windll.user32
    pid = int(open(PIDF).read())
    hw = []
    @ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    def cb(h, l):
        p = W.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(p))
        b = ctypes.create_unicode_buffer(64); u.GetWindowTextW(h, b, 64)
        if p.value == pid and b.value == 'Telltale Games' and u.IsWindowVisible(h): hw.append(h)
        return True
    u.EnumWindows(cb, 0)
    if not hw: raise SystemExit('game window not found')
    return hw[0]

def front():
    """ゲームは前面にいないと描画を止めることがある（別の窓が前に出たとき）。テスト中は前に出しておく"""
    try:
        h = game_hwnd(); u = ctypes.windll.user32
        if u.GetForegroundWindow() != h: u.SetForegroundWindow(h)
    except SystemExit: pass

def mouse_to(x, y):
    """cursor position comes from WM_MOUSEMOVE (client coords = 1280x1024 virtual)"""
    h = game_hwnd()
    for k in range(3):
        ctypes.windll.user32.PostMessageW(h, 0x200, 0, (int(y) << 16) | (int(x) & 0xffff))
        time.sleep(0.05)

def main():
    a = sys.argv[1:]
    if not a: print(__doc__); return
    c = a[0]
    if c == 'start': start()
    elif c == 'stop': stop()
    elif c == 'shot': front(); shot(a[1], a[2] if len(a) > 2 else None)
    elif c == 'key':
        m = shm()
        for k in a[1:]: tap(m, k)
        settle(m)
    elif c == 'hold':
        m = shm(); code = DIK[a[1]]
        push(m, (1, code), (6, int(a[2])), (2, code), (6, 60)); settle(m)
    elif c == 'move':
        m = shm(); push(m, (3, int(a[1]), int(a[2])), (6, 40)); settle(m)
    elif c == 'click':
        m = shm(); b = 1 if (len(a) > 1 and a[1] == 'right') else 0
        push(m, (4, b), (6, 60), (5, b), (6, 80)); settle(m)
    elif c == 'mouse':
        mouse_to(a[1], a[2]); time.sleep(0.2)
    elif c == 'lclick' or c == 'rclick':
        mouse_to(a[1], a[2]); time.sleep(0.2)
        m = shm(); b = 1 if c == 'rclick' else 0
        push(m, (4, b), (6, 80), (5, b), (6, 150)); settle(m)
    elif c == 'type':
        # text entry uses WM_KEYDOWN (virtual key codes)
        h = game_hwnd()
        for ch in ' '.join(a[1:]):
            vk = ctypes.windll.user32.VkKeyScanW(ord(ch)) & 0xff
            ctypes.windll.user32.PostMessageW(h, 0x100, vk, 1)
            time.sleep(0.08)
            ctypes.windll.user32.PostMessageW(h, 0x101, vk, 0xc0000001)
            time.sleep(0.08)
    elif c == 'vk':
        h = game_hwnd()
        for k in a[1:]:
            vk = int(k, 0)
            ctypes.windll.user32.PostMessageW(h, 0x100, vk, 1); time.sleep(0.08)
            ctypes.windll.user32.PostMessageW(h, 0x101, vk, 0xc0000001); time.sleep(0.08)
    elif c == 'wait':
        front(); m = shm(); push(m, (6, int(a[1]))); settle(m)
    elif c == 'watch':
        m = shm(); slot = int(a[2]) if len(a) > 2 else 0
        rw = (1 if (len(a) > 3 and a[3] == 'w') else 3) | (4 << 4)
        push(m, (8, int(a[1], 16), slot, rw)); settle(m)
    elif c == 'dump':
        m = shm(); push(m, (9,)); settle(m); time.sleep(0.3)
        for l in open(os.path.join(ROOT, 'work', 'bttfjp.log'), encoding='utf-8', errors='replace').read().split('---- hits')[-1].splitlines(): print(l)
    elif c == 'cur':
        m = shm(); push(m, (10, int(a[1]), int(a[2])), (6, 100)); settle(m)
    elif c == 'lc':
        m = shm(); push(m, (10, int(a[1]), int(a[2])), (6, 150), (4, 0), (6, 80), (5, 0), (6, 150)); settle(m)
    elif c == 'pc':
        # カーソルを置いてからウインドウメッセージでクリック
        m = shm(); x, y = int(a[1]), int(a[2])
        push(m, (10, x, y), (6, 200)); settle(m)
        h = game_hwnd(); lp = (y << 16) | x
        ctypes.windll.user32.PostMessageW(h, 0x200, 0, lp); time.sleep(0.1)
        ctypes.windll.user32.PostMessageW(h, 0x201, 1, lp); time.sleep(0.1)
        ctypes.windll.user32.PostMessageW(h, 0x202, 0, lp); time.sleep(0.3)
    elif c == 'frame':
        print(frame(shm()))
    else: print(__doc__)

if __name__ == '__main__':
    main()
