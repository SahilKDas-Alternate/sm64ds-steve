"""Stop the owned server before the workspace reaches its hard 1 GiB cap."""
import ctypes
import sys
from check_budget import root,workspace_bytes

kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.OpenProcess.argtypes=(ctypes.c_ulong,ctypes.c_int,ctypes.c_ulong)
kernel.OpenProcess.restype=ctypes.c_void_p
kernel.WaitForSingleObject.argtypes=(ctypes.c_void_p,ctypes.c_ulong)
kernel.CloseHandle.argtypes=(ctypes.c_void_p,)
handle=kernel.OpenProcess(0x100000,False,int(sys.argv[1]))
if not handle: sys.exit(0)
try:
    while kernel.WaitForSingleObject(handle,0)==258:
        if workspace_bytes()>=950*1024*1024:
            (root/'runtime/minecraft/budget-stop.txt').write_text('Workspace reached 950 MiB; leave room for a clean save.\n')
            break
        if kernel.WaitForSingleObject(handle,10000)!=258: break
finally:kernel.CloseHandle(handle)
