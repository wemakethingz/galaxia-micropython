// The Galaxia's Wi-Fi chip and its network stack, for the WebAssembly build: what
// ports/esp32/network_wlan.c, network_common.c, modsocket.c and thingz_radio call
// (ESP-IDF 5.4 Wi-Fi driver, ESP-NOW, esp_netif, default event loop, lwIP sockets,
// mDNS). Those files run unchanged on top; this one plays the chip and lwIP.
//
// The network is made of the boards open in the same browser. Their frames go
// through a BroadcastChannel (library.js, web/worker.js); each board keeps its own
// state, as real stations do:
//   - access points: the page's (the classroom router: SSID, password, channel set
//     in the page; no Internet behind it) and the boards in soft-AP mode, which send
//     beacons, answer probes, associate stations and run a DHCP server;
//   - a station picks its address on the page's network itself (an ARP probe
//     avoids the addresses in use), as if the router's DHCP server gave it;
//   - IP frames carry TCP and UDP: connections with flow control (lwIP's window and
//     send buffer), RST, FIN, TIME_WAIT; datagrams, broadcast, multicast;
//   - mDNS: "name.local" is answered by the board whose hostname it is;
//   - the page's router has an Internet made of one host: the virtual MQTT broker
//     (web/broker.js), whose names its DNS resolves.
// The values the programs see (errno, Wi-Fi reason codes, constants, socket file
// descriptors, default addresses, buffer sizes) are those of the board: ESP-IDF 5.4
// and the Galaxia's sdkconfig. The timings are approximations (README.md).

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>

#include "idf/idf_all.h"
#include "net.h"

extern void galaxia_js_radio_send(int channel, const uint8_t *mac, const uint8_t *data, int len);
extern int galaxia_js_radio_next(uint8_t *data, int max, uint8_t *mac, int *channel);
extern void galaxia_js_mac(uint8_t *mac);
extern void galaxia_js_lan_send(const uint8_t *frame, int len);
extern int galaxia_js_lan_next(uint8_t *frame, int max);
extern int galaxia_js_page_ap(int index, char *ssid, char *password, int *authmode, int *channel);
extern uint32_t galaxia_js_dns(const char *name);
extern void galaxia_thread_sleep_ms(int ms);   // the other threads run meanwhile
extern void galaxia_poll(void);
extern uint32_t mp_js_random_u32(void);

#define MP_ARRAY_SIZE_(a) (sizeof(a) / sizeof((a)[0]))
#define NOW() esp_timer_get_time()
#define MS(x) ((int64_t)(x) * 1000)

// ---- timings (approximations of the chip's, see README.md) ----
#define SCAN_MS                 1600    // esp_wifi_scan_start: 13 channels, active scan
#define SCAN_CONNECTED_MS       2500    // the same, back on the home channel between channels
#define CONNECT_SCAN_MS         2000    // esp_wifi_connect: the scan for the SSID
#define AUTH_MS                 300     // authentication and association
#define HANDSHAKE_FAIL_MS       3000    // wrong key: the 4-way handshake times out
#define DHCP_MS                 700     // DHCP exchange
#define ARP_PROBE_MS            300
#define BEACON_PERIOD_MS        1000    // soft-AP beacons (sent less often than the chip's)
#define BEACON_TIMEOUT_MS       6000    // station: no beacon from its AP
#define AP_INACTIVE_MS          300000  // soft-AP: station silent for 5 minutes
#define KEEPALIVE_MS            15000   // station -> soft-AP null data
#define IP_LOST_MS              (CONFIG_ESP_NETIF_IP_LOST_TIMER_INTERVAL * 1000)
#define DNS_TIMEOUT_MS          7000    // lwIP: DNS_MAX_RETRIES 4, timer 1+1+2+3 s
#define DNS_ANSWER_MS           30      // the router's DNS answers
#define TCP_SYN_TIMEOUT_MS      20000   // no answer to the SYN: the connection is aborted
#define CONFIG_ESP_NETIF_IP_LOST_TIMER_INTERVAL 120

// ---- lwIP sizes (sdkconfig of the Galaxia) ----
#define TCP_WND         5760    // CONFIG_LWIP_TCP_WND_DEFAULT
#define TCP_SND_BUF     5760    // CONFIG_LWIP_TCP_SND_BUF_DEFAULT
#define TCP_MSS         1440    // CONFIG_LWIP_TCP_MSS
#define TCP_MSL_MS      60000   // CONFIG_LWIP_TCP_MSL: TIME_WAIT lasts 2 MSL
#define TCP_ACCEPTMBOX  6       // CONFIG_LWIP_TCP_ACCEPTMBOX_SIZE
#define UDP_RECVMBOX    6       // CONFIG_LWIP_UDP_RECVMBOX_SIZE
#define MAX_PCBS        16      // CONFIG_LWIP_MAX_ACTIVE_TCP 12 + the pending ones
#define MAX_TIME_WAIT   16
#define UDP_MAX         65507
#define DHCPS_MAX       8       // CONFIG_LWIP_DHCPS_MAX_STATION_NUM

#define IP4(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define IP_ANY          0
#define IP_BROADCAST    0xffffffff
#define IS_LOOPBACK(ip) (((ip) & 0xff) == 127)
#define IS_MULTICAST(ip) (((ip) & 0xf0) == 0xe0)

static const uint8_t BCAST_MAC[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

const char WIFI_EVENT[] = "WIFI_EVENT";
const char IP_EVENT[] = "IP_EVENT";

// ================================================================= frames

enum {
    F_PROBE_REQ = 1, F_BEACON, F_ASSOC_REQ, F_ASSOC_RESP, F_DEAUTH, F_KEEPALIVE,
    F_DHCP_REQ, F_DHCP_ACK, F_ARP_PROBE, F_ARP_REPLY, F_IP, F_MDNS_Q, F_MDNS_A,
};

// All the boards run this code (same layout, same byte order): the frames are
// these structures as they are in memory
typedef struct __attribute__((packed)) {
    uint8_t kind;
    uint8_t src[6];
    uint8_t dst[6];
    uint8_t bssid[6];
} frame_hdr_t;

typedef struct __attribute__((packed)) {
    uint8_t ssid[32];
    uint8_t ssid_len;
    uint8_t channel;
    uint8_t authmode;
    uint8_t hidden;
} f_beacon_t;

typedef struct __attribute__((packed)) {
    uint32_t ip, mask, gw, dns;
} f_dhcp_t;

typedef struct __attribute__((packed)) {
    uint32_t src, dst;
    uint8_t proto;
} f_ip_t;

enum { TCP_SYN = 1, TCP_SYNACK = 2, TCP_FIN = 4, TCP_RST = 8, TCP_DATA = 16, TCP_WIN = 32 };

typedef struct __attribute__((packed)) {
    uint16_t sport, dport;
    uint8_t flags;
    uint16_t n;             // DATA: length; SYNACK, WIN: window given to the peer
} f_tcp_t;

typedef struct __attribute__((packed)) {
    uint16_t sport, dport;
    uint16_t len;
} f_udp_t;

typedef struct __attribute__((packed)) {
    char name[64];
    uint32_t ip;
} f_mdns_t;

#define FRAME_MAX (sizeof(frame_hdr_t) + sizeof(f_ip_t) + sizeof(f_udp_t) + UDP_MAX)
static uint8_t tx_frame[FRAME_MAX];
static uint8_t rx_frame[FRAME_MAX];

static void lan_send(int kind, const uint8_t *src, const uint8_t *dst, const uint8_t *bssid,
    const void *body, size_t body_len, const void *data, size_t data_len) {
    frame_hdr_t *h = (frame_hdr_t *)tx_frame;
    h->kind = kind;
    memcpy(h->src, src, 6);
    memcpy(h->dst, dst, 6);
    memcpy(h->bssid, bssid ? bssid : BCAST_MAC, 6);
    memcpy(tx_frame + sizeof(*h), body, body_len);
    if (data_len) {
        memcpy(tx_frame + sizeof(*h) + body_len, data, data_len);
    }
    galaxia_js_lan_send(tx_frame, sizeof(*h) + body_len + data_len);
}

// ================================================================= events

static struct {
    esp_event_base_t base;
    int32_t id;
    esp_event_handler_t fn;
    void *arg;
} handlers[8];

#define EVENT_QUEUE 16
static struct {
    esp_event_base_t base;
    int32_t id;
    bool has_data;
    uint8_t data[sizeof(wifi_event_sta_disconnected_t)];
} events[EVENT_QUEUE];
static int events_head, events_len;

esp_err_t esp_event_loop_create_default(void) {
    return ESP_OK;
}

esp_err_t esp_event_handler_instance_register(esp_event_base_t event_base, int32_t event_id,
    esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *instance) {
    for (size_t i = 0; i < MP_ARRAY_SIZE_(handlers); i++) {
        if (!handlers[i].fn) {
            handlers[i].base = event_base;
            handlers[i].id = event_id;
            handlers[i].fn = event_handler;
            handlers[i].arg = event_handler_arg;
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

// As the event task: the handlers run later, from net_poll()
static void post_event(esp_event_base_t base, int32_t id, const void *data, size_t len) {
    if (events_len == EVENT_QUEUE) {
        return;
    }
    int i = (events_head + events_len++) % EVENT_QUEUE;
    events[i].base = base;
    events[i].id = id;
    events[i].has_data = data != NULL;
    if (data) {
        memcpy(events[i].data, data, len);
    }
}

static void run_events(void) {
    while (events_len) {
        int i = events_head;
        events_head = (events_head + 1) % EVENT_QUEUE;
        events_len--;
        uint8_t data[sizeof(events[0].data)];
        memcpy(data, events[i].data, sizeof(data));
        esp_event_base_t base = events[i].base;
        int32_t id = events[i].id;
        bool has_data = events[i].has_data;
        for (size_t h = 0; h < MP_ARRAY_SIZE_(handlers); h++) {
            if (handlers[h].fn && handlers[h].base == base && (handlers[h].id == ESP_EVENT_ANY_ID || handlers[h].id == id)) {
                handlers[h].fn(handlers[h].arg, base, id, has_data ? data : NULL);
            }
        }
    }
}

// ================================================================= netifs

enum { NETIF_STA = 0, NETIF_AP = 1, NETIF_LOOP = 2, NETIF_NONE = -1 };

struct galaxia_netif {
    int id;
    esp_netif_ip_info_t ip;
    esp_ip4_addr_t dns;                 // AP: the DNS server its DHCP server offers
    esp_netif_dhcp_status_t dhcp;       // STA: client, AP: server
    char hostname[33];
};
static struct galaxia_netif netifs[2] = {
    { .id = NETIF_STA },
    { .id = NETIF_AP },
};
static ip_addr_t dns_servers[3];        // lwIP's (CONFIG_LWIP_DNS_MAX_SERVERS)

// ================================================================= Wi-Fi chip

typedef struct {
    uint8_t bssid[6];
    char ssid[33];
    uint8_t ssid_len;
    uint8_t channel;
    uint8_t authmode;
    bool hidden;
    bool page;              // the page's access point (else a board in soft-AP mode)
    char password[65];      // page AP: its key, checked by the station
    int8_t rssi;
    int64_t seen;           // board AP: last beacon or probe response
} bss_t;

enum { STA_IDLE, STA_SCANNING, STA_AUTH, STA_DHCP, STA_UP };

static struct {
    bool inited, started;
    wifi_mode_t mode;
    uint8_t channel;                    // home channel when not associated
    int8_t tx_power;
    uint8_t protocol[2];
    wifi_ps_type_t ps;
    wifi_config_t cfg[2];               // STA, AP
    uint8_t mac[2][6];
    // station
    int sta_state;
    int64_t sta_due;
    bss_t bss;                          // the AP it connects or is connected to
    int64_t last_beacon, next_keepalive;
    bool assoc_answer, auth_failed;
    uint8_t assoc_status;
    bool dhcp_answer;
    f_dhcp_t dhcp_offer;
    uint32_t candidate;
    int candidate_try;
    bool candidate_taken;
    int64_t ip_lost_at;                 // the IP kept after a disconnection, until then
    // scan
    bool scanning;
    wifi_ap_record_t scan[32];
    uint16_t nscan;
    // soft-AP
    bool ap_up;
    int64_t next_beacon;
    struct {
        uint8_t mac[6];
        uint32_t ip;
        int64_t seen;
    } stations[ESP_WIFI_MAX_CONN_NUM];
    int nstations;
    uint8_t leases[DHCPS_MAX][6];       // DHCP server: 192.168.4.2 + i
} wifi = {
    .mode = WIFI_MODE_STA,
    .channel = 1,
    .tx_power = 80,
    .protocol = { WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N },
    .ps = WIFI_PS_MIN_MODEM,
};

// The access points of other boards heard lately
#define SEEN_MAX 16
static bss_t seen[SEEN_MAX];

static bool mac_eq(const uint8_t *a, const uint8_t *b) {
    return memcmp(a, b, 6) == 0;
}

static bool sta_associated(void) {
    return wifi.sta_state == STA_DHCP || wifi.sta_state == STA_UP;
}

static bool netif_up(int n) {
    if (n == NETIF_STA) {
        return wifi.sta_state == STA_UP;
    }
    return wifi.ap_up;
}

static uint8_t home_channel(void) {
    if (sta_associated()) {
        return wifi.bss.channel;
    }
    if (wifi.ap_up) {
        return wifi.cfg[WIFI_IF_AP].ap.channel;
    }
    return wifi.channel;
}

// A page access point: SSID, key, security and channel set in the page; its BSSID
// is made from its SSID
static bool page_ap(int index, bss_t *b) {
    char ssid[33] = { 0 }, password[65] = { 0 };
    int authmode = 0, channel = 1;
    if (!galaxia_js_page_ap(index, ssid, password, &authmode, &channel)) {
        return false;
    }
    memset(b, 0, sizeof(*b));
    strncpy(b->ssid, ssid, 32);
    b->ssid_len = strlen(b->ssid);
    strncpy(b->password, password, 64);
    b->authmode = authmode;
    b->channel = channel;
    b->page = true;
    b->rssi = -45 - (int)(mp_js_random_u32() % 6);
    uint32_t h = 2166136261u;
    for (const char *p = ssid; *p; p++) {
        h = (h ^ (uint8_t)*p) * 16777619u;
    }
    b->bssid[0] = 0x02;
    b->bssid[1] = 0x47;     // 'G'
    memcpy(b->bssid + 2, &h, 4);
    return true;
}

static bool page_ap_by_bssid(const uint8_t *bssid, bss_t *b) {
    for (int i = 0; page_ap(i, b); i++) {
        if (mac_eq(b->bssid, bssid)) {
            return true;
        }
    }
    return false;
}

static void sta_set_ip(uint32_t ip, uint32_t mask, uint32_t gw) {
    netifs[NETIF_STA].ip.ip.addr = ip;
    netifs[NETIF_STA].ip.netmask.addr = mask;
    netifs[NETIF_STA].ip.gw.addr = gw;
}

static void sta_got_ip(void) {
    wifi.sta_state = STA_UP;
    wifi.ip_lost_at = 0;
    post_event(IP_EVENT, IP_EVENT_STA_GOT_IP, NULL, 0);
}

static void sockets_netif_down(int n);

static void sta_start_dhcp(void);

static void sta_disconnected(uint8_t reason) {
    bool was_up = wifi.sta_state == STA_UP;
    if (sta_associated() && !wifi.bss.page) {
        lan_send(F_DEAUTH, wifi.mac[WIFI_IF_STA], wifi.bss.bssid, wifi.bss.bssid, &reason, 1, NULL, 0);
    }
    wifi.sta_state = STA_IDLE;
    if (was_up) {
        sockets_netif_down(NETIF_STA);
        if (netifs[NETIF_STA].dhcp != ESP_NETIF_DHCP_STOPPED) {
            wifi.ip_lost_at = NOW() + MS(IP_LOST_MS);     // as esp_netif: the IP is kept a while
        }
    }
    wifi_event_sta_disconnected_t ev = { 0 };
    memcpy(ev.ssid, wifi.bss.ssid, 32);
    ev.ssid_len = wifi.bss.ssid_len;
    memcpy(ev.bssid, wifi.bss.bssid, 6);
    ev.reason = reason;
    ev.rssi = wifi.bss.rssi;
    post_event(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &ev, sizeof(ev));
}

// A beacon, or a probe response: a hidden AP only gives its SSID to a probe for it
static void send_beacon(const uint8_t *dst, bool reveal) {
    const wifi_ap_config_t *ap = &wifi.cfg[WIFI_IF_AP].ap;
    f_beacon_t b = { 0 };
    if (!ap->ssid_hidden || reveal) {
        memcpy(b.ssid, ap->ssid, 32);
        b.ssid_len = ap->ssid_len;
    }
    b.channel = home_channel();
    b.authmode = ap->authmode;
    b.hidden = ap->ssid_hidden;
    lan_send(F_BEACON, wifi.mac[WIFI_IF_AP], dst, wifi.mac[WIFI_IF_AP], &b, sizeof(b), NULL, 0);
}

static void ap_remove_station(int i, bool notify, uint8_t reason) {
    if (notify) {
        lan_send(F_DEAUTH, wifi.mac[WIFI_IF_AP], wifi.stations[i].mac, wifi.mac[WIFI_IF_AP], &reason, 1, NULL, 0);
    }
    wifi.stations[i] = wifi.stations[--wifi.nstations];
    post_event(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, NULL, 0);
}

static void ap_start(void) {
    if (wifi.ap_up) {
        return;
    }
    wifi.ap_up = true;
    wifi.next_beacon = 0;
    if (netifs[NETIF_AP].dhcp == ESP_NETIF_DHCP_INIT) {
        netifs[NETIF_AP].dhcp = ESP_NETIF_DHCP_STARTED;
    }
    post_event(WIFI_EVENT, WIFI_EVENT_AP_START, NULL, 0);
}

static void ap_stop(void) {
    if (!wifi.ap_up) {
        return;
    }
    while (wifi.nstations) {
        ap_remove_station(0, true, WIFI_REASON_AUTH_LEAVE);
    }
    wifi.ap_up = false;
    sockets_netif_down(NETIF_AP);
    post_event(WIFI_EVENT, WIFI_EVENT_AP_STOP, NULL, 0);
}

esp_err_t esp_wifi_init(const wifi_init_config_t *config) {
    if (!wifi.inited) {
        galaxia_js_mac(wifi.mac[WIFI_IF_STA]);
        memcpy(wifi.mac[WIFI_IF_AP], wifi.mac[WIFI_IF_STA], 6);
        wifi.mac[WIFI_IF_AP][5]++;          // soft-AP MAC: base MAC + 1
        // Default soft-AP: "ESP_" and the end of its MAC, open, channel 1,
        // 192.168.4.1/24, DHCP server
        wifi_ap_config_t *ap = &wifi.cfg[WIFI_IF_AP].ap;
        ap->ssid_len = snprintf((char *)ap->ssid, sizeof(ap->ssid), "ESP_%02X%02X%02X",
            wifi.mac[WIFI_IF_AP][3], wifi.mac[WIFI_IF_AP][4], wifi.mac[WIFI_IF_AP][5]);
        ap->channel = 1;
        ap->authmode = WIFI_AUTH_OPEN;
        ap->max_connection = 4;
        ap->beacon_interval = 100;
        netifs[NETIF_AP].ip.ip.addr = IP4(192, 168, 4, 1);
        netifs[NETIF_AP].ip.netmask.addr = IP4(255, 255, 255, 0);
        netifs[NETIF_AP].ip.gw.addr = IP4(192, 168, 4, 1);
        netifs[NETIF_AP].dns.addr = IP4(192, 168, 4, 1);
    }
    wifi.inited = true;
    return ESP_OK;
}

esp_err_t esp_wifi_deinit(void) {
    wifi.inited = false;
    return ESP_OK;
}

esp_err_t esp_wifi_set_storage(wifi_storage_t storage) {
    return ESP_OK;
}

static void mode_changed(wifi_mode_t old, wifi_mode_t mode) {
    if ((old & WIFI_MODE_STA) && !(mode & WIFI_MODE_STA)) {
        if (wifi.sta_state != STA_IDLE) {
            sta_disconnected(WIFI_REASON_ASSOC_LEAVE);
        }
        post_event(WIFI_EVENT, WIFI_EVENT_STA_STOP, NULL, 0);
    }
    if (!(old & WIFI_MODE_STA) && (mode & WIFI_MODE_STA)) {
        post_event(WIFI_EVENT, WIFI_EVENT_STA_START, NULL, 0);
    }
    if ((old & WIFI_MODE_AP) && !(mode & WIFI_MODE_AP)) {
        ap_stop();
    }
    if (!(old & WIFI_MODE_AP) && (mode & WIFI_MODE_AP)) {
        ap_start();
    }
}

esp_err_t esp_wifi_start(void) {
    if (!wifi.inited) {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    if (!wifi.started) {
        wifi.started = true;
        mode_changed(WIFI_MODE_NULL, wifi.mode);
    }
    return ESP_OK;
}

esp_err_t esp_wifi_stop(void) {
    if (!wifi.inited) {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    if (wifi.started) {
        mode_changed(wifi.mode, WIFI_MODE_NULL);
        wifi.started = false;
    }
    return ESP_OK;
}

esp_err_t esp_wifi_set_mode(wifi_mode_t mode) {
    if (!wifi.inited) {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    if (mode >= WIFI_MODE_NAN) {
        return ESP_ERR_INVALID_ARG;
    }
    wifi_mode_t old = wifi.mode;
    wifi.mode = mode;
    if (wifi.started) {
        mode_changed(old, mode);
    }
    return ESP_OK;
}

esp_err_t esp_wifi_get_mode(wifi_mode_t *mode) {
    if (!wifi.inited) {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    *mode = wifi.mode;
    return ESP_OK;
}

esp_err_t esp_wifi_connect(void) {
    if (!wifi.inited) {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    if (!wifi.started) {
        return ESP_ERR_WIFI_NOT_STARTED;
    }
    if (!(wifi.mode & WIFI_MODE_STA)) {
        return ESP_ERR_WIFI_MODE;
    }
    if (sta_associated()) {
        sta_disconnected(WIFI_REASON_ASSOC_LEAVE);
    }
    // Probe for the access points, then look for the SSID when the scan ends
    wifi.sta_state = STA_SCANNING;
    wifi.sta_due = NOW() + MS(CONNECT_SCAN_MS);
    memset(&wifi.bss, 0, sizeof(wifi.bss));
    uint8_t probe[33] = { 0 };
    memcpy(probe, wifi.cfg[WIFI_IF_STA].sta.ssid, 32);
    probe[32] = strnlen((const char *)probe, 32);
    lan_send(F_PROBE_REQ, wifi.mac[WIFI_IF_STA], BCAST_MAC, NULL, probe, sizeof(probe), NULL, 0);
    return ESP_OK;
}

esp_err_t esp_wifi_disconnect(void) {
    if (!wifi.inited) {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    if (!wifi.started) {
        return ESP_ERR_WIFI_NOT_STARTED;
    }
    if (wifi.sta_state != STA_IDLE) {
        sta_disconnected(WIFI_REASON_ASSOC_LEAVE);
    }
    return ESP_OK;
}

esp_err_t esp_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf) {
    if (!wifi.inited) {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    if (interface == WIFI_IF_AP) {
        wifi_ap_config_t *ap = &conf->ap;
        if (ap->ssid_len == 0 || ap->ssid_len > 32) {
            ap->ssid_len = strnlen((char *)ap->ssid, 32);
        }
        if (ap->authmode != WIFI_AUTH_OPEN) {
            size_t len = strnlen((char *)ap->password, 64);
            if (len < 8 || len > 63) {
                return ESP_ERR_WIFI_PASSWORD;
            }
        }
        if (ap->channel < 1 || ap->channel > 13) {
            return ESP_ERR_INVALID_ARG;
        }
        if (ap->max_connection > 10) {
            return ESP_ERR_INVALID_ARG;
        }
        wifi_ap_config_t *old = &wifi.cfg[WIFI_IF_AP].ap;
        bool restart = wifi.ap_up && (old->ssid_len != ap->ssid_len || memcmp(old->ssid, ap->ssid, 32)
            || strcmp((char *)old->password, (char *)ap->password) || old->authmode != ap->authmode);
        wifi.cfg[WIFI_IF_AP] = *conf;
        if (restart) {
            // The soft-AP restarts with its new settings: the stations are dropped
            while (wifi.nstations) {
                ap_remove_station(0, true, WIFI_REASON_AUTH_LEAVE);
            }
        }
    } else if (interface == WIFI_IF_STA) {
        wifi.cfg[WIFI_IF_STA] = *conf;
    } else {
        return ESP_ERR_WIFI_IF;
    }
    return ESP_OK;
}

esp_err_t esp_wifi_get_config(wifi_interface_t interface, wifi_config_t *conf) {
    if (!wifi.inited) {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    if (interface != WIFI_IF_AP && interface != WIFI_IF_STA) {
        return ESP_ERR_WIFI_IF;
    }
    *conf = wifi.cfg[interface];
    return ESP_OK;
}

static int scan_cmp(const void *a, const void *b) {
    return ((const wifi_ap_record_t *)b)->rssi - ((const wifi_ap_record_t *)a)->rssi;
}

static void scan_add(const bss_t *b) {
    if (wifi.nscan == MP_ARRAY_SIZE_(wifi.scan)) {
        return;
    }
    wifi_ap_record_t *r = &wifi.scan[wifi.nscan++];
    memset(r, 0, sizeof(*r));
    memcpy(r->bssid, b->bssid, 6);
    memcpy(r->ssid, b->ssid, b->ssid_len);
    r->primary = b->channel;
    r->rssi = b->rssi;
    r->authmode = b->authmode;
}

static void net_sleep_ms(int ms);

esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config, bool block) {
    if (!wifi.inited) {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    if (!wifi.started) {
        return ESP_ERR_WIFI_NOT_STARTED;
    }
    if (!(wifi.mode & WIFI_MODE_STA)) {
        return ESP_ERR_WIFI_MODE;
    }
    // The station cannot scan while it connects
    if (wifi.scanning || wifi.sta_state == STA_SCANNING || wifi.sta_state == STA_AUTH || wifi.sta_state == STA_DHCP) {
        return ESP_ERR_WIFI_STATE;
    }
    wifi.scanning = true;
    int64_t start = NOW();
    lan_send(F_PROBE_REQ, wifi.mac[WIFI_IF_STA], BCAST_MAC, NULL, NULL, 0, NULL, 0);
    net_sleep_ms(sta_associated() ? SCAN_CONNECTED_MS : SCAN_MS);
    wifi.nscan = 0;
    bss_t b;
    for (int i = 0; page_ap(i, &b); i++) {
        scan_add(&b);
    }
    for (int i = 0; i < SEEN_MAX; i++) {
        if (seen[i].seen && seen[i].seen >= start - MS(BEACON_PERIOD_MS) && (!seen[i].hidden || (config && config->show_hidden))) {
            bss_t found = seen[i];
            if (found.hidden) {
                found.ssid_len = 0;     // a scan does not reveal a hidden SSID
            }
            scan_add(&found);
        }
    }
    qsort(wifi.scan, wifi.nscan, sizeof(wifi.scan[0]), scan_cmp);
    wifi.scanning = false;
    post_event(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, NULL, 0);
    return ESP_OK;
}

esp_err_t esp_wifi_scan_get_ap_num(uint16_t *number) {
    *number = wifi.nscan;
    return ESP_OK;
}

esp_err_t esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records) {
    if (!ap_records) {
        return ESP_ERR_INVALID_ARG;
    }
    if (*number > wifi.nscan) {
        *number = wifi.nscan;
    }
    memcpy(ap_records, wifi.scan, *number * sizeof(*ap_records));
    wifi.nscan = 0;     // the records are freed once read
    return ESP_OK;
}

esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info) {
    if (!sta_associated()) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    memset(ap_info, 0, sizeof(*ap_info));
    memcpy(ap_info->bssid, wifi.bss.bssid, 6);
    memcpy(ap_info->ssid, wifi.bss.ssid, wifi.bss.ssid_len);
    ap_info->primary = wifi.bss.channel;
    ap_info->rssi = wifi.bss.rssi - 2 + (int)(mp_js_random_u32() % 5);
    ap_info->authmode = wifi.bss.authmode;
    return ESP_OK;
}

esp_err_t esp_wifi_ap_get_sta_list(wifi_sta_list_t *sta) {
    memset(sta, 0, sizeof(*sta));
    if (!wifi.inited) {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    if (!(wifi.mode & WIFI_MODE_AP)) {
        return ESP_ERR_WIFI_MODE;
    }
    for (int i = 0; i < wifi.nstations; i++) {
        memcpy(sta->sta[i].mac, wifi.stations[i].mac, 6);
        sta->sta[i].rssi = -50;
    }
    sta->num = wifi.nstations;
    return ESP_OK;
}

esp_err_t esp_wifi_set_mac(wifi_interface_t ifx, const uint8_t mac[6]) {
    if (ifx != WIFI_IF_STA && ifx != WIFI_IF_AP) {
        return ESP_ERR_WIFI_IF;
    }
    if (mac[0] & 1) {
        return ESP_ERR_WIFI_MAC;    // multicast address
    }
    if ((ifx == WIFI_IF_STA && sta_associated()) || (ifx == WIFI_IF_AP && wifi.ap_up)) {
        return ESP_ERR_WIFI_MODE;
    }
    memcpy(wifi.mac[ifx], mac, 6);
    return ESP_OK;
}

esp_err_t esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6]) {
    if (ifx != WIFI_IF_STA && ifx != WIFI_IF_AP) {
        return ESP_ERR_WIFI_IF;
    }
    memcpy(mac, wifi.mac[ifx], 6);
    return ESP_OK;
}

esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second) {
    if (!wifi.inited) {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    if (primary < 1 || primary > 13) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!sta_associated()) {
        wifi.channel = primary;     // associated: the AP's channel stays
    }
    return ESP_OK;
}

esp_err_t esp_wifi_get_channel(uint8_t *primary, wifi_second_chan_t *second) {
    *primary = home_channel();
    if (second) {
        *second = WIFI_SECOND_CHAN_NONE;
    }
    return ESP_OK;
}

esp_err_t esp_wifi_set_max_tx_power(int8_t power) {
    if (power < 8 || power > 84) {
        return ESP_ERR_INVALID_ARG;
    }
    wifi.tx_power = power;
    return ESP_OK;
}

esp_err_t esp_wifi_get_max_tx_power(int8_t *power) {
    *power = wifi.tx_power;
    return ESP_OK;
}

esp_err_t esp_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap) {
    if (ifx != WIFI_IF_STA && ifx != WIFI_IF_AP) {
        return ESP_ERR_WIFI_IF;
    }
    wifi.protocol[ifx] = protocol_bitmap;
    return ESP_OK;
}

esp_err_t esp_wifi_get_protocol(wifi_interface_t ifx, uint8_t *protocol_bitmap) {
    if (ifx != WIFI_IF_STA && ifx != WIFI_IF_AP) {
        return ESP_ERR_WIFI_IF;
    }
    *protocol_bitmap = wifi.protocol[ifx];
    return ESP_OK;
}

esp_err_t esp_wifi_set_ps(wifi_ps_type_t type) {
    wifi.ps = type;
    return ESP_OK;
}

esp_err_t esp_wifi_get_ps(wifi_ps_type_t *type) {
    *type = wifi.ps;
    return ESP_OK;
}

esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type) {
    galaxia_js_mac(mac);
    if (type == ESP_MAC_WIFI_SOFTAP) {
        mac[5]++;
    }
    return ESP_OK;
}

// ---- the station's connection, step by step (from net_poll) ----

static bool find_bss(bss_t *out) {
    const wifi_sta_config_t *c = &wifi.cfg[WIFI_IF_STA].sta;
    size_t ssid_len = strnlen((const char *)c->ssid, 32);
    bss_t b;
    bool found = false;
    for (int i = 0; page_ap(i, &b); i++) {
        if (b.ssid_len == ssid_len && !memcmp(b.ssid, c->ssid, ssid_len) && (!c->bssid_set || mac_eq(b.bssid, c->bssid))) {
            *out = b;
            found = true;
        }
    }
    for (int i = 0; i < SEEN_MAX; i++) {
        bss_t *s = &seen[i];
        if (!s->seen || NOW() - s->seen > MS(CONNECT_SCAN_MS + BEACON_PERIOD_MS)) {
            continue;
        }
        if (s->ssid_len == ssid_len && !memcmp(s->ssid, c->ssid, ssid_len)
            && (!c->bssid_set || mac_eq(s->bssid, c->bssid))
            && (!found || s->rssi > out->rssi)) {
            *out = *s;
            found = true;
        }
    }
    return ssid_len > 0 && found;
}

// The DHCP client (or the static address)
static void sta_start_dhcp(void) {
    wifi.sta_state = STA_DHCP;
    struct galaxia_netif *n = &netifs[NETIF_STA];
    if (n->dhcp == ESP_NETIF_DHCP_STOPPED) {
        if (n->ip.ip.addr) {
            sta_got_ip();
        }
        return;
    }
    n->dhcp = ESP_NETIF_DHCP_STARTED;
    wifi.dhcp_answer = false;
    wifi.candidate_try = 0;
    wifi.candidate = 0;
    wifi.sta_due = NOW() + MS(DHCP_MS);
    if (!wifi.bss.page) {
        lan_send(F_DHCP_REQ, wifi.mac[WIFI_IF_STA], wifi.bss.bssid, wifi.bss.bssid, NULL, 0, NULL, 0);
    }
}

static void sta_associate(void) {
    wifi.last_beacon = NOW();
    wifi.next_keepalive = NOW() + MS(KEEPALIVE_MS);
    post_event(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, NULL, 0);
    sta_start_dhcp();
}

// The page's network: 192.168.1.0/24, router .1; the station takes a free address
// in .100-.199, starting from one given by its MAC (the same one each time)
static uint32_t page_candidate(int attempt) {
    const uint8_t *m = wifi.mac[WIFI_IF_STA];
    unsigned h = (m[3] * 31 + m[4]) * 31 + m[5];
    return IP4(192, 168, 1, 100 + (h + attempt * 37) % 100);
}

static void sta_step(void) {
    int64_t now = NOW();
    switch (wifi.sta_state) {
        case STA_SCANNING:
            if (now < wifi.sta_due) {
                return;
            }
            if (!find_bss(&wifi.bss)) {
                sta_disconnected(WIFI_REASON_NO_AP_FOUND);
                return;
            }
            {
                // Security: a key of 8 characters or more raises the threshold to
                // WPA2 (ESP-IDF 5): an open or WEP/WPA network is not taken
                const wifi_sta_config_t *c = &wifi.cfg[WIFI_IF_STA].sta;
                size_t key_len = strnlen((const char *)c->password, 64);
                if (key_len >= 8 && wifi.bss.authmode < WIFI_AUTH_WPA2_PSK) {
                    sta_disconnected(WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD);
                    return;
                }
            }
            wifi.sta_state = STA_AUTH;
            wifi.sta_due = now + MS(AUTH_MS);
            wifi.assoc_answer = false;
            if (!wifi.bss.page) {
                lan_send(F_ASSOC_REQ, wifi.mac[WIFI_IF_STA], wifi.bss.bssid, wifi.bss.bssid,
                    wifi.cfg[WIFI_IF_STA].sta.password, 64, NULL, 0);
            }
            return;
        case STA_AUTH: {
            if (now < wifi.sta_due) {
                return;
            }
            if (wifi.auth_failed) {
                // The wrong key is only found when the 4-way handshake times out
                wifi.auth_failed = false;
                sta_disconnected(WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT);
                return;
            }
            uint8_t status;
            if (wifi.bss.page) {
                const char *key = (const char *)wifi.cfg[WIFI_IF_STA].sta.password;
                status = wifi.bss.authmode == WIFI_AUTH_OPEN || !strncmp(key, wifi.bss.password, 64) ? 0 : WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT;
            } else if (wifi.assoc_answer) {
                status = wifi.assoc_status;
            } else if (now < wifi.sta_due + MS(1000)) {
                return;     // the soft-AP has not answered yet
            } else {
                status = WIFI_REASON_AUTH_EXPIRE;
            }
            if (status == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) {
                wifi.auth_failed = true;
                wifi.sta_due = now + MS(HANDSHAKE_FAIL_MS - AUTH_MS);
                return;
            }
            if (status) {
                sta_disconnected(status);
                return;
            }
            sta_associate();
            return;
        }
        case STA_DHCP: {
            if (now < wifi.sta_due) {
                return;
            }
            if (wifi.bss.page) {
                // Page network: probe a candidate, take it when nobody answers
                if (wifi.candidate && !wifi.candidate_taken) {
                    sta_set_ip(wifi.candidate, IP4(255, 255, 255, 0), IP4(192, 168, 1, 1));
                    ip_addr_t dns = { .u_addr.ip4.addr = IP4(192, 168, 1, 1) };
                    dns_setserver(0, &dns);
                    sta_got_ip();
                    return;
                }
                wifi.candidate = page_candidate(wifi.candidate_try++);
                wifi.candidate_taken = false;
                lan_send(F_ARP_PROBE, wifi.mac[WIFI_IF_STA], BCAST_MAC, wifi.bss.bssid, &wifi.candidate, 4, NULL, 0);
                wifi.sta_due = now + MS(ARP_PROBE_MS);
                return;
            }
            if (wifi.dhcp_answer) {
                sta_set_ip(wifi.dhcp_offer.ip, wifi.dhcp_offer.mask, wifi.dhcp_offer.gw);
                ip_addr_t dns = { .u_addr.ip4.addr = wifi.dhcp_offer.dns };
                dns_setserver(0, &dns);
                sta_got_ip();
                return;
            }
            // No offer yet (soft-AP full or gone): ask again, as the DHCP client
            lan_send(F_DHCP_REQ, wifi.mac[WIFI_IF_STA], wifi.bss.bssid, wifi.bss.bssid, NULL, 0, NULL, 0);
            wifi.sta_due = now + MS(DHCP_MS * 2);
            return;
        }
        default:
            return;
    }
}

static void wifi_tick(void) {
    int64_t now = NOW();
    if (!wifi.started) {
        return;
    }
    if (wifi.mode & WIFI_MODE_STA) {
        sta_step();
        if (sta_associated()) {
            bss_t page;
            if (wifi.bss.page) {
                if (page_ap_by_bssid(wifi.bss.bssid, &page) && page.authmode == wifi.bss.authmode
                    && !strncmp(page.password, wifi.bss.password, 64)) {
                    wifi.last_beacon = now;     // the page's AP is still there, unchanged
                }
            } else if (now >= wifi.next_keepalive) {
                wifi.next_keepalive = now + MS(KEEPALIVE_MS);
                lan_send(F_KEEPALIVE, wifi.mac[WIFI_IF_STA], wifi.bss.bssid, wifi.bss.bssid, NULL, 0, NULL, 0);
            }
            if (now - wifi.last_beacon > MS(BEACON_TIMEOUT_MS)) {
                sta_disconnected(WIFI_REASON_BEACON_TIMEOUT);
            }
        }
    }
    if (wifi.ip_lost_at && now >= wifi.ip_lost_at && wifi.sta_state != STA_UP) {
        wifi.ip_lost_at = 0;
        sta_set_ip(0, 0, 0);
        post_event(IP_EVENT, IP_EVENT_STA_LOST_IP, NULL, 0);
    }
    if (wifi.ap_up) {
        if (now >= wifi.next_beacon) {
            wifi.next_beacon = now + MS(BEACON_PERIOD_MS);
            send_beacon(BCAST_MAC, false);
        }
        for (int i = wifi.nstations - 1; i >= 0; i--) {
            if (now - wifi.stations[i].seen > MS(AP_INACTIVE_MS)) {
                ap_remove_station(i, true, WIFI_REASON_ASSOC_EXPIRE);
            }
        }
    }
}

// ================================================================= ESP-NOW

#define RADIO_MAX 256

static esp_now_recv_cb_t radio_recv_cb;
static esp_now_send_cb_t radio_send_cb;
static bool radio_on;
static esp_now_peer_info_t radio_peers[20];     // ESP_NOW_MAX_TOTAL_PEER_NUM
static int radio_npeers;

esp_err_t esp_now_init(void) {
    if (!wifi.started) {
        return ESP_ERR_ESPNOW_INTERNAL;
    }
    radio_on = true;
    return ESP_OK;
}

esp_err_t esp_now_deinit(void) {
    radio_on = false;
    radio_npeers = 0;
    return ESP_OK;
}

static int radio_peer(const uint8_t *mac) {
    for (int i = 0; i < radio_npeers; i++) {
        if (mac_eq(radio_peers[i].peer_addr, mac)) {
            return i;
        }
    }
    return -1;
}

esp_err_t esp_now_add_peer(const esp_now_peer_info_t *peer) {
    if (!radio_on) {
        return ESP_ERR_ESPNOW_NOT_INIT;
    }
    if (radio_peer(peer->peer_addr) >= 0) {
        return ESP_ERR_ESPNOW_EXIST;
    }
    if (radio_npeers == MP_ARRAY_SIZE_(radio_peers)) {
        return ESP_ERR_ESPNOW_FULL;
    }
    radio_peers[radio_npeers++] = *peer;
    return ESP_OK;
}

esp_err_t esp_now_mod_peer(const esp_now_peer_info_t *peer) {
    if (!radio_on) {
        return ESP_ERR_ESPNOW_NOT_INIT;
    }
    int i = radio_peer(peer->peer_addr);
    if (i < 0) {
        return ESP_ERR_ESPNOW_NOT_FOUND;
    }
    radio_peers[i] = *peer;
    return ESP_OK;
}

esp_err_t esp_now_del_peer(const uint8_t *peer_addr) {
    if (!radio_on) {
        return ESP_ERR_ESPNOW_NOT_INIT;
    }
    int i = radio_peer(peer_addr);
    if (i < 0) {
        return ESP_ERR_ESPNOW_NOT_FOUND;
    }
    radio_peers[i] = radio_peers[--radio_npeers];
    return ESP_OK;
}

esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t cb) {
    radio_recv_cb = cb;
    return ESP_OK;
}

esp_err_t esp_now_unregister_recv_cb(void) {
    radio_recv_cb = NULL;
    return ESP_OK;
}

esp_err_t esp_now_register_send_cb(esp_now_send_cb_t cb) {
    radio_send_cb = cb;
    return ESP_OK;
}

esp_err_t esp_now_unregister_send_cb(void) {
    radio_send_cb = NULL;
    return ESP_OK;
}

esp_err_t esp_now_send(const uint8_t *peer_addr, const uint8_t *data, size_t len) {
    if (!radio_on) {
        return ESP_ERR_ESPNOW_NOT_INIT;
    }
    if (!wifi.started) {
        return ESP_ERR_ESPNOW_IF;
    }
    if (len > 250) {
        return ESP_ERR_ESPNOW_ARG;
    }
    int p = radio_peer(peer_addr);
    if (p < 0) {
        return ESP_ERR_ESPNOW_NOT_FOUND;
    }
    // A peer registered on a channel can only be reached on that channel: the
    // station associated to an AP on another channel cannot send to it
    if (radio_peers[p].channel && radio_peers[p].channel != home_channel()) {
        return ESP_ERR_ESPNOW_CHAN;
    }
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    galaxia_js_radio_send(home_channel(), mac, data, len);
    if (radio_send_cb) {
        radio_send_cb(peer_addr, ESP_NOW_SEND_SUCCESS);
    }
    return ESP_OK;
}

static void radio_poll(void) {
    uint8_t data[RADIO_MAX], mac[6];
    int channel, len;
    while ((len = galaxia_js_radio_next(data, sizeof(data), mac, &channel)) >= 0) {
        if (radio_on && wifi.started && radio_recv_cb && channel == home_channel()) {
            uint8_t dest[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
            wifi_pkt_rx_ctrl_t rx = { .rssi = -40, .channel = channel };
            esp_now_recv_info_t info = { .src_addr = mac, .des_addr = dest, .rx_ctrl = &rx };
            radio_recv_cb(&info, data, len);
        }
    }
}

// ================================================================= esp_netif

esp_err_t esp_netif_init(void) {
    return ESP_OK;
}

esp_netif_t *esp_netif_create_default_wifi_sta(void) {
    return &netifs[NETIF_STA];
}

esp_netif_t *esp_netif_create_default_wifi_ap(void) {
    return &netifs[NETIF_AP];
}

void esp_netif_destroy(esp_netif_t *netif) {
}

esp_err_t esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *ip_info) {
    if (!netif) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    *ip_info = netif->ip;
    return ESP_OK;
}

esp_err_t esp_netif_set_ip_info(esp_netif_t *netif, const esp_netif_ip_info_t *ip_info) {
    if (!netif) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    if (netif->id == NETIF_STA && netif->dhcp != ESP_NETIF_DHCP_STOPPED) {
        return ESP_ERR_ESP_NETIF_DHCP_NOT_STOPPED;
    }
    if (netif->id == NETIF_AP && netif->dhcp == ESP_NETIF_DHCP_STARTED) {
        return ESP_ERR_ESP_NETIF_DHCP_NOT_STOPPED;
    }
    bool changed = netif->ip.ip.addr != ip_info->ip.addr;
    netif->ip = *ip_info;
    if (netif->id == NETIF_STA && changed && sta_associated() && ip_info->ip.addr) {
        sta_got_ip();
    }
    return ESP_OK;
}

esp_err_t esp_netif_get_dns_info(esp_netif_t *netif, esp_netif_dns_type_t type, esp_netif_dns_info_t *dns) {
    if (!netif || type >= ESP_NETIF_DNS_MAX) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    memset(dns, 0, sizeof(*dns));
    if (netif->id == NETIF_AP) {
        dns->ip.u_addr.ip4 = netif->dns;
    } else {
        dns->ip.u_addr.ip4.addr = dns_servers[type].u_addr.ip4.addr;
    }
    return ESP_OK;
}

esp_err_t esp_netif_set_dns_info(esp_netif_t *netif, esp_netif_dns_type_t type, esp_netif_dns_info_t *dns) {
    if (!netif || type >= ESP_NETIF_DNS_MAX) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    if (netif->id == NETIF_AP) {
        netif->dns = dns->ip.u_addr.ip4;
    } else {
        dns_servers[type].u_addr.ip4 = (ip4_addr_t) { dns->ip.u_addr.ip4.addr };
    }
    return ESP_OK;
}

esp_err_t esp_netif_dhcpc_start(esp_netif_t *netif) {
    if (netif->id != NETIF_STA) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    if (netif->dhcp == ESP_NETIF_DHCP_STARTED) {
        return ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED;
    }
    netif->dhcp = ESP_NETIF_DHCP_STARTED;
    if (sta_associated()) {
        // Back to DHCP while connected: a new lease
        sta_set_ip(0, 0, 0);
        sta_start_dhcp();
    }
    return ESP_OK;
}

esp_err_t esp_netif_dhcpc_stop(esp_netif_t *netif) {
    if (netif->id != NETIF_STA) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    if (netif->dhcp == ESP_NETIF_DHCP_STOPPED) {
        return ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED;
    }
    netif->dhcp = ESP_NETIF_DHCP_STOPPED;
    return ESP_OK;
}

esp_err_t esp_netif_dhcpc_get_status(esp_netif_t *netif, esp_netif_dhcp_status_t *status) {
    *status = netif->dhcp;
    return ESP_OK;
}

esp_err_t esp_netif_dhcps_start(esp_netif_t *netif) {
    if (netif->id != NETIF_AP) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    if (netif->dhcp == ESP_NETIF_DHCP_STARTED) {
        return ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED;
    }
    netif->dhcp = ESP_NETIF_DHCP_STARTED;
    return ESP_OK;
}

esp_err_t esp_netif_dhcps_stop(esp_netif_t *netif) {
    if (netif->id != NETIF_AP) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    if (netif->dhcp == ESP_NETIF_DHCP_STOPPED) {
        return ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED;
    }
    netif->dhcp = ESP_NETIF_DHCP_STOPPED;
    return ESP_OK;
}

esp_err_t esp_netif_set_hostname(esp_netif_t *netif, const char *hostname) {
    if (strlen(hostname) > 32) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    strcpy(netif->hostname, hostname);
    return ESP_OK;
}

esp_err_t esp_netif_get_netif_impl_name(esp_netif_t *netif, char *name) {
    // lwIP's netif names: lo0 first, then the station and the soft-AP
    strcpy(name, netif->id == NETIF_STA ? "st1" : "ap2");
    return ESP_OK;
}

uint32_t esp_netif_htonl(uint32_t x) {
    return __builtin_bswap32(x);
}

// ================================================================= lwIP: helpers

u16_t lwip_htons(u16_t x) {
    return __builtin_bswap16(x);
}

u32_t lwip_htonl(u32_t x) {
    return __builtin_bswap32(x);
}

char *ip4addr_ntoa_r(const ip4_addr_t *addr, char *buf, int buflen) {
    const uint8_t *b = (const uint8_t *)&addr->addr;
    snprintf(buf, buflen, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return buf;
}

char *ipaddr_ntoa_r(const ip_addr_t *addr, char *buf, int buflen) {
    return ip4addr_ntoa_r(&addr->u_addr.ip4, buf, buflen);
}

static bool ip4_aton(const char *cp, uint32_t *out) {
    unsigned v[4];
    char extra;
    if (sscanf(cp, "%u.%u.%u.%u%c", &v[0], &v[1], &v[2], &v[3], &extra) != 4) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        if (v[i] > 255) {
            return false;
        }
    }
    *out = IP4(v[0], v[1], v[2], v[3]);
    return true;
}

int ipaddr_aton(const char *cp, ip_addr_t *addr) {
    uint32_t ip;
    if (!ip4_aton(cp, &ip)) {
        return 0;
    }
    memset(addr, 0, sizeof(*addr));
    addr->u_addr.ip4.addr = ip;
    return 1;
}

const ip_addr_t *dns_getserver(u8_t numdns) {
    static const ip_addr_t none;
    return numdns < MP_ARRAY_SIZE_(dns_servers) ? &dns_servers[numdns] : &none;
}

void dns_setserver(u8_t numdns, const ip_addr_t *dnsserver) {
    if (numdns < MP_ARRAY_SIZE_(dns_servers)) {
        dns_servers[numdns] = *dnsserver;
    }
}

#define MAX_GROUPS 8
static uint32_t groups[MAX_GROUPS];

err_t igmp_joingroup(const ip4_addr_t *ifaddr, const ip4_addr_t *groupaddr) {
    if (!IS_MULTICAST(groupaddr->addr)) {
        return ERR_VAL;
    }
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i] == groupaddr->addr) {
            return ERR_OK;
        }
    }
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (!groups[i]) {
            groups[i] = groupaddr->addr;
            return ERR_OK;
        }
    }
    return -1;      // ERR_MEM
}

static bool joined(uint32_t group) {
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i] == group) {
            return true;
        }
    }
    return false;
}

void *memp_malloc(int type) {
    return calloc(1, NETDB_ELEM_SIZE);
}

// The netif a local address is on, NETIF_LOOP for 127.x, else NETIF_NONE
static int local_netif(uint32_t ip) {
    if (IS_LOOPBACK(ip)) {
        return NETIF_LOOP;
    }
    for (int n = 0; n < 2; n++) {
        if (netif_up(n) && netifs[n].ip.ip.addr == ip) {
            return n;
        }
    }
    return NETIF_NONE;
}

// lwIP's routing: the netif whose subnet holds the destination, else the default
// one (the station)
static int route(uint32_t dst) {
    int local = local_netif(dst);
    if (local != NETIF_NONE) {
        return NETIF_LOOP;
    }
    for (int n = 0; n < 2; n++) {
        const esp_netif_ip_info_t *ip = &netifs[n].ip;
        if (netif_up(n) && (dst & ip->netmask.addr) == (ip->ip.addr & ip->netmask.addr)) {
            return n;
        }
    }
    if (netif_up(NETIF_STA)) {
        return NETIF_STA;
    }
    if (netif_up(NETIF_AP) && (dst == IP_BROADCAST || IS_MULTICAST(dst))) {
        return NETIF_AP;
    }
    return NETIF_NONE;
}

static const uint8_t *netif_bssid(int n) {
    return n == NETIF_STA ? wifi.bss.bssid : wifi.mac[WIFI_IF_AP];
}

static const uint8_t *netif_mac(int n) {
    return n == NETIF_STA ? wifi.mac[WIFI_IF_STA] : wifi.mac[WIFI_IF_AP];
}

static void deliver_ip(int n, uint32_t src, uint32_t dst, uint8_t proto, const uint8_t *payload, size_t len);

// Loopback: delivered from net_poll(), as lwIP's loopback netif does
#define LOOP_MAX 16
static struct {
    uint32_t src, dst;
    uint8_t proto;
    size_t len;
    uint8_t *data;
} loop_q[LOOP_MAX];
static int loop_head, loop_len;

static void send_ip(int n, uint32_t src, uint32_t dst, uint8_t proto, const void *hdr, size_t hdr_len, const void *data, size_t data_len) {
    if (n == NETIF_LOOP) {
        if (loop_len == LOOP_MAX) {
            return;
        }
        int i = (loop_head + loop_len++) % LOOP_MAX;
        loop_q[i].src = src;
        loop_q[i].dst = dst;
        loop_q[i].proto = proto;
        loop_q[i].len = hdr_len + data_len;
        loop_q[i].data = malloc(hdr_len + data_len);
        memcpy(loop_q[i].data, hdr, hdr_len);
        if (data_len) {
            memcpy(loop_q[i].data + hdr_len, data, data_len);
        }
        return;
    }
    uint8_t body[sizeof(f_ip_t) + 16];
    f_ip_t ip = { .src = src, .dst = dst, .proto = proto };
    memcpy(body, &ip, sizeof(ip));
    memcpy(body + sizeof(ip), hdr, hdr_len);
    lan_send(F_IP, netif_mac(n), BCAST_MAC, netif_bssid(n), body, sizeof(ip) + hdr_len, data, data_len);
}

static void loop_poll(void) {
    while (loop_len) {
        int i = loop_head;
        loop_head = (loop_head + 1) % LOOP_MAX;
        loop_len--;
        deliver_ip(NETIF_LOOP, loop_q[i].src, loop_q[i].dst, loop_q[i].proto, loop_q[i].data, loop_q[i].len);
        free(loop_q[i].data);
    }
}

// ================================================================= lwIP: sockets

enum { T_FREE, T_SYN_SENT, T_ESTABLISHED, T_CLOSED };

struct pcb {
    int state;
    int netif;
    uint32_t lip, rip;
    uint16_t lport, rport;
    bool reuse;
    uint8_t *rx;
    int rx_head, rx_len;
    bool rx_fin;
    int snd_wnd;            // what the peer can still take
    int err;                // pending error (newlib errno)
    int64_t syn_deadline;
    int listener;           // the listening socket, until accept()
};
static struct pcb pcbs[MAX_PCBS];

// The connections closed lately: their segments still coming (window updates, FIN)
// are absorbed as by FIN_WAIT/LAST_ACK; after an active close the local port stays
// in TIME_WAIT for 2 MSL
#define CLOSED_LINGER_MS 10000
static struct {
    uint32_t lip, rip;
    uint16_t lport, rport;
    bool reuse;
    bool time_wait;
    int64_t until;
} time_wait[MAX_TIME_WAIT];

typedef struct {
    uint32_t ip;
    uint16_t port;
    uint16_t len;
    uint8_t *data;
} dgram_t;

struct sock {
    bool used;
    int type;
    bool nonblock;
    int rcvtimeo, sndtimeo;     // ms, 0: forever
    bool reuse;
    uint32_t lip;
    uint16_t lport;
    bool bound;
    // TCP
    bool listening;
    int backlog;
    int pending[TCP_ACCEPTMBOX];
    int npending;
    int pcb;
    int err;
    // UDP
    uint32_t rip;
    uint16_t rport;
    bool connected;
    dgram_t q[UDP_RECVMBOX];
    int qhead, qlen;
};
static struct sock socks[CONFIG_LWIP_MAX_SOCKETS];

static uint16_t next_port;

static struct sock *get_sock(int s) {
    int i = s - LWIP_SOCKET_OFFSET;
    if (i < 0 || i >= CONFIG_LWIP_MAX_SOCKETS || !socks[i].used) {
        return NULL;
    }
    return &socks[i];
}

static int set_errno(int e) {
    errno = e;
    return -1;
}

static void tcp_send(struct pcb *p, uint8_t flags, uint16_t n, const void *data, size_t len) {
    f_tcp_t t = { .sport = p->lport, .dport = p->rport, .flags = flags, .n = n };
    int netif = p->netif;
    if (netif != NETIF_LOOP && !netif_up(netif)) {
        return;
    }
    send_ip(netif, p->lip, p->rip, IPPROTO_TCP, &t, sizeof(t), data, len);
}

static void tcp_reply_rst(int n, uint32_t lip, uint32_t rip, const f_tcp_t *in) {
    f_tcp_t t = { .sport = in->dport, .dport = in->sport, .flags = TCP_RST };
    send_ip(n == NETIF_LOOP ? NETIF_LOOP : n, lip, rip, IPPROTO_TCP, &t, sizeof(t), NULL, 0);
}

static struct pcb *pcb_new(void) {
    for (int i = 0; i < MAX_PCBS; i++) {
        if (pcbs[i].state == T_FREE) {
            memset(&pcbs[i], 0, sizeof(pcbs[i]));
            pcbs[i].rx = malloc(TCP_WND);
            pcbs[i].listener = -1;
            return &pcbs[i];
        }
    }
    return NULL;
}

static void pcb_free(struct pcb *p) {
    free(p->rx);
    p->rx = NULL;
    p->state = T_FREE;
}

static bool port_in_use(int type, uint32_t ip, uint16_t port, bool reuse) {
    for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++) {
        struct sock *s = &socks[i];
        if (s->used && s->bound && s->type == type && s->lport == port
            && (s->lip == IP_ANY || ip == IP_ANY || s->lip == ip) && !(reuse && s->reuse)) {
            return true;
        }
    }
    if (type == SOCK_STREAM) {
        for (int i = 0; i < MAX_PCBS; i++) {
            if (pcbs[i].state != T_FREE && pcbs[i].lport == port && !(reuse && pcbs[i].reuse)) {
                return true;
            }
        }
        for (int i = 0; i < MAX_TIME_WAIT; i++) {
            if (time_wait[i].time_wait && time_wait[i].until > NOW() && time_wait[i].lport == port && !(reuse && time_wait[i].reuse)) {
                return true;
            }
        }
    }
    return false;
}

// An ephemeral port (lwIP: 0xc000-0xffff, from a random start)
static uint16_t ephemeral_port(int type) {
    if (!next_port) {
        next_port = 0xc000 + mp_js_random_u32() % 0x4000;
    }
    for (int i = 0; i < 0x4000; i++) {
        uint16_t p = next_port;
        next_port = next_port == 0xffff ? 0xc000 : next_port + 1;
        if (!port_in_use(type, IP_ANY, p, false)) {
            return p;
        }
    }
    return 0;
}

static void sock_free_dgrams(struct sock *s) {
    while (s->qlen) {
        free(s->q[s->qhead].data);
        s->qhead = (s->qhead + 1) % UDP_RECVMBOX;
        s->qlen--;
    }
}

int lwip_socket(int domain, int type, int protocol) {
    if (domain != AF_INET) {
        return set_errno(EAFNOSUPPORT);     // IPv6 is not simulated
    }
    if (type != SOCK_STREAM && type != SOCK_DGRAM) {
        return set_errno(EINVAL);
    }
    for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++) {
        if (!socks[i].used) {
            memset(&socks[i], 0, sizeof(socks[i]));
            socks[i].used = true;
            socks[i].type = type;
            socks[i].pcb = -1;
            return LWIP_SOCKET_OFFSET + i;
        }
    }
    return set_errno(ENFILE);
}

static bool sockaddr_in_get(const struct sockaddr *name, socklen_t namelen, uint32_t *ip, uint16_t *port) {
    if (!name || namelen < sizeof(struct sockaddr_in) || name->sa_family != AF_INET) {
        return false;
    }
    const struct sockaddr_in *in = (const struct sockaddr_in *)name;
    *ip = in->sin_addr.s_addr;
    *port = lwip_ntohs(in->sin_port);
    return true;
}

static void sockaddr_in_set(struct sockaddr *addr, socklen_t *addrlen, uint32_t ip, uint16_t port) {
    if (!addr || !addrlen) {
        return;
    }
    struct sockaddr_in in = { .sin_len = sizeof(in), .sin_family = AF_INET, .sin_port = lwip_htons(port) };
    in.sin_addr.s_addr = ip;
    memcpy(addr, &in, *addrlen < sizeof(in) ? *addrlen : sizeof(in));
    *addrlen = sizeof(in);
}

int lwip_bind(int fd, const struct sockaddr *name, socklen_t namelen) {
    struct sock *s = get_sock(fd);
    if (!s) {
        return set_errno(EBADF);
    }
    uint32_t ip;
    uint16_t port;
    if (!sockaddr_in_get(name, namelen, &ip, &port)) {
        return set_errno(EINVAL);
    }
    if (s->bound) {
        return set_errno(EINVAL);
    }
    if (ip != IP_ANY && !IS_LOOPBACK(ip) && local_netif(ip) == NETIF_NONE) {
        return set_errno(EADDRNOTAVAIL);
    }
    if (port == 0) {
        port = ephemeral_port(s->type);
    } else if (port_in_use(s->type, ip, port, s->reuse)) {
        return set_errno(EADDRINUSE);
    }
    s->lip = ip;
    s->lport = port;
    s->bound = true;
    return 0;
}

int lwip_listen(int fd, int backlog) {
    struct sock *s = get_sock(fd);
    if (!s) {
        return set_errno(EBADF);
    }
    if (s->type != SOCK_STREAM) {
        return set_errno(EOPNOTSUPP);
    }
    if (s->pcb >= 0) {
        return set_errno(EINVAL);
    }
    if (!s->bound) {
        s->lport = ephemeral_port(SOCK_STREAM);
        s->bound = true;
    }
    // TCP_LISTEN_BACKLOG: at most `backlog` connections wait for accept() (0 is 1)
    backlog = backlog < 1 ? 1 : backlog > 0xff ? 0xff : backlog;
    s->backlog = backlog;
    s->listening = true;
    return 0;
}

static bool wait_for(bool (*ready)(struct sock *), struct sock *s, int timeout_ms);

static bool accept_ready(struct sock *s) {
    return s->npending > 0 || !s->listening;
}

int lwip_accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    struct sock *s = get_sock(fd);
    if (!s) {
        return set_errno(EBADF);
    }
    if (!s->listening) {
        return set_errno(EINVAL);
    }
    if (!s->npending) {
        if (s->nonblock || !wait_for(accept_ready, s, s->rcvtimeo) || !s->npending) {
            return set_errno(EWOULDBLOCK);
        }
    }
    struct pcb *p = &pcbs[s->pending[0]];
    memmove(s->pending, s->pending + 1, --s->npending * sizeof(s->pending[0]));
    int nfd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (nfd < 0) {
        // No socket left: the connection is dropped
        tcp_send(p, TCP_RST, 0, NULL, 0);
        pcb_free(p);
        return set_errno(ENFILE);
    }
    struct sock *ns = get_sock(nfd);
    p->listener = -1;
    ns->pcb = p - pcbs;
    ns->lip = p->lip;
    ns->lport = p->lport;
    ns->reuse = p->reuse;
    sockaddr_in_set(addr, addrlen, p->rip, p->rport);
    return nfd;
}

static bool connect_ready(struct sock *s) {
    return s->pcb < 0 || pcbs[s->pcb].state != T_SYN_SENT;
}

int lwip_connect(int fd, const struct sockaddr *name, socklen_t namelen) {
    struct sock *s = get_sock(fd);
    if (!s) {
        return set_errno(EBADF);
    }
    uint32_t ip;
    uint16_t port;
    if (!sockaddr_in_get(name, namelen, &ip, &port)) {
        return set_errno(EINVAL);
    }
    int n = route(ip);
    if (s->type == SOCK_DGRAM) {
        if (!s->bound) {
            s->lport = ephemeral_port(SOCK_DGRAM);
            s->bound = true;
        }
        s->rip = ip;
        s->rport = port;
        s->connected = true;
        return 0;
    }
    if (s->listening) {
        return set_errno(EISCONN);
    }
    if (s->pcb >= 0) {
        return set_errno(pcbs[s->pcb].state == T_SYN_SENT ? EALREADY : EISCONN);
    }
    if (n == NETIF_NONE) {
        return set_errno(EHOSTUNREACH);
    }
    struct pcb *p = pcb_new();
    if (!p) {
        return set_errno(ENOMEM);
    }
    if (!s->bound) {
        s->lport = ephemeral_port(SOCK_STREAM);
        s->bound = true;
    }
    p->state = T_SYN_SENT;
    p->netif = n;
    p->lip = n == NETIF_LOOP ? ip : netifs[n].ip.ip.addr;
    if (s->lip != IP_ANY) {
        p->lip = s->lip;
    }
    p->rip = ip;
    p->lport = s->lport;
    p->rport = port;
    p->reuse = s->reuse;
    p->syn_deadline = NOW() + MS(TCP_SYN_TIMEOUT_MS);
    s->pcb = p - pcbs;
    tcp_send(p, TCP_SYN, 0, NULL, 0);
    if (s->nonblock) {
        return set_errno(EINPROGRESS);
    }
    wait_for(connect_ready, s, 0);
    if (p->state == T_ESTABLISHED) {
        return 0;
    }
    int e = p->err ? p->err : ECONNABORTED;
    pcb_free(p);
    s->pcb = -1;
    return set_errno(e);
}

static void remember_closed(struct pcb *p, bool active) {
    int slot = 0;
    for (int i = 0; i < MAX_TIME_WAIT; i++) {
        if (time_wait[i].until <= NOW()) {
            slot = i;
            break;
        }
        if (time_wait[i].until < time_wait[slot].until) {
            slot = i;       // all in use: the oldest one goes
        }
    }
    time_wait[slot].lip = p->lip;
    time_wait[slot].rip = p->rip;
    time_wait[slot].lport = p->lport;
    time_wait[slot].rport = p->rport;
    time_wait[slot].reuse = p->reuse;
    time_wait[slot].time_wait = active;
    time_wait[slot].until = NOW() + MS(active ? 2 * TCP_MSL_MS : CLOSED_LINGER_MS);
}

static void tcp_close_pcb(struct pcb *p) {
    if (p->state == T_ESTABLISHED) {
        if (p->rx_len) {
            tcp_send(p, TCP_RST, 0, NULL, 0);   // unread data: lwIP resets
        } else {
            tcp_send(p, TCP_FIN, 0, NULL, 0);
            // Active close (the peer has not closed): TIME_WAIT for 2 MSL
            remember_closed(p, !p->rx_fin);
        }
    } else if (p->state == T_SYN_SENT) {
        tcp_send(p, TCP_RST, 0, NULL, 0);
    }
    pcb_free(p);
}

int lwip_close(int fd) {
    struct sock *s = get_sock(fd);
    if (!s) {
        return set_errno(EBADF);
    }
    if (s->pcb >= 0) {
        tcp_close_pcb(&pcbs[s->pcb]);
    }
    for (int i = 0; i < s->npending; i++) {
        struct pcb *p = &pcbs[s->pending[i]];
        tcp_send(p, TCP_RST, 0, NULL, 0);
        pcb_free(p);
    }
    sock_free_dgrams(s);
    s->used = false;
    return 0;
}

static bool readable(struct sock *s) {
    if (s->type == SOCK_DGRAM) {
        return s->qlen > 0;
    }
    if (s->listening) {
        return s->npending > 0;
    }
    if (s->pcb < 0) {
        return s->err != 0;
    }
    struct pcb *p = &pcbs[s->pcb];
    return p->rx_len > 0 || p->rx_fin || p->err || p->state == T_CLOSED;
}

static bool writable(struct sock *s) {
    if (s->type == SOCK_DGRAM) {
        return true;
    }
    if (s->listening) {
        return false;
    }
    if (s->pcb < 0) {
        return s->err != 0;
    }
    struct pcb *p = &pcbs[s->pcb];
    return (p->state == T_ESTABLISHED && p->snd_wnd > 0) || p->err || p->state == T_CLOSED;
}

static bool has_error(struct sock *s) {
    if (s->pcb >= 0) {
        return pcbs[s->pcb].err != 0;
    }
    return s->err != 0;
}

static void tcp_window_update(struct pcb *p, int n) {
    if (n > 0 && p->state == T_ESTABLISHED) {
        tcp_send(p, TCP_WIN, n, NULL, 0);
    }
}

int lwip_recvfrom(int fd, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen) {
    struct sock *s = get_sock(fd);
    if (!s) {
        return set_errno(EBADF);
    }
    if (!readable(s)) {
        if (s->type == SOCK_STREAM && (s->listening || s->pcb < 0)) {
            return set_errno(ENOTCONN);
        }
        if (s->nonblock || !wait_for(readable, s, s->rcvtimeo)) {
            return set_errno(EWOULDBLOCK);
        }
    }
    if (s->type == SOCK_DGRAM) {
        dgram_t *d = &s->q[s->qhead];
        size_t n = d->len < len ? d->len : len;     // the rest of the datagram is lost
        memcpy(mem, d->data, n);
        sockaddr_in_set(from, fromlen, d->ip, d->port);
        free(d->data);
        s->qhead = (s->qhead + 1) % UDP_RECVMBOX;
        s->qlen--;
        return n;
    }
    if (s->pcb < 0) {
        int e = s->err;
        s->err = 0;
        return set_errno(e ? e : ENOTCONN);
    }
    struct pcb *p = &pcbs[s->pcb];
    if (p->rx_len) {
        size_t n = 0;
        while (n < len && p->rx_len) {
            ((uint8_t *)mem)[n++] = p->rx[p->rx_head];
            p->rx_head = (p->rx_head + 1) % TCP_WND;
            p->rx_len--;
        }
        sockaddr_in_set(from, fromlen, p->rip, p->rport);
        tcp_window_update(p, n);
        return n;
    }
    if (p->err) {
        int e = p->err;
        p->err = 0;
        p->state = T_CLOSED;
        return set_errno(e);
    }
    if (p->rx_fin) {
        return 0;
    }
    return set_errno(ENOTCONN);     // T_CLOSED, error already reported
}

static bool tcp_can_send(struct sock *s) {
    return writable(s);
}

static int tcp_write(struct sock *s, const uint8_t *data, size_t size) {
    if (s->pcb < 0) {
        int e = s->err;
        s->err = 0;
        return set_errno(e ? e : ENOTCONN);
    }
    struct pcb *p = &pcbs[s->pcb];
    size_t sent = 0;
    for (;;) {
        if (p->err) {
            if (sent) {
                return sent;
            }
            int e = p->err;
            p->err = 0;
            p->state = T_CLOSED;
            return set_errno(e);
        }
        if (p->state == T_SYN_SENT) {
            return sent ? (int)sent : set_errno(EINPROGRESS);
        }
        if (p->state != T_ESTABLISHED) {
            return sent ? (int)sent : set_errno(ENOTCONN);
        }
        while (sent < size && p->snd_wnd > 0) {
            size_t n = size - sent;
            if (n > TCP_MSS) {
                n = TCP_MSS;
            }
            if (n > (size_t)p->snd_wnd) {
                n = p->snd_wnd;
            }
            tcp_send(p, TCP_DATA, n, data + sent, n);
            p->snd_wnd -= n;
            sent += n;
        }
        if (sent == size || s->nonblock) {
            return sent ? (int)sent : set_errno(EWOULDBLOCK);
        }
        // Send buffer full: wait for the peer to read (SO_SNDTIMEO)
        if (!wait_for(tcp_can_send, s, s->sndtimeo)) {
            return sent ? (int)sent : set_errno(EWOULDBLOCK);
        }
    }
}

static void deliver_udp(int n, uint32_t src, uint32_t dst, const f_udp_t *u, const uint8_t *data);

static int udp_send(struct sock *s, const void *data, size_t size, uint32_t ip, uint16_t port) {
    if (size > UDP_MAX) {
        return set_errno(EMSGSIZE);
    }
    int n = route(ip);
    if (n == NETIF_NONE) {
        return set_errno(EHOSTUNREACH);
    }
    if (!s->bound) {
        s->lport = ephemeral_port(SOCK_DGRAM);
        s->bound = true;
    }
    uint32_t src = n == NETIF_LOOP ? (IS_LOOPBACK(ip) ? IP4(127, 0, 0, 1) : ip) : netifs[n].ip.ip.addr;
    f_udp_t u = { .sport = s->lport, .dport = port, .len = size };
    send_ip(n, src, ip, IPPROTO_UDP, &u, sizeof(u), data, size);
    return size;
}

int lwip_write(int fd, const void *dataptr, size_t size) {
    struct sock *s = get_sock(fd);
    if (!s) {
        return set_errno(EBADF);
    }
    if (s->type == SOCK_DGRAM) {
        if (!s->connected) {
            return set_errno(ENOTCONN);
        }
        return udp_send(s, dataptr, size, s->rip, s->rport);
    }
    return tcp_write(s, dataptr, size);
}

int lwip_sendto(int fd, const void *dataptr, size_t size, int flags, const struct sockaddr *to, socklen_t tolen) {
    struct sock *s = get_sock(fd);
    if (!s) {
        return set_errno(EBADF);
    }
    if (s->type == SOCK_STREAM) {
        return tcp_write(s, dataptr, size);
    }
    uint32_t ip;
    uint16_t port;
    if (!sockaddr_in_get(to, tolen, &ip, &port)) {
        return set_errno(EINVAL);
    }
    return udp_send(s, dataptr, size, ip, port);
}

static int timeval_ms(const void *optval, socklen_t optlen) {
    const struct timeval *tv = optval;
    return tv->tv_sec * 1000 + tv->tv_usec / 1000;
}

int lwip_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) {
    struct sock *s = get_sock(fd);
    if (!s) {
        return set_errno(EBADF);
    }
    if (level == SOL_SOCKET) {
        switch (optname) {
            case SO_REUSEADDR:
                s->reuse = *(const int *)optval != 0;
                if (s->pcb >= 0) {
                    pcbs[s->pcb].reuse = s->reuse;
                }
                return 0;
            case SO_BROADCAST:
                return 0;       // IP_SOF_BROADCAST is 0: broadcast always allowed
            case SO_RCVTIMEO:
            case SO_SNDTIMEO:
                if (optlen < sizeof(struct timeval)) {
                    return set_errno(EINVAL);
                }
                if (optname == SO_RCVTIMEO) {
                    s->rcvtimeo = timeval_ms(optval, optlen);
                } else {
                    s->sndtimeo = timeval_ms(optval, optlen);
                }
                return 0;
            case SO_BINDTODEVICE:
                return 0;
            default:
                return set_errno(ENOPROTOOPT);
        }
    }
    if (level == IPPROTO_TCP && optname == TCP_NODELAY) {
        return s->type == SOCK_STREAM ? 0 : set_errno(ENOPROTOOPT);
    }
    return set_errno(ENOPROTOOPT);
}

int lwip_getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen) {
    struct sock *s = get_sock(fd);
    if (!s) {
        return set_errno(EBADF);
    }
    if (level == SOL_SOCKET && optname == SO_ERROR) {
        int e = 0;
        if (s->pcb >= 0) {
            e = pcbs[s->pcb].err;
            pcbs[s->pcb].err = 0;
            if (e) {
                pcbs[s->pcb].state = T_CLOSED;
            }
        } else {
            e = s->err;
            s->err = 0;
        }
        *(int *)optval = e;
        *optlen = sizeof(int);
        return 0;
    }
    if (level == SOL_SOCKET && optname == SO_REUSEADDR) {
        *(int *)optval = s->reuse;
        *optlen = sizeof(int);
        return 0;
    }
    return set_errno(ENOPROTOOPT);
}

int lwip_fcntl(int fd, int cmd, int val) {
    struct sock *s = get_sock(fd);
    if (!s) {
        return set_errno(EBADF);
    }
    if (cmd == F_GETFL) {
        return s->nonblock ? O_NONBLOCK : 0;
    }
    if (cmd == F_SETFL) {
        s->nonblock = (val & O_NONBLOCK) != 0;
        return 0;
    }
    return set_errno(ENOSYS);
}

int lwip_fcntl_va(int fd, int cmd, int val, ...) {
    return lwip_fcntl(fd, cmd, val);
}

static int select_scan(int maxfdp1, fd_set *r, fd_set *w, fd_set *e, fd_set *ro, fd_set *wo, fd_set *eo) {
    int count = 0;
    FD_ZERO(ro);
    FD_ZERO(wo);
    FD_ZERO(eo);
    for (int fd = 0; fd < maxfdp1; fd++) {
        bool want_r = r && FD_ISSET(fd, r), want_w = w && FD_ISSET(fd, w), want_e = e && FD_ISSET(fd, e);
        if (!want_r && !want_w && !want_e) {
            continue;
        }
        struct sock *s = get_sock(fd);
        if (!s) {
            return set_errno(EBADF);
        }
        if (want_r && readable(s)) {
            FD_SET(fd, ro);
            count++;
        }
        if (want_w && writable(s)) {
            FD_SET(fd, wo);
            count++;
        }
        if (want_e && has_error(s)) {
            FD_SET(fd, eo);
            count++;
        }
    }
    return count;
}

int lwip_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout) {
    fd_set ro, wo, eo;
    int64_t until = timeout ? NOW() + MS(timeout->tv_sec * 1000 + timeout->tv_usec / 1000) : INT64_MAX;
    int count;
    for (;;) {
        count = select_scan(maxfdp1, readset, writeset, exceptset, &ro, &wo, &eo);
        if (count != 0 || NOW() >= until) {
            break;
        }
        int64_t left = (until - NOW()) / 1000;
        galaxia_poll();
        galaxia_thread_sleep_ms(left > 20 ? 20 : (int)left);
    }
    if (count < 0) {
        return count;
    }
    if (readset) {
        *readset = ro;
    }
    if (writeset) {
        *writeset = wo;
    }
    if (exceptset) {
        *exceptset = eo;
    }
    return count;
}

// A blocking call waits as lwIP's task does: the rest of the board goes on
static bool wait_for(bool (*ready)(struct sock *), struct sock *s, int timeout_ms) {
    int64_t until = timeout_ms > 0 ? NOW() + MS(timeout_ms) : INT64_MAX;
    for (;;) {
        net_poll();
        if (ready(s)) {
            return true;
        }
        if (NOW() >= until) {
            return false;
        }
        int64_t left = (until - NOW()) / 1000;
        galaxia_poll();
        galaxia_thread_sleep_ms(left > 20 ? 20 : left < 1 ? 1 : (int)left);
    }
}

static void net_sleep_ms(int ms) {
    int64_t until = NOW() + MS(ms);
    while (NOW() < until) {
        galaxia_poll();
        int64_t left = (until - NOW()) / 1000;
        galaxia_thread_sleep_ms(left > 20 ? 20 : left < 1 ? 1 : (int)left);
    }
}

// The sockets of a netif that goes down: lwIP aborts their connections
static void sockets_netif_down(int n) {
    for (int i = 0; i < MAX_PCBS; i++) {
        struct pcb *p = &pcbs[i];
        if (p->state != T_FREE && p->netif == n && (p->state == T_ESTABLISHED || p->state == T_SYN_SENT)) {
            p->err = ECONNABORTED;
            p->state = T_CLOSED;
        }
    }
}

// ---- input ----

static struct sock *find_listener(uint32_t ip, uint16_t port) {
    for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++) {
        struct sock *s = &socks[i];
        if (s->used && s->listening && s->lport == port && (s->lip == IP_ANY || s->lip == ip)) {
            return s;
        }
    }
    return NULL;
}

static void deliver_tcp(int n, uint32_t src, uint32_t dst, const f_tcp_t *t, const uint8_t *data) {
    struct pcb *p = NULL;
    for (int i = 0; i < MAX_PCBS; i++) {
        struct pcb *c = &pcbs[i];
        if (c->state != T_FREE && c->lport == t->dport && c->rport == t->sport && c->rip == src && (c->lip == dst || n == NETIF_LOOP)) {
            p = c;
            break;
        }
    }
    if (t->flags & TCP_SYN) {
        struct sock *l = find_listener(dst, t->dport);
        if (!l) {
            tcp_reply_rst(n, dst, src, t);
            return;
        }
        if (p) {
            return;     // duplicate
        }
        if (l->npending >= l->backlog || l->npending >= TCP_ACCEPTMBOX) {
            return;     // backlog full: the SYN is dropped, the client times out
        }
        p = pcb_new();
        if (!p) {
            return;
        }
        p->state = T_ESTABLISHED;
        p->netif = n;
        p->lip = dst;
        p->rip = src;
        p->lport = t->dport;
        p->rport = t->sport;
        p->reuse = l->reuse;
        p->snd_wnd = TCP_WND;
        p->listener = l - socks;
        l->pending[l->npending++] = p - pcbs;
        tcp_send(p, TCP_SYNACK, TCP_WND, NULL, 0);
        return;
    }
    if (!p) {
        // A connection closed here: window updates and FIN are absorbed, data
        // (the application will never read it) is answered with RST
        for (int i = 0; i < MAX_TIME_WAIT; i++) {
            if (time_wait[i].until > NOW() && time_wait[i].lport == t->dport && time_wait[i].rport == t->sport
                && time_wait[i].rip == src && !(t->flags & (TCP_DATA | TCP_SYN))) {
                return;
            }
        }
        if (!(t->flags & TCP_RST)) {
            tcp_reply_rst(n, dst, src, t);
        }
        return;
    }
    if (t->flags & TCP_RST) {
        if (p->listener >= 0) {
            // Reset before accept(): the connection disappears
            struct sock *l = &socks[p->listener];
            for (int i = 0; i < l->npending; i++) {
                if (l->pending[i] == p - pcbs) {
                    memmove(l->pending + i, l->pending + i + 1, (--l->npending - i) * sizeof(l->pending[0]));
                    break;
                }
            }
            pcb_free(p);
            return;
        }
        p->err = ECONNRESET;
        p->state = T_CLOSED;
        return;
    }
    if (t->flags & TCP_SYNACK) {
        if (p->state == T_SYN_SENT) {
            p->state = T_ESTABLISHED;
            p->snd_wnd = t->n;
        }
        return;
    }
    if (p->state != T_ESTABLISHED) {
        return;
    }
    if (t->flags & TCP_DATA) {
        int len = t->n;
        if (p->rx_fin) {
            return;
        }
        for (int i = 0; i < len && p->rx_len < TCP_WND; i++) {
            p->rx[(p->rx_head + p->rx_len++) % TCP_WND] = data[i];
        }
    }
    if (t->flags & TCP_WIN) {
        p->snd_wnd += t->n;
        if (p->snd_wnd > TCP_SND_BUF) {
            p->snd_wnd = TCP_SND_BUF;
        }
    }
    if (t->flags & TCP_FIN) {
        p->rx_fin = true;
    }
}

static void deliver_udp(int n, uint32_t src, uint32_t dst, const f_udp_t *u, const uint8_t *data) {
    bool many = dst == IP_BROADCAST || IS_MULTICAST(dst)
        || (n >= 0 && n < 2 && dst == (netifs[n].ip.ip.addr | ~netifs[n].ip.netmask.addr));
    if (IS_MULTICAST(dst) && !joined(dst)) {
        return;
    }
    for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++) {
        struct sock *s = &socks[i];
        if (!s->used || s->type != SOCK_DGRAM || !s->bound || s->lport != u->dport) {
            continue;
        }
        if (!many && s->lip != IP_ANY && s->lip != dst) {
            continue;
        }
        if (s->connected && (s->rip != src || s->rport != u->sport)) {
            continue;
        }
        if (s->qlen < UDP_RECVMBOX) {
            dgram_t *d = &s->q[(s->qhead + s->qlen++) % UDP_RECVMBOX];
            d->ip = src;
            d->port = u->sport;
            d->len = u->len;
            d->data = malloc(u->len ? u->len : 1);
            memcpy(d->data, data, u->len);
        }
        if (!many) {
            return;     // unicast: the first matching socket
        }
    }
}

static void deliver_ip(int n, uint32_t src, uint32_t dst, uint8_t proto, const uint8_t *payload, size_t len) {
    if (proto == IPPROTO_TCP && len >= sizeof(f_tcp_t)) {
        f_tcp_t t;
        memcpy(&t, payload, sizeof(t));
        deliver_tcp(n, src, dst, &t, payload + sizeof(t));
    } else if (proto == IPPROTO_UDP && len >= sizeof(f_udp_t)) {
        f_udp_t u;
        memcpy(&u, payload, sizeof(u));
        deliver_udp(n, src, dst, &u, payload + sizeof(u));
    }
}

// ================================================================= DNS, mDNS

static struct {
    bool on;
    char hostname[64];
} mdns;

static struct {
    bool waiting;
    char name[64];
    bool answered;
    uint32_t ip;
} mdns_query;

esp_err_t mdns_init(void) {
    mdns.on = true;
    return ESP_OK;
}

esp_err_t mdns_hostname_set(const char *hostname) {
    strncpy(mdns.hostname, hostname, sizeof(mdns.hostname) - 1);
    return ESP_OK;
}

esp_err_t mdns_instance_name_set(const char *instance_name) {
    return ESP_OK;
}

esp_err_t mdns_query_a(const char *host_name, uint32_t timeout, esp_ip4_addr_t *addr) {
    // Itself
    if (mdns.on && !strcasecmp(host_name, mdns.hostname)) {
        for (int n = 0; n < 2; n++) {
            if (netif_up(n)) {
                addr->addr = netifs[n].ip.ip.addr;
                return ESP_OK;
            }
        }
    }
    f_mdns_t q = { 0 };
    strncpy(q.name, host_name, sizeof(q.name) - 1);
    mdns_query.waiting = true;
    mdns_query.answered = false;
    strcpy(mdns_query.name, q.name);
    bool sent = false;
    for (int n = 0; n < 2; n++) {
        if (netif_up(n)) {
            lan_send(F_MDNS_Q, netif_mac(n), BCAST_MAC, netif_bssid(n), &q, sizeof(q), NULL, 0);
            sent = true;
        }
    }
    int64_t until = NOW() + MS(timeout);
    while (sent && !mdns_query.answered && NOW() < until) {
        galaxia_poll();
        galaxia_thread_sleep_ms(20);
    }
    mdns_query.waiting = false;
    if (!mdns_query.answered) {
        return ESP_ERR_NOT_FOUND;
    }
    addr->addr = mdns_query.ip;
    return ESP_OK;
}

int lwip_getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res) {
    *res = NULL;
    if (!nodename && !servname) {
        return EAI_NONAME;
    }
    int port = 0;
    if (servname) {
        char *end;
        long v = strtol(servname, &end, 10);
        if (*end || v < 0 || v > 0xffff) {
            return EAI_SERVICE;     // lwIP: numeric ports only
        }
        port = v;
    }
    if (hints && hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET) {
        return EAI_FAMILY;
    }
    uint32_t ip = IP4(127, 0, 0, 1);
    if (nodename && !ip4_aton(nodename, &ip)) {
        // A name: asked to the DNS server, if there is one. The page's router
        // knows the names of the virtual broker (its whole Internet); for the
        // others, and on a soft-AP, the query times out.
        if (!dns_servers[0].u_addr.ip4.addr || route(dns_servers[0].u_addr.ip4.addr) == NETIF_NONE) {
            return EAI_FAIL;
        }
        uint32_t found = 0;
        if (wifi.sta_state == STA_UP && wifi.bss.page && route(dns_servers[0].u_addr.ip4.addr) == NETIF_STA) {
            found = galaxia_js_dns(nodename);
        }
        if (!found) {
            net_sleep_ms(DNS_TIMEOUT_MS);
            return EAI_FAIL;
        }
        net_sleep_ms(DNS_ANSWER_MS);
        ip = found;
    }
    struct addrinfo *ai = memp_malloc(MEMP_NETDB);
    if (!ai) {
        return EAI_MEMORY;
    }
    struct sockaddr_in *sa = (struct sockaddr_in *)((uint8_t *)ai + sizeof(struct addrinfo));
    sa->sin_len = sizeof(*sa);
    sa->sin_family = AF_INET;
    sa->sin_port = lwip_htons(port);
    sa->sin_addr.s_addr = ip;
    ai->ai_family = AF_INET;
    ai->ai_addr = (struct sockaddr *)sa;
    ai->ai_addrlen = sizeof(struct sockaddr_storage);
    if (hints) {
        ai->ai_socktype = hints->ai_socktype;
        ai->ai_protocol = hints->ai_protocol;
    }
    if (nodename && hints && (hints->ai_flags & AI_CANONNAME)) {
        ai->ai_canonname = (char *)sa + sizeof(struct sockaddr_storage);
        strncpy(ai->ai_canonname, nodename, DNS_MAX_NAME_LENGTH);
    }
    *res = ai;
    return 0;
}

void lwip_freeaddrinfo(struct addrinfo *ai) {
    while (ai) {
        struct addrinfo *next = ai->ai_next;
        free(ai);
        ai = next;
    }
}

// ================================================================= frames in

static void seen_add(const uint8_t *bssid, const f_beacon_t *b) {
    int slot = -1;
    for (int i = 0; i < SEEN_MAX && slot < 0; i++) {
        if (seen[i].seen && mac_eq(seen[i].bssid, bssid)) {
            slot = i;
        }
    }
    for (int i = 0; i < SEEN_MAX && slot < 0; i++) {
        if (!seen[i].seen || NOW() - seen[i].seen > MS(BEACON_TIMEOUT_MS)) {
            slot = i;
        }
    }
    if (slot < 0) {
        return;
    }
    bss_t *s = &seen[slot];
    bool keep_ssid = b->hidden && b->ssid_len == 0 && mac_eq(s->bssid, bssid);
    if (!keep_ssid) {
        memset(s->ssid, 0, sizeof(s->ssid));
        memcpy(s->ssid, b->ssid, b->ssid_len);
        s->ssid_len = b->ssid_len;
    }
    memcpy(s->bssid, bssid, 6);
    s->page = false;
    s->channel = b->channel;
    s->authmode = b->authmode;
    s->hidden = b->hidden;
    s->rssi = -50 - (int)(mp_js_random_u32() % 8);
    s->seen = NOW();
}

static int ap_station(const uint8_t *mac) {
    for (int i = 0; i < wifi.nstations; i++) {
        if (mac_eq(wifi.stations[i].mac, mac)) {
            return i;
        }
    }
    return -1;
}

static void ap_frame(const frame_hdr_t *h, const uint8_t *body, size_t len) {
    const uint8_t *ap_mac = wifi.mac[WIFI_IF_AP];
    int st = ap_station(h->src);
    if (st >= 0) {
        wifi.stations[st].seen = NOW();
    }
    switch (h->kind) {
        case F_ASSOC_REQ: {
            const wifi_ap_config_t *ap = &wifi.cfg[WIFI_IF_AP].ap;
            uint8_t status = 0;
            if (ap->authmode != WIFI_AUTH_OPEN && (len < 64 || strncmp((const char *)body, (const char *)ap->password, 64))) {
                status = WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT;
            } else if (st < 0 && wifi.nstations >= ap->max_connection) {
                status = WIFI_REASON_ASSOC_TOOMANY;
            } else if (st < 0) {
                st = wifi.nstations++;
                memcpy(wifi.stations[st].mac, h->src, 6);
                wifi.stations[st].ip = 0;
                wifi.stations[st].seen = NOW();
                post_event(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, NULL, 0);
            }
            lan_send(F_ASSOC_RESP, ap_mac, h->src, ap_mac, &status, 1, NULL, 0);
            return;
        }
        case F_DEAUTH:
            if (st >= 0) {
                ap_remove_station(st, false, 0);
            }
            return;
        case F_DHCP_REQ: {
            if (st < 0 || netifs[NETIF_AP].dhcp != ESP_NETIF_DHCP_STARTED) {
                return;
            }
            int lease = -1;
            for (int i = 0; i < DHCPS_MAX; i++) {
                if (mac_eq(wifi.leases[i], h->src)) {
                    lease = i;
                    break;
                }
            }
            for (int i = 0; lease < 0 && i < DHCPS_MAX; i++) {
                bool used = false;
                for (int j = 0; j < wifi.nstations; j++) {
                    used |= mac_eq(wifi.stations[j].mac, wifi.leases[i]);
                }
                if (!used) {
                    lease = i;
                    memcpy(wifi.leases[i], h->src, 6);
                }
            }
            if (lease < 0) {
                return;     // pool exhausted
            }
            const esp_netif_ip_info_t *ip = &netifs[NETIF_AP].ip;
            uint32_t host = (ip->ip.addr & ip->netmask.addr) | IP4(0, 0, 0, 2 + lease);
            f_dhcp_t ack = { .ip = host, .mask = ip->netmask.addr, .gw = ip->ip.addr, .dns = netifs[NETIF_AP].dns.addr };
            wifi.stations[st].ip = host;
            lan_send(F_DHCP_ACK, ap_mac, h->src, ap_mac, &ack, sizeof(ack), NULL, 0);
            post_event(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, NULL, 0);
            return;
        }
        default:
            return;
    }
}

static void mdns_answer(int n, const frame_hdr_t *h, const f_mdns_t *q) {
    if (!mdns.on || strcasecmp(q->name, mdns.hostname) || !netif_up(n)) {
        return;
    }
    f_mdns_t a = { 0 };
    strcpy(a.name, q->name);
    a.ip = netifs[n].ip.ip.addr;
    lan_send(F_MDNS_A, netif_mac(n), h->src, netif_bssid(n), &a, sizeof(a), NULL, 0);
}

static void lan_frame(const uint8_t *frame, int len) {
    if (len < (int)sizeof(frame_hdr_t) || !wifi.started) {
        return;
    }
    frame_hdr_t h;
    memcpy(&h, frame, sizeof(h));
    const uint8_t *body = frame + sizeof(h);
    size_t body_len = len - sizeof(h);
    bool to_sta = mac_eq(h.dst, wifi.mac[WIFI_IF_STA]);
    bool to_ap = mac_eq(h.dst, wifi.mac[WIFI_IF_AP]);
    bool bcast = mac_eq(h.dst, BCAST_MAC);
    if (!to_sta && !to_ap && !bcast) {
        return;
    }
    bool on_sta = (wifi.mode & WIFI_MODE_STA) && sta_associated() && mac_eq(h.bssid, wifi.bss.bssid);
    bool on_ap = wifi.ap_up && mac_eq(h.bssid, wifi.mac[WIFI_IF_AP]);
    switch (h.kind) {
        case F_PROBE_REQ:
            if (wifi.ap_up) {
                const wifi_ap_config_t *ap = &wifi.cfg[WIFI_IF_AP].ap;
                bool reveal = body_len >= 33 && body[32] == ap->ssid_len && !memcmp(body, ap->ssid, ap->ssid_len);
                send_beacon(h.src, reveal);
            }
            return;
        case F_BEACON:
            if ((wifi.mode & WIFI_MODE_STA) && body_len >= sizeof(f_beacon_t)) {
                f_beacon_t b;
                memcpy(&b, body, sizeof(b));
                seen_add(h.bssid, &b);
                if (sta_associated() && mac_eq(h.bssid, wifi.bss.bssid)) {
                    wifi.last_beacon = NOW();
                    wifi.bss.channel = b.channel;
                }
            }
            return;
        case F_ASSOC_RESP:
            if (to_sta && wifi.sta_state == STA_AUTH && mac_eq(h.bssid, wifi.bss.bssid) && body_len >= 1) {
                wifi.assoc_answer = true;
                wifi.assoc_status = body[0];
            }
            return;
        case F_DHCP_ACK:
            if (to_sta && wifi.sta_state == STA_DHCP && mac_eq(h.bssid, wifi.bss.bssid) && body_len >= sizeof(f_dhcp_t)) {
                memcpy(&wifi.dhcp_offer, body, sizeof(f_dhcp_t));
                wifi.dhcp_answer = true;
                wifi.sta_due = NOW();
            }
            return;
        case F_DEAUTH:
            if (to_ap) {
                ap_frame(&h, body, body_len);
            } else if (to_sta && sta_associated() && mac_eq(h.bssid, wifi.bss.bssid)) {
                sta_disconnected(body_len ? body[0] : WIFI_REASON_AUTH_LEAVE);
            }
            return;
        case F_ASSOC_REQ:
        case F_DHCP_REQ:
        case F_KEEPALIVE:
            if (to_ap && wifi.ap_up && mac_eq(h.bssid, wifi.mac[WIFI_IF_AP])) {
                ap_frame(&h, body, body_len);
            }
            return;
        case F_ARP_PROBE: {
            uint32_t ip;
            if (body_len < 4) {
                return;
            }
            memcpy(&ip, body, 4);
            if ((on_sta && netif_up(NETIF_STA) && netifs[NETIF_STA].ip.ip.addr == ip)
                || (on_ap && netifs[NETIF_AP].ip.ip.addr == ip)) {
                lan_send(F_ARP_REPLY, on_sta ? wifi.mac[WIFI_IF_STA] : wifi.mac[WIFI_IF_AP], h.src, h.bssid, &ip, 4, NULL, 0);
            }
            return;
        }
        case F_ARP_REPLY: {
            uint32_t ip;
            if (to_sta && body_len >= 4 && wifi.sta_state == STA_DHCP) {
                memcpy(&ip, body, 4);
                if (ip == wifi.candidate) {
                    wifi.candidate_taken = true;
                    wifi.sta_due = NOW();
                }
            }
            return;
        }
        case F_MDNS_Q:
            if (body_len >= sizeof(f_mdns_t) && (on_sta || on_ap)) {
                f_mdns_t q;
                memcpy(&q, body, sizeof(q));
                q.name[sizeof(q.name) - 1] = 0;
                mdns_answer(on_sta ? NETIF_STA : NETIF_AP, &h, &q);
            }
            return;
        case F_MDNS_A:
            if (body_len >= sizeof(f_mdns_t) && mdns_query.waiting && (to_sta || to_ap)) {
                f_mdns_t a;
                memcpy(&a, body, sizeof(a));
                a.name[sizeof(a.name) - 1] = 0;
                if (!strcasecmp(a.name, mdns_query.name) && !mdns_query.answered) {
                    mdns_query.answered = true;
                    mdns_query.ip = a.ip;
                }
            }
            return;
        case F_IP: {
            if (body_len < sizeof(f_ip_t)) {
                return;
            }
            int n = on_sta && netif_up(NETIF_STA) ? NETIF_STA : on_ap ? NETIF_AP : NETIF_NONE;
            if (n == NETIF_NONE) {
                return;
            }
            if (n == NETIF_AP) {
                int st = ap_station(h.src);
                if (st >= 0) {
                    wifi.stations[st].seen = NOW();
                }
            }
            f_ip_t ip;
            memcpy(&ip, body, sizeof(ip));
            const esp_netif_ip_info_t *info = &netifs[n].ip;
            uint32_t subnet_bcast = info->ip.addr | ~info->netmask.addr;
            if (ip.dst != info->ip.addr && ip.dst != IP_BROADCAST && ip.dst != subnet_bcast && !IS_MULTICAST(ip.dst)) {
                return;
            }
            deliver_ip(n, ip.src, ip.dst, ip.proto, body + sizeof(ip), body_len - sizeof(ip));
            return;
        }
        default:
            return;
    }
}

// ================================================================= poll

static void lan_poll(void) {
    int len;
    while ((len = galaxia_js_lan_next(rx_frame, sizeof(rx_frame))) >= 0) {
        lan_frame(rx_frame, len);
    }
}

static void tcp_timers(void) {
    int64_t now = NOW();
    for (int i = 0; i < MAX_PCBS; i++) {
        struct pcb *p = &pcbs[i];
        if (p->state == T_SYN_SENT && now >= p->syn_deadline) {
            p->err = ECONNABORTED;  // SYN retransmissions exhausted
            p->state = T_CLOSED;
        }
    }
}

void net_poll(void) {
    static bool busy;
    if (busy) {
        return;
    }
    busy = true;
    radio_poll();
    lan_poll();
    loop_poll();
    wifi_tick();
    tcp_timers();
    run_events();
    busy = false;
}
