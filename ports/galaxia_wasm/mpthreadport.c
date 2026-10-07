// _thread for the WebAssembly build: each MicroPython thread is an Emscripten fiber
// (its own C stack, switched by Asyncify), on the one thread of the Worker.
//
// As on the board (ports/esp32/mpthreadport.c, MICROPY_PY_THREAD_GIL): only the
// thread holding the GIL runs Python. The board's FreeRTOS tasks switch where the
// GIL is released: every 32 jumps of the VM (MICROPY_PY_THREAD_GIL_VM_DIVISOR) the
// running thread releases it and yields (taskYIELD() in mp_thread_mutex_unlock), and
// a thread that waits (sleep, socket, REPL input) releases it until it is woken. Here
// the switches happen at those same points; a thread that waits sleeps until its
// deadline, and the Worker itself sleeps when no thread can run.
//
// Same sizes as the board: a thread's stack is 5 kB by default (4 kB minimum) and
// MicroPython's stack check stops 1 kB before its end. The fiber's own C stack is
// larger, for the C code below Python.

#include <stdlib.h>
#include <string.h>
#include <emscripten.h>
#include <emscripten/fiber.h>

#include "py/runtime.h"
#include "py/gc.h"
#include "py/mpthread.h"
#include "py/mphal.h"
#include "mpthreadport.h"
#include "idf/idf_all.h"

#if MICROPY_PY_THREAD

extern void galaxia_js_sleep(int ms);
extern void galaxia_note_yield(void);

#define MP_THREAD_MIN_STACK_SIZE        (4 * 1024)
#define MP_THREAD_DEFAULT_STACK_SIZE    (MP_THREAD_MIN_STACK_SIZE + MICROPY_STACK_CHECK_MARGIN)
#define FIBER_C_STACK                   (64 * 1024)
#define FIBER_ASYNCIFY_STACK            (64 * 1024)

enum { T_RUNNABLE, T_SLEEPING, T_BLOCKED, T_FINISHED };

typedef struct _galaxia_thread_t {
    emscripten_fiber_t fiber;
    void *c_stack;
    void *asyncify_stack;
    int status;
    int64_t wake_at;                // T_SLEEPING: until then
    mp_thread_mutex_t *waiting;     // T_BLOCKED: on this mutex
    mp_state_thread_t *state;
    void *(*entry)(void *);
    void (*task)(void *);           // a FreeRTOS task of a driver (not Python)
    void *arg;                      // the Python arguments: a GC root
    bool ready;                     // started (mp_thread_start), not finished
    struct _galaxia_thread_t *next;
} galaxia_thread_t;

static galaxia_thread_t main_thread;
static galaxia_thread_t *threads;           // all of them, main_thread first
static galaxia_thread_t *current = &main_thread;
static bool main_fiber_ready;

static int64_t now_us(void) {
    return (int64_t)(emscripten_get_now() * 1000.0);
}

void mp_thread_init(void) {
    main_thread.status = T_RUNNABLE;
    main_thread.ready = true;
    main_thread.state = &mp_state_ctx.thread;
    main_thread.next = NULL;
    threads = &main_thread;
    current = &main_thread;
}

mp_state_thread_t *mp_thread_get_state(void) {
    return current->state;
}

void mp_thread_set_state(mp_state_thread_t *state) {
    current->state = state;
}

mp_uint_t mp_thread_get_id(void) {
    return (mp_uint_t)current;
}

void mp_thread_start(void) {
    current->ready = true;
}

void mp_thread_finish(void) {
    current->ready = false;
}

static void free_thread(galaxia_thread_t *t) {
    free(t->c_stack);
    free(t->asyncify_stack);
    free(t);
}

// Removes the finished threads (not the current one: it runs on its stack)
static void reap(void) {
    for (galaxia_thread_t **p = &threads; *p;) {
        galaxia_thread_t *t = *p;
        if (t->status == T_FINISHED && t != current && t != &main_thread) {
            *p = t->next;
            free_thread(t);
        } else {
            p = &t->next;
        }
    }
}

static void switch_to(galaxia_thread_t *t) {
    galaxia_thread_t *prev = current;
    current = t;
    emscripten_fiber_swap(&prev->fiber, &t->fiber);
}

// The next thread that can run after the current one, round robin; NULL if none
static galaxia_thread_t *next_runnable(void) {
    int64_t now = now_us();
    for (galaxia_thread_t *t = threads; t; t = t->next) {
        if (t->status == T_SLEEPING && now >= t->wake_at) {
            t->status = T_RUNNABLE;
        }
    }
    galaxia_thread_t *t = current;
    do {
        t = t->next ? t->next : threads;
        if (t->status == T_RUNNABLE) {
            return t;
        }
    } while (t != current);
    return NULL;
}

// Gives the CPU to another thread until the current one can run again
static void schedule(void) {
    for (;;) {
        galaxia_thread_t *t = next_runnable();
        if (t && t != current) {
            switch_to(t);
            reap();
        }
        if (current->status == T_RUNNABLE) {
            return;
        }
        if (next_runnable()) {
            continue;
        }
        // Nobody can run: the Worker sleeps until the first deadline. An input
        // (a key, a frame) ends the sleep early: the sleepers then look again.
        int64_t first = INT64_MAX;
        for (galaxia_thread_t *s = threads; s; s = s->next) {
            if (s->status == T_SLEEPING && s->wake_at < first) {
                first = s->wake_at;
            }
        }
        int64_t now = now_us();
        int ms = first == INT64_MAX ? 50 : (int)((first - now + 999) / 1000);
        galaxia_js_sleep(ms < 0 ? 0 : ms > 50 ? 50 : ms);
        galaxia_note_yield();
        if (now_us() < first) {
            for (galaxia_thread_t *s = threads; s; s = s->next) {
                if (s->status == T_SLEEPING) {
                    s->status = T_RUNNABLE;
                }
            }
        }
    }
}

void galaxia_thread_sleep_ms(int ms) {
    if (threads == NULL || threads->next == NULL) {
        galaxia_js_sleep(ms);       // one thread: as before threads existed
        galaxia_note_yield();
        return;
    }
    current->status = T_SLEEPING;
    current->wake_at = now_us() + (int64_t)ms * 1000;
    schedule();
}

static void fiber_entry(void *arg) {
    galaxia_thread_t *t = arg;
    if (t->task) {
        t->task(t->arg);
    } else {
        t->entry(t->arg);
    }
    t->status = T_FINISHED;
    t->ready = false;
    schedule();     // never comes back
    abort();
}

static galaxia_thread_t *new_fiber(size_t stack_size) {
    if (!main_fiber_ready) {
        main_thread.asyncify_stack = malloc(FIBER_ASYNCIFY_STACK);
        emscripten_fiber_init_from_current_context(&main_thread.fiber, main_thread.asyncify_stack, FIBER_ASYNCIFY_STACK);
        main_fiber_ready = true;
    }
    galaxia_thread_t *t = calloc(1, sizeof(*t));
    size_t c_size = stack_size > FIBER_C_STACK / 4 ? stack_size * 4 : FIBER_C_STACK;
    if (!t) {
        return NULL;
    }
    t->c_stack = malloc(c_size);
    t->asyncify_stack = malloc(FIBER_ASYNCIFY_STACK);
    if (!t->c_stack || !t->asyncify_stack) {
        free_thread(t);
        return NULL;
    }
    emscripten_fiber_init(&t->fiber, fiber_entry, t, t->c_stack, c_size, t->asyncify_stack, FIBER_ASYNCIFY_STACK);
    t->status = T_RUNNABLE;
    // After the main thread in the list: it runs at the next switch
    t->next = threads->next;
    threads->next = t;
    return t;
}

mp_uint_t mp_thread_create(void *(*entry)(void *), void *arg, size_t *stack_size) {
    if (*stack_size == 0) {
        *stack_size = MP_THREAD_DEFAULT_STACK_SIZE;
    } else if (*stack_size < MP_THREAD_MIN_STACK_SIZE) {
        *stack_size = MP_THREAD_MIN_STACK_SIZE;
    }
    galaxia_thread_t *t = new_fiber(*stack_size);
    if (!t) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("can't create thread"));
    }
    t->entry = entry;
    t->arg = arg;
    return (mp_uint_t)t;
}

// ---- FreeRTOS tasks of the drivers: fibers in the same scheduler, outside Python
// (they only schedule callbacks, with the main thread's state) ----

bool galaxia_in_task(void) {
    return current->task != NULL;
}

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name, uint32_t stack_depth, void *arg,
    UBaseType_t priority, TaskHandle_t *created_task, BaseType_t core_id) {
    if (threads == NULL) {
        mp_thread_init();
    }
    galaxia_thread_t *t = new_fiber(stack_depth * sizeof(StackType_t));
    if (!t) {
        return pdFAIL;
    }
    t->task = fn;
    t->arg = arg;
    t->state = &mp_state_ctx.thread;
    if (created_task) {
        *created_task = t;
    }
    return pdPASS;
}

void vTaskDelete(TaskHandle_t task) {
    galaxia_thread_t *t = task ? task : current;
    t->status = T_FINISHED;
    if (t == current) {
        schedule();     // never comes back
    }
}

static void gc_range(void *lo, void *hi) {
    lo = (void *)(((uintptr_t)lo + sizeof(void *) - 1) & ~(sizeof(void *) - 1));
    if (hi > lo) {
        gc_collect_root(lo, ((uintptr_t)hi - (uintptr_t)lo) / sizeof(void *));
    }
}

// The other threads' stacks: their C stack and, since they are suspended by
// Asyncify, the locals saved in their Asyncify stack
void mp_thread_gc_others(void) {
    for (galaxia_thread_t *t = threads; t; t = t->next) {
        gc_collect_root(&t->arg, 1);
        if (t == current || t->status == T_FINISHED) {
            continue;
        }
        if (t == &main_thread && !main_fiber_ready) {
            continue;
        }
        gc_range(t->fiber.stack_ptr, t->fiber.stack_base);
        gc_range(t->asyncify_stack, t->fiber.asyncify_data.stack_ptr);
    }
}

void mp_thread_deinit(void) {
    // Soft reset: the other threads stop where they are
    for (galaxia_thread_t *t = threads; t; t = t->next) {
        if (t != current) {
            t->status = T_FINISHED;
            t->ready = false;
        }
    }
    reap();
}

void mp_thread_mutex_init(mp_thread_mutex_t *mutex) {
    mutex->locked = 0;
}

int mp_thread_mutex_lock(mp_thread_mutex_t *mutex, int wait) {
    while (mutex->locked) {
        if (!wait) {
            return 0;
        }
        current->status = T_BLOCKED;
        current->waiting = mutex;
        schedule();
    }
    mutex->locked = 1;
    return 1;
}

void mp_thread_mutex_unlock(mp_thread_mutex_t *mutex) {
    mutex->locked = 0;
    for (galaxia_thread_t *t = threads; t; t = t->next) {
        if (t->status == T_BLOCKED && t->waiting == mutex) {
            t->status = T_RUNNABLE;
            t->waiting = NULL;
        }
    }
    // As the board: yield, so that a thread releasing and taking the GIL again
    // lets the others run (taskYIELD())
    if (threads && threads->next && current->status == T_RUNNABLE) {
        galaxia_thread_t *t = next_runnable();
        if (t && t != current) {
            switch_to(t);
            reap();
        }
    }
}

#endif // MICROPY_PY_THREAD
