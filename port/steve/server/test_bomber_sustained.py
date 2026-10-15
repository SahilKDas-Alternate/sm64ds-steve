"""Real-engine sustained bombing, retained TNT, movement, and blast IPC."""
from ipc import IPC,ROOT
import json,time,sys
c=IPC();scene=int(sys.argv[1]) if len(sys.argv)>1 else 976
registry=[l.split('\t') for l in (ROOT/'runtime/minecraft/registry.tsv').read_text().splitlines()]
names={int(f[1]):f[2].removeprefix('minecraft:') for f in registry if f[0]=='B'}
def pose():c.send('pose',scene,15,120,30,0,0,0,0,0,0)
try:
 pose();c.send('creative',1)
 for x in range(19,50):
  for z in range(4,14):c.send('terrain',x,43,z,'grass_block',wait=False)
 pose()
 costs=[('slime_block',8),('sticky_piston',2),('observer',2),('tnt',1),('dead_brain_coral_fan',1),('cobblestone_wall',1),('detector_rail',1),('minecart',1)]
 for slot,(name,count) in enumerate(costs):c.send('slot',slot);c.send('give',name,count)
 c.send('creative',0);c.send('machine',20,120,8,1)
 assert all(c.inventory[i][1]==0 for i in range(8)),c.inventory
 seen=set();drops=[];animation=False;retained=False;travel=False;end=int(c.state['tick'])+400
 while int(c.state['tick'])<end:
  pose();time.sleep(.025);animation|=bool(c.moving)
  for ident,e in c.entities.items():
   if int(e[7])>=0 and ident not in seen:seen.add(ident);drops.append([int(c.state['tick']),float(e[1]),float(e[2]),float(e[3])])
  retained |= any(names.get(s)=='tnt' and p[0]>25 for p,s in c.blocks.items()) or any(names.get(int(p[3]))=='tnt' and int(p[0])>25 for p in c.moving)
  travel |= any(names.get(s)=='slime_block' and p[0]>35 for p,s in c.blocks.items())
 destroyed=[p for p,s in c.blocks.items() if s==0 and p[1]==43 and 19<=p[0]<50 and 4<=p[2]<14]
 report=dict(drops=drops,unique_tnt=len(seen),retained_tnt=retained,engine_traveled=travel,moving_piston_snapshots=animation,destroyed_floor_cells=len(destroyed),materials_consumed=True)
 (ROOT/'build/steve/sustained-bomber-test.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
 assert len(seen)>=8,'Bomber failed to sustain repeated TNT drops'
 assert retained and travel and animation,'Retained payload or engine synchronization missing'
 assert destroyed,'Automatic explosion terrain updates missing'
 c.send('save')
finally:c.close()
