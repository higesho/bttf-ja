# 複数のスクリーンショットを縮小して 1 枚に並べる: montage.py out.png a.png b.png ...
import sys
from PIL import Image
fs=sys.argv[2:]; ims=[Image.open(f).convert('RGB') for f in fs]
w,h=ims[0].size; s=0.5; cw,ch=int(w*s),int(h*s); cols=2; rows=(len(ims)+1)//2
M=Image.new('RGB',(cw*cols,ch*rows))
for i,im in enumerate(ims): M.paste(im.resize((cw,ch)),((i%cols)*cw,(i//cols)*ch))
M.save(sys.argv[1])
