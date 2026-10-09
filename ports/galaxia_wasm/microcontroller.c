// The firmware's microcontroller module (ports/esp32/modmicrocontroller.c), as it is.
// It includes "mphalport.h" by its quoted name, which finds the esp32 port's (Xtensa
// assembly) next to it: ours comes first, under the same include guard.
#include "mphalport.h"
#include "ports/esp32/modmicrocontroller.c"
