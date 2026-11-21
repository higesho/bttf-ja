# エピソード5のアーカイブ復号：ゲーム自身の Blowfish（Telltale 版）を unicorn で動かす
#   exe のメモリイメージ（work/dump5.bin、起動中のゲームから読み出したもの）が要る
#   init: 0x5ad850（thiscall、既定の鍵）  decrypt: 0x5ac9a0（thiscall、buf, len。len は 8 の倍数）
import os, struct
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EIP
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
ADDR = {5: (os.path.join(ROOT, 'work', 'dump5.bin'), 0x5ad850, 0x5ac9a0)}
BASE, STACK, OBJ, BUF, RET = 0x400000, 0x10000000, 0x11000000, 0x12000000, 0x13000000

class BF:
    def __init__(self, ep=5):
        dump, self.init, self.dec = ADDR[ep]
        img = open(dump, 'rb').read()
        u = self.u = Uc(UC_ARCH_X86, UC_MODE_32)
        u.mem_map(BASE, (len(img) + 0xfff) & ~0xfff); u.mem_write(BASE, img)
        u.mem_map(STACK, 0x100000); u.mem_map(OBJ, 0x10000); u.mem_map(RET, 0x1000)
        u.mem_write(RET, b'\xcc')
        self.bufsz = 0
        self.call(self.init, [])
    def call(self, fn, args):
        u = self.u
        sp = STACK + 0xff000
        for a in reversed(args): sp -= 4; u.mem_write(sp, struct.pack('<I', a))
        sp -= 4; u.mem_write(sp, struct.pack('<I', RET))
        u.reg_write(UC_X86_REG_ESP, sp); u.reg_write(UC_X86_REG_ECX, OBJ)
        u.emu_start(fn, RET)
    def decrypt(self, data):
        n = len(data) & ~7
        if n == 0: return data
        need = (n + 0xfff) & ~0xfff
        if need > self.bufsz:
            if self.bufsz: self.u.mem_unmap(BUF, self.bufsz)
            self.u.mem_map(BUF, need); self.bufsz = need
        self.u.mem_write(BUF, bytes(data[:n]))
        self.call(self.dec, [BUF, n])
        return bytes(self.u.mem_read(BUF, n)) + bytes(data[n:])

    def meta(self, d):
        """Telltale のメタデータ暗号（先頭 AA DE AF 64）を解いて ERTM で始まるデータにする。
        0x100 バイトごとの区切り（魔法の 4 バイトの後から数える）。8 個目ごとに Blowfish、
        それ以外で 0x18 の倍数でないものは全バイト反転（exe 0x44e0c0）。最後の半端な区切りはそのまま"""
        if d[:4] != bytes([0xaa, 0xde, 0xaf, 0x64]): return d
        out = bytearray(d[4:]); bs = 0x100
        for i in range(len(out) // bs):
            blk = bytes(out[i * bs:(i + 1) * bs])
            if i % 8 == 0: out[i * bs:(i + 1) * bs] = self.decrypt(blk)
            elif i % 0x18 != 0: out[i * bs:(i + 1) * bs] = bytes(x ^ 0xff for x in blk)
        return b'ERTM' + bytes(out)

if __name__ == '__main__':
    import sys
    bf = BF()
    d = open(sys.argv[1], 'rb').read()
    print(bf.decrypt(d[:64]).hex())
