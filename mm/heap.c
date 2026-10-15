#include "mm/heap.h"
#include "arch/x86_64/io.h"
#include "lib/mem.h"
#include <stdint.h>

#define HEAP_BYTES (12u * 1024u * 1024u)
#define HEAP_MAGIC 0x54594850u
#define ALIGN16(v) (((v) + 15u) & ~(size_t)15u)

typedef struct heap_block {
    size_t size;
    struct heap_block *next;
    struct heap_block *prev;
    uint32_t magic;
    uint8_t free;
    uint8_t pad[11];
} __attribute__((aligned(16))) heap_block_t;

static uint8_t arena[HEAP_BYTES] __attribute__((aligned(4096)));
static heap_block_t *first;
static size_t used_bytes;
static int ready;

static uint64_t lock_heap(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}
static void unlock_heap(uint64_t flags) {
    if (flags & (1ULL << 9)) __asm__ volatile("sti" : : : "memory");
}
static int inside_arena(const void *p) {
    uintptr_t v = (uintptr_t)p, lo = (uintptr_t)arena;
    return v >= lo && v < lo + sizeof(arena);
}

int heap_init(void) {
    uint64_t f = lock_heap();
    memset(arena, 0, sizeof(heap_block_t));
    first = (heap_block_t *)arena;
    first->size = sizeof(arena) - sizeof(*first);
    first->next = 0; first->prev = 0; first->magic = HEAP_MAGIC; first->free = 1;
    used_bytes = 0; ready = 1;
    unlock_heap(f);
    return 0;
}

void *kmalloc(size_t size) {
    if (!size) return 0;
    if (!ready && heap_init() != 0) return 0;
    if (size > SIZE_MAX - 15u) return 0;
    size = ALIGN16(size);
    uint64_t f = lock_heap();
    for (heap_block_t *b = first; b; b = b->next) {
        if (b->magic != HEAP_MAGIC) { unlock_heap(f); return 0; }
        if (!b->free || b->size < size) continue;
        if (b->size >= size + sizeof(heap_block_t) + 16u) {
            heap_block_t *split = (heap_block_t *)((uint8_t *)(b + 1) + size);
            split->size = b->size - size - sizeof(*split);
            split->next = b->next; split->prev = b; split->magic = HEAP_MAGIC; split->free = 1;
            if (split->next) split->next->prev = split;
            b->next = split; b->size = size;
        }
        b->free = 0; used_bytes += b->size;
        void *result = b + 1;
        unlock_heap(f);
        return result;
    }
    unlock_heap(f);
    return 0;
}

void kfree(void *ptr) {
    if (!ptr || !inside_arena(ptr)) return;
    heap_block_t *b = ((heap_block_t *)ptr) - 1;
    if (!inside_arena(b)) return;
    uint64_t f = lock_heap();
    if (b->magic != HEAP_MAGIC || b->free || (uint8_t *)(b + 1) > arena + sizeof(arena)) {
        unlock_heap(f); return;
    }
    b->free = 1;
    if (used_bytes >= b->size) used_bytes -= b->size; else used_bytes = 0;
    if (b->next && b->next->magic == HEAP_MAGIC && b->next->free) {
        heap_block_t *n = b->next;
        b->size += sizeof(*n) + n->size;
        b->next = n->next;
        if (b->next) b->next->prev = b;
        n->magic = 0;
    }
    if (b->prev && b->prev->magic == HEAP_MAGIC && b->prev->free) {
        heap_block_t *p = b->prev;
        p->size += sizeof(*b) + b->size;
        p->next = b->next;
        if (p->next) p->next->prev = p;
        b->magic = 0;
    }
    unlock_heap(f);
}

size_t heap_capacity(void) { return sizeof(arena); }
size_t heap_bytes_used(void) { return used_bytes; }
