# drag.py X0 Y0 X1 Y1 SEC : DirectInput の左ボタンを押したままカーソルを動かし SEC 秒保持（歩かせる）
import ctypes,time,sys,os
sys.path.insert(0,os.path.dirname(__file__))
import run
x0,y0,x1,y1=map(int,sys.argv[1:5]); sec=float(sys.argv[5])
h=run.game_hwnd(); P=ctypes.windll.user32.PostMessageW
m=run.shm()
run.push(m,(10,x0,y0),(6,100),(4,0),(6,100)); run.settle(m); P(h,0x201,1,(y0<<16)|x0)
for i in range(1,11):
    x=x0+(x1-x0)*i//10; y=y0+(y1-y0)*i//10
    run.push(m,(10,x,y),(6,30)); run.settle(m); P(h,0x200,1,(y<<16)|x)
time.sleep(sec)
run.push(m,(5,0),(6,100)); run.settle(m); P(h,0x202,0,(y1<<16)|x1)
