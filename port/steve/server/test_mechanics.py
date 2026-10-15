from ipc import IPC, ROOT
import time,json
c=IPC(); results=[]
def pose(wet=0): c.send('pose',103,.5,65,.5,0,0,wet,0,0,1)
def wait_for(predicate,seconds=5,wet=0):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  pose(wet)
  if predicate(): return
  time.sleep(.1)
 raise AssertionError(c.state)
try:
 pose(); c.send('heal',20)
 c.send('slot',0); c.send('give','oak_log',1)
 c.send('inventory',1)
 c.send('click',36,0); c.send('click',1,0)
 ids={}
 for line in (ROOT/'runtime/minecraft/registry.tsv').read_text().splitlines():
  p=line.split('\t'); ids[p[0],p[2]]=int(p[1])
 wait_for(lambda:c.container.get(0,(0,0))[0]==ids['I','minecraft:oak_planks'])
 assert c.container[0][1]==4
 c.send('click',0,0)
 wait_for(lambda:c.container.get(1,(0,0))[1]==0)
 results.append('Vanilla 2x2 crafting consumes one log and produces four planks')
 c.send('inventory',0); c.send('inventory',1,3)
 wait_for(lambda:len(c.container)==46)
 results.append('Vanilla crafting-table container exposes 3x3 grid and output')
 c.send('inventory',0)
 pose(1); start=int(float(c.state['air']))
 wait_for(lambda:int(float(c.state['air']))<start-25,4,1)
 results.append('Native underwater state consumes Minecraft oxygen at 20 Hz')
 c.send('air'); assert float(c.state['air'])>=299
 results.append('Air refill restores oxygen')
 pose(0); c.send('save')
 (ROOT/'build/steve/mechanics-test.json').write_text(json.dumps(results,indent=2)); print(json.dumps(results,indent=2))
finally: c.close()
