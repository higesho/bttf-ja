# 外部デバッガとしてアタッチし、指定アドレスの読み取りをハードウェアブレークポイントで監視して EIP を集める
import ctypes,ctypes.wintypes as W,sys,time,subprocess,collections
k=ctypes.windll.kernel32
pid=int(subprocess.check_output(['powershell','-NoProfile','(Get-Process BackToTheFuture101).Id']).split()[0])
addrs=[int(x,16) for x in sys.argv[1].split(',')]
dur=float(sys.argv[2]) if len(sys.argv)>2 else 10
class FLOATING_SAVE_AREA(ctypes.Structure):
    _fields_=[('a',W.DWORD*7),('r',ctypes.c_byte*80),('c',W.DWORD)]
class CONTEXT(ctypes.Structure):
    _fields_=[('ContextFlags',W.DWORD),('Dr0',W.DWORD),('Dr1',W.DWORD),('Dr2',W.DWORD),('Dr3',W.DWORD),('Dr6',W.DWORD),('Dr7',W.DWORD),
    ('Flt',FLOATING_SAVE_AREA),('SegGs',W.DWORD),('SegFs',W.DWORD),('SegEs',W.DWORD),('SegDs',W.DWORD),
    ('Edi',W.DWORD),('Esi',W.DWORD),('Ebx',W.DWORD),('Edx',W.DWORD),('Ecx',W.DWORD),('Eax',W.DWORD),
    ('Ebp',W.DWORD),('Eip',W.DWORD),('SegCs',W.DWORD),('EFlags',W.DWORD),('Esp',W.DWORD),('SegSs',W.DWORD),('Ext',ctypes.c_byte*512)]
class DEBUG_EVENT(ctypes.Structure):
    _fields_=[('code',W.DWORD),('pid',W.DWORD),('tid',W.DWORD),('pad',W.DWORD),('u',ctypes.c_byte*176)]
CONTEXT_DEBUG=0x10010; CONTEXT_FULL=0x1001f
k.Wow64GetThreadContext.argtypes=[W.HANDLE,ctypes.c_void_p]; k.Wow64SetThreadContext.argtypes=[W.HANDLE,ctypes.c_void_p]
threads={}
def setbp(th,on=True):
    c=CONTEXT(); c.ContextFlags=CONTEXT_DEBUG
    if not k.Wow64GetThreadContext(th,ctypes.byref(c)): return
    d7=0
    for i,a in enumerate(addrs[:4]):
        setattr(c,'Dr%d'%i,a if on else 0)
        if on: d7|=(1<<(i*2))|(3<<(16+i*4))|(3<<(18+i*4))  # RW, len4
    c.Dr7=d7; c.Dr6=0
    k.Wow64SetThreadContext(th,ctypes.byref(c))
if not k.DebugActiveProcess(pid): print('attach fail',k.GetLastError()); sys.exit()
k.DebugSetProcessKillOnExit(False)
hits=collections.Counter(); regs={}
ev=DEBUG_EVENT(); t0=time.time(); armed=False; nev=collections.Counter()
while time.time()-t0<dur:
    if not k.WaitForDebugEvent(ctypes.byref(ev),200):
        nev_idle=1
        continue
    st=0x00010002 # DBG_CONTINUE
    nev[ev.code]+=1
    if ev.code==1: nev[hex(W.DWORD.from_buffer(ev.u,0).value)]+=1
    if ev.code==3 or ev.code==2: # create process/thread
        h=ctypes.c_void_p.from_buffer(ev.u,16 if ev.code==3 else 0).value
        threads[ev.tid]=h; setbp(h)
    elif ev.code==1:
        exc=W.DWORD.from_buffer(ev.u,0).value
        if exc==0x4000001e or exc==0x80000004: # WOW single step
            th=k.OpenThread(0x1fffff,False,ev.tid)
            c=CONTEXT(); c.ContextFlags=CONTEXT_FULL|CONTEXT_DEBUG; k.Wow64GetThreadContext(th,ctypes.byref(c))
            hits[c.Eip]+=1
            if c.Eip not in regs: regs[c.Eip]=(c.Eax,c.Ebx,c.Ecx,c.Edx,c.Esi,c.Edi,c.Ebp,c.Esp)
            c.Dr6=0; k.Wow64SetThreadContext(th,ctypes.byref(c)); k.CloseHandle(th)
        elif exc==0x4000001f or exc==0x80000003:
            if not armed:
                armed=True
                # 全スレッドに設定
                import subprocess
            pass
        else:
            st=0x80010001 # not handled
    k.ContinueDebugEvent(ev.pid,ev.tid,st)
# 解除
for tid,h in threads.items(): setbp(h,False)
k.DebugActiveProcessStop(pid)
print("threads",len(threads),"events",nev)
for e,n in hits.most_common(40): print(hex(e),n,[hex(x) for x in regs[e]])
