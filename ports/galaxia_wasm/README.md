# Galaxia WebAssembly

The Galaxia's MicroPython firmware compiled to WebAssembly: a board running in a web
page, with nothing to install and no server beyond static files. It is meant for
classes. To test the firmware itself (drivers, timings), use galaxia_sim (QEMU), which
runs the real `.bin`.

## What runs

These parts are the firmware's own code, compiled unchanged:
- the MicroPython 1.24 core of the Thingz tree, including the Thingz changes
  (`ReloadInterrupt`, the last exception kept for the screen header);
- `common-thingz`: `thingz` (screen, buttons, touch pads, accelerometer, compass, LED
  and light level, sound, radio, log, temperature);
- the drivers below it: `lib/tft/ili9340`, `hal/accel_lis2dh12`, `hal/magneto`,
  `lib/lis2dh12`, `lib/qmc6310u`;
- the boot sequence of `ports/esp32/main.c`: splash, `/data.csv` copied from the log,
  `main.py`, then the REPL.

These parts are simulated:

| Board | Simulation |
|---|---|
| FreeRTOS, `esp_timer`, GPIO (`idf_shim.c`) | one thread; timers run from the VM hook, from sleeps and while the REPL waits |
| ST7735 screen on SPI (`st7735.c`) | the galaxia_sim QEMU model, without QEMU |
| I2C0: LIS2DH12, QMC6310 (`peripherals.c`) | the QEMU register models |
| touch pads, LEDC, ADC (light), DAC, ESP-NOW | at the ESP-IDF driver level |
| FAT partition ("USB key") | 2 MB in memory (`galaxia_flash.c`), mounted at `/` |
| SPIFFS (log) | Emscripten file system, `/spiffs` |

The page (`web/`) runs the board in a Web Worker and provides:
- the screen, the LED, the A/B buttons (also keys A and B) and the N/S/E/W touch pads;
- sensors: tilt and shake, compass heading, chip temperature, light level;
- the sound, through WebAudio: the DAC waveform, a looped table or WAV samples;
- the radio between the boards open in tabs of the same browser (`BroadcastChannel`);
- the REPL, and "Exécuter", which writes `main.py` through the raw REPL and then
  soft-resets the board;
- the board's files, kept in IndexedDB. A firmware crash reboots the board with its
  files, as on the board.

## Build

You need Emscripten (`~/emsdk`, `./emsdk install latest && ./emsdk activate latest`).
`emcc` requires Python 3.10 or later: set `EMSDK_PYTHON` if the `python3` in your path
is older.

    source ~/emsdk/emsdk_env.sh
    make                         # -> build/www: galaxia.mjs, galaxia.wasm, index.html, worker.js
    python3 -m http.server -d build/www 8790

The site is static files only (370 kB brotli for the `.wasm`) and needs no special
HTTP header. In the MicroPython tree (`ports/galaxia_wasm`), it builds from that tree.
From galaxia_sim, it builds from `build/wasm-src`.

Tests:
- `node test/node_smoke.mjs`: boot, REPL, a button;
- `node test/peripherals.mjs`: every peripheral;
- `node test/run.mjs GALAXIA.mjs SESSION END_REGEX`: a REPL session;
- `node test/browser_check.mjs URL DIR`: the page in headless Chrome.

## Choices and measurements

- **Asyncify**: the GC must see the pointers held in wasm locals
  (`emscripten_scan_registers`), and sleeps and the REPL's wait for input hand control
  back to the Worker's event loop. Messages from the page get in without a
  SharedArrayBuffer. Asyncify costs about 1.8x on a busy loop, and Binaryen's
  `--spill-pointers` alternative fails with this Emscripten.
- **Wasm exceptions** for MicroPython's `nlr` (`LONGJMP=wasm`): twice as fast as through
  JavaScript, with a third of the native stack. They need Chrome 95, Firefox 100 or
  Safari 15.2.
- **Speed**: 1M iterations of a small-int loop take ~0.5 s, faster than the board. An
  idle board takes ~1 % of a core.
- **Heap**: 76 kB. A program gets as much memory as on the board (67 kB of
  `bytearray(1000)` before `MemoryError`).
- **Recursion depth**: ~29 Python levels. MicroPython's check (8 kB of the Emscripten
  stack) must trip before the browser's native stack overflows.

## Known differences with the board

- **Infinite recursion**: the firmware crashes, as on the board, then the page reboots
  it. The Thingz change to `nlr_jump` (`py/nlrsetjmp.c`) prints each exception while it
  is being raised. For a recursion error, that print raises again until no handler is
  left. That is a firmware bug, not a bug of this port.
- **Garbage collection**: the GC is conservative and sees more stale pointers than on the
  board, so memory freed inside a running block (`l = None; gc.collect()`) may only come
  back once the block ends.
- **Compass**: the page sets the field so that `compass.heading()` gives the chosen
  heading with the firmware's default QMC6310U calibration. After `compass.calibrate()`,
  the heading drifts, as on a board.
- **Not modelled**: microphone and pins (not built in the firmware either), Wi-Fi
  networking, the debug menu.
