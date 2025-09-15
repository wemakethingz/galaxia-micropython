#include "thingz_radio.h"

#include "py/runtime.h"
#include "py/objstr.h"
#include "py/mperrno.h"
#include "py/objint.h"
#include "py/mpz.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_mac.h"

#include <string.h>

#include "modnetwork.h"

typedef struct{
    esp_now_send_status_t status;
    uint8_t mac_addr[6];
} THGZRadioSendEvent_t;

typedef struct{
    uint8_t payload[256];
    uint8_t size;
    uint8_t mac_addr[6];
} THGZRadioReceiveEvent_t;

static const char TAG[] = "THGZ_RADIO";
static StaticQueue_t receive_queue_static;
static StaticQueue_t send_queue_static;
static QueueHandle_t receive_queue;
static QueueHandle_t send_queue;
static uint8_t* receive_queue_data;
static uint8_t* send_queue_data;
static esp_netif_t* netif_interface;

static bool sendSuccess;

static uint8_t thingz_radio_send(const uint8_t* data, size_t len){
    
    size_t send = 0;
    uint8_t packet_send = 0;
    uint8_t packet_confirmed = 0;
    THGZRadioSendEvent_t arg;
    const uint8_t mac[] =  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    xQueueReset(send_queue);
    while(send < len){
        size_t length;
        if(len - send < 251){
            length = len - send;
        }else{
            length = 250;
        }
        esp_err_t err = esp_now_send(mac, data+send, length);
        if(err == ESP_OK){
            ESP_LOGI(TAG, "send success\n");
            packet_send++;
        }else{
            ESP_LOGE(TAG, "send failed: %d\n", err == ESP_ERR_ESPNOW_IF);
            return 1;
        }
        //wait cb
        if(xQueueReceive(send_queue, &arg, 10)){
            ESP_LOGI(TAG, "send cb received\n");
            send+=length;
            if(arg.status == ESP_NOW_SEND_SUCCESS){
                ESP_LOGI(TAG, "send cb success\n");
                packet_confirmed++;
            }else{
                ESP_LOGE(TAG, "send cb failed\n");
                return 1;
            }
        }

    }
    if(packet_send == packet_confirmed){
        ESP_LOGI(TAG, "all packets send successfully\n");
        return 0;
    }
    ESP_LOGE(TAG, "send %d packets confirmed %d\n", packet_send, packet_confirmed);
    return 1;
    
}

static void thgz_radio_printer(void *env, const char *str, size_t len) {
    (void)env;
    if(thingz_radio_send((uint8_t*)str, len) == ESP_OK){
        sendSuccess = true;
    }else{
        sendSuccess = false;
    }
}

const mp_print_t thgz_radio_print = {
    NULL,
    thgz_radio_printer
};

static void esp_now_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len){
    ESP_LOGE(TAG, "receive: len %d data %s", data_len, data);
    THGZRadioReceiveEvent_t receive;
    int i;
    for(i = 0; i < 6; i++){
        receive.mac_addr[i] = info->src_addr[i];
    }
    for(i = 0; i < data_len; i++){
        receive.payload[i] = data[i];
    }
    receive.size = data_len;

    xQueueSend(receive_queue, &receive, 0);

}

static void esp_now_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status){
    THGZRadioSendEvent_t send;
    memcpy(send.mac_addr, mac_addr, 6);
    send.status = status;
    xQueueSend(send_queue, &send, 0);
}

extern bool wifi_started;//defined in network_wlan
void thingz_radio_init(thingz_radio_obj_t* radio){
    int i;
    uint8_t current_channel;
    wifi_second_chan_t second_channel;
    radio->base.type = &thingz_radio_type;

    receive_queue_data = malloc(sizeof(THGZRadioReceiveEvent_t)*20);
    send_queue_data = malloc(sizeof(THGZRadioSendEvent_t)*5);

    //if(!receive_queue)
        receive_queue = xQueueCreateStatic(20, sizeof(THGZRadioReceiveEvent_t), receive_queue_data, &receive_queue_static);
    // else
    //     xQueueReset(receive_queue);

    // if(!send_queue)
        send_queue = xQueueCreateStatic(5, sizeof(THGZRadioSendEvent_t), send_queue_data, &send_queue_static);
    // else
    //     xQueueReset(send_queue);

    // if(!wifi_ever_inited){
        ESP_LOGE(TAG, "netif init %d",esp_netif_init());
        ESP_LOGE(TAG, "loop %d",esp_event_loop_create_default());
        // wifi_ever_inited = true;
    // }

    // netif_interface = esp_netif_create_default_wifi_sta();
    
    esp_initialise_wifi();
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_started = true;
    
    esp_wifi_set_max_tx_power(78);
    esp_wifi_get_channel(&current_channel, &second_channel);
    radio->channel = current_channel;
    
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);

    ESP_LOGE(TAG, "init %d", esp_now_init());
    esp_now_peer_info_t broadcast;
    broadcast.channel = radio->channel;
    broadcast.ifidx = ESP_IF_WIFI_STA;
    broadcast.encrypt = false;
    for(i = 0; i < 6; i++){
        broadcast.peer_addr[i] = 0xff;
    }
    ESP_LOGE(TAG, "add %d", esp_now_add_peer(&broadcast));
    esp_now_register_recv_cb(esp_now_recv_cb);
    esp_now_register_send_cb(esp_now_send_cb); 

    radio->enabled = true;   

}

void thingz_radio_deinit(thingz_radio_obj_t* radio){
    if(!radio->enabled)
        return;
    free(receive_queue_data);
    free(send_queue_data);
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    uint8_t addr[6];
    int i;
    for(i = 0; i < 6; i++){
        addr[i] = 0xff;
    }
    esp_now_del_peer(addr);

    esp_now_deinit();

    // esp_wifi_stop();
    // esp_wifi_deinit();

    // esp_netif_destroy(netif_interface);

    radio->enabled = false;
    // wifi_initialized = 0;

}

static int thingz_radio_set_channel(thingz_radio_obj_t* radio, uint8_t channel){

    if(!radio->enabled){
        mp_raise_ValueError("radio isn't enabled");
    }

    if(channel > 10 || channel < 1){
        mp_raise_ValueError("channel should be between 1 and 10");
        return 1;
    } //TODO throw exception, init channel to 1 add get channel
    ESP_LOGI(TAG, "Setting channel to %d", channel);
    esp_now_peer_info_t *broadcast = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    int i;

    radio->channel = channel;
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_ABOVE);
    broadcast->channel = radio->channel;
    broadcast->ifidx = ESP_IF_WIFI_STA;
    broadcast->encrypt = false;
    for(i = 0; i < 6; i++){
        broadcast->peer_addr[i] = 0xff;
    }
    esp_now_mod_peer(broadcast);
    free(broadcast);
    return 0;
}
//|
//| """ Thingz Radio
//| """
//|
//| class Radio:
//|    """Send and receive messages between boards"""
//|
//|
//NEW
static mp_obj_t mp_thingz_radio_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw,const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_radio_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_radio_obj_t));
    
    self->base.type = &thingz_radio_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_radio_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_radio_del_obj, mp_thingz_radio_del);


//SEND
//|    def send(self, data: str) -> None:
//|        """
//|        Send a message. The message is broadcasted, the board around, if on the same channel, will receive it
//|
//|        :param str data: The data to send
//|        """
//|        ...
//|
static const mp_arg_t mp_thingz_radio_send_args[] = {
    { MP_QSTR_data,      MP_ARG_REQUIRED, {.u_obj = mp_const_none}},
};
#define MP_THINGZ_RADIO_SEND_NUM_ARGS MP_ARRAY_SIZE(mp_thingz_radio_send_args)

static mp_obj_t mp_thingz_radio_send(uint n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    thingz_radio_obj_t *radio = MP_OBJ_TO_PTR(args[0]);

    if(!radio->enabled){
        mp_raise_ValueError("radio isn't enabled");
    }

    // parse args
    mp_arg_val_t vals[MP_THINGZ_RADIO_SEND_NUM_ARGS];
    mp_arg_parse_all(n_args - 1, args + 1, kw_args, MP_THINGZ_RADIO_SEND_NUM_ARGS, mp_thingz_radio_send_args, vals);

    sendSuccess = false;
    mp_obj_print_helper(&thgz_radio_print, vals[0].u_obj, PRINT_STR);
    if(sendSuccess){
        return mp_const_true;
    }else{
        return mp_const_false;
    }
    // mp_obj_type_t* type = mp_obj_get_type(vals[0].u_obj);
    // if( type == &mp_type_str){
    //     GET_STR_DATA_LEN(vals[0].u_obj, str_data, str_len);
    //     if(str_len < 1){
    //         mp_raise_ValueError(translate("arg data: length is zero"));
    //     }else{
    //         if(thingz_radio_send(str_data, str_len)){
    //             return mp_const_true;
    //         }else{
    //             return mp_const_false;
    //         }
    //     }
    // }else if(MP_OBJ_IS_SMALL_INT(vals[0].u_obj)){
    //     char data[512];
    //     sprintf(data, "%d", MP_OBJ_SMALL_INT_VALUE(vals[0].u_obj));
    //     if(thingz_radio_send((uint8_t*) data, strlen(data))){
    //         return mp_const_true;
    //     }else{
    //         return mp_const_false;
    //     }
    // }else if(type == &mp_type_float){
    //     char data[512];
    //     sprintf(data, "%f", (double)mp_obj_get_float(vals[0].u_obj));
    //     if(thingz_radio_send((uint8_t*)data, strlen(data))){
    //         return mp_const_true;
    //     }else{
    //         return mp_const_false;
    //     }
    // }else if(type == &mp_type_int){
    //     mp_uint_t msg;
    //     mpz_t mp = ((mp_obj_int_t*)vals[0].u_obj)->mpz;
    //     if(mpz_as_uint_checked(&mp, &msg)){
    //         char data[512];
    //         sprintf(data, "%d", msg);
    //         if(thingz_radio_send((uint8_t*)data, strlen(data))){
    //             return mp_const_true;
    //         }else{
    //             return mp_const_false;
    //         }
    //     }else{
    //         mp_raise_TypeError(translate("argument 'data': cannot get long"));
    //     }
    // }else if(type == &mp_type_bool){
    //     char data[512];
    //     sprintf(data, "%s", vals[0].u_obj.u_bool ? "True" : "False");
    //     ESP_LOGI(TAG, "str to send: %s", data);
    //     if(thingz_radio_send((uint8_t*)data, strlen(data))){
    //         return mp_const_true;
    //     }else{
    //         return mp_const_false;
    //     }
    // }else{
    //     mp_raise_TypeError(translate("argument 'data': unknown type"));
    // }

    return mp_const_false;
}

static MP_DEFINE_CONST_FUN_OBJ_KW(mp_thingz_radio_send_obj, 1, mp_thingz_radio_send);

//RECEIVE
//|    def receive(self) -> str:
//|        """
//|        Wait for data to be received
//|
//|        :return: The data received
//|        :rtype: str
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_radio_receive(mp_obj_t self_in) {
    THGZRadioReceiveEvent_t arg;
    if(xQueueReceive(receive_queue, &arg, 0)){
        vstr_t vstr;
        if(arg.size == 0)
            return mp_const_none;
    
        mp_obj_t receive = mp_obj_new_list(0, 0);
        mp_obj_list_append(receive, mp_obj_new_bytes(arg.mac_addr, sizeof(arg.mac_addr)));
        
        vstr_init(&vstr, arg.size);
        vstr_add_strn(&vstr, (char*)(arg.payload), arg.size);
        mp_obj_list_append(receive, mp_obj_new_str_from_vstr(&vstr));

        return receive;
    }
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_radio_receive_obj, mp_thingz_radio_receive);

//SET CHANNEL
//|    def set_chanel(self, channel: int) -> None:
//|        """
//|        Change the channel used by the radio module
//|
//|        :param int channel: The channel between 1 and 10
//|        """
//|        ...
//|
static const mp_arg_t mp_thingz_radio_set_channel_args[] = {
    { MP_QSTR_channel,      MP_ARG_REQUIRED | MP_ARG_INT, {.u_obj = mp_const_none}},
};

#define MP_THINGZ_RADIO_SET_CHANNEL_NUM_ARGS MP_ARRAY_SIZE(mp_thingz_radio_set_channel_args)
static mp_obj_t mp_thingz_radio_set_channel(uint n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    thingz_radio_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    
    // parse args
    mp_arg_val_t vals[MP_THINGZ_RADIO_SET_CHANNEL_NUM_ARGS];
    mp_arg_parse_all(n_args - 1, args + 1, kw_args, MP_THINGZ_RADIO_SET_CHANNEL_NUM_ARGS, mp_thingz_radio_set_channel_args, vals);
    // return mp_const_false;
    if(thingz_radio_set_channel(self, vals[0].u_int) == ESP_OK){
        return mp_const_true;
    }

    return mp_const_false;
}

static MP_DEFINE_CONST_FUN_OBJ_KW(mp_thingz_radio_set_channel_obj, 1, mp_thingz_radio_set_channel);


//GET CHANNEL
//|    def get_channel(self) -> int:
//|        """
//|        Get the channel used by the radio module
//|
//|        :return: The channel used by the radio module
//|        :rtype: int
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_radio_get_channel(mp_obj_t self_in) {
	thingz_radio_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
	return mp_obj_new_int(self->channel);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_radio_get_channel_obj, mp_thingz_radio_get_channel);

//GET MAC
//|    def get_mac(self) -> bytes:
//|        """
//|        Get the mac address used by the radio module
//|
//|        :return: The mac address used by the radio module
//|        :rtype: bytes
//|        """
//|        ...
//|
//':'.join('%02x' % b for b in mac_string)
static mp_obj_t mp_thingz_radio_get_mac(mp_obj_t self_in) {
	thingz_radio_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
	return mp_obj_new_bytes(mac, sizeof(mac));
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_radio_get_mac_obj, mp_thingz_radio_get_mac);

static const mp_map_elem_t thingz_radio_local_dict_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__),     (mp_obj_t)&mp_thingz_radio_del_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_send),     (mp_obj_t)&mp_thingz_radio_send_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_receive),     (mp_obj_t)&mp_thingz_radio_receive_obj },   
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_channel),     (mp_obj_t)&mp_thingz_radio_set_channel_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_channel),     (mp_obj_t)&mp_thingz_radio_get_channel_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_mac),     (mp_obj_t)&mp_thingz_radio_get_mac_obj },
};

static MP_DEFINE_CONST_DICT (
	mp_thingz_radio_local_dict,
	thingz_radio_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_radio_type,
    MP_QSTR_Radio,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_radio_make_new,
    locals_dict, &mp_thingz_radio_local_dict
);