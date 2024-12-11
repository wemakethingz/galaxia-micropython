#include "thingz_screen_plot.h"
#include "thingz_screen.h"

#include "py/objint.h"
#include "py/mpz.h"
#include "py/runtime.h"

#include "mphalport.h"

#include "esp_timer.h"

static void thingz_screen_plot_animate_cb(void *args);

static esp_timer_handle_t animate_timer;
static esp_timer_create_args_t animate_timer_args = { .callback = &thingz_screen_plot_animate_cb, .name = "animate" };


static mp_obj_t thingz_screen_plot_add_point_cb(mp_obj_t pl){
    thingz_screen_plot_t* plot = (thingz_screen_plot_t*)pl;
    
    if(!plot || !plot->animate_cb || !plot->is_shown)
        return mp_const_none;
    mp_obj_fun_bc_t* fun = (mp_obj_fun_bc_t*)plot->animate_cb;
    mp_obj_t point = MP_OBJ_TYPE_GET_SLOT(&mp_type_fun_bc, call)(fun, 0, 0, NULL);
    const mp_obj_type_t* type = mp_obj_get_type(point);
    float p=0;
    if(mp_obj_is_int(point)){
        p =  mp_obj_get_int(point);
    }else if(type == &mp_type_float){
        p = mp_obj_get_float(point);
    }else if(type == &mp_type_int){
        mp_uint_t mpint;
        mpz_t mp = ((mp_obj_int_t*)point)->mpz;
        if(mpz_as_uint_checked(&mp, &mpint)){
            p = (float)mpint;
        }else{
            mp_raise_TypeError(("argument 'value': cannot get long"));
        }
    }
    thingz_screen_plot_add_point(plot, p);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(thingz_screen_plot_add_point_cb_obj, thingz_screen_plot_add_point_cb);


static void thingz_screen_plot_animate_cb(void *args){
    //mp_printf(MP_PYTHON_PRINTER, "PLOP!\n");
    mp_sched_schedule((mp_obj_t)&thingz_screen_plot_add_point_cb_obj, args);
    //mp_hal_wake_main_task_from_isr();
}

static uint8_t thingz_screen_plot_scale(thingz_screen_plot_t* plot, float value){
    return (value - (float)plot->scale[0]) * 128 / (plot->scale[1] - plot->scale[0]);
} 

void thingz_screen_plot_refresh(thingz_screen_plot_t *plot){
    
}

void thingz_screen_plot_enter(thingz_screen_plot_t *plot){
    plot->is_shown = 1;
}

void thingz_screen_plot_exit(thingz_screen_plot_t *plot){
    plot->is_shown = 0;
}

void thingz_screen_plot_add_point(thingz_screen_plot_t *plot, float value){
    
    int i, last_y;
    uint8_t last_x;

    value = thingz_screen_plot_scale(plot, value);

    if(value > 127)
        value = 127;

    //value = 127-value;


    if(!plot->is_shown)
        return;

    if(plot->next_point_x == 0){
        lcdFillScreen(&thingz_screen.dev, BLACK);
    }
    
    last_x = plot->next_point_x - 1 >= 0 ? plot->next_point_x -1: 159;
    last_y = plot->axis[last_x]; 

    // thingz_screen_plot_clear_column(plot, plot->next_point_x);
    if(last_x != 159)
        lcdDrawPixel(&(plot->screen->dev), last_y, last_x, 0xffff);
    if(last_y != -1){
        int inc = last_y > value ? -1 : 1;
        for(i = last_y; i != value; i = i + inc){
            if(i < 0 || i > 159)
                break;
            lcdDrawPixel(&(plot->screen->dev), i, plot->next_point_x, 0xffff);
        }
    }
    
    lcdDrawPixel(&(plot->screen->dev), value, plot->next_point_x, 0xf800);
    plot->axis[plot->next_point_x] = value;
    plot->next_point_x++;
    if(plot->next_point_x == 160){
        plot->next_point_x = 0;
    }

}

void thingz_screen_plot_clear_column(thingz_screen_plot_t* plot, uint8_t x){
    uint8_t i;
    for(i=0; i < 128; i++){
        lcdDrawPixel(&(plot->screen->dev), i, x, 0);

    }
}

void thingz_screen_plot_set_animate_function(thingz_screen_plot_t* plot, mp_obj_fun_bc_t* fun, uint32_t interval){
    plot->animate_cb = fun;
    plot->animate_interval = interval * 1000;
    if(plot->animate_cb == NULL){
        esp_timer_stop(animate_timer);
    }else{
        esp_timer_start_periodic(animate_timer, plot->animate_interval);
    }
}

void thingz_screen_plot_set_scale(thingz_screen_plot_t* plot, int32_t* scale){
    plot->scale[0] = scale[0];
    plot->scale[1] = scale[1];
}

void thingz_screen_plot_init(thingz_screen_plot_t *plot, thingz_screen_obj_t *screen){
    
    plot->next_point_x = 0;
    plot->scale[0] = 0;
    plot->scale[1] = 100;
    plot->screen = screen;
    plot->animate_cb = NULL;

    animate_timer_args.arg = (void*)plot;
    esp_timer_create(&animate_timer_args, &animate_timer);
}
 