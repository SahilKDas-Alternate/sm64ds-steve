"""Bake the requested 1.18 models/textures against the real server's state IDs.

Downloaded originals and generated pixels stay in ignored runtime/, not source.
No Minecraft executable code is copied by this exporter.
"""
from pathlib import Path
import argparse
import itertools
import json
import math
import struct
from functools import lru_cache
from PIL import Image
from special_models import special

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / 'reference/minecraft-assets/assets/minecraft'
OUT = ROOT / 'runtime/steve-assets'
DIRS = {'east': 0, 'west': 1, 'up': 2, 'down': 3, 'south': 4, 'north': 5}

def resource(name):
    return name.removeprefix('minecraft:')

@lru_cache(None)
def model(name):
    path = ASSETS / 'models' / (resource(name) + '.json')
    if not path.exists():
        return {}
    data = json.loads(path.read_text())
    parent = model(data['parent']) if 'parent' in data else {}
    out = {**parent, **data}
    out['textures'] = {**parent.get('textures', {}), **data.get('textures', {})}
    return out

def condition(when, properties):
    if 'OR' in when:
        return any(condition(branch, properties) for branch in when['OR'])
    if 'AND' in when:
        return all(condition(branch, properties) for branch in when['AND'])
    return all(properties.get(key) in value.split('|') for key, value in when.items())

def applications(name, properties):
    path = ASSETS / 'blockstates' / (name + '.json')
    if not path.exists():
        return []
    state = json.loads(path.read_text())
    out = []
    for key, variants in state.get('variants', {}).items():
        if not key or condition(dict(part.split('=', 1) for part in key.split(',')), properties):
            out.append(variants[0] if isinstance(variants, list) else variants)
            break
    for part in state.get('multipart', []):
        if 'when' not in part or condition(part['when'], properties):
            apply = part['apply']
            out.append(apply[0] if isinstance(apply, list) else apply)
    return out

def rotate(v, axis, angle, origin=(0.5, 0.5, 0.5), rescale=False):
    if not angle:
        return v
    xyz = [v[i] - origin[i] for i in range(3)]
    a = 'xyz'.index(axis)
    b, c = (a + 1) % 3, (a + 2) % 3
    s, co = math.sin(math.radians(angle)), math.cos(math.radians(angle))
    xyz[b], xyz[c] = xyz[b] * co - xyz[c] * s, xyz[b] * s + xyz[c] * co
    if rescale:
        xyz[b] /= co
        xyz[c] /= co
    return [xyz[i] + origin[i] for i in range(3)]

def corners(lo, hi, face):
    x,y,z = lo
    X,Y,Z = hi
    return {
        'east': [(X,Y,z),(X,Y,Z),(X,y,Z),(X,y,z)],
        'west': [(x,Y,Z),(x,Y,z),(x,y,z),(x,y,Z)],
        'up': [(x,Y,z),(X,Y,z),(X,Y,Z),(x,Y,Z)],
        'down': [(x,y,Z),(X,y,Z),(X,y,z),(x,y,z)],
        'south': [(X,Y,Z),(x,Y,Z),(x,y,Z),(X,y,Z)],
        'north': [(x,Y,z),(X,Y,z),(X,y,z),(x,y,z)],
    }[face]

def default_uv(lo, hi, face):
    x,y,z = lo
    X,Y,Z = hi
    return {
        'down': [x,16-Z,X,16-z], 'up': [x,z,X,Z],
        'north': [16-X,16-Y,16-x,16-y], 'south': [x,16-Y,X,16-y],
        'west': [z,16-Y,Z,16-y], 'east': [16-Z,16-Y,16-z,16-y],
    }[face]

def geometry(name, props):
    if name in ('air', 'cave_air', 'void_air', 'structure_void', 'barrier', 'light', 'moving_piston'):
        return []
    specialized = special(name, props, ASSETS, corners, rotate)
    if specialized:
        return specialized
    result = []
    for application in applications(name, props):
        data = model(application['model'])
        textures = data.get('textures', {})
        for element in data.get('elements', []):
            lo = [v / 16 for v in element['from']]
            hi = [v / 16 for v in element['to']]
            for face, details in element.get('faces', {}).items():
                texture = details['texture']
                seen = set()
                while texture.startswith('#') and texture not in seen:
                    seen.add(texture)
                    texture = textures.get(texture[1:], 'block/stone')
                texture = resource(texture)
                verts = corners(lo, hi, face)
                if 'rotation' in element:
                    rotation = element['rotation']
                    origin = tuple(v/16 for v in rotation['origin'])
                    verts = [rotate(v, rotation['axis'], rotation['angle'], origin,
                                    rotation.get('rescale', False)) for v in verts]
                verts = [rotate(rotate(v, 'x', -application.get('x', 0)),
                                'y', -application.get('y', 0)) for v in verts]
                uv = details.get('uv', default_uv(element['from'], element['to'], face))
                u0,v0,u1,v1 = uv
                uvs = [(u0,v0),(u1,v0),(u1,v1),(u0,v1)]
                shift = details.get('rotation', 0)//90
                uvs = uvs[shift:]+uvs[:shift]
                # A transformed face's cull direction follows its normal.
                cull = -1
                if details.get('cullface') in DIRS:
                    normal = {'east':[1,0,0], 'west':[-1,0,0], 'up':[0,1,0],
                              'down':[0,-1,0], 'south':[0,0,1], 'north':[0,0,-1]}[details['cullface']]
                    normal = rotate(rotate(normal, 'x', -application.get('x',0), (0,0,0)),
                                    'y', -application.get('y',0), (0,0,0))
                    axis = max(range(3), key=lambda i:abs(normal[i]))
                    cull = axis*2 + (normal[axis]<0)
                tint = details.get('tintindex', -1)
                result.append((texture,cull,tint,[(*v,*uvs[i]) for i,v in enumerate(verts)]))
    if not result and name.endswith('_wall'):
        return [] # A multipart wall with no post or arms has no visible mesh.
    if not result:
        texture = 'block/'+name
        if name=='bubble_column': texture='block/water_still'
        if name in ('end_portal','end_gateway'): texture='entity/end_portal'
        if name in ('water','lava'):
            texture += '_still'
        if not (ASSETS/'textures'/f'{texture}.png').exists():
            data = model('block/'+name)
            texture = resource(data.get('textures', {}).get('particle','block/stone'))
            if texture.startswith('#'):
                texture = 'block/stone'
        for face in DIRS:
            result.append((texture,DIRS[face],-1,
                           [(*v,*uv) for v,uv in zip(corners((0,0,0),(1,1,1),face),
                                                   [(0,0),(16,0),(16,16),(0,16)])]))
    return result

def string(file, value):
    data = value.encode('utf-8')
    file.write(struct.pack('<H',len(data)))
    file.write(data)

def export_textures():
    textures = sorted(p for p in (ASSETS/'textures').rglob('*.png')
                      if p.relative_to(ASSETS/'textures').parts[0] in ('block','item','entity','gui','font','colormap')
                      and (p.relative_to(ASSETS/'textures').parts[0]!='font' or (p.name=='ascii.png' or p.name.startswith('unicode_page_'))))
    with (OUT/'textures.bin').open('wb') as file:
        file.write(b'STX1')
        file.write(struct.pack('<I',len(textures)))
        for path in textures:
            name = path.relative_to(ASSETS/'textures').with_suffix('').as_posix()
            image = Image.open(path).convert('RGBA')
            width,height = image.size
            string(file,name)
            frame_order = [(0,1)]
            metadata = path.with_suffix('.png.mcmeta')
            if metadata.exists():
                animation = json.loads(metadata.read_text()).get('animation',{})
                duration = animation.get('frametime',1)
                frame_order = [(f if isinstance(f,int) else f['index'],
                                duration if isinstance(f,int) else f.get('time',duration))
                               for f in animation.get('frames',range(height//width))]
            file.write(struct.pack('<HHHH',width,height,width if metadata.exists() else height,len(frame_order)))
            for frame,duration in frame_order:
                file.write(struct.pack('<HH',frame,max(1,duration)))
            file.write(image.tobytes('raw','BGRA'))
    return len(textures)

def export_models(registry):
    entries = [line.rstrip('\n').split('\t') for line in registry.read_text().splitlines()]
    blocks = [e for e in entries if e[0]=='B']
    # Builtin item renderers have entity atlases instead of layer0 textures.
    # Private IDs exist only in the renderer's map, never in the IPC registry.
    blocks += [['B',str(0xffff0000+i),name,''] for i,name in enumerate(('shield','trident'))]
    fallback = set()
    with (OUT/'models.bin').open('wb') as file:
        file.write(b'SMD1')
        file.write(struct.pack('<I',len(blocks)))
        for entry in blocks:
            _,ident,name,props=entry[:4]
            name = resource(name)
            properties = dict(p.split('=',1) for p in props.split(',') if p)
            faces = geometry(name,properties)
            if faces and name not in ('water','lava','bubble_column','end_portal','end_gateway') and not special(name,properties,ASSETS,corners,rotate) and not any(model(a['model']).get('elements') for a in applications(name,properties)):
                fallback.add(name)
            file.write(struct.pack('<I',int(ident)))
            string(file,name)
            string(file,props)
            file.write(struct.pack('<I',len(faces)))
            for texture,cull,tint,vertices in faces:
                string(file,texture)
                file.write(struct.pack('<ii',cull,tint))
                for vertex in vertices:
                    file.write(struct.pack('<5f',*vertex))
    # Names/IDs also power the native catalog, entity rendering and item icons.
    (OUT/'registry.tsv').write_text(registry.read_text(),encoding='utf-8')
    shape_file=registry.parent/'shapes.bin'
    if shape_file.exists(): (OUT/'shapes.bin').write_bytes(shape_file.read_bytes())
    (OUT/'model-fallbacks.json').write_text(json.dumps(sorted(fallback),indent=2))
    return len(blocks),len(fallback)

def export_items(registry):
    rows=[]
    for line in registry.read_text().splitlines():
        fields=line.split('\t')
        if fields[0]!='I': continue
        name=resource(fields[2]);textures=model('item/'+name).get('textures',{})
        layers=[]
        for index in range(16):
            texture=textures.get('layer'+str(index))
            if texture is None: break
            seen=set()
            while texture.startswith('#') and texture not in seen:
                seen.add(texture);texture=textures.get(texture[1:],'')
            texture=resource(texture)
            if texture and (ASSETS/'textures'/f'{texture}.png').exists(): layers.append(texture)
        rows.append('\t'.join([fields[1],*layers]))
    (OUT/'items.tsv').write_text('\n'.join(rows)+'\n',encoding='utf-8')
    return len(rows)

def main():
    global OUT
    parser=argparse.ArgumentParser()
    parser.add_argument('--textures-only',action='store_true')
    args=parser.parse_args()
    destination=OUT
    OUT=destination/'.bake'
    OUT.mkdir(parents=True,exist_ok=True)
    # A failed earlier bake must not leak an unfinished model pack into a
    # later --textures-only publish.
    for stale in OUT.iterdir():
        if stale.is_file(): stale.unlink()
    textures=export_textures()
    print(f'1.18 textures: {textures}')
    if not args.textures_only:
        count,fallback=export_models(ROOT/'runtime/minecraft/registry.tsv')
        print(f'1.18 meshes including item previews: {count}; unresolved model fallbacks: {fallback}')
        print(f"1.18 item models: {export_items(ROOT/'runtime/minecraft/registry.tsv')}")
    # Readers must never observe a half-written pack during a live rebake.
    for path in OUT.iterdir():
        if path.is_file(): path.replace(destination/path.name)
    OUT.rmdir()
    # Fail if all outputs, including source history and local runtime, breach cap.
    import subprocess
    subprocess.run(['python',str(ROOT/'port/steve/check_budget.py')],check=True)

if __name__=='__main__':
    main()
