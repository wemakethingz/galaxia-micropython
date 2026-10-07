// Entry point of the galaxia_sim WebAssembly port: the sequence of mp_task in the
// firmware's ports/esp32/main.c (splash, soft reset loop, thingz_init, the program
// chosen by thingz_get_python_file_to_exec, then the REPL), in a Web Worker.
// galaxia_main() never returns: sleeps and waits for input yield to the Worker's
// event loop (Asyncify).

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <emscripten.h>

#include "py/compile.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "py/stackctrl.h"
#include "py/stream.h"
#include "extmod/vfs.h"
#include "shared/readline/readline.h"
#include "shared/runtime/pyexec.h"

#include "common-thingz/thingz/thingz.h"
#include "common-thingz/thingz_memory/thingz_memory.h"
#include "common-thingz/thingz_i2c/thingz_i2c.h"
#include "common-thingz/thingz_screen/thingz_screen.h"
#include "debug_mode/debug_mode.h"
#include "idf_shim.h"
#include "machine_hw.h"
#include "ports/esp32/modmachine.h"
#include "ports/esp32/modnetwork.h"

#define HEAP_SIZE (76 * 1024)       // a program gets as much as on the board (67 kB of bytearray(1000))
// Recursion depth: MicroPython's check must trip before the browser's native stack
// overflows (Node: at ~53 Python levels, 256 bytes of this stack each): ~29 levels.
#define C_STACK_LIMIT (8 * 1024)

void galaxia_screen_ready(void);

static void exec_str(const char *src) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, src, strlen(src), 0);
        mp_parse_compile_execute(lex, MP_PARSE_FILE_INPUT, mp_globals_get(), mp_locals_get());
        nlr_pop();
    } else {
        mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
    }
}

// The log (thingz.log) is kept in the SPIFFS partition; at each start the firmware
// copies it to the USB key as /data.csv (same code as ports/esp32/main.c)
static void copy_log_to_usb_key(void) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t args[] = { mp_obj_new_str("/data.csv", 9), mp_obj_new_str("w", 1) };
        FILE *f2 = fopen("/spiffs/data.csv", "r");
        if (f2 != NULL) {
            mp_obj_t f = mp_vfs_open(2, args, (mp_map_t *)&mp_const_empty_map);
            char line[100];
            int err;
            while (fgets(line, 100, f2)) {
                mp_stream_write_exactly(f, line, strlen(line), &err);
            }
            mp_stream_close(f);
            fclose(f2);
        }
        nlr_pop();
    } else {
        mp_printf(MP_PYTHON_PRINTER, "Log csv transfer fail\n");
        mp_obj_print_exception(MP_PYTHON_PRINTER, MP_OBJ_FROM_PTR(nlr.ret_val));
    }
}

// The FAT partition at the root, as _boot_fat.py does on the board (formatted the
// first time)
static void mount_root(void) {
    exec_str(
        "import vfs, _galaxia\n"
        "try:\n"
        "    vfs.mount(vfs.VfsFat(_galaxia.flash), '/')\n"
        "except OSError:\n"
        "    vfs.VfsFat.mkfs(_galaxia.flash)\n"
        "    v = vfs.VfsFat(_galaxia.flash)\n"
        "    v.set_label('Galaxia')\n"
        "    vfs.mount(v, '/')\n"
        "    with open('boot.py', 'w') as f:\n"
        "        f.write('# This file is executed on every boot (including wake-boot from deepsleep)\\n#import esp\\n#esp.osdebug(None)\\n#import webrepl\\n#webrepl.start()\\n')\n"
        "    del v\n"
        "del vfs, _galaxia\n");
}

EMSCRIPTEN_KEEPALIVE
void galaxia_main(void) {
    mp_thread_init();
    gpio_shim_init();
    machine_hw_boot();      // the reset cause, the RTC clock and memory kept
    machine_init();
    char *heap = malloc(HEAP_SIZE);
    mkdir("/spiffs", 0777);     // the SPIFFS partition (thingz.log), Emscripten's file system

    #if MICROPY_THINGZ_I2C
    common_thingz_i2c_init(0, MICROPY_THINGZ_I2C_SCL, MICROPY_THINGZ_I2C_SDA, 1000000, MICROPY_THINGZ_I2C_USE_PULLUPS);
    #endif

    #if MICROPY_THINGZ_MEMORY
    thingz_memory_init();
    #endif
    #if MICROPY_THINGZ_SCREEN
    thingz_screen_init();
    thingz_screen_show_splash();
    mp_hal_delay_ms(2000);
    #endif

soft_reset:
    mp_thread_init();
    mp_stack_ctrl_init();
    mp_stack_set_limit(C_STACK_LIMIT);
    gc_init(heap, heap + HEAP_SIZE);
    mp_init();
    machine_pins_init();
    mount_root();
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_lib));
    readline_init0();

    thingz_screen_switch_mode(COMMON_THINGZ_SCREEN_MODE_REPL);
    galaxia_screen_ready();
    thingz_init();

    copy_log_to_usb_key();
    EM_ASM({ Module.galaxia.ready && Module.galaxia.ready(); });    // boot done: main.py or the REPL next

    #if MICROPY_THINGZ_DEBUG_MODE
    debug_mode_reset_last_exception();
    #endif
    int ret = pyexec_file_if_exists("boot.py");
    if (ret & PYEXEC_FORCED_EXIT) {
        goto soft_reset_exit;
    }
    const char *name = (const char *)thingz_get_python_file_to_exec(true, 1);
    if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL && ret != 0) {
        ret = pyexec_file_if_exists(name);
        thingz_screen_switch_mode(COMMON_THINGZ_SCREEN_MODE_REPL);
        if (ret & PYEXEC_FORCED_EXIT) {
            goto soft_reset_exit;
        }
    }

    debug_mode_expose_last_exception();
    char *exception = debug_mode_get_last_exception();
    thingz_screen_print_header(exception && strlen(exception) > 0 ? "Erreur" : "REPL");
    gc_collect();

    for (;;) {
        if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
            if (pyexec_raw_repl() != 0) {
                break;
            }
        } else {
            if (pyexec_friendly_repl() != 0) {
                break;
            }
        }
    }

soft_reset_exit:
    gc_collect();
    machine_timer_deinit_all();
    mp_thread_deinit();     // as the board: the other threads stop
    gc_sweep_all();
    machine_pwm_deinit_all();
    machine_pins_deinit();
    machine_deinit();
    socket_events_deinit();
    thingz_deinit();
    mp_hal_stdout_tx_str("Redémarrage du programme\r\n");
    mp_deinit();
    fflush(stdout);
    goto soft_reset;
}

static void gc_scan_func(void *begin, void *end) {
    gc_collect_root((void **)begin, (void **)end - (void **)begin + 1);
}

void gc_collect(void) {
    gc_collect_start();
    emscripten_scan_stack(gc_scan_func);
    emscripten_scan_registers(gc_scan_func);
    mp_thread_gc_others();
    gc_collect_end();
}

void nlr_jump_fail(void *val) {
    printf("FATAL: uncaught NLR %p\n", val);
    abort();
}

#ifndef NDEBUG
void MP_WEAK __assert_func(const char *file, int line, const char *func, const char *expr) {
    printf("Assertion '%s' failed, at file %s:%d\n", expr, file, line);
    abort();
}
#endif
