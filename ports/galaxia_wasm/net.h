// The Galaxia's Wi-Fi chip and network stack for the WebAssembly build (net.c)
#ifndef GALAXIA_NET_H
#define GALAXIA_NET_H

// Delivers the frames received from the other boards (LAN, ESP-NOW) and the Wi-Fi
// events; called from idf_shim_poll()
void net_poll(void);

#endif
