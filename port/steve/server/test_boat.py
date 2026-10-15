from ipc import IPC, ROOT
import time
c=IPC()
try:
 c.send('pose',101,.5,65,.5,0,0,0,0,0,1);c.send('creative',1)
 for x in range(-3,4):
  for z in range(-3,7): c.send('block',x,64,z,'water',wait=False)
 c.send('spawn','boat',.5,65,.5)
 types={}
 for line in (ROOT/'runtime/minecraft/registry.tsv').read_text().splitlines():
  p=line.split('\t');types[p[0],p[2]]=int(p[1])
 for _ in range(30):
  c.receive()
  boats=[(i,e) for i,e in c.entities.items() if int(e[0])==types['E','minecraft:boat'] and abs(float(e[1])-.5)<2 and abs(float(e[3])-.5)<2]
  if boats: break
 assert boats,'Boat not spawned'
 ident,e=boats[-1];start=(float(e[1]),float(e[3]));c.send('ride',ident)
 end=time.monotonic()+3
 while time.monotonic()<end:
  c.send('pose',101,.5,65,.5,0,0,0,0,0,0)
  c.send('boat',0,0,1,0)
  time.sleep(.05)
 e=c.entities[ident];distance=((float(e[1])-start[0])**2+(float(e[3])-start[1])**2)**.5
 assert distance>1,('Boat did not travel',distance,e)
 print('Vanilla boat traveled',distance,'blocks')
 c.send('dismount');c.send('save')
finally: c.close()
