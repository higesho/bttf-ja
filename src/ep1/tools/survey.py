import sys,glob,re,os,random; sys.path.insert(0,os.path.dirname(__file__)); from landb import Landb
from collections import Counter
allt=[]; files={}
for f in sorted(glob.glob('orig/*/*.landb')):
    L=Landb(open(f,'rb').read()); n=os.path.basename(f)
    files.setdefault(n,[]).append(f)
    for e in L.entries: allt.append((n,e['prefix'].decode('latin1'),e['text'].decode('latin1')))
print({k:len(v) for k,v in files.items() if len(v)>1})
uniq=set(t for _,_,t in allt); print('entries',len(allt),'unique texts',len(uniq), 'chars',sum(len(t) for t in uniq))
tags=Counter(m for t in uniq for m in re.findall(r'\[[^\]]*\]|\{[^}]*\}|<[^>]*>',t))
print(tags.most_common(50))
random.seed(1)
for n,p,t in random.sample(allt,25): print(n[:30],'|',p,'|',t)
