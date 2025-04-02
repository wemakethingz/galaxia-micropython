import gc
import vfs

try:
    vfs.umount("/")
except:
    pass

gc.collect()
