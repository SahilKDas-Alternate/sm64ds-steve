"""A queued terrain import may not erase placements or refill carved cells."""
from ipc import IPC,ROOT
import json,time
c=IPC();scene=1000
try:
 c.send('pose',scene,0,70,0,0,0,0,0,0,0);c.send('creative',1)
 c.send('block',1,64,1,'chest');chest=c.blocks[(1,64,1)]
 c.send('terrain',1,64,1,'stone');assert c.blocks[(1,64,1)]==chest and (1,64,1) not in c.terrain
 c.send('terrain',2,64,1,'stone');assert c.blocks[(2,64,1)]==1 and c.terrain[(2,64,1)]==1
 c.send('block',2,64,1,'air');c.send('terrain',2,64,1,'stone');assert c.blocks[(2,64,1)]==0 and c.terrain[(2,64,1)]==1
 c.send('block',2,64,1,'blue_shulker_box');shulker=c.blocks[(2,64,1)]
 c.send('terrain',2,64,1,'dirt');assert c.blocks[(2,64,1)]==shulker and c.terrain[(2,64,1)]==1
 ids={f[2]:int(f[1]) for line in (ROOT/'runtime/minecraft/registry.tsv').read_text().splitlines() if (f:=line.split('\t'))[0]=='B' and not f[3]}
 c.send('block',3,64,1,'prismarine',wait=False);c.send('terrain',3,64,1,'stone',wait=False);c.send('pose',scene,0,70,0,0,0,0,0,0,0)
 assert c.blocks[(3,64,1)]==ids['minecraft:prismarine'] and (3,64,1) not in c.terrain
 result={'placement_preserved':True,'original_imported_once':True,'mined_air_preserved':True,'original_id_immutable':True,'same_batch_race_preserved':True}
 (ROOT/'build/steve/terrain-import-race-test.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2));c.send('save')
finally:c.close()
