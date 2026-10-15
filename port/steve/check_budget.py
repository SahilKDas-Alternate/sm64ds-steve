"""Count every workspace file, including ignored outputs, against a 1 GiB cap."""
from pathlib import Path
import os
import stat
import sys

root = Path(__file__).resolve().parents[2]
limit = 1_073_741_824
def workspace_bytes():
    total=0
    for directory,dirs,files in os.walk(root,followlinks=False):
        # Never follow junctions into other workspaces.
        dirs[:]=[d for d in dirs if not (Path(directory)/d).is_symlink()
                 and not (getattr((Path(directory)/d).lstat(),'st_file_attributes',0)
                          & getattr(stat,'FILE_ATTRIBUTE_REPARSE_POINT',0))]
        for name in files:
            path=Path(directory)/name
            try:
                if not path.is_symlink(): total+=path.stat().st_size
            except FileNotFoundError: pass # Atomic saves can replace files during a scan.
    return total
if __name__=='__main__':
    total=workspace_bytes()
    print(f'Workspace: {total:,} bytes / {limit:,} bytes ({total / limit:.1%})')
    sys.exit(0 if total<limit else 1)
