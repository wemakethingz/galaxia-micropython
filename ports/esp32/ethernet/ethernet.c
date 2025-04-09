#include <stdbool.h>
#include <stdio.h>

#include "driver/spi_common.h"
#include "freertos/projdefs.h"
#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"

#include "ethernet.h"


#include <string.h>
#include <time.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_netif_glue.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_interface.h"
#include "esp_mac.h"

#include "shared/netutils/netutils.h"

#include "extmod/modmachine.h"
#include "modnetwork.h"


static mp_obj_t _lan = NULL;

static mp_obj_t _get_lan(){
    // network.LAN(None, phy_addr=1, phy_type=network.PHY_W5500, cs=machine.Pin(15), int=machine.Pin(21), spi=machine.SPI(2, dma=2))

    mp_obj_t lan = MP_OBJ_FROM_PTR(&esp_network_get_lan_obj);

    mp_obj_t spi_args[] = {
        MP_OBJ_NEW_SMALL_INT(2),
        MP_OBJ_NEW_QSTR(MP_QSTR_dma), MP_OBJ_NEW_SMALL_INT(2),
    };

    mp_obj_t spi = MP_OBJ_TYPE_GET_SLOT(&machine_spi_type, make_new)((mp_obj_t)&machine_spi_type, 1, 1, spi_args);

    mp_obj_t cs_args[] = {
        MP_OBJ_NEW_SMALL_INT(15),
    };

    mp_obj_t cs = MP_OBJ_TYPE_GET_SLOT(&machine_pin_type, make_new)((mp_obj_t)&machine_pin_type, 1, 0, cs_args);

    mp_obj_t int_args[] = {
        MP_OBJ_NEW_SMALL_INT(21),
    };

    mp_obj_t inter = MP_OBJ_TYPE_GET_SLOT(&machine_pin_type, make_new)((mp_obj_t)&machine_pin_type, 1, 0, int_args);

    mp_obj_t lan_args[] = {
        MP_OBJ_NEW_QSTR(MP_QSTR_phy_addr), MP_OBJ_NEW_SMALL_INT(1),
        MP_OBJ_NEW_QSTR(MP_QSTR_phy_type), MP_OBJ_NEW_SMALL_INT(PHY_W5500),
        MP_OBJ_NEW_QSTR(MP_QSTR_cs), cs,
        MP_OBJ_NEW_QSTR(MP_QSTR_int), inter,
        MP_OBJ_NEW_QSTR(MP_QSTR_spi), spi,
    };

    return mp_call_function_n_kw(lan, 0, 5, lan_args);
}


//| def active(self, state:bool) -> True:
//|     """If called with no args, return the current state of the ethernet driver.
//|     Else start or stop the ethernet driver 
//|
//|     :param bool state: activate the ethernet driver if True else stop it""" 
//|     ...
//|
static mp_obj_t ethernet_active(size_t n_args, const mp_obj_t *args) {
    // ethernet_obj_t *self = &ethernet_obj;
    if(_lan == NULL){
        _lan = _get_lan();
    }
    // mp_obj_type_t* lan_type = mp_obj_get_type(_lan);
    // mp_map_t *locals_map_lan = &(MP_OBJ_TYPE_GET_SLOT(&lan_type,locals_dict)->map);
    // mp_map_elem_t *active = mp_map_lookup(locals_map_sensor, MP_OBJ_NEW_QSTR(MP_QSTR_active), MP_MAP_LOOKUP);
    
    mp_obj_t meth[2 + n_args];
    mp_load_method(_lan, MP_QSTR_active, meth);
    if (args != NULL) {
        memcpy(meth + 2, args, n_args * sizeof(*args));
    }
    return mp_call_method_n_kw(n_args, 0, meth);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ethernet_active_obj, 0, 1, ethernet_active);

static mp_obj_t ethernet___init__(void) {
    mp_obj_t act = mp_obj_new_bool(false);
         
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        ethernet_active(1, &act);
        act = mp_obj_new_bool(true);
        ethernet_active(1, &act);
        nlr_pop();
    }else{
        mp_printf(MP_PYTHON_PRINTER, "Eth adapater not found\n");
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(ethernet___init___obj, ethernet___init__);


//| def status(self) -> int:
//|     """Return the current status of the driver can be:
//|
//|     * INITIALIZED: 0, intial status
//|     * STARTED: 1, the driver is started and the module was found
//|     * STOPPED: 2, the driver is stopped
//|     * CONNECTED: 3, ethernet connection established
//|     * DISCONNECTED: 4, ethernet connection lost
//|     * GOT_IP: 5, ethernet connection established and IP address set
//|     """
//| 
//|     ...
//|
static mp_obj_t ethernet_status(void) {
    if(_lan == NULL){
        _lan = _get_lan();
    }
    mp_obj_t meth[2];
    mp_load_method(_lan, MP_QSTR_status, meth);
    
    return mp_call_method_n_kw(0, 0, meth);
}
static MP_DEFINE_CONST_FUN_OBJ_0(ethernet_status_obj, ethernet_status);

//| def isconnected(self) -> bool:
//|     """Return True if IP is set
//|     """
//| 
//|     ...
//|
static mp_obj_t ethernet_isconnected(void) {
    if(_lan == NULL){
        _lan = _get_lan();
    }
    mp_obj_t meth[2];
    mp_load_method(_lan, MP_QSTR_status, meth);
    
    mp_obj_t status = mp_call_method_n_kw(0, 0, meth);
    return mp_obj_new_bool(mp_obj_get_int(status) == ETH_GOT_IP);
}
static MP_DEFINE_CONST_FUN_OBJ_0(ethernet_isconnected_obj, ethernet_isconnected);

//| def ifconfig(self, ip_config: list/tuple/str) -> int:
//|     """If called with no arg, return the current IP address.
//|     Else set the interface configuration
//|         
//|     :param list/tuple/str ip_config: Can be "dhcp" to configure the interface using DHCP or a list/tuple with the following args:
//|     * ip address: str
//|     * netmask: str
//|     * gateway: str
//|     * DNS: str
//|     """
//| 
//|     ...
//|
static mp_obj_t ethernet_ifconfig(size_t n_args, const mp_obj_t *args) {
    if(_lan == NULL){
        _lan = _get_lan();
    }

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t meth[2 + n_args];
        mp_load_method(_lan, MP_QSTR_ifconfig, meth);
        if (args != NULL) {
            memcpy(meth + 2, args, n_args * sizeof(*args));
        }
        mp_obj_t ret = mp_call_method_n_kw(n_args, 0, meth);
        nlr_pop();
        return ret;
    } else {
        const char* msg = mp_obj_str_get_str(mp_obj_exception_get_value(MP_OBJ_FROM_PTR(nlr.ret_val)));
        if(strcmp(msg, "Wifi Unknown Error 0x5004") == 0){
            return mp_const_none;
        }
        mp_raise_msg(mp_obj_get_type(MP_OBJ_FROM_PTR(nlr.ret_val)), msg);
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ethernet_ifconfig_obj, 0, 1, ethernet_ifconfig);

//| def get_lan(self) -> network.LAN:
//|     """Return the intern LAN object
//|     """
//| 
//|     ...
//|
static mp_obj_t ethernet_get_lan(void) {
    return _get_lan();
}
static MP_DEFINE_CONST_FUN_OBJ_0(ethernet_get_lan_obj, ethernet_get_lan);


static const mp_map_elem_t ethernet_module_globals_table[] = {
	{ MP_ROM_QSTR(MP_QSTR___name__), 		MP_ROM_QSTR(MP_QSTR_ethernet) },

     // Initialization
    { MP_ROM_QSTR(MP_QSTR___init__),    (mp_obj_t)(&ethernet___init___obj) },
    { MP_ROM_QSTR(MP_QSTR_active), (mp_obj_t)&ethernet_active_obj },
    { MP_ROM_QSTR(MP_QSTR_isconnected), (mp_obj_t)&ethernet_isconnected_obj },
    { MP_ROM_QSTR(MP_QSTR_status), (mp_obj_t)(&ethernet_status_obj) },
    { MP_ROM_QSTR(MP_QSTR_ifconfig), (mp_obj_t)(&ethernet_ifconfig_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_lan), (mp_obj_t)(&ethernet_get_lan_obj) },
};

static MP_DEFINE_CONST_DICT (
	ethernet_module_globals,
	ethernet_module_globals_table
);

const mp_obj_module_t ethernet_module = {
.base = { &mp_type_module },
.globals = (mp_obj_dict_t*)&ethernet_module_globals,
};


MP_REGISTER_MODULE(MP_QSTR_ethernet, ethernet_module);