# BTTF（Telltale Tool 2010）の .landb を読む／書く
import struct
class Landb:
    def __init__(self, data):
        self.d=data
        nc=struct.unpack_from('<I',data,4)[0]; p=8+12*nc
        self.head=data[:p]
        self.pre=data[p:p+16]           # [blk 8 v][blk 8 v]
        p+=16
        self.mapoff=p
        msize,cnt=struct.unpack_from('<II',data,p); p+=8
        self.entries=[]
        end=self.mapoff+msize
        for i in range(cnt):
            e={}
            e['key'],e['id'],e['z1']=struct.unpack_from('<3I',data,p); p+=12
            b0=p
            for k in range(3): p+=struct.unpack_from('<I',data,p)[0]   # ハンドル類（anm / wav など）
            e['b']=data[b0:p]
            e['z2']=struct.unpack_from('<I',data,p)[0]; p+=4
            sb=struct.unpack_from('<I',data,p)[0]; s0=p; p+=4
            strs=[]
            for k in range(2):
                bl,ln=struct.unpack_from('<II',data,p)
                assert bl==ln+8,(hex(p),bl,ln)
                strs.append(data[p+8:p+8+ln]); p+=bl
            e['inner']=data[p:s0+sb]; p=s0+sb   # [blk 8 v]
            e['prefix'],e['text']=strs
            e['tail']=data[p:p+4]; p+=4     # flags
            self.entries.append(e)
        assert p==end,(p,end)
        self.post=data[end:]
    def build(self):
        out=[self.head,self.pre]
        body=[]
        for e in self.entries:
            b=struct.pack('<3I',e['key'],e['id'],e['z1'])+e['b']+struct.pack('<I',e['z2'])
            ss=b''.join(struct.pack('<II',len(s)+8,len(s))+s for s in (e['prefix'],e['text']))+e['inner']
            b+=struct.pack('<I',len(ss)+4)+ss+e['tail']
            body.append(b)
        body=b''.join(body)
        out.append(struct.pack('<II',len(body)+8,len(self.entries))+body)
        out.append(self.post)
        return b''.join(out)
if __name__=='__main__':
    import sys,glob
    for f in sys.argv[1:]:
        L=Landb(open(f,'rb').read())
        assert L.build()==L.d
        print(f.split('/')[-1],len(L.entries))
