"""Read-only checks for the fresh user runtime after QA fixtures are archived."""
from ipc import IPC,ROOT
import json
c=IPC()
try:
 ids={f[2].removeprefix('minecraft:'):int(f[1]) for line in (ROOT/'runtime/minecraft/registry.tsv').read_text().splitlines() if (f:=line.split('\t'))[0]=='I'}
 expected=[('diamond_pickaxe',1),('diamond_sword',1),('stone',64),('tnt',64),('redstone',64),('sticky_piston',64),('observer',64),('slime_block',64),('oak_boat',64)]
 assert all(c.inventory[i][:2]==(ids[name],count) for i,(name,count) in enumerate(expected)),c.inventory
 extras=['dead_brain_coral_fan','cobblestone_wall','detector_rail','minecart']
 assert all(c.inventory[17+i][:2]==(ids[name],1 if i==3 else 64) for i,name in enumerate(extras)),c.inventory
 assert float(c.state['health'])==20 and float(c.state['air'])==300 and not int(c.state['flags'])&1,c.state
 assert not c.blocks and not c.entities,(c.blocks,c.entities)
 report={'clean_start':True,'health':20,'air':300,'survival':True,'starter_inventory':True,'bomber_materials':True,'cached_blocks':0,'entities':0}
 (ROOT/'build/steve/clean-start-test.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
finally:c.close()
