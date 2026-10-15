from ipc import IPC,ROOT
import time,json
c=IPC(); ids={}
for line in (ROOT/'runtime/minecraft/registry.tsv').read_text().splitlines():
 p=line.split('\t');ids[p[0],p[2]]=int(p[1])
def scene(token):
 c.blocks.clear();c.send('pose',token,.5,65,.5,0,0,0,0,0,1)
 assert int(c.state['level'])==token
 assert abs(float(c.state['x'])-.5)<.01
 c.send('sync')
def expect(at,name):
 for _ in range(80):
  if c.blocks.get(at)==ids['B','minecraft:'+name]:return
  c.receive()
 raise AssertionError((at,name,c.blocks.get(at)))
try:
 scene(931);c.send('terrain',0,64,0,'stone');c.send('block',0,65,0,'redstone_block');expect((0,65,0),'redstone_block')
 scene(932);assert (0,65,0) not in c.blocks,'Other level leaked its block'
 c.send('block',0,65,0,'gold_block');expect((0,65,0),'gold_block')
 scene(931);expect((0,65,0),'redstone_block')
 c.send('block',0,64,0,'air');expect((0,64,0),'air')
 scene(932);expect((0,65,0),'gold_block')
 scene(931);expect((0,64,0),'air');c.send('save')
 report=['Level/area coordinates isolated in separate vanilla arenas','Returning to a level replays its blocks','Mined terrain air cells survive level switches']
 (ROOT/'build/steve/isolation-test.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
finally:c.close()
