#include "thingz_display_mode.h"


const mp_obj_type_t thingz_display_mode_type;

const thingz_display_mode_obj_t thingz_display_mode_console_obj = {
    { &thingz_display_mode_type },
};

const thingz_display_mode_obj_t thingz_display_mode_plot_obj = {
    { &thingz_display_mode_type },
};

static const mp_rom_map_elem_t thingz_display_mode_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_CONSOLE),    MP_ROM_PTR(&thingz_display_mode_console_obj) },
    { MP_ROM_QSTR(MP_QSTR_PLOT),   MP_ROM_PTR(&thingz_display_mode_plot_obj) },
};
static MP_DEFINE_CONST_DICT(thingz_display_mode_locals_dict, thingz_display_mode_locals_dict_table);

static void thingz_display_mode_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    qstr display_mode = MP_QSTR_PLOT;
    if (MP_OBJ_TO_PTR(self_in) == MP_ROM_PTR(&thingz_display_mode_console_obj)) {
        display_mode = MP_QSTR_CONSOLE;
    }
    mp_printf(print, "%q.%q.%q", MP_QSTR_thingz, MP_QSTR_DisplayMode, display_mode);
}

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_display_mode_type,
    MP_QSTR_DisplayMode,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_display_console_make_new,
    print, thingz_display_mode_print
    locals_dict, &thingz_display_mode_locals_dict
);
