from ipc import IPC,ROOT
import time,json
c=IPC()
try:
 c.send('pose',21,.5,66,.5,0,0,0,0,0,0);c.send('creative',1)
 for x in range(-2,3):
  for z in range(-2,9):
   c.send('block',x,63,z,'stone',wait=False)
   for y in range(64,69):c.send('block',x,y,z,'stone' if x in (-2,2) or z in (-2,8) else 'water',wait=False)
  c.send('pose',21,.5,66,.5,0,0,1,0,0,0)
 c.send('creative',0);c.send('heal',20);c.send('pose',21,.5,66,.5,0,0,1,0,0,0);c.send('swim',1,0,0,1)
 start={k:float(c.state[k]) for k in ('x','y','z')};air=float(c.state['air'])
 end=time.monotonic()+2
 while time.monotonic()<end:c.send('pose',21,.5,66,.5,0,0,1,0,0,0);time.sleep(.05)
 distance=((float(c.state['x'])-start['x'])**2+(float(c.state['z'])-start['z'])**2)**.5
 assert distance>1,('Swimming did not advance',c.state)
 assert int(c.state['flags'])&16,c.state
 assert float(c.state['air'])<air-20,c.state
 c.send('swim',1,0,1,0);bottom=float(c.state['y']);end=time.monotonic()+1.5
 while time.monotonic()<end:c.send('pose',21,.5,66,.5,0,0,1,0,0,0);time.sleep(.05)
 top=float(c.state['y']);assert top>bottom+.5,('Swim ascent failed',bottom,top)
 c.send('swim',1,0,-1,0);end=time.monotonic()+1
 while time.monotonic()<end:c.send('pose',21,.5,66,.5,0,0,1,0,0,0);time.sleep(.05)
 assert float(c.state['y'])<top-.5,('Swim descent failed',top,c.state)
 report={'ascent_blocks':top-bottom,'descent_blocks':top-float(c.state['y']),'distance_blocks':distance,'start':start,'end':c.state}
 (ROOT/'build/steve/swimming-test.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
 c.send('swim',0,0,0,0);c.send('pose',21,.5,70,.5,0,0,0,0,0,1);c.send('save')
finally:c.close()
