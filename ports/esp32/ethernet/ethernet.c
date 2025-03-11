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

enum { ETH_INITIALIZED, ETH_STARTED, ETH_STOPPED, ETH_CONNECTED, ETH_DISCONNECTED, ETH_GOT_IP };

typedef struct _lan_if_obj_t {
    mp_obj_base_t base;
    esp_interface_t if_id;
    esp_netif_t *netif;
    volatile bool active;
    bool initialized;
    int8_t mdc_pin;
    int8_t mdio_pin;
    int8_t phy_power_pin;
    int8_t phy_cs_pin;
    int8_t phy_int_pin;
    uint8_t phy_addr;
    uint8_t phy_type;
    esp_eth_mac_t *mac;
    esp_eth_phy_t *phy;
    esp_eth_handle_t eth_handle;
    esp_eth_netif_glue_handle_t glue_handle;
} ethernet_obj_t;

typedef struct {
    esp_eth_mac_t parent;
    esp_eth_mediator_t *eth;
    spi_device_handle_t spi_hdl;
    SemaphoreHandle_t spi_lock;
    TaskHandle_t rx_task_hdl;
    uint32_t sw_reset_timeout_ms;
    int int_gpio_num;
    uint8_t addr[6];
    bool packets_remain;
} emac_w5500_t;

const mp_obj_type_t ethernet_type;
static ethernet_obj_t ethernet_obj = {{&ethernet_type}};
static uint8_t eth_status = 0;
static spi_host_device_t spi_host_id = -1;
static esp_eth_handle_t eth_handle = NULL;
static esp_eth_netif_glue_handle_t glue_handle = NULL;
static esp_netif_t *netif = NULL;
static esp_eth_mac_t *w5500_mac;
static esp_eth_phy_t *w5500_phy;

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data);

void ethernet_deinit(void){
     ethernet_obj_t *self = &ethernet_obj;
    
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler);

    if(self->eth_handle){
        esp_eth_stop(self->eth_handle);
    }

    if(self->glue_handle)
        esp_eth_del_netif_glue(self->glue_handle);

    if(self->mac)
        self->mac->del(self->mac);
    if(self->phy)
        self->phy->del(self->phy);
        
        
    if(self->eth_handle)
        esp_eth_driver_uninstall(self->eth_handle);
    
    if(self->netif)
        esp_netif_destroy(self->netif);


    spi_bus_free(spi_host_id);
    spi_host_id = -1;
    self->netif = NULL;
    self->eth_handle = NULL;
    self->glue_handle = NULL;
    self->mac = NULL;
    self->phy = NULL;
}

static inline uint8_t enc28j60_cal_spi_cs_hold_time(int clock_speed_mhz)
{
    if (clock_speed_mhz <= 0 || clock_speed_mhz > 20) {
        return 0;
    }
    int temp = clock_speed_mhz * 210;
    uint8_t cs_posttrans = temp / 1000;
    if (temp % 1000) {
        cs_posttrans += 1;
    }

    return cs_posttrans+1;  //<< 
}

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t handle = *(esp_eth_handle_t *)event_data;
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(handle, ETH_CMD_G_MAC_ADDR, mac_addr);

        eth_status = ETH_CONNECTED;
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        eth_status = ETH_DISCONNECTED;
        break;
    case ETHERNET_EVENT_START:
        eth_status = ETH_STARTED;
        break;
    case ETHERNET_EVENT_STOP:
        eth_status = ETH_STOPPED;
        break;
    case IP_EVENT_ETH_GOT_IP:
            eth_status = ETH_GOT_IP;
            break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    // ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    // const esp_netif_ip_info_t *ip_info = &event->ip_info;

    // ESP_LOGI(TAG, "Ethernet Got IP Address");
    // ESP_LOGI(TAG, "~~~~~~~~~~~");
    // ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    // ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    // ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    // ESP_LOGI(TAG, "~~~~~~~~~~~");
    eth_status = ETH_GOT_IP;
}

static mp_obj_t ethernet___init__(void) {
    ethernet_obj_t *self = &ethernet_obj;
    gpio_install_isr_service(0);

    spi_host_id = SPI3_HOST;
    // for (spi_host_device_t host_id = SPI2_HOST; host_id < SOC_SPI_PERIPH_NUM; host_id++) {
    //     if (spi_bus_get_attr(host_id) == NULL) {
    //         spi_host_id = host_id;
    //         break;
    //     }
    // }

    if(spi_host_id == -1){
        mp_printf(MP_PYTHON_PRINTER, "No SPI\n");
        return mp_const_none;
    }

    spi_bus_config_t buscfg = {
        .miso_io_num = 35,
        .mosi_io_num = 36,
        .sclk_io_num = 34,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    //  mp_printf(MP_PYTHON_PRINTER, "SPI\n");
    spi_bus_initialize(spi_host_id, &buscfg, 2);

    spi_device_interface_config_t devcfg = {
        .command_bits = 16, // Actually it's the address phase in W5500 SPI frame
        .address_bits = 8,  // Actually it's the control phase in W5500 SPI frame
        .mode = 0,
        .clock_speed_hz = 8 * 1000 * 1000,
        .spics_io_num = 15,
        .queue_size = 20
    };
        
   
    esp_netif_init();
    esp_event_loop_create_default();
    

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    netif = esp_netif_new(&netif_cfg);

    esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL);

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spi_host_id, &devcfg);
    w5500_config.int_gpio_num = 21;
    w5500_mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    w5500_phy = esp_eth_phy_new_w5500(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(w5500_mac, w5500_phy);
    eth_handle = NULL;
    if(esp_eth_driver_install(&eth_config, &eth_handle) != ESP_OK){
        mp_printf(MP_PYTHON_PRINTER, "Eth adapater not found\n");
        self->eth_handle = NULL;
        self->netif = netif;
        self->mac = w5500_mac;
        self->phy = w5500_phy;
        ethernet_deinit();
        return mp_const_none;
    }
    glue_handle = esp_eth_new_netif_glue(eth_handle);

    unsigned char mac_base[6] = {0};
    esp_read_mac(mac_base, ESP_MAC_ETH);
    w5500_mac->set_addr(w5500_mac, mac_base);

    esp_netif_attach(netif, glue_handle);
    
    esp_netif_set_hostname(netif, "GALAXIA");
    
    self->eth_handle = eth_handle;
    self->netif = netif;
    self->glue_handle = glue_handle;
    self->mac = w5500_mac;
    self->phy = w5500_phy;
   
    /* attach Ethernet driver to TCP/IP stack */
    esp_eth_start(self->eth_handle);
    
    self->active = true;

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(ethernet___init___obj, ethernet___init__);

static mp_obj_t ethernet_active(size_t n_args, const mp_obj_t *args) {
    ethernet_obj_t *self = &ethernet_obj;

    if(spi_host_id == 0){
        return mp_obj_new_bool(false);
    }

    if (n_args > 0) {
        if (mp_obj_is_true(args[0])) {
            esp_netif_set_hostname(self->netif, "GALAXIA");
            self->active = (esp_eth_start(self->eth_handle) == ESP_OK);
            if (!self->active) {
                //mp_raise_msg(&mp_type_OSError, "ethernet enable failed");
            }
        } else {
            self->active = !(esp_eth_stop(self->eth_handle) == ESP_OK);
            if (self->active) {
                //mp_raise_msg(&mp_type_OSError, "ethernet disable failed");
            }
        }
    }

    return mp_obj_new_bool(self->active);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ethernet_active_obj, 0, 1, ethernet_active);

static mp_obj_t ethernet_status(void) {
    return MP_OBJ_NEW_SMALL_INT(eth_status);
}
static MP_DEFINE_CONST_FUN_OBJ_0(ethernet_status_obj, ethernet_status);

static mp_obj_t ethernet_isconnected(void) {
    ethernet_obj_t *self = &ethernet_obj;
    return mp_obj_new_bool(self->active && (eth_status == ETH_GOT_IP));
}
static MP_DEFINE_CONST_FUN_OBJ_0(ethernet_isconnected_obj, ethernet_isconnected);

static mp_obj_t ethernet_ifconfig(size_t n_args, const mp_obj_t *args) {
    ethernet_obj_t *self = &ethernet_obj;

    if(spi_host_id == 0){
        return mp_const_none;
    }

    esp_netif_ip_info_t info;
    esp_netif_dns_info_t dns_info;
    esp_netif_get_ip_info(self->netif, &info);
    esp_netif_get_dns_info(self->netif, ESP_NETIF_DNS_MAIN, &dns_info);
    if (n_args == 0) {
        // get
        mp_obj_t tuple[4] = {
            netutils_format_ipv4_addr((uint8_t *)&info.ip, NETUTILS_BIG),
            netutils_format_ipv4_addr((uint8_t *)&info.netmask, NETUTILS_BIG),
            netutils_format_ipv4_addr((uint8_t *)&info.gw, NETUTILS_BIG),
            netutils_format_ipv4_addr((uint8_t *)&dns_info.ip, NETUTILS_BIG),
        };
        return mp_obj_new_tuple(4, tuple);
    } else {
        // set
        if (mp_obj_is_type(args[0], &mp_type_tuple) || mp_obj_is_type(args[0], &mp_type_list)) {
            mp_obj_t *items;
            mp_obj_get_array_fixed_n(args[0], 4, &items);
            netutils_parse_ipv4_addr(items[0], (void *)&info.ip, NETUTILS_BIG);
            if (mp_obj_is_integer(items[1])) {
                // allow numeric netmask, i.e.:
                // 24 -> 255.255.255.0
                // 16 -> 255.255.0.0
                // etc...
                uint32_t *m = (uint32_t *)&info.netmask;
                *m = esp_netif_htonl(0xffffffff << (32 - mp_obj_get_int(items[1])));
            } else {
                netutils_parse_ipv4_addr(items[1], (void *)&info.netmask, NETUTILS_BIG);
            }
            netutils_parse_ipv4_addr(items[2], (void *)&info.gw, NETUTILS_BIG);
            netutils_parse_ipv4_addr(items[3], (void *)&dns_info.ip, NETUTILS_BIG);
            // To set a static IP we have to disable DHCP first
            esp_err_t e = esp_netif_dhcpc_stop(self->netif);
            if (e != ESP_OK && e != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
                // mp_raise_OSError(MP_ERROR_TEXT("cant stop dhcp"));
            }
            esp_netif_set_ip_info(self->netif, &info);
            esp_netif_set_dns_info(self->netif, ESP_NETIF_DNS_MAIN, &dns_info);
        } else {
            // check for the correct string
            const char *mode = mp_obj_str_get_str(args[0]);
            if (strcmp("dhcp", mode)) {
                // mp_raise_ValueError(MP_ERROR_TEXT("invalid arguments"));
            }
            esp_netif_dhcpc_start(self->netif);
        }
        return mp_const_none;
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ethernet_ifconfig_obj, 0, 1, ethernet_ifconfig);

// static mp_obj_t lan_config(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
//     if (n_args != 1 && kwargs->used != 0) {
//         mp_raise_TypeError(MP_ERROR_TEXT("either pos or kw args are allowed"));
//     }
//     lan_if_obj_t *self = MP_OBJ_TO_PTR(args[0]);

//     if (kwargs->used != 0) {

//         for (size_t i = 0; i < kwargs->alloc; i++) {
//             if (mp_map_slot_is_filled(kwargs, i)) {
//                 switch (mp_obj_str_get_qstr(kwargs->table[i].key)) {
//                     case MP_QSTR_mac: {
//                         mp_buffer_info_t bufinfo;
//                         mp_get_buffer_raise(kwargs->table[i].value, &bufinfo, MP_BUFFER_READ);
//                         if (bufinfo.len != 6) {
//                             mp_raise_ValueError(MP_ERROR_TEXT("invalid buffer length"));
//                         }
//                         if (
//                             (esp_eth_ioctl(self->eth_handle, ETH_CMD_S_MAC_ADDR, bufinfo.buf) != ESP_OK) ||
//                             (esp_netif_set_mac(self->base.netif, bufinfo.buf) != ESP_OK)
//                             ) {
//                             mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("failed setting MAC address"));
//                         }
//                         break;
//                     }
//                     default:
//                         break;
//                 }
//             }
//         }
//         return mp_const_none;
//     }

//     if (n_args != 2) {
//         mp_raise_TypeError(MP_ERROR_TEXT("can query only one param"));
//     }

//     mp_obj_t val = mp_const_none;

//     switch (mp_obj_str_get_qstr(args[1])) {
//         case MP_QSTR_mac: {
//             uint8_t mac[6];
//             esp_eth_ioctl(self->eth_handle, ETH_CMD_G_MAC_ADDR, mac);
//             return mp_obj_new_bytes(mac, sizeof(mac));
//         }
//         case MP_QSTR_ifname: {
//             val = esp_ifname(self->base.netif);
//             break;
//         }
//         default:
//             mp_raise_ValueError(MP_ERROR_TEXT("unknown config param"));
//     }

//     return val;
// }
// static MP_DEFINE_CONST_FUN_OBJ_KW(lan_config_obj, 1, lan_config);

static const mp_map_elem_t ethernet_module_globals_table[] = {
	{ MP_ROM_QSTR(MP_QSTR___name__), 		MP_ROM_QSTR(MP_QSTR_ethernet) },

     // Initialization
    { MP_ROM_QSTR(MP_QSTR___init__),    (mp_obj_t)(&ethernet___init___obj) },
    { MP_ROM_QSTR(MP_QSTR_active), (mp_obj_t)&ethernet_active_obj },
    { MP_ROM_QSTR(MP_QSTR_isconnected), (mp_obj_t)&ethernet_isconnected_obj },
    { MP_ROM_QSTR(MP_QSTR_status), (mp_obj_t)(&ethernet_status_obj) },
    { MP_ROM_QSTR(MP_QSTR_ifconfig), (mp_obj_t)(&ethernet_ifconfig_obj) },
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