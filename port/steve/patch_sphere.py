"""Add per-contact carving to the hosted KCL sphere walk; never edit ROM code."""
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
anchor = '                        /* Voronoi region select:'
assert source.count(anchor)==1, 'KCL contact insertion point changed'
hook = '''                        if (steve::native_enabled()) {
                            tp.x = vtx[0] << 6; tp.y = vtx[1] << 6; tp.z = vtx[2] << 6;
                            KCL_VERTEX(&vb.x, en2, tp.x, tp.y, tp.z)
                            KCL_VERTEX(&vc.x, en1, tp.x, tp.y, tp.z)
                            auto point = steve::closestTriangle(
                                {c->x/4096.f,c->y/4096.f,c->z/4096.f},
                                {tp.x/4096.f,tp.y/4096.f,tp.z/4096.f},
                                {vb.x/4096.f,vb.y/4096.f,vb.z/4096.f},
                                {vc.x/4096.f,vc.y/4096.f,vc.z/4096.f});
                            if (steve::native_removed(point.x,point.y,point.z)) continue;
                        }

'''
includes = '\n'.join('#include "'+str(Path(__file__).parent/n).replace('\\','/')+'"' for n in ('native.h','geometry.h'))+'\n'
Path(sys.argv[2]).write_text(includes+source.replace(anchor,hook+anchor))
