from ipc import IPC,ROOT
import time,json
c=IPC()
try:
 c.send('pose',912,20,80,8,0,0,0,0,0,0);c.send('creative',0);c.send('heal',20)
 c.send('slot',0);c.send('give','diamond_pickaxe',1)
 c.send('pose',912,20,80,8,0,0,0,0,20,0)
 assert float(c.state['health'])==0,c.state
 c.send('respawn',20,80,8)
 assert float(c.state['health'])==20,c.state
 assert abs(float(c.state['x'])-20)<.01
 assert c.inventory[0][1]==1,c.inventory
 report={'death_verified':True,'respawn_health':20,'inventory_preserved':True,'scene_position':c.state}
 (ROOT/'build/steve/respawn-test.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
finally:c.close()
