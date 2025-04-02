import gc
import vfs
from flashbdev import bdev

gc.collect()
try:
    if bdev:
        vfs.mount(bdev, "/")
except OSError:
    import inisetup_fat
    
    vfs = inisetup_fat.setup()

gc.collect()
