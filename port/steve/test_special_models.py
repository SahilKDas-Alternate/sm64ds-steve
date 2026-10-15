"""Validate entity-atlas meshes against the pinned assets, without Minecraft."""
import math
import unittest
from export_assets import geometry, ASSETS

class SpecialModels(unittest.TestCase):
    def test_special_families_have_entity_atlas_faces(self):
        for name in ('chest','trapped_chest','ender_chest','red_bed','blue_shulker_box',
                     'oak_sign','spruce_wall_sign','white_banner','skeleton_skull',
                     'creeper_wall_head','dragon_head','conduit','bell','shield','trident'):
            with self.subTest(name=name):
                faces=geometry(name,{'facing':'south','part':'head'})
                self.assertGreaterEqual(len(faces),6)
                for texture,cull,tint,vertices in faces:
                    self.assertTrue(texture.startswith('entity/'),texture)
                    self.assertTrue((ASSETS/'textures'/f'{texture}.png').is_file())
                    self.assertEqual(len(vertices),4)
                    self.assertTrue(all(math.isfinite(n) for p in vertices for n in p))

    def test_chest_uses_lid_base_latch_not_full_cube(self):
        faces=geometry('chest',{'facing':'south'})
        self.assertEqual(len(faces),18)
        vertices=[v for *_,quad in faces for v in quad]
        self.assertAlmostEqual(max(v[1] for v in vertices),14/16)
        self.assertAlmostEqual(min(v[0] for v in vertices),1/16)
        self.assertTrue(any(0<v[3]<16 for v in vertices))

    def test_rotations_and_double_chest_join(self):
        north=geometry('chest',{'facing':'north'})
        south=geometry('chest',{'facing':'south'})
        self.assertAlmostEqual(north[-1][3][0][2]+south[-1][3][0][2],1)
        left=geometry('chest',{'facing':'south','type':'left'})
        right=geometry('chest',{'facing':'south','type':'right'})
        self.assertEqual(left[0][0],'entity/chest/normal_left')
        self.assertEqual(right[0][0],'entity/chest/normal_right')

    def test_invisible_states_do_not_become_stone(self):
        for name in ('light','barrier','moving_piston','air'):
            self.assertEqual(geometry(name,{}),[])
        self.assertEqual(geometry('cobblestone_wall',dict(up='false',north='none',south='none',east='none',west='none')),[])

if __name__=='__main__': unittest.main()
