"""Version-1 localhost protocol client used by real-server verification."""
from pathlib import Path
import socket
import struct
import time

ROOT=Path(__file__).resolve().parents[3]
class IPC:
    def __init__(self, timeout=10):
        self.socket=socket.create_connection(('127.0.0.1',25576),timeout=timeout)
        token=(ROOT/'runtime/minecraft/ipc-token.txt').read_text().strip()
        self.write('SM64DS1\t'+token)
        self.sequence=0
        self.state={}
        self.blocks={}
        self.entities={}
        self.inventory={}
        self.container={}
        self.terrain={}
        self.moving=[]
        self.visuals={}
        self.receive()
        self.sequence=int(self.state['ack'])
    def write(self,message):
        data=message.encode()
        self.socket.sendall(struct.pack('<I',len(data))+data)
    def exact(self,count):
        chunks=[]
        while count:
            part=self.socket.recv(count)
            if not part: raise EOFError('backend disconnected')
            count-=len(part)
            chunks.append(part)
        return b''.join(chunks)
    def receive(self):
        length=struct.unpack('<I',self.exact(4))[0]
        if not 0<length<=1024*1024: raise ValueError('invalid IPC packet size')
        text=self.exact(length).decode()
        self.entities={}
        self.container={}
        self.moving=[]
        self.visuals={}
        for line in text.splitlines():
            p=line.split('\t')
            if p[0]=='S':
                if self.state.get('level')!=p[4]: self.blocks.clear();self.terrain.clear()
                self.state=dict(zip(('version','tick','ack','level','x','y','z','yaw','pitch','health','air','food','flags','mining','slot'),p[1:]))
            elif p[0]=='B':
                self.blocks[tuple(map(int,p[1:4]))]=int(p[4])
            elif p[0]=='E': self.entities[int(p[1])]=p[2:]
            elif p[0]=='I': self.inventory[int(p[1])]=tuple(map(int,p[2:]))
            elif p[0]=='C': self.container[int(p[1])]=tuple(map(int,p[2:]))
            elif p[0]=='T': self.terrain[tuple(map(int,p[1:4]))]=int(p[4])
            elif p[0]=='V': self.visuals[tuple(map(int,p[1:4]))]=p[4:]
            elif p[0]=='P': self.moving.append(p[1:])
            elif p[0]=='R':
                import base64
                raise RuntimeError(base64.b64decode(p[1]).decode())
        return self.state
    def send(self,verb,*args,wait=True):
        self.sequence+=1
        self.write('\t'.join(map(str,(1,self.sequence,verb,*args))))
        if wait:
            deadline=time.monotonic()+10
            while int(self.state.get('ack',0))<self.sequence:
                if time.monotonic()>deadline: raise TimeoutError(f'{verb} acknowledgement')
                self.receive()
        return self.state
    def close(self): self.socket.close()

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser()
    parser.add_argument('--stop',action='store_true')
    args=parser.parse_args()
    client=IPC()
    print('Backend:',client.state)
    if args.stop:
        # Keep the socket alive until the tick thread consumes stop. Closing
        # immediately races the writer, which can close the socket before the
        # reader has received the queued request.
        try: client.send('stop')
        except (EOFError,ConnectionError): pass
    client.close()
