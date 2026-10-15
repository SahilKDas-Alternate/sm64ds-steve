from ipc import IPC,ROOT
import time,json
c=IPC();ids={}
for l in (ROOT/'runtime/minecraft/registry.tsv').read_text().splitlines():
 f=l.split('\t');ids[f[0],f[2]]=int(f[1])
try:
 c.send('pose',930,.5,100,.5,0,-10,0,0,0,0);c.send('creative',0);c.send('heal',20)
 c.send('slot',0);c.send('give','diamond_chestplate',1);c.send('inventory',1);c.send('click',36,0);c.send('click',6,0);c.send('inventory',0)
 c.send('pose',930,.5,100,.5,0,-10,0,0,0,0);c.send('wing');assert int(c.state['flags'])&2,c.state
 c.send('wing',0);c.send('inventory',1)
 assert c.container[6][:2]==(ids['I','minecraft:diamond_chestplate'],1),c.container
 c.send('inventory',0);c.send('slot',1);c.send('give','arrow',64);c.send('slot',0);c.send('give','bow',1);c.send('item')
 end=time.monotonic()+1.2
 while time.monotonic()<end:c.send('pose',930,.5,100,.5,0,0,0,0,0,0);time.sleep(.05)
 c.send('release');end=time.monotonic()+1
 while time.monotonic()<end:
  c.receive()
  if any(int(e[0])==ids['E','minecraft:arrow'] for e in c.entities.values()):break
 assert any(int(e[0])==ids['E','minecraft:arrow'] for e in c.entities.values()),'Bow release produced no vanilla arrow'
 assert c.inventory[1][1]==63,c.inventory
 assert c.inventory[0][2]>0,c.inventory
 report={'chest_restored_after_flight':True,'vanilla_bow_release':True,'ammo_consumed':True,'bow_durability':c.inventory[0][2]}
 (ROOT/'build/steve/equipment-test.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));c.send('save')
finally:c.close()
