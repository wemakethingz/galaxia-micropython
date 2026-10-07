# Galaxia WebAssembly port: the Thingz MicroPython firmware's Python layer
# (common-thingz: display, buttons, sensors...) compiled with Emscripten, to run a
# Galaxia in a browser Web Worker. Derived from MicroPython's ports/webassembly.
#
#   make            (Emscripten in the PATH: source ~/emsdk/emsdk_env.sh; emcc needs
#                    Python >= 3.10, set EMSDK_PYTHON if `python3` is older)
#
# In the MicroPython tree (ports/galaxia_wasm) it builds from that tree; from
# galaxia_sim it builds from build/wasm-src, a `git archive` of the Thingz tree.

PORT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
ifneq ($(wildcard $(PORT_DIR)/../../py/mkenv.mk),)
MPY_DIR ?= $(abspath $(PORT_DIR)/../..)
BUILD ?= $(PORT_DIR)/build
else
MPY_DIR ?= $(abspath $(PORT_DIR)/../build/wasm-src)
BUILD ?= $(abspath $(PORT_DIR)/../build/wasm)
endif
ESP32_DIR = $(MPY_DIR)/ports/esp32

include $(MPY_DIR)/py/mkenv.mk

MICROPY_VFS_FAT = 1
# The firmware's frozen modules that run here (manifest.py)
FROZEN_MANIFEST ?= $(PORT_DIR)/manifest.py


QSTR_DEFS = qstrdefsport.h

include $(TOP)/py/py.mk
include $(TOP)/extmod/extmod.mk

CC = emcc
LD = emcc
NODE ?= node

# Our port directory first: mpconfigport.h, mphalport.h and mpconfigboard.h are ours,
# not the esp32 port's. Then the ESP-IDF shim headers, then the esp32 port for
# "common-thingz/..." and "lib/tft/..." includes.
INC += -I.
INC += -Inewlib
INC += -Iidf
INC += -I$(TOP)
INC += -I$(BUILD)
INC += -I$(ESP32_DIR)

CFLAGS += -std=gnu11 -Wall -Werror -Wdouble-promotion -Wfloat-conversion
CFLAGS += -Wno-unused-but-set-global
# -Os: 18 % smaller than -O2 (342 kB brotli), no measurable speed difference
OPT ?= -Os
CFLAGS += $(OPT) -DNDEBUG
# setjmp/longjmp (MicroPython's nlr) with wasm exceptions (Chrome 95, Firefox 100, Safari
# 15.2): twice as fast as through JS (LONGJMP=emscripten), and a third of the native stack
LONGJMP ?= wasm
CFLAGS += -sSUPPORT_LONGJMP=$(LONGJMP)
CFLAGS += $(INC)

# Thingz code (built as on the board, warnings are not ours to fix)
# (size_t is unsigned long on wasm32, unsigned int on the ESP32: same 32-bit wasm signature)
THINGZ_CFLAGS = -Wno-error -w -Wno-error=incompatible-function-pointer-types -include thingz_compat.h

JSFLAGS += -s EXPORTED_FUNCTIONS="_galaxia_main,_malloc,_free"
JSFLAGS += -s EXPORTED_RUNTIME_METHODS="ccall,cwrap,FS,HEAPU8,HEAPU16"
JSFLAGS += --js-library library.js
JSFLAGS += -sSUPPORT_LONGJMP=$(LONGJMP)
# Asyncify: the GC scans the wasm registers, sleeps and the REPL's wait for a key go
# back to the Worker's event loop (messages from the page get in, no SharedArrayBuffer).
# (Measured: Asyncify costs ~1.8x on a busy loop, but the GC cannot see the pointers
# held in wasm locals without it, and Binaryen's --spill-pointers fails here. The
# result is still faster than the board, and an idle board takes ~1 % of a core.)
JSFLAGS += -s ASYNCIFY -s ASYNCIFY_STACK_SIZE=65536
JSFLAGS += -s MODULARIZE -s EXPORT_ES6 -s EXPORT_NAME=createGalaxia
JSFLAGS += -s ENVIRONMENT=worker,node
JSFLAGS += -s ALLOW_MEMORY_GROWTH -s STACK_SIZE=262144
# e.g. EXTRA_LDFLAGS=--profiling-funcs for function names in stack traces
JSFLAGS += $(EXTRA_LDFLAGS)

SRC_SHARED = $(addprefix shared/,\
	netutils/netutils.c \
	runtime/interrupt_char.c \
	runtime/stdout_helpers.c \
	runtime/sys_stdio_mphal.c \
	runtime/pyexec.c \
	readline/readline.c \
	timeutils/timeutils.c \
	)

SRC_C += \
	main.c \
	mphalport.c \
	idf_shim.c \
	st7735.c \
	peripherals.c \
	thingz_stubs.c \
	galaxia_flash.c \
	net.c \
	mpthreadport.c \

# Thingz sources, as they are in the firmware
SRC_THINGZ = $(addprefix ports/esp32/,\
	common-thingz/thingz/thingz.c \
	common-thingz/thingz_button/thingz_button.c \
	common-thingz/thingz_button_touch/thingz_button_touch.c \
	common-thingz/thingz_i2c/thingz_i2c.c \
	common-thingz/thingz_accel/thingz_accel.c \
	common-thingz/thingz_led/thingz_led.c \
	common-thingz/thingz_sound/thingz_sound.c \
	common-thingz/thingz_radio/thingz_radio.c \
	network_common.c \
	network_wlan.c \
	modsocket.c \
	common-thingz/thingz_log/thingz_log.c \
	common-thingz/thingz_memory/thingz_memory.c \
	hal/eeprom.c \
	hal/accel_lis2dh12.c \
	hal/magneto.c \
	lib/lis2dh12/lis2dh12.c \
	lib/lis2mdl/lis2mdl_reg.c \
	lib/qmc6310u/qmc6310u.c \
	lib/qmc6309/qmc6309.c \
	common-thingz/thingz_screen/thingz_screen.c \
	common-thingz/thingz_screen/thingz_screen_repl.c \
	common-thingz/thingz_screen/thingz_screen_plot.c \
	common-thingz/thingz_screen/thingz_screen_debug.c \
	common-thingz/thingz_screen/thingz_screen_raw.c \
	common-thingz/thingz_display/thingz_display.c \
	common-thingz/thingz_display/Console/thingz_display_console.c \
	common-thingz/thingz_display/Plot/thingz_display_plot.c \
	common-thingz/thingz_display/Raw/thingz_display_raw.c \
	common-thingz/thingz_display/Raw/thingz_display_raw_image.c \
	common-thingz/thingz_display/Raw/thingz_display_raw_rectangle.c \
	common-thingz/thingz_display/Raw/thingz_display_raw_text.c \
	lib/tft/ili9340.c \
	lib/tft/fontx.c \
	lib/tft/font.c \
	)

SRC_QSTR += $(SRC_C) $(SRC_SHARED) $(addprefix $(TOP)/,$(SRC_THINGZ))

OBJ += $(PY_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_SHARED:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_THINGZ:.c=.o))

$(addprefix $(BUILD)/, $(SRC_THINGZ:.c=.o)): CFLAGS += $(THINGZ_CFLAGS)

all: $(BUILD)/www/index.html

# The site: static files only, no special HTTP header needed
WEB_FILES = $(wildcard web/*)
$(BUILD)/www/index.html: $(BUILD)/galaxia.mjs $(WEB_FILES)
	$(ECHO) "WWW $(BUILD)/www"
	$(Q)mkdir -p $(BUILD)/www
	$(Q)cp $(BUILD)/galaxia.mjs $(BUILD)/galaxia.wasm $(WEB_FILES) $(BUILD)/www/

$(BUILD)/galaxia.mjs: $(OBJ) library.js
	$(ECHO) "LINK $@"
	$(Q)emcc $(LDFLAGS) -o $@ $(OBJ) $(JSFLAGS)

include $(TOP)/py/mkrules.mk
