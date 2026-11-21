# ゲームの窓をプロセス名で特定して画面取得・入力を行う
import ctypes,ctypes.wintypes as W,sys,subprocess,time
u=ctypes.windll.user32; g=ctypes.windll.gdi32
PROC='BackToTheFuture101'
def pids():
    o=subprocess.run(['powershell','-NoProfile','(Get-Process %s -ErrorAction SilentlyContinue).Id'%PROC],capture_output=True,text=True).stdout.split()
    return [int(x) for x in o]
def hwnd():
    ps=set(pids()); res=[]
    @ctypes.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
    def cb(h,l):
        p=W.DWORD(); u.GetWindowThreadProcessId(h,ctypes.byref(p))
        if p.value in ps and u.IsWindowVisible(h):
            r=W.RECT(); u.GetClientRect(h,ctypes.byref(r))
            res.append((r.right*r.bottom,h))
        return True
    u.EnumWindows(cb,0)
    return max(res)[1] if res else None
def title(h):
    b=ctypes.create_unicode_buffer(256); u.GetWindowTextW(h,b,256); return b.value
def shot(path):
    from PIL import Image
    h=hwnd(); r=W.RECT(); u.GetClientRect(h,ctypes.byref(r)); w,hh=r.right,r.bottom
    dc=u.GetDC(h); mdc=g.CreateCompatibleDC(dc); bm=g.CreateCompatibleBitmap(dc,w,hh); g.SelectObject(mdc,bm)
    ok=u.PrintWindow(h,mdc,3)
    class BIH(ctypes.Structure):
        _fields_=[('biSize',W.DWORD),('biWidth',W.LONG),('biHeight',W.LONG),('biPlanes',W.WORD),('biBitCount',W.WORD),('biCompression',W.DWORD),('a',W.DWORD*5)]
    bi=BIH(40,w,-hh,1,32,0); buf=ctypes.create_string_buffer(w*hh*4)
    g.GetDIBits(mdc,bm,0,hh,buf,ctypes.byref(bi),0)
    Image.frombuffer('RGBA',(w,hh),buf,'raw','BGRA',0,1).convert('RGB').save(path)
    g.DeleteObject(bm); g.DeleteDC(mdc); u.ReleaseDC(h,dc); print(path,w,hh,ok)
if __name__=='__main__':
    c=sys.argv[1]
    if c=='info':
        h=hwnd(); r=W.RECT(); u.GetWindowRect(h,ctypes.byref(r)); print(h,title(h),r.left,r.top,r.right,r.bottom)
    elif c=='shot': shot(sys.argv[2])
def post_click(x,y):
    h=hwnd(); lp=(y<<16)|x
    u.PostMessageW(h,0x200,0,lp); time.sleep(0.05)
    u.PostMessageW(h,0x201,1,lp); time.sleep(0.08); u.PostMessageW(h,0x202,0,lp)
def post_move(x,y):
    h=hwnd(); u.PostMessageW(h,0x200,0,(y<<16)|x)
if __name__=='__main__' and sys.argv[1]=='click': post_click(int(sys.argv[2]),int(sys.argv[3]))
if __name__=='__main__' and sys.argv[1]=='move': post_move(int(sys.argv[2]),int(sys.argv[3]))
