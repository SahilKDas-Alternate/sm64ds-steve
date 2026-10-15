"""Static block-entity meshes using the original 1.18 entity texture atlases.

Coordinates are in Minecraft model pixels. UVs use ModelPart's unfolded box
layout, rather than stretching the complete entity atlas across every face.
"""
from PIL import Image
from functools import lru_cache

@lru_cache(None)
def dimensions(path):
    with Image.open(path) as image:
        return image.size


def special(name, props, assets, corners, rotate):
    result = []
    facing = props.get('facing', 'south')
    angle = {'south': 0, 'west': 90, 'north': 180, 'east': 270}.get(facing, 0)

    def box(lo, hi, texture, uv=(0, 0), yaw=angle, uvsize=None):
        path = assets/'textures'/f'{texture}.png'
        if not path.exists():
            raise ValueError(f'Missing special model texture: {texture}')
        width, height = dimensions(path)
        x, y, z = uvsize or tuple(hi[i]-lo[i] for i in range(3))
        u, v = uv
        rectangles = {
            'west': (u, v+z, u+z, v+z+y),
            'north': (u+z, v+z, u+z+x, v+z+y),
            'east': (u+z+x, v+z, u+2*z+x, v+z+y),
            'south': (u+2*z+x, v+z, u+2*z+2*x, v+z+y),
            'up': (u+z, v, u+z+x, v+z),
            'down': (u+z+x, v, u+z+2*x, v+z),
        }
        for face, (a,b,c,d) in rectangles.items():
            vertices = [rotate(p, 'y', yaw) for p in
                        corners(tuple(n/16 for n in lo), tuple(n/16 for n in hi), face)]
            uvpoints = [(a,b),(c,b),(c,d),(a,d)]
            result.append((texture, -1, -1,
                           [(*p, s*16/width, t*16/height) for p,(s,t) in zip(vertices,uvpoints)]))

    if name=='shield':
        box((2,1,7),(14,23,8),'entity/shield_base',(0,0),0)
        box((7,8,5),(9,14,11),'entity/shield_base',(26,0),0)
    elif name=='trident':
        box((7.5,0,7.5),(8.5,25,8.5),'entity/trident',(0,6),0)
        box((3,24,7),(13,27,9),'entity/trident',(4,0),0)
        for x in (3,7,11): box((x,27,7),(x+2,32,9),'entity/trident',(4,3),0)
    elif name in ('chest', 'trapped_chest', 'ender_chest'):
        kind = {'chest':'normal','trapped_chest':'trapped','ender_chest':'ender'}[name]
        half = props.get('type','single')
        texture = 'entity/chest/'+kind+('' if half=='single' else '_'+half)
        # Double chests meet on the shared edge and retain their half-atlas.
        left, right = (1,15) if half=='single' else ((0,15) if half=='left' else (1,16))
        box((left,0,1),(right,10,15),texture,(0,19))
        box((left,10,1),(right,14,15),texture,(0,0))
        latch = (7,9) if half=='single' else ((0,1) if half=='left' else (15,16))
        box((latch[0],7,15),(latch[1],11,16),texture,(0,0))
    elif name.endswith('shulker_box'):
        color = name.removesuffix('_shulker_box') if name!='shulker_box' else 'shulker'
        texture = 'entity/shulker/'+('shulker' if color=='shulker' else 'shulker_'+color)
        box((0,0,0),(16,8,16),texture,(0,28))
        box((0,4,0),(16,16,16),texture,(0,0))
    elif name.endswith('_bed'):
        texture = 'entity/bed/'+name.removesuffix('_bed')
        head = props.get('part')=='head'
        box((0,0,0),(16,16,6),texture,(0,0) if head else (0,22),0)
        for i,(t,c,tint,vertices) in enumerate(result):
            transformed=[]
            for x,y,z,u,v in vertices:
                p=rotate((x,9/16-z,y),'y',angle)
                transformed.append((*p,u,v))
            result[i]=(t,c,tint,transformed)
        for x in (0,13):
            box((x,0,13 if head else 0),(x+3,3,16 if head else 3),texture,(50,0))
    elif name.endswith('_sign') or name.endswith('_wall_sign'):
        wood = name.replace('_wall_sign','').replace('_sign','')
        texture = 'entity/signs/'+wood
        angle = angle if '_wall_' in name else -int(props.get('rotation','0'))*22.5
        z=0 if '_wall_' in name else 7+1/3
        box((0,7,z),(16,15,z+4/3),texture,(0,0),angle,(24,12,2))
        if '_wall_' not in name:
            box((7,0,7),(9,7,9),texture,(0,14),angle)
    elif name.endswith('_banner'):
        wall = '_wall_' in name
        angle = angle if wall else -int(props.get('rotation','0'))*22.5
        texture = 'entity/banner/base'
        box((4/3,2,7+2/3),(16-4/3,28+2/3,8+1/3),texture,(0,0),angle,(20,40,1))
        result[:] = [(t,c,0,vertices) for t,c,_,vertices in result]
        box((4/3,28,7+1/3),(16-4/3,29+1/3,8+2/3),texture,(0,42),angle,(20,2,2))
        if not wall:
            box((7+1/3,0,7+1/3),(8+2/3,28,8+2/3),texture,(44,0),angle,(2,42,2))
    elif name in ('skeleton_skull','skeleton_wall_skull','wither_skeleton_skull','wither_skeleton_wall_skull',
                  'zombie_head','zombie_wall_head','player_head','player_wall_head','creeper_head','creeper_wall_head'):
        texture = ('entity/skeleton/wither_skeleton' if name.startswith('wither') else
                   'entity/skeleton/skeleton' if name.startswith('skeleton') else
                   'entity/zombie/zombie' if name.startswith('zombie') else
                   'entity/creeper/creeper' if name.startswith('creeper') else 'entity/steve')
        angle = angle if '_wall_' in name else -int(props.get('rotation','0'))*22.5
        wall='_wall_' in name
        box((4,4 if wall else 0,0 if wall else 4),(12,12 if wall else 8,8 if wall else 12),texture,(0,0),angle)
    elif name=='conduit':
        box((5,5,5),(11,11,11),'entity/conduit/base',(0,0))
    elif name in ('dragon_head','dragon_wall_head'):
        texture = 'entity/enderdragon/dragon'
        angle = angle if '_wall_' in name else -int(props.get('rotation','0'))*22.5
        box((0,0,0),(16,16,16),texture,(112,30),angle)
        box((2,0,16),(14,5,28),texture,(176,65),angle)
        box((2,5,16),(14,10,28),texture,(176,44),angle)
        box((2,16,4),(4,20,8),texture,(0,0),angle)
        box((12,16,4),(14,20,8),texture,(0,0),angle)
    elif name=='bell':
        box((5,3,5),(11,10,11),'entity/bell/bell_body',(0,0))
        box((4,2,4),(12,4,12),'entity/bell/bell_body',(0,13))
    return result
