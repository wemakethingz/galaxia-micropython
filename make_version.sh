#!/bin/sh

rm -r micropython-stubs
mkdir micropython-stubs

python tools/extract_pyi.py ports/esp32 micropython-stubs

mv micropython-stubs/common-thingz micropython-stubs/thingz
mv micropython-stubs/thingz/thingz/* micropython-stubs/thingz/.
rm -r micropython-stubs/thingz/thingz

python tools/join_bins.py micropy.bin 0x1000 ports/esp32/build-GALAXIA/bootloader/bootloader.bin 0x8000 ports/esp32/build-GALAXIA/partition_table/partition-table.bin 0x10000 ports/esp32/build-GALAXIA/micropython.bin 0x00200000 $thingz/python/micropython_galaxia_v1-24_release/usbempty.bin