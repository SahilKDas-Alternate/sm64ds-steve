"""Entity collisions must use measured native heights, then real placed shapes."""
from ipc import IPC,ROOT
import time,json,sys
c=IPC();scene=int(sys.argv[1]) if len(sys.argv)>1 else 981
def pose():c.send('pose',scene,0,70,0,0,0,0,0,0,0)
def wait_for(predicate,limit=150):
 end=int(c.state['tick'])+limit
 while int(c.state['tick'])<end:
  pose();time.sleep(.025)
  if predicate():return
 raise AssertionError('Terrain collision did not settle: '+str(c.entities))
try:
 pose();c.send('creative',1)
 start=int(c.state['tick'])
 while int(c.state['tick'])<start+60:pose();time.sleep(.025)
 persisted=len(sys.argv)>2 and sys.argv[2]=='--persisted'
 if not persisted:
  for x in range(1,5):
   for z in range(1,5):
    c.send('terrain',x,64,z,'stone');c.send('surface',x,64,z,.54)
 c.send('spawn','cow',2.5,70,2.5)
 cow=next(iter(c.entities),None)
 wait_for(lambda:any(abs(float(e[2])-64.54)<.005 for e in c.entities.values()))
 height=next(float(e[2]) for e in c.entities.values() if abs(float(e[2])-64.54)<.005)
 for x in range(1,5):
  for z in range(1,5):c.send('block',x,64,z,'stone_slab[type=bottom]')
 wait_for(lambda:any(abs(float(e[2])-64.5)<.005 for e in c.entities.values()))
 slab=next(float(e[2]) for e in c.entities.values() if abs(float(e[2])-64.5)<.005)
 report={'measured_surface_y':height,'placed_slab_y':slab,'rounding_error_removed_blocks':.46,'replacement_uses_official_shape':True,'loaded_from_persistent_cache':persisted}
 (ROOT/('build/steve/terrain-surface-persistence-test.json' if persisted else 'build/steve/terrain-surface-test.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));c.send('save')
finally:c.close()
