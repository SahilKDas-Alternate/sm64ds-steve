"""Minecart passengers must not receive Boat-only reflective input calls."""
from ipc import IPC,ROOT
import time,json,sys
c=IPC();scene=int(sys.argv[1]) if len(sys.argv)>1 else 1004
def pose():c.send('pose',scene,0,66,0,0,0,0,0,0,1)
def wait(ticks):
 end=int(c.state['tick'])+ticks
 while int(c.state['tick'])<end:pose();time.sleep(.025)
try:
 pose();c.send('creative',1);wait(60)
 c.send('block',2,64,2,'stone');c.send('block',2,65,2,'rail[shape=east_west]')
 c.send('slot',0);c.send('give','minecart',1);c.send('use',2,65,2,'up');wait(5)
 cart_id=None
 for line in (ROOT/'runtime/minecraft/registry.tsv').read_text().splitlines():
  p=line.split('\t')
  if p[0]=='E' and p[2]=='minecraft:minecart':cart_id=int(p[1]);break
 cart=next(i for i,e in c.entities.items() if int(e[0])==cart_id)
 c.send('ride',cart);assert int(c.state['flags'])&4
 c.send('boat',0,0,1,0);wait(60)
 assert cart in c.entities and int(c.state['flags'])&4
 c.send('boat',0,0,0,0);c.send('dismount');assert not int(c.state['flags'])&4
 report={'mounted':True,'boat_input_ignored_for_minecart':True,'persistent_after_60_ticks':True,'dismounted':True}
 (ROOT/'build/steve/minecart-input-test.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));c.send('save')
finally:c.close()
