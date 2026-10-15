"""Real decorative NBT, container animation, transfer and persistence checks."""
from ipc import IPC,ROOT
import base64,json,time,sys
c=IPC();scene=990
persisted='--persisted' in sys.argv

def pose():c.send('pose',scene,0,64,0,0,0,0,0,0,0)
def settle(n=15):
 end=int(c.state['tick'])+n
 while int(c.state['tick'])<end:pose();time.sleep(.02)
def decorate(at,s):c.send('decorate',*at,base64.b64encode(s.encode()).decode())
try:
 pose();c.send('creative',1);settle(30)
 if not persisted:
  for x,name in [(1,'chest[facing=south]'),(3,'blue_shulker_box'),(-1,'oak_sign[rotation=0]'),(-3,'red_banner[rotation=0]')]:
   c.send('block',x,63,1,'stone');c.send('block',x,64,1,name)
  decorate((-1,64,1),'{Text1:\'{"text":"STEVE","bold":true,"color":"gold"}\',Text2:\'{"text":"SM64 DS"}\',Text3:\'{"text":"NBT works"}\',Text4:\'{"text":"Hello \u4e16\u754c","italic":true}\',Color:"blue",GlowingText:1b}')
  decorate((-3,64,1),'{Patterns:[{Pattern:"cr",Color:0},{Pattern:"bo",Color:11},{Pattern:"mc",Color:4}]}')
 settle()
 sign=c.visuals[(-1,64,1)];banner=c.visuals[(-3,64,1)]
 sign_text=base64.b64decode(sign[4]).decode()
 assert sign[0]=='sign' and sign_text.startswith('STEVE\nSM64 DS\nNBT works'),sign
 assert '\u4e16\u754c' in sign_text,sign_text
 assert sign[3]=='1' and int(sign[2])>0,sign
 assert 'ffaa00:1' in sign[6],sign
 assert 'cross:' in banner[5] and 'border:' in banner[5] and 'circle:' in banner[5],banner
 checks={}
 for x,kind in [(1,'chest'),(3,'shulker')]:
  c.send('give','stone',7);c.send('use',x,64,1,'up');settle(18)
  opened=c.visuals[(x,64,1)];assert float(opened[1])>.9,opened
  assert len(c.container)==63,len(c.container)
  assert c.container[54][1]==7,c.container
  c.send('click',54,0);assert c.container[54][1]==0
  c.send('click',0,0);assert c.container[0][1]>0
  c.send('inventory',0);settle(18)
  closed=c.visuals[(x,64,1)];assert float(closed[1])<.01,closed
  checks[kind]={'open':float(opened[1]),'closed':float(closed[1]),'slots':63,'real_item_transfer':True}
 c.send('save')
 report={'persisted':persisted,'styled_text':True,'unicode':True,'sign_text':sign_text,'sign_rgb':int(sign[2]),'sign_glowing':True,'banner_layers':banner[5],**checks}
 path='build/steve/block-entity-persistence-test.json' if persisted else 'build/steve/block-entity-test.json'
 (ROOT/path).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
finally:c.close()
