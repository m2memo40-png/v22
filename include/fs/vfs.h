#ifndef FLUXMAX_VFS_H
#define FLUXMAX_VFS_H
#include <stddef.h>
#include <stdint.h>

typedef enum { VFS_NODE_FILE = 1, VFS_NODE_DIRECTORY = 2 } vfs_node_type_t;
typedef struct vfs_node vfs_node_t;
struct vfs_node {
    char name[64];
    vfs_node_type_t type;
    uint64_t size;
    uint64_t capacity;
    uint8_t *data;
    vfs_node_t *parent;
    vfs_node_t *first_child;
    vfs_node_t *next_sibling;
};
typedef int (*vfs_list_fn)(const vfs_node_t *node, void *context);

int vfs_init(void);
vfs_node_t *vfs_root(void);
vfs_node_t *vfs_lookup(const char *path);
vfs_node_t *vfs_create(const char *path, vfs_node_type_t type);
int vfs_mkdir(const char *path);
int vfs_unlink(const char *path);
int64_t vfs_read(vfs_node_t *node, uint64_t offset, void *buffer, size_t length);
int64_t vfs_write(vfs_node_t *node, uint64_t offset, const void *buffer, size_t length);
int vfs_list(vfs_node_t *directory, vfs_list_fn visit, void *context);
#endif
