// The network part of the ESP-IDF 5.4 API that the firmware uses (thingz_radio,
// ports/esp32/network_wlan.c, network_common.c, modsocket.c), for the WebAssembly
// build: Wi-Fi driver, ESP-NOW, esp_netif, the default event loop, the lwIP
// socket API, mDNS. The values that reach Python (constants, error and reason codes)
// are ESP-IDF's and lwIP's. The behaviour is in net.c.
#ifndef GALAXIA_IDF_NET_H
#define GALAXIA_IDF_NET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/select.h>     // fd_set, FD_SET..., struct timeval: as lwIP with newlib

#define ESP_IDF_VERSION_VAL(major, minor, patch) (((major) << 16) | ((minor) << 8) | (patch))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5, 4, 2)

// ---- esp_interface ----
typedef enum {
    ESP_IF_WIFI_STA = 0,
    ESP_IF_WIFI_AP,
    ESP_IF_WIFI_NAN,
    ESP_IF_ETH,
    ESP_IF_MAX
} esp_interface_t;

// ---- Wi-Fi types (esp_wifi_types_generic.h) ----
#define ESP_ERR_WIFI_BASE           0x3000
#define ESP_ERR_WIFI_NOT_INIT       (ESP_ERR_WIFI_BASE + 1)
#define ESP_ERR_WIFI_NOT_STARTED    (ESP_ERR_WIFI_BASE + 2)
#define ESP_ERR_WIFI_NOT_STOPPED    (ESP_ERR_WIFI_BASE + 3)
#define ESP_ERR_WIFI_IF             (ESP_ERR_WIFI_BASE + 4)
#define ESP_ERR_WIFI_MODE           (ESP_ERR_WIFI_BASE + 5)
#define ESP_ERR_WIFI_STATE          (ESP_ERR_WIFI_BASE + 6)
#define ESP_ERR_WIFI_CONN           (ESP_ERR_WIFI_BASE + 7)
#define ESP_ERR_WIFI_NVS            (ESP_ERR_WIFI_BASE + 8)
#define ESP_ERR_WIFI_MAC            (ESP_ERR_WIFI_BASE + 9)
#define ESP_ERR_WIFI_SSID           (ESP_ERR_WIFI_BASE + 10)
#define ESP_ERR_WIFI_PASSWORD       (ESP_ERR_WIFI_BASE + 11)
#define ESP_ERR_WIFI_TIMEOUT        (ESP_ERR_WIFI_BASE + 12)
#define ESP_ERR_WIFI_WAKE_FAIL      (ESP_ERR_WIFI_BASE + 13)
#define ESP_ERR_WIFI_WOULD_BLOCK    (ESP_ERR_WIFI_BASE + 14)
#define ESP_ERR_WIFI_NOT_CONNECT    (ESP_ERR_WIFI_BASE + 15)

typedef enum {
    WIFI_MODE_NULL = 0,
    WIFI_MODE_STA,
    WIFI_MODE_AP,
    WIFI_MODE_APSTA,
    WIFI_MODE_NAN,
    WIFI_MODE_MAX
} wifi_mode_t;

typedef enum {
    WIFI_IF_STA = ESP_IF_WIFI_STA,
    WIFI_IF_AP = ESP_IF_WIFI_AP,
    WIFI_IF_NAN = ESP_IF_WIFI_NAN,
    WIFI_IF_MAX
} wifi_interface_t;

typedef enum {
    WIFI_AUTH_OPEN = 0,
    WIFI_AUTH_WEP,
    WIFI_AUTH_WPA_PSK,
    WIFI_AUTH_WPA2_PSK,
    WIFI_AUTH_WPA_WPA2_PSK,
    WIFI_AUTH_ENTERPRISE,
    WIFI_AUTH_WPA2_ENTERPRISE = WIFI_AUTH_ENTERPRISE,
    WIFI_AUTH_WPA3_PSK,
    WIFI_AUTH_WPA2_WPA3_PSK,
    WIFI_AUTH_WAPI_PSK,
    WIFI_AUTH_OWE,
    WIFI_AUTH_WPA3_ENT_192,
    WIFI_AUTH_WPA3_EXT_PSK,
    WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE,
    WIFI_AUTH_DPP,
    WIFI_AUTH_WPA3_ENTERPRISE,
    WIFI_AUTH_WPA2_WPA3_ENTERPRISE,
    WIFI_AUTH_WPA_ENTERPRISE,
    WIFI_AUTH_MAX
} wifi_auth_mode_t;

typedef enum {
    WIFI_REASON_UNSPECIFIED = 1,
    WIFI_REASON_AUTH_EXPIRE = 2,
    WIFI_REASON_AUTH_LEAVE = 3,
    WIFI_REASON_ASSOC_EXPIRE = 4,
    WIFI_REASON_ASSOC_TOOMANY = 5,
    WIFI_REASON_ASSOC_LEAVE = 8,
    WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT = 15,
    WIFI_REASON_BEACON_TIMEOUT = 200,
    WIFI_REASON_NO_AP_FOUND = 201,
    WIFI_REASON_AUTH_FAIL = 202,
    WIFI_REASON_ASSOC_FAIL = 203,
    WIFI_REASON_HANDSHAKE_TIMEOUT = 204,
    WIFI_REASON_CONNECTION_FAIL = 205,
    WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY = 210,
    WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD = 211,
    WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD = 212,
} wifi_err_reason_t;

typedef enum { WIFI_SECOND_CHAN_NONE = 0, WIFI_SECOND_CHAN_ABOVE, WIFI_SECOND_CHAN_BELOW } wifi_second_chan_t;
typedef enum { WIFI_PS_NONE, WIFI_PS_MIN_MODEM, WIFI_PS_MAX_MODEM } wifi_ps_type_t;
typedef enum { WIFI_STORAGE_FLASH, WIFI_STORAGE_RAM } wifi_storage_t;

#define WIFI_PROTOCOL_11B 0x1
#define WIFI_PROTOCOL_11G 0x2
#define WIFI_PROTOCOL_11N 0x4
#define WIFI_PROTOCOL_LR  0x8

typedef struct {
    uint8_t ssid[32];
    uint8_t password[64];
    uint8_t ssid_len;
    uint8_t channel;
    wifi_auth_mode_t authmode;
    uint8_t ssid_hidden;
    uint8_t max_connection;
    uint16_t beacon_interval;
} wifi_ap_config_t;

typedef struct {
    uint8_t ssid[32];
    uint8_t password[64];
    int scan_method;
    bool bssid_set;
    uint8_t bssid[6];
    uint8_t channel;
} wifi_sta_config_t;

typedef union {
    wifi_ap_config_t ap;
    wifi_sta_config_t sta;
} wifi_config_t;

typedef struct {
    uint8_t bssid[6];
    uint8_t ssid[33];
    uint8_t primary;
    wifi_second_chan_t second;
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_ap_record_t;

typedef struct {
    uint8_t *ssid;
    uint8_t *bssid;
    uint8_t channel;
    bool show_hidden;
} wifi_scan_config_t;

#define ESP_WIFI_MAX_CONN_NUM 15
typedef struct {
    uint8_t mac[6];
    int8_t rssi;
} wifi_sta_info_t;
typedef struct {
    wifi_sta_info_t sta[ESP_WIFI_MAX_CONN_NUM];
    int num;
} wifi_sta_list_t;

typedef struct {
    int dummy;
} wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() { 0 }

esp_err_t esp_wifi_init(const wifi_init_config_t *config);
esp_err_t esp_wifi_deinit(void);
esp_err_t esp_wifi_set_storage(wifi_storage_t storage);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_set_mode(wifi_mode_t mode);
esp_err_t esp_wifi_get_mode(wifi_mode_t *mode);
esp_err_t esp_wifi_connect(void);
esp_err_t esp_wifi_disconnect(void);
esp_err_t esp_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf);
esp_err_t esp_wifi_get_config(wifi_interface_t interface, wifi_config_t *conf);
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config, bool block);
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *number);
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records);
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info);
esp_err_t esp_wifi_ap_get_sta_list(wifi_sta_list_t *sta);
esp_err_t esp_wifi_set_mac(wifi_interface_t ifx, const uint8_t mac[6]);
esp_err_t esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6]);
esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second);
esp_err_t esp_wifi_get_channel(uint8_t *primary, wifi_second_chan_t *second);
esp_err_t esp_wifi_set_max_tx_power(int8_t power);
esp_err_t esp_wifi_get_max_tx_power(int8_t *power);
esp_err_t esp_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap);
esp_err_t esp_wifi_get_protocol(wifi_interface_t ifx, uint8_t *protocol_bitmap);
esp_err_t esp_wifi_set_ps(wifi_ps_type_t type);
esp_err_t esp_wifi_get_ps(wifi_ps_type_t *type);

// ---- MAC ----
typedef enum { ESP_MAC_WIFI_STA, ESP_MAC_WIFI_SOFTAP, ESP_MAC_BT, ESP_MAC_ETH } esp_mac_type_t;
esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type);

// ---- ESP-NOW ----
#define ESP_ERR_ESPNOW_BASE         (ESP_ERR_WIFI_BASE + 100)
#define ESP_ERR_ESPNOW_NOT_INIT     (ESP_ERR_ESPNOW_BASE + 1)
#define ESP_ERR_ESPNOW_ARG          (ESP_ERR_ESPNOW_BASE + 2)
#define ESP_ERR_ESPNOW_NO_MEM       (ESP_ERR_ESPNOW_BASE + 3)
#define ESP_ERR_ESPNOW_FULL         (ESP_ERR_ESPNOW_BASE + 4)
#define ESP_ERR_ESPNOW_NOT_FOUND    (ESP_ERR_ESPNOW_BASE + 5)
#define ESP_ERR_ESPNOW_INTERNAL     (ESP_ERR_ESPNOW_BASE + 6)
#define ESP_ERR_ESPNOW_EXIST        (ESP_ERR_ESPNOW_BASE + 7)
#define ESP_ERR_ESPNOW_IF           (ESP_ERR_ESPNOW_BASE + 8)
#define ESP_ERR_ESPNOW_CHAN         (ESP_ERR_ESPNOW_BASE + 9)
#define ESP_NOW_ETH_ALEN 6
#define ESP_NOW_KEY_LEN 16
typedef enum { ESP_NOW_SEND_SUCCESS = 0, ESP_NOW_SEND_FAIL } esp_now_send_status_t;
typedef struct { int rssi; int channel; } wifi_pkt_rx_ctrl_t;
typedef struct {
    uint8_t *src_addr;
    uint8_t *des_addr;
    wifi_pkt_rx_ctrl_t *rx_ctrl;
} esp_now_recv_info_t;
typedef struct {
    uint8_t peer_addr[ESP_NOW_ETH_ALEN];
    uint8_t lmk[ESP_NOW_KEY_LEN];
    uint8_t channel;
    wifi_interface_t ifidx;
    bool encrypt;
    void *priv;
} esp_now_peer_info_t;
typedef void (*esp_now_recv_cb_t)(const esp_now_recv_info_t *info, const uint8_t *data, int len);
typedef void (*esp_now_send_cb_t)(const uint8_t *mac_addr, esp_now_send_status_t status);
esp_err_t esp_now_init(void);
esp_err_t esp_now_deinit(void);
esp_err_t esp_now_add_peer(const esp_now_peer_info_t *peer);
esp_err_t esp_now_mod_peer(const esp_now_peer_info_t *peer);
esp_err_t esp_now_del_peer(const uint8_t *peer_addr);
esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t cb);
esp_err_t esp_now_unregister_recv_cb(void);
esp_err_t esp_now_register_send_cb(esp_now_send_cb_t cb);
esp_err_t esp_now_unregister_send_cb(void);
esp_err_t esp_now_send(const uint8_t *peer_addr, const uint8_t *data, size_t len);

// ---- default event loop ----
typedef const char *esp_event_base_t;
typedef void *esp_event_handler_instance_t;
typedef void (*esp_event_handler_t)(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
extern const char WIFI_EVENT[];
extern const char IP_EVENT[];
#define ESP_EVENT_ANY_ID -1
typedef enum {
    WIFI_EVENT_WIFI_READY = 0,
    WIFI_EVENT_SCAN_DONE,
    WIFI_EVENT_STA_START,
    WIFI_EVENT_STA_STOP,
    WIFI_EVENT_STA_CONNECTED,
    WIFI_EVENT_STA_DISCONNECTED,
    WIFI_EVENT_STA_AUTHMODE_CHANGE,
    WIFI_EVENT_STA_WPS_ER_SUCCESS,
    WIFI_EVENT_STA_WPS_ER_FAILED,
    WIFI_EVENT_STA_WPS_ER_TIMEOUT,
    WIFI_EVENT_STA_WPS_ER_PIN,
    WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP,
    WIFI_EVENT_AP_START,
    WIFI_EVENT_AP_STOP,
    WIFI_EVENT_AP_STACONNECTED,
    WIFI_EVENT_AP_STADISCONNECTED,
} wifi_event_t;
typedef enum {
    IP_EVENT_STA_GOT_IP,
    IP_EVENT_STA_LOST_IP,
    IP_EVENT_AP_STAIPASSIGNED,
} ip_event_t;
typedef struct {
    uint8_t ssid[32];
    uint8_t ssid_len;
    uint8_t bssid[6];
    uint8_t reason;
    int8_t rssi;
} wifi_event_sta_disconnected_t;
esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_handler_instance_register(esp_event_base_t event_base, int32_t event_id,
    esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *instance);

// ---- esp_netif ----
#define ESP_ERR_ESP_NETIF_BASE                  0x5000
#define ESP_ERR_ESP_NETIF_INVALID_PARAMS        (ESP_ERR_ESP_NETIF_BASE + 0x01)
#define ESP_ERR_ESP_NETIF_IF_NOT_READY          (ESP_ERR_ESP_NETIF_BASE + 0x02)
#define ESP_ERR_ESP_NETIF_DHCPC_START_FAILED    (ESP_ERR_ESP_NETIF_BASE + 0x03)
#define ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED  (ESP_ERR_ESP_NETIF_BASE + 0x04)
#define ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED  (ESP_ERR_ESP_NETIF_BASE + 0x05)
#define ESP_ERR_ESP_NETIF_DHCP_NOT_STOPPED      (ESP_ERR_ESP_NETIF_BASE + 0x07)

typedef struct galaxia_netif esp_netif_t;
typedef struct { uint32_t addr; } esp_ip4_addr_t;
typedef struct { uint32_t addr[4]; uint8_t zone; } esp_ip6_addr_t;
typedef struct {
    union {
        esp_ip4_addr_t ip4;
        esp_ip6_addr_t ip6;
    } u_addr;
    uint8_t type;
} esp_ip_addr_t;
typedef struct {
    esp_ip4_addr_t ip;
    esp_ip4_addr_t netmask;
    esp_ip4_addr_t gw;
} esp_netif_ip_info_t;
typedef enum { ESP_NETIF_DNS_MAIN = 0, ESP_NETIF_DNS_BACKUP, ESP_NETIF_DNS_FALLBACK, ESP_NETIF_DNS_MAX } esp_netif_dns_type_t;
typedef struct { esp_ip_addr_t ip; } esp_netif_dns_info_t;
typedef enum { ESP_NETIF_DHCP_INIT = 0, ESP_NETIF_DHCP_STARTED, ESP_NETIF_DHCP_STOPPED } esp_netif_dhcp_status_t;

esp_err_t esp_netif_init(void);
esp_netif_t *esp_netif_create_default_wifi_sta(void);
esp_netif_t *esp_netif_create_default_wifi_ap(void);
void esp_netif_destroy(esp_netif_t *netif);
esp_err_t esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *ip_info);
esp_err_t esp_netif_set_ip_info(esp_netif_t *netif, const esp_netif_ip_info_t *ip_info);
esp_err_t esp_netif_get_dns_info(esp_netif_t *netif, esp_netif_dns_type_t type, esp_netif_dns_info_t *dns);
esp_err_t esp_netif_set_dns_info(esp_netif_t *netif, esp_netif_dns_type_t type, esp_netif_dns_info_t *dns);
esp_err_t esp_netif_dhcpc_start(esp_netif_t *netif);
esp_err_t esp_netif_dhcpc_stop(esp_netif_t *netif);
esp_err_t esp_netif_dhcpc_get_status(esp_netif_t *netif, esp_netif_dhcp_status_t *status);
esp_err_t esp_netif_dhcps_start(esp_netif_t *netif);
esp_err_t esp_netif_dhcps_stop(esp_netif_t *netif);
esp_err_t esp_netif_set_hostname(esp_netif_t *netif, const char *hostname);
esp_err_t esp_netif_get_netif_impl_name(esp_netif_t *netif, char *name);
uint32_t esp_netif_htonl(uint32_t x);

// ---- lwIP (ESP-IDF port: lwipopts.h, sdkconfig of the Galaxia) ----
typedef uint8_t u8_t;
typedef int8_t s8_t;
typedef uint16_t u16_t;
typedef int16_t s16_t;
typedef uint32_t u32_t;
typedef int32_t s32_t;
typedef s8_t err_t;
#define ERR_OK 0
#define ERR_VAL -6
#define ERR_ARG -16

#define CONFIG_LWIP_MAX_SOCKETS 6
// lwIP gives the sockets the last descriptors below newlib's FD_SETSIZE (64): 58-63
#define LWIP_SOCKET_OFFSET (64 - CONFIG_LWIP_MAX_SOCKETS)
#define NETIF_NAMESIZE 6

typedef struct ip4_addr { u32_t addr; } ip4_addr_t;
typedef struct {
    union { ip4_addr_t ip4; u32_t ip6[4]; } u_addr;
    u8_t type;
} ip_addr_t;
#define IPADDR_TYPE_V4 0
#define IPADDR_STRLEN_MAX 46
char *ip4addr_ntoa_r(const ip4_addr_t *addr, char *buf, int buflen);
char *ipaddr_ntoa_r(const ip_addr_t *addr, char *buf, int buflen);
int ipaddr_aton(const char *cp, ip_addr_t *addr);
const ip_addr_t *dns_getserver(u8_t numdns);
void dns_setserver(u8_t numdns, const ip_addr_t *dnsserver);
err_t igmp_joingroup(const ip4_addr_t *ifaddr, const ip4_addr_t *groupaddr);

typedef u8_t sa_family_t;
typedef u16_t in_port_t;
typedef u32_t in_addr_t;
typedef u32_t socklen_t;
struct in_addr { in_addr_t s_addr; };
struct sockaddr {
    u8_t sa_len;
    sa_family_t sa_family;
    char sa_data[14];
};
struct sockaddr_in {
    u8_t sin_len;
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};
struct sockaddr_storage {
    u8_t s2_len;
    sa_family_t ss_family;
    char s2_data1[2];
    u32_t s2_data2[3];
    u32_t s2_data3[3];
};
#define inet_addr_from_ip4addr(target_inaddr, source_ipaddr) ((target_inaddr)->s_addr = (source_ipaddr)->addr)

#define AF_UNSPEC       0
#define AF_INET         2
#define AF_INET6        10
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3
#define IPPROTO_IP      0
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define SOL_SOCKET      0xfff
#define SO_REUSEADDR    0x0004
#define SO_BROADCAST    0x0020
#define SO_SNDTIMEO     0x1005
#define SO_RCVTIMEO     0x1006
#define SO_ERROR        0x1007
#define SO_BINDTODEVICE 0x100b
#define TCP_NODELAY     0x01
#define IP_ADD_MEMBERSHIP 3
#define F_GETFL 3
#define F_SETFL 4
#undef O_NONBLOCK
#define O_NONBLOCK 1

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};
#define AI_PASSIVE      0x01
#define AI_CANONNAME    0x02
#define AI_NUMERICHOST  0x04
#define EAI_NONAME      200
#define EAI_SERVICE     201
#define EAI_FAIL        202
#define EAI_MEMORY      203
#define EAI_FAMILY      204
#define DNS_MAX_NAME_LENGTH 256
#define MEMP_NETDB 0
#define NETDB_ELEM_SIZE (sizeof(struct addrinfo) + sizeof(struct sockaddr_storage) + DNS_MAX_NAME_LENGTH + 1)
void *memp_malloc(int type);

u16_t lwip_htons(u16_t x);
u32_t lwip_htonl(u32_t x);
#define lwip_ntohs(x) lwip_htons(x)
#define lwip_ntohl(x) lwip_htonl(x)
#undef htons
#undef ntohs
#undef htonl
#undef ntohl
#define htons(x) lwip_htons(x)
#define ntohs(x) lwip_ntohs(x)
#define htonl(x) lwip_htonl(x)
#define ntohl(x) lwip_ntohl(x)

int lwip_socket(int domain, int type, int protocol);
int lwip_bind(int s, const struct sockaddr *name, socklen_t namelen);
int lwip_listen(int s, int backlog);
int lwip_accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int lwip_connect(int s, const struct sockaddr *name, socklen_t namelen);
int lwip_close(int s);
int lwip_recvfrom(int s, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
int lwip_write(int s, const void *dataptr, size_t size);
int lwip_sendto(int s, const void *dataptr, size_t size, int flags, const struct sockaddr *to, socklen_t tolen);
int lwip_setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen);
int lwip_getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen);
int lwip_fcntl(int s, int cmd, int val);
int lwip_fcntl_va(int s, int cmd, int val, ...);
int lwip_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout);
int lwip_getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res);
void lwip_freeaddrinfo(struct addrinfo *ai);

// ---- mDNS (espressif/mdns component) ----
esp_err_t mdns_init(void);
esp_err_t mdns_hostname_set(const char *hostname);
esp_err_t mdns_instance_name_set(const char *instance_name);
esp_err_t mdns_query_a(const char *host_name, uint32_t timeout, esp_ip4_addr_t *addr);

#endif
