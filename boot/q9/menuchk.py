#!/usr/bin/env python3
# menuchk.py FILE X1 Y1 X2 Y2: a win2k menu (gray face + black text) in region
import sys
f=open(sys.argv[1],'rb').read(); t=[];o=0
while len(t)<4:
    while f[o:o+1].isspace(): o+=1
    if f[o:o+1]==b'#':
        while f[o:o+1]!=b'\n': o+=1
        continue
    s=o
    while not f[o:o+1].isspace(): o+=1
    t.append(f[s:o])
o+=1; w,h=int(t[1]),int(t[2]); px=f[o:o+w*h*3]
x1,y1,x2,y2=map(int,sys.argv[2:6])
gray=0
for y in range(y1,y2,2):
    for x in range(x1,x2,2):
        i=(y*w+x)*3
        r,g,b=px[i],px[i+1],px[i+2]
        if abs(r-192)<14 and abs(g-192)<14 and abs(b-192)<14:
            gray+=1
sys.exit(0 if gray>150 else 1)
