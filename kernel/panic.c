#include "kernel/panic.h"
#include "kernel/log.h"
#include "arch/x86_64/io.h"
NORETURN void panic(const char *reason) {
    cpu_cli();
    log_write(LOG_FATAL, "panic", reason ? reason : "unknown fatal error");
    for (;;) cpu_hlt();
}
