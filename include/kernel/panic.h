#ifndef FLUXMAX_PANIC_H
#define FLUXMAX_PANIC_H
#include "kernel/types.h"
NORETURN void panic(const char *reason);
#endif
