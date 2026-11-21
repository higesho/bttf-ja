# .text を線形に逆アセンブルして pickle に保存
import capstone,pickle,sys,os
sys.path.insert(0,os.path.dirname(__file__))
from x86 import D,BASE
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
start=0x401000; end=0x401000+0x42ac3c
out=[]; a=start
while a<end:
    got=False
    for i in md.disasm_lite(D[a-BASE:end-BASE],a):
        out.append((i[0],i[2],i[3])); a=i[0]+i[1]; got=True
    if a<end: a+=1
pickle.dump(out,open(os.path.join(os.path.dirname(__file__),'..','work','sweep.pkl'),'wb'))
print(len(out))
