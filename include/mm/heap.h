#ifndef FLUXMAX_HEAP_H
#define FLUXMAX_HEAP_H
#include <stddef.h>
#include <stdint.h>
int heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t heap_capacity(void);
size_t heap_bytes_used(void);
#endif
