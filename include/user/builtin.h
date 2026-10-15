#ifndef FLUXMAX_USER_BUILTIN_H
#define FLUXMAX_USER_BUILTIN_H
#include <stddef.h>
#include <stdint.h>
extern const uint8_t fluxmax_builtin_hello_elf_start[];
extern const uint8_t fluxmax_builtin_hello_elf_end[];
static inline size_t fluxmax_builtin_hello_elf_size(void) {
    return (size_t)(fluxmax_builtin_hello_elf_end - fluxmax_builtin_hello_elf_start);
}
#endif
