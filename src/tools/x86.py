import capstone,sys,struct
import os
D=open(os.environ.get('X86DUMP',os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','work','dump.bin')),'rb').read()
BASE=0x400000
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32); md.detail=False
def rd(a,n): return D[a-BASE:a-BASE+n]
def u32(a): return struct.unpack_from('<I',D,a-BASE)[0]
def dis(a,n=40):
    out=[]
    for i in md.disasm(rd(a,n*8),a):
        out.append(i)
        if len(out)>=n: break
    return out
def pr(a,n=40):
    for i in dis(a,n): print('%08x  %-24s %s %s'%(i.address,i.bytes.hex(),i.mnemonic,i.op_str))
if __name__=='__main__':
    pr(int(sys.argv[1],16), int(sys.argv[2]) if len(sys.argv)>2 else 40)
def around(h,back=40,fwd=8):
    for s in range(h-back,h):
        ins=dis(s,back+fwd)
        if any(i.address==h for i in ins): return [i for i in ins if i.address<=h+fwd*4]
    return dis(h,fwd)
