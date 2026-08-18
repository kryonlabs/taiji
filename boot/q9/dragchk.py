#!/usr/bin/env python3
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
i=(340*w+760)*3
r,g,b=px[i],px[i+1],px[i+2]
sys.exit(0 if not (r<40 and 100<g<150 and 100<b<150) else 1)
