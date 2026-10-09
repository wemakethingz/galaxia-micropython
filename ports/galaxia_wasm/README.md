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
  `lib/lis2dh12`, `lib/lis2mdl`, and the board settings (`thingz_memory`, `hal/eeprom`:
  PCB version, compass calibration);
- the network modules of the esp32 port: `network` (`network_common.c`,
  `network_wlan.c`) and `socket` (`modsocket.c`), with the board's error numbers
  (newlib's: `ETIMEDOUT` is 116, `newlib/errno.h`);
- `_thread` as on the board: threads under the GIL, released every 32 jumps with a
  yield and while a thread waits, 5 kB stacks by default (`mpthreadport.c`, below);
- `machine` and `time` of the esp32 port (`modmachine.c`, `machine_pin.c`,
  `machine_timer.c`, `machine_rtc.c`, `machine_pwm.c`, `machine_i2c.c`,
  `machine_hw_spi.c`, `machine_uart.c`, `machine_adc.c`, `machine_dac.c`,
  `machine_touchpad.c`, `machine_bitstream.c`, `machine_wdt.c`, `modtime.c`), with
  SoftI2C, SoftSPI, `time_pulse_us`, `dht_readinto`; thingz_led's PWM objects are
  machine's, as on the board;
- the system menu (`debug_mode/`: SYSTEME button on GPIO0, last error, file to
  execute, variables, buttons, LED, sensors, restart), its FreeRTOS task a fiber like
  the drivers' (`reboot.c` through `debug_mode_reboot.c`: no USB device to detach);
- `microcontroller` (`modmicrocontroller.c`, through `microcontroller.c`), which the
  programming page uses to choose the file to execute (`set_file_to_execute`, kept in
  NVS) and restart the program;
- the firmware's frozen modules that can run here (`manifest.py`): `supervisor`,
  `asyncio`, `umqtt.simple`/`umqtt.robust`, `requests`/`urequests`, `mip`, `ntptime`,
  `upysh`, `dht`, `ds18x20`, `neopixel`, `onewire`;
- the boot sequence of `ports/esp32/main.c`: splash, `/data.csv` copied from the log,
  `main.py`, then the REPL.

These parts are simulated:

| Board | Simulation |
|---|---|
| FreeRTOS, `esp_timer`, GPIO (`idf_shim.c`) | one thread; timers run from the main thread's VM hook, sleeps and REPL wait (never on a `_thread`'s stack: `test/thread_timers.mjs`); a task waiting on a queue is blocked until a send or its timeout |
| FreeRTOS tasks of `_thread` (`mpthreadport.c`) | Emscripten fibers (a C stack each, switched by Asyncify) at the board's switch points |
| ST7735 screen on SPI (`st7735.c`) | the galaxia_sim QEMU model, without QEMU |
| I2C0 of a PCB 1.0.9: LIS2DH12, LIS2MDL, settings EEPROM (`peripherals.c`) | register models (the LIS2DH12 one from QEMU) |
| GPIO pads and matrix (`idf_shim.c`) | output, open drain, pulls, hold, interrupts on every level change; the board's wiring (buttons with pull-ups) |
| touch pads, LEDC, I2C ports, SPI hosts, ADC1/2, DAC (`peripherals.c`, `idf_shim.c`) | at the ESP-IDF driver level, with the ESP32-S2's limits (LEDC dividers, SPI2 taken by the screen) |
| timers, resets, sleep, RTC, watchdog, UART, RMT (`machine_hw.c`) | timer_ll registers; the page keeps what the RTC keeps across a reset |
| Wi-Fi chip, ESP-NOW, esp_netif, event loop, lwIP sockets, mDNS (`net.c`) | the ESP-IDF 5.4 API, with its constants, reason codes and the Galaxia's lwIP settings |
| FAT partition ("USB key") | 2 MB in memory (`galaxia_flash.c`), mounted at `/` |
| SPIFFS (log) | Emscripten file system, `/spiffs` |

The page (`web/`) runs the board in a Web Worker and provides:
- the screen, the LED, the A/B buttons (also keys A and B), the SYSTEME button and the
  N/S/E/W touch pads. Their changes carry the page's time and reach the firmware as
  far apart as they were made (`library.js`), even when the Worker is busy: a short
  click stays a pulse its debounce timers see;
- sensors: tilt and shake, compass heading, chip temperature, light level;
- the sound, through WebAudio: the DAC waveform, a looped table or WAV samples;
- the radio between the boards open in tabs of the same browser (`BroadcastChannel`);
- a Wi-Fi network between those boards: the classroom's access point (SSID, password
  and channel set in the page, shared by the tabs) and the boards' own soft-APs;
- a virtual MQTT broker (`web/broker.js`), the only host of the Internet behind the
  classroom's router: its names (broker.hivemq.com, test.mosquitto.org... set in the
  page) resolve to 203.0.113.10, port 1883. One of the open tabs hosts it (Web
  Locks); every tab shows its log and can publish;
- the REPL, and "Exécuter", which writes `main.py` through the raw REPL and then
  soft-resets the board;
- an upload as the programming page's (worker message `upload`: files and the file to
  execute), written straight into the flash: it stops the running program as
  `microcontroller.set_file_to_execute()` does (`ReloadInterrupt`, or Ctrl-C Ctrl-D in
  the REPL), and the soft reset writes the files after mounting the FAT partition
  (`main.c`) before running the program (thingz-front's simulator uses it);
- the board's files, kept in IndexedDB. A firmware crash reboots the board with its
  files, as on the board.

## Build

You need Emscripten (`~/emsdk`, `./emsdk install latest && ./emsdk activate latest`).
`emcc` requires Python 3.10 or later: set `EMSDK_PYTHON` if the `python3` in your path
is older.

    source ~/emsdk/emsdk_env.sh
    make                         # -> build/www: galaxia.mjs, galaxia.wasm, index.html, worker.js
    python3 -m http.server -d build/www 8790

The site is static files only (496 kB brotli for the `.wasm`) and needs no special
HTTP header. In the MicroPython tree (`ports/galaxia_wasm`), it builds from that tree.
From galaxia_sim, it builds from `build/wasm-src`.

Tests:
- `node test/node_smoke.mjs`: boot, REPL, a button;
- `node test/peripherals.mjs`: every peripheral;
- `node test/network.mjs`: Wi-Fi, TCP, UDP, mDNS and a soft-AP between two boards;
- `node test/threads.mjs`: `_thread`, `supervisor`, Ctrl-C and soft reset with threads;
- `node test/mqtt.mjs`: two boards and the virtual broker, with `umqtt.simple`;
- `node test/machine.mjs`: `machine` (Pin, PWM, Timer, RTC, I2C, SPI, UART, ADC, DAC,
  TouchPad, NeoPixel) and the resets;
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
- **Board version**: the board is a PCB 1.0.9 (EEPROM at 0x50: protocol 1, version 3),
  so the firmware drives a LIS2MDL compass, as on that board. The EEPROM keeps the
  compass calibration across reboots, with the files. Its write cycle time is not
  modelled.
- **Compass**: the page sets the field so that `compass.heading()` gives the chosen
  heading with the calibration the firmware uses (the EEPROM's, else its default one,
  the QMC6310U's on a 1.0.9).
- **Network**: the boards of the same browser form the network (`net.c`):
  - the page's access point is a router whose Internet is the virtual broker: a
    station takes an address in 192.168.1.100-199 (probed, as if its DHCP server gave
    it), router and DNS 192.168.1.1; the DNS answers the broker names, any other name
    fails after the DNS timeout (`OSError -202`); on a soft-AP, no Internet at all;
  - the broker: MQTT 3.1.1 and 3.1, any credentials, QoS 0/1/2, retained messages,
    `+`/`#`, last will, keep-alive (1.5 times the period), a client reconnecting with
    the same id replaces the old one; clean sessions only; no TLS (8883 refused), no
    WebSocket; its state is lost when the tab hosting it closes (another tab takes
    over, as a broker restarting);
  - a soft-AP is the ESP-IDF default (`ESP_XXXXXX`, open, 192.168.4.1, DHCP from .2),
    with beacons, station list, deauthentication when it stops;
  - TCP: flow control with lwIP's window and send buffer (5760 bytes), RST on a closed
    port (`ECONNRESET`), TIME_WAIT of 2 MSL after an active close (`EADDRINUSE` without
    `SO_REUSEADDR`), the segments that follow a close absorbed (FIN_WAIT/LAST_ACK),
    backlog, 6 sockets (fd 58-63); UDP: 6 datagrams queued, broadcast,
    multicast after `IP_ADD_MEMBERSHIP`; loopback;
  - the timings are approximations: scan 1.6 s, connection 2.3 s plus DHCP, wrong key
    found after 5 s, beacon timeout 6 s, SYN timeout 20 s;
  - to check on a board: the reason code of a wrong key (15, 4-way handshake timeout,
    so `status()` gives 15 and not `STAT_WRONG_PASSWORD`), the default soft-AP
    `max_clients` (4), and ESP-NOW once the station is associated on another channel
    than the radio's (`esp_now_send` fails with `ESP_ERR_ESPNOW_CHAN`, so
    `radio.send()` returns `False` until `radio.set_channel()`);
  - not modelled: IPv6, `ssl` (TLS), `network.PPP`, radio interference and range,
    RSSI (fixed around -45 to -57 dBm).
- **Threads**: they switch where the board's tasks do under the GIL (every 32 jumps,
  and when a thread sleeps or waits for the network or the REPL), not on the 10 ms
  FreeRTOS tick; a thread that runs C code without releasing the GIL keeps the CPU,
  as on the board. Recursion in a thread: about 12 Python levels with the default
  stack (to check on a board).
- **machine**:
  - the RTC clock starts at 0 at power-on: `time.localtime()` gives 2000-01-01 until
    the program sets it (`RTC().datetime()`), as on the board; it and `RTC.memory()`
    survive `machine.reset()`, the watchdog and deep sleep, not the page's reset
    button (a power-on);
  - `machine.reset()`, a watchdog not fed (cause `WDT_RESET`) and `deepsleep(ms)` (the
    board is off, the page boots it again after `ms`) give the board's reset causes
    and wake reason; deep sleep wakes on its timer only;
  - the pads read what drives them: their output, the board (buttons A/B, released 1),
    a pull-up or pull-down, else 0; nothing is wired to the free pins (I2C finds no
    device, SPI and UART read nothing, ADC reads a little noise, an output toggles
    nothing); the board's I2C bus (37/33) answers on hardware I2C, not SoftI2C;
  - the LED: thingz_led sets 13-bit duties on the 14-bit timer machine.PWM sets up at
    2 kHz, so the LED never exceeds a 50 % duty, on the board too (the page shows the
    duty with the eye's response: 186 for 255); a Pin or PWM of the program drives it
    too, as the GPIO matrix routes it;
  - not modelled: sound from PWM or `DAC.write()` on the speaker pins (only thingz's
    sound plays), I2S, `esp` and `esp32` modules, light sleep without a wake-up source
    (it returns at once), ADC2 conflicts with Wi-Fi (to check on a board).
- **Frozen modules**: left out, since they need what is not here: `aioespnow`
  (`espnow`), `ssl`, `webrepl`; and the board's boot scripts. `requests` works in
  http only.
- **Not modelled**: microphone (not built in the firmware either), the debug menu.
