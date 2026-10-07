# Frozen modules of the WebAssembly build: those of the Galaxia firmware
# (ports/esp32/boards/GALAXIA/manifest.py) that can run here. Left out: the esp32
# port's boot modules (the page mounts the flash itself), the board's boot scripts,
# and what needs machine, espnow or TLS (dht, ds18x20, neopixel, onewire,
# aioespnow, ssl, webrepl).
freeze("$(PORT_DIR)/../esp32/boards/GALAXIA/modules", "supervisor.py")
include("$(MPY_DIR)/extmod/asyncio")
require("mip")
require("ntptime")
require("requests")
require("urequests")
require("umqtt.robust")
require("umqtt.simple")
require("upysh")
