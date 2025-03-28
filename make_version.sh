#!/bin/sh

rm -r micropython-stubs
mkdir micropython-stubs

python tools/extract_pyi.py ports/esp32 micropython-stubs

mv micropython-stubs/common-thingz micropython-stubs/thingz
mv micropython-stubs/thingz/thingz/* micropython-stubs/thingz/.
rm -r micropython-stubs/thingz/thingz