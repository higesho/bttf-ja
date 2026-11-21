# Ep1 のフック箇所のバイト列（アドレスを伏せたもの）を、ほかのエピソードのダンプから探す
#   python tools/sigs.py work/dump2.bin
import sys, re, capstone, struct
BASE = 0x400000
D1 = open('work/dump.bin', 'rb').read()
SITES = {'GETGLYPH': 0x65b710, 'PAGELOOP': 0x6eccbd, 'BINDCALL': 0x6f05c6, 'BIND': 0x75b190, 'SETTEXT': 0x6ec540,
         'E0': 0x6eaec0, 'E1': 0x6eb040, 'E2': 0x6eb120, 'E3': 0x6eb250, 'E4': 0x6eb620, 'E5': 0x6ec700, 'E6': 0x6ee8f0}
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True
def pattern(a, n=40, back=0):
    out = b''
    for i in md.disasm(D1[a - BASE:a - BASE + n + 16], a):
        if i.address >= a + n: break
        b = bytearray(i.bytes); mask = [True] * len(b)
        # rel32 / 絶対アドレスを伏せる
        if i.mnemonic in ('call', 'jmp') and b[0] in (0xe8, 0xe9): mask[1:5] = [False] * 4
        if b[0] == 0x0f and len(b) == 6 and 0x80 <= b[1] <= 0x8f: mask[2:6] = [False] * 4
        for k in range(len(b) - 3):
            v = struct.unpack_from('<I', b, k)[0]
            if 0x400000 <= v < 0xa00000: mask[k:k + 4] = [False] * 4
        out += b''.join((re.escape(bytes([x])) if m else b'.') for x, m in zip(b, mask))
    return out
D2 = open(sys.argv[1], 'rb').read()
for name, a in SITES.items():
    for n in (24, 40, 64):
        p = pattern(a, n)
        assert len(re.findall(p, D1, re.S)) >= 1
        hits = [BASE + m.start() for m in re.finditer(p, D2, re.S)]
        c1 = len(re.findall(p, D1, re.S))
        if c1 == 1 and len(hits) <= 1: break
    print('%-9s ep1 %08x  ep1hits %d  -> %s' % (name, a, c1, ' '.join('%08x' % h for h in hits[:5]) or 'NONE'))
