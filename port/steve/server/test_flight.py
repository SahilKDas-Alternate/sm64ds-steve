from ipc import IPC,ROOT
import time,json
c=IPC()
try:
 c.send('pose',102,0.5,100,0.5,0,-10,0,0,0,0)
 c.send('wing')
 start={k:float(c.state[k]) for k in ('x','y','z')}
 assert int(c.state['flags'])&2,('Elytra did not start',c.state)
 end=time.monotonic()+2
 while time.monotonic()<end:
  c.send('pose',102,0.5,100,0.5,0,-10,0,0,0,0)
  time.sleep(.05)
 distance=((float(c.state['x'])-start['x'])**2+(float(c.state['z'])-start['z'])**2)**.5
 assert distance>1,('No flight motion',c.state)
 assert int(c.state['flags'])&2,('Flight ended prematurely',c.state)
 report={'distance_blocks':distance,'start':start,'end':c.state}
 (ROOT/'build/steve/flight-test.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
finally:c.close()
