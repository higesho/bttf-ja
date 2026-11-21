import struct, sys, zlib, os
class TTArch:
    def __init__(self, path, dec=None):
        # dec: 暗号化されたアーカイブ（エピソード5）のファイル一覧を復号する関数（bfemu.BF().decrypt）
        self.path=path; f=open(path,'rb'); self.f=f; self.dec=dec
        h=struct.unpack('<4I',f.read(16)); self.ver,self.enc,self.u2,self.mode=h
        self.chunks=[]
        if self.mode==2:
            n=struct.unpack('<I',f.read(4))[0]
            self.chunks=list(struct.unpack('<%dI'%n,f.read(4*n)))
        else:
            n=struct.unpack('<I',f.read(4))[0]; assert n==0
        self.datasize,self.x1,self.x2,self.x3,self.x4,self.chunksz=struct.unpack('<6I',f.read(24))
        self.flag=f.read(1)[0]
        self.infosize=struct.unpack('<I',f.read(4))[0]
        if self.mode==2:
            csz=struct.unpack('<I',f.read(4))[0]
            info=zlib.decompress(f.read(csz),-15)
        else:
            info=f.read(self.infosize)
        if self.enc: info=dec(info) if dec else None
        self.info=info
        self.dataoff=f.tell()
        if info is not None: self.parse(info)
    def parse(self,b):
        p=0
        def u32():
            nonlocal p; v=struct.unpack_from('<I',b,p)[0]; p+=4; return v
        nd=u32(); self.dirs=[]
        for i in range(nd):
            l=u32(); self.dirs.append(b[p:p+l].decode('latin1')); p+=l
        nf=u32(); self.files=[]
        for i in range(nf):
            l=u32(); name=b[p:p+l].decode('latin1'); p+=l
            z=u32(); off=u32(); sz=u32()
            self.files.append((name,off,sz,z))
        self.rest=b[p:]
    def read(self,off,sz):
        if self.mode!=2:
            self.f.seek(self.dataoff+off); return self.f.read(sz)
        cs=self.chunksz*1024 if self.chunksz<0x1000 else self.chunksz
        out=b''; ci=off//cs; co=off%cs
        while len(out)<co+sz and False: pass
        res=bytearray()
        while len(res)<sz:
            res+=self.chunk(ci)[co:]; co=0; ci+=1
        return bytes(res[:sz])
    def chunk(self,i):
        o=self.dataoff+sum(self.chunks[:i]); self.f.seek(o); d=self.f.read(self.chunks[i])
        if self.enc and self.dec: d=self.dec(d)       # 暗号化されたアーカイブは、チャンクを復号してから展開する
        return zlib.decompress(d,-15)
if __name__=='__main__':
    a=TTArch(sys.argv[1])
    print(a.ver,a.enc,a.u2,a.mode,len(a.chunks),hex(a.datasize),hex(a.x1),hex(a.x2),a.x3,a.x4,a.chunksz,a.flag,a.infosize)
    if a.info:
        print(a.dirs[:10]); print(len(a.files))
        for x in a.files[:int(sys.argv[2]) if len(sys.argv)>2 else 20]: print(x)
        print('rest',a.rest[:32])
