// errno values of the Galaxia's C library (newlib of the ESP-IDF 5.4 toolchain,
// xtensa-esp-elf/include/sys/errno.h), for the WebAssembly build: the firmware's
// OSError numbers are newlib's (ETIMEDOUT is 116, not Linux's 110), and programs
// test them (e.errno == 116). Emscripten's libc keeps its own values for its own
// errors; only the numbers MicroPython and the network shim use are these.
#ifndef GALAXIA_NEWLIB_ERRNO_H
#define GALAXIA_NEWLIB_ERRNO_H

#include_next <errno.h>

#undef EPERM
#define EPERM 1
#undef ENOENT
#define ENOENT 2
#undef ESRCH
#define ESRCH 3
#undef EINTR
#define EINTR 4
#undef EIO
#define EIO 5
#undef ENXIO
#define ENXIO 6
#undef E2BIG
#define E2BIG 7
#undef ENOEXEC
#define ENOEXEC 8
#undef EBADF
#define EBADF 9
#undef ECHILD
#define ECHILD 10
#undef EAGAIN
#define EAGAIN 11
#undef ENOMEM
#define ENOMEM 12
#undef EACCES
#define EACCES 13
#undef EFAULT
#define EFAULT 14
#undef ENOTBLK
#define ENOTBLK 15
#undef EBUSY
#define EBUSY 16
#undef EEXIST
#define EEXIST 17
#undef EXDEV
#define EXDEV 18
#undef ENODEV
#define ENODEV 19
#undef ENOTDIR
#define ENOTDIR 20
#undef EISDIR
#define EISDIR 21
#undef EINVAL
#define EINVAL 22
#undef ENFILE
#define ENFILE 23
#undef EMFILE
#define EMFILE 24
#undef ENOTTY
#define ENOTTY 25
#undef ETXTBSY
#define ETXTBSY 26
#undef EFBIG
#define EFBIG 27
#undef ENOSPC
#define ENOSPC 28
#undef ESPIPE
#define ESPIPE 29
#undef EROFS
#define EROFS 30
#undef EMLINK
#define EMLINK 31
#undef EPIPE
#define EPIPE 32
#undef EDOM
#define EDOM 33
#undef ERANGE
#define ERANGE 34
#undef ENOMSG
#define ENOMSG 35
#undef EIDRM
#define EIDRM 36
#undef ECHRNG
#define ECHRNG 37
#undef EL2NSYNC
#define EL2NSYNC 38
#undef EL3HLT
#define EL3HLT 39
#undef EL3RST
#define EL3RST 40
#undef ELNRNG
#define ELNRNG 41
#undef EUNATCH
#define EUNATCH 42
#undef ENOCSI
#define ENOCSI 43
#undef EL2HLT
#define EL2HLT 44
#undef EDEADLK
#define EDEADLK 45
#undef ENOLCK
#define ENOLCK 46
#undef EBADE
#define EBADE 50
#undef EBADR
#define EBADR 51
#undef EXFULL
#define EXFULL 52
#undef ENOANO
#define ENOANO 53
#undef EBADRQC
#define EBADRQC 54
#undef EBADSLT
#define EBADSLT 55
#undef EDEADLOCK
#define EDEADLOCK 56
#undef EBFONT
#define EBFONT 57
#undef ENOSTR
#define ENOSTR 60
#undef ENODATA
#define ENODATA 61
#undef ETIME
#define ETIME 62
#undef ENOSR
#define ENOSR 63
#undef ENONET
#define ENONET 64
#undef ENOPKG
#define ENOPKG 65
#undef EREMOTE
#define EREMOTE 66
#undef ENOLINK
#define ENOLINK 67
#undef EADV
#define EADV 68
#undef ESRMNT
#define ESRMNT 69
#undef ECOMM
#define ECOMM 70
#undef EPROTO
#define EPROTO 71
#undef EMULTIHOP
#define EMULTIHOP 74
#undef ELBIN
#define ELBIN 75
#undef EDOTDOT
#define EDOTDOT 76
#undef EBADMSG
#define EBADMSG 77
#undef EFTYPE
#define EFTYPE 79
#undef ENOTUNIQ
#define ENOTUNIQ 80
#undef EBADFD
#define EBADFD 81
#undef EREMCHG
#define EREMCHG 82
#undef ELIBACC
#define ELIBACC 83
#undef ELIBBAD
#define ELIBBAD 84
#undef ELIBSCN
#define ELIBSCN 85
#undef ELIBMAX
#define ELIBMAX 86
#undef ELIBEXEC
#define ELIBEXEC 87
#undef ENOSYS
#define ENOSYS 88
#undef ENMFILE
#define ENMFILE 89
#undef ENOTEMPTY
#define ENOTEMPTY 90
#undef ENAMETOOLONG
#define ENAMETOOLONG 91
#undef ELOOP
#define ELOOP 92
#undef EOPNOTSUPP
#define EOPNOTSUPP 95
#undef EPFNOSUPPORT
#define EPFNOSUPPORT 96
#undef ECONNRESET
#define ECONNRESET 104
#undef ENOBUFS
#define ENOBUFS 105
#undef EAFNOSUPPORT
#define EAFNOSUPPORT 106
#undef EPROTOTYPE
#define EPROTOTYPE 107
#undef ENOTSOCK
#define ENOTSOCK 108
#undef ENOPROTOOPT
#define ENOPROTOOPT 109
#undef ESHUTDOWN
#define ESHUTDOWN 110
#undef ECONNREFUSED
#define ECONNREFUSED 111
#undef EADDRINUSE
#define EADDRINUSE 112
#undef ECONNABORTED
#define ECONNABORTED 113
#undef ENETUNREACH
#define ENETUNREACH 114
#undef ENETDOWN
#define ENETDOWN 115
#undef ETIMEDOUT
#define ETIMEDOUT 116
#undef EHOSTDOWN
#define EHOSTDOWN 117
#undef EHOSTUNREACH
#define EHOSTUNREACH 118
#undef EINPROGRESS
#define EINPROGRESS 119
#undef EALREADY
#define EALREADY 120
#undef EDESTADDRREQ
#define EDESTADDRREQ 121
#undef EMSGSIZE
#define EMSGSIZE 122
#undef EPROTONOSUPPORT
#define EPROTONOSUPPORT 123
#undef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT 124
#undef EADDRNOTAVAIL
#define EADDRNOTAVAIL 125
#undef ENETRESET
#define ENETRESET 126
#undef EISCONN
#define EISCONN 127
#undef ENOTCONN
#define ENOTCONN 128
#undef ETOOMANYREFS
#define ETOOMANYREFS 129
#undef EPROCLIM
#define EPROCLIM 130
#undef EUSERS
#define EUSERS 131
#undef EDQUOT
#define EDQUOT 132
#undef ESTALE
#define ESTALE 133
#undef ENOTSUP
#define ENOTSUP 134
#undef ENOMEDIUM
#define ENOMEDIUM 135
#undef ENOSHARE
#define ENOSHARE 136
#undef ECASECLASH
#define ECASECLASH 137
#undef EILSEQ
#define EILSEQ 138
#undef EOVERFLOW
#define EOVERFLOW 139
#undef ECANCELED
#define ECANCELED 140
#undef ENOTRECOVERABLE
#define ENOTRECOVERABLE 141
#undef EOWNERDEAD
#define EOWNERDEAD 142
#undef ESTRPIPE
#define ESTRPIPE 143
#undef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN

#endif
