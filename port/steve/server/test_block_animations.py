"""Other live block-entity states: double/ender chest, bell, dragon, conduit."""
from ipc import IPC,ROOT
import time,json
c=IPC()
def pose():c.send('pose',990,0,64,0,0,0,0,0,0,0)
def settle(n):
 end=int(c.state['tick'])+n
 while int(c.state['tick'])<end:pose();time.sleep(.02)
try:
 pose();c.send('creative',1);c.send('inventory',0)
 for x,z,name in [(-1,3,'ender_chest'),(-4,3,'chest[facing=north,type=right]'),(-5,3,'chest[facing=north,type=left]'),(1,3,'bell'),(3,3,'dragon_head')]:
  c.send('block',x,63,z,'stone');c.send('block',x,64,z,name)
 settle(15)
 checks={}
 for at,count,kind in [((-1,64,3),63,'ender_chest'),((-4,64,3),90,'double_chest')]:
  c.send('use',*at,'up');settle(15);assert len(c.container)==count,(kind,len(c.container))
  assert float(c.visuals[at][1])>.9,c.visuals[at]
  if kind=='double_chest':assert float(c.visuals[(-5,64,3)][1])>.9,c.visuals
  c.send('inventory',0);settle(15);assert float(c.visuals[at][1])<.01
  checks[kind]={'open_close':True,'slots':count}
 c.send('use',1,64,3,'north');settle(3);assert float(c.visuals[(1,64,3)][1])>0,c.visuals
 settle(65);assert float(c.visuals[(1,64,3)][1])==0;checks['bell_ring_settles']=True
 c.send('block',3,63,3,'redstone_block');settle(4);assert float(c.visuals[(3,64,3)][1])==1
 c.send('block',3,63,3,'stone');settle(4);assert float(c.visuals[(3,64,3)][1])==0;checks['dragon_redstone_state']=True
 origin=(0,67,8)
 for x in range(-1,2):
  for y in range(-1,2):
   for z in range(-1,2):c.send('block',x+origin[0],y+origin[1],z+origin[2],'water')
 frame=[]
 for x in range(-2,3):
  for y in range(-2,3):
   for z in range(-2,3):
    if (x==0 and (abs(y)==2 or abs(z)==2)) or (y==0 and (abs(x)==2 or abs(z)==2)) or (z==0 and (abs(x)==2 or abs(y)==2)):
     frame.append((x,y,z));c.send('block',x+origin[0],y+origin[1],z+origin[2],'prismarine')
 assert len(frame)==42,len(frame)
 c.send('block',*origin,'conduit');settle(85)
 conduit=c.visuals[origin];assert conduit[0]=='conduit' and float(conduit[1])==1 and conduit[3]=='1',conduit
 checks['conduit']={'active':True,'hunting':True,'frame':len(frame)}
 c.send('block',*origin,'air');settle(4);assert origin not in c.visuals;checks['removed_entity_disappears']=True
 (ROOT/'build/steve/block-animation-test.json').write_text(json.dumps(checks,indent=2));print(json.dumps(checks,indent=2));c.send('save')
finally:c.close()
