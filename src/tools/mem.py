import os
import ctypes,ctypes.wintypes as W,subprocess,re
k=ctypes.windll.kernel32
def pidof(name='BackToTheFuture10'+os.environ.get('BTTF_EP','1')):
    out=subprocess.check_output(['powershell','-NoProfile','(Get-Process %s).Id'%name]).split()
    return int(out[0])
pid=pidof()
h=k.OpenProcess(0x0438,False,pid)
def read(a,n):
    b=ctypes.create_string_buffer(n); r=ctypes.c_size_t()
    k.ReadProcessMemory(h,ctypes.c_void_p(a),b,n,ctypes.byref(r)); return b.raw[:r.value]
def write(a,data):
    r=ctypes.c_size_t(); return k.WriteProcessMemory(h,ctypes.c_void_p(a),data,len(data),ctypes.byref(r))
class MBI(ctypes.Structure):
    _fields_=[('BaseAddress',ctypes.c_void_p),('AllocationBase',ctypes.c_void_p),('AllocationProtect',W.DWORD),('RegionSize',ctypes.c_size_t),('State',W.DWORD),('Protect',W.DWORD),('Type',W.DWORD)]
def regions():
    a=0;m=MBI()
    while k.VirtualQueryEx(h,ctypes.c_void_p(a),ctypes.byref(m),ctypes.sizeof(m)):
        if m.State==0x1000 and m.Protect&0xee and not m.Protect&0x100: yield (m.BaseAddress or 0),m.RegionSize
        a=(m.BaseAddress or 0)+m.RegionSize
        if a>=0x7fff0000: break
def find(p,maxsize=0x20000000):
    out=[]
    for base,size in regions():
        if size>maxsize: continue
        d=read(base,size)
        out+=[base+m.start() for m in re.finditer(re.escape(p),d)]
    return out
