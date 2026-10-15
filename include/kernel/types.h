#ifndef FLUXMAX_KERNEL_TYPES_H
#define FLUXMAX_KERNEL_TYPES_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef uint64_t paddr_t;
typedef uint64_t vaddr_t;
typedef int64_t ssize_t;
#define PACKED __attribute__((packed))
#define ALIGNED(n) __attribute__((aligned(n)))
#define UNUSED __attribute__((unused))
#define NORETURN __attribute__((noreturn))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define PAGE_SIZE 4096ULL
#endif
