from ipc import IPC
c=IPC()
try:
 c.send('pose',8,-12,2.54,68,180,0,0,0,0,1)
 for x in (-13,-12):
  for z in (67,68):
   c.send('terrain',x,2,z,'grass_block');c.send('terrain',x,1,z,'stone')
finally:c.close()
