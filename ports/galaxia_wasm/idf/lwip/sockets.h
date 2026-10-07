// ESP-IDF shim for the WebAssembly build: see idf_net.h
#include "../idf_all.h"

// As ESP-IDF's VFS, which hands the socket file descriptors to lwIP
#ifndef GALAXIA_LWIP_SOCKETS_H
#define GALAXIA_LWIP_SOCKETS_H
#define select lwip_select
#define fcntl(...) lwip_fcntl_va(__VA_ARGS__, 0)
#define getsockopt lwip_getsockopt
#endif
