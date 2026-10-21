"""Initialize only the QA player's controls/health before a native game test."""
import sys
from ipc import IPC

c=IPC()
try:
    c.send('swim',0,0,0,0)
    c.send('dismount')
    c.send('wing',0)
    if float(c.state['health'])<=0:
        c.send('respawn',0,80,0)
    c.send('creative',0)
    c.send('heal',20)
    c.send('air')
    c.send('pose',int(sys.argv[1]),0,80,0,0,0,0,1,0,0)
    assert float(c.state['health'])==20,c.state
finally:
    c.close()
