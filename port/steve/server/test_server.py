"""Exercise the official 1.18 server, not a mock Minecraft implementation."""
from ipc import IPC, ROOT
import json
import time

ids={}
for line in (ROOT/'runtime/minecraft/registry.tsv').read_text().splitlines():
    fields=line.split('\t')
    ids[(fields[0],fields[2])]=int(fields[1])

client=IPC()
results=[]
def check(condition,message):
    if not condition: raise AssertionError(message)
    results.append(message)

def pose():
    return client.send('pose',100,0.5,65,0.5,0,0,0,0,0,1)

def wait(predicate,timeout=8):
    deadline=time.monotonic()+timeout
    last_pose=0
    while time.monotonic()<deadline:
        if predicate(): return True
        if time.monotonic()-last_pose>=0.15:
            pose()
            last_pose=time.monotonic()
        else: client.receive()
    return predicate()

try:
    pose();client.send('creative',0);client.send('heal',20)
    first=int(client.state['tick'])
    client.receive()
    check(int(client.state['tick'])>first,'live 20-Hz backend snapshots advance')
    for x in range(-2,3):
        for z in range(-2,4): client.send('block',x,64,z,'stone',wait=False)
    client.send('slot',0)
    client.send('give','diamond_pickaxe',1)
    client.send('block',0,65,2,'stone')
    check(wait(lambda:client.blocks.get((0,65,2))==ids[('B','minecraft:stone')]),'placed block state reaches frontend')
    before=client.inventory[0][2]
    client.send('mine',0,65,2)
    check(wait(lambda:client.blocks.get((0,65,2))==0),'vanilla mining completes')
    check(client.inventory[0][2]>before,'vanilla mining consumes tool durability')
    client.send('cancel')
    client.send('block',5,64,5,'stone')
    client.send('block',5,65,5,'tnt')
    client.send('block',6,65,5,'redstone_block')
    tnt_id=ids[('E','minecraft:tnt')]
    check(wait(lambda:any(int(e[0])==tnt_id and int(e[7])>=0 for e in client.entities.values())),'vanilla redstone primes moving TNT')
    check(wait(lambda:not any(int(e[0])==tnt_id for e in client.entities.values()),10),'vanilla TNT fuse and explosion complete')
    for name in ('zombie','skeleton','creeper','spider','pig','cow','sheep','chicken'):
        client.send('spawn',name,8,65,8)
        ident=ids[('E','minecraft:'+name)]
        check(wait(lambda ident=ident:any(int(e[0])==ident for e in client.entities.values())),name+' is a real vanilla entity')
    client.send('spawn','boat',2,65,0)
    boat_id=ids[('E','minecraft:boat')]
    check(wait(lambda:any(int(e[0])==boat_id for e in client.entities.values())),'vanilla boat entity is synchronized')
    boat=next(k for k,e in client.entities.items() if int(e[0])==boat_id)
    client.send('ride',boat)
    check(wait(lambda:int(client.state['flags'])&4),'boat passenger state is synchronized')
    client.send('dismount')
    client.send('inventory',1)
    check(wait(lambda:bool(client.container)),'native inventory exposes vanilla container slots')
    client.send('inventory',0)
    client.send('save')
    check((ROOT/'runtime/minecraft/steve-player.dat').exists(),'Minecraft inventory persists as vanilla NBT')
    report={'passed':results,'minecraft':'1.18','tick':int(client.state['tick'])}
    (ROOT/'build/steve/server-test.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))
finally:
    client.close()
